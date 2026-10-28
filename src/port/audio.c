/*
 * audio.c - Previous's sound output for BMNeXT.
 *
 * Replaces Previous's src/gui-sdl/sdlaudio.c (an SDL audio stream) with the
 * kernel's sound output (host.h). The NeXT plays 16-bit big endian stereo at
 * 44.1 kHz (snd.c); its DMA hands each batch to Audio_Output_Queue_Put(),
 * which plays it at once, converted to the output's rate (48 kHz over the
 * Pi 4 and 5's HDMI). Previous sends faster than it plays and waits while
 * Audio_Output_Queue_Size() says the output has enough (over half full),
 * so its pace is the output's.
 *
 * Sound input (the NeXT's microphone, and the DSP's) is silence.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

const char Audio_fileid[] = "BMNeXT audio.c";

#include "main.h"
#include "audio.h"
#include "configuration.h"

#include "../host.h"
#include "bmnext.h"

#include <math.h>

#define NEXT_RATE 44100

static bool playing;
static unsigned out_rate;            /* 0: no sound */
static unsigned out_latency;         /* frames */
static int16_t silence[2048 * 2];
static int volume = -1;              /* 0-100, -1 until read */

/* ---- Volume ---------------------------------------------------------------- */

int bmnext_volume(void)
{
	if (volume < 0) {
		volume = host_option_int("volume", 100);
		if (volume < 0)
			volume = 0;
		if (volume > 100)
			volume = 100;
	}
	return volume;
}

void bmnext_set_volume(int v)
{
	char text[8];
	volume = v < 0 ? 0 : v > 100 ? 100 : v;
	snprintf(text, sizeof text, "%d", volume);
	if (!host_option_set("volume", text) || !host_options_save())
		printf("BMNeXT: cannot save volume= in bmnext.cfg\n");
}

/* N stereo frames to the output, at the volume. */
static void write_frames(const int16_t *samples, unsigned n)
{
	static int16_t scaled[1024 * 2];
	int v = bmnext_volume();
	double t = host_time();
	if (v >= 100) {
		host_sound_write(samples, n);
		bmnext_prof[PROF_SOUND] += host_time() - t;
		return;
	}
	while (n > 0) {
		unsigned chunk = n > 1024 ? 1024 : n, i;
		for (i = 0; i < chunk * 2; i++)
			scaled[i] = (int16_t) (samples[i] * v / 100);
		host_sound_write(scaled, chunk);
		samples += chunk * 2;
		n -= chunk;
	}
	bmnext_prof[PROF_SOUND] += host_time() - t;
}

/* ---- Output ---------------------------------------------------------------- */

unsigned bmnext_sound_open(void)
{
	unsigned ms = (unsigned) host_option_int("audio_buffer_ms", 120);
	if (ms < 20)
		ms = 20;
	if (ms > 500)
		ms = 500;
	/* The latency is asked for in 44.1 kHz frames (host.h). */
	out_rate = host_sound_open(NEXT_RATE, 2, 44100 * ms / 1000);
	out_latency = (out_rate ? out_rate : NEXT_RATE) * ms / 1000;
	if (out_rate == 0)
		printf("BMNeXT: no sound\n");
	return out_rate;
}

void bmnext_feed_silence(void)
{
	unsigned frames;
	if (out_rate == 0)
		return;
	frames = host_sound_free_frames();
	if (frames > out_latency / 2) {
		frames -= out_latency / 2;
		while (frames > 0) {
			unsigned n = frames > 2048 ? 2048 : frames;
			host_sound_write(silence, n);
			frames -= n;
		}
	}
}

#define PLAY_CHUNK 512

/* N stereo frames resampled to WANT (linear interpolation, in 16.16 fixed
   point) into OUT. */
static void resample(const int16_t *in, unsigned n, int16_t *out, unsigned want)
{
	unsigned i, ch;
	for (i = 0; i < want; i++) {
		uint32_t pos = want > 1 ? (uint32_t) (((uint64_t) i * (n - 1) << 16) / (want - 1)) : 0;
		unsigned k = pos >> 16;
		int frac = (int) (pos & 0xffff);
		for (ch = 0; ch < 2; ch++) {
			int a = in[k * 2 + ch];
			int b = k + 1 < n ? in[(k + 1) * 2 + ch] : a;
			out[i * 2 + ch] = (int16_t) (a + (((b - a) * frac) >> 16));
		}
	}
}

/* Up to PLAY_CHUNK stereo frames (at 44.1 kHz) to the output, at its rate.
   The NeXT waits while the output has enough (Audio_Output_Queue_Size()),
   so the sound keeps pace by itself: converted at exactly the two rates'
   ratio, its pitch true. Only when the output has almost run dry (the
   NeXT slower than real time: a Pi too slow for the machine) is it
   stretched, to up to twice its length, lower in pitch, rather than gaps.
   The Pi 5's HDMI takes 32 ms at a time, so "almost dry" is an eighth of
   the buffer, with the default 120 ms buffer still more than a transfer
   below half full. */
static void play_chunk(const int16_t *samples, unsigned n)
{
	static int16_t buf[(PLAY_CHUNK * 4 + 1) * 2];
	static double carry;   /* the fraction of a frame left over */
	unsigned free_frames, used, want, dry;
	double scale = (double) out_rate / NEXT_RATE;
	free_frames = host_sound_free_frames();
	used = free_frames < out_latency ? out_latency - free_frames : 0;
	dry = out_latency / 8;
	if (n < 2) {
		want = n;
	} else if (used < dry) {
		want = (unsigned) (n * scale) + 1 + (unsigned) (n * scale) * (dry - used) / dry;
		carry = 0.0;
	} else {
		double exact = n * scale + carry;
		want = (unsigned) exact;
		carry = exact - want;
	}
	if (want > PLAY_CHUNK * 4 + 1)
		want = PLAY_CHUNK * 4 + 1;
	if (want != n) {
		resample(samples, n, buf, want);
		samples = buf;
		n = want;
	}
	if (n > free_frames) {
		/* No room at all (far ahead): dropped. */
		static int told;
		if (!told) {
			printf("BMNeXT: sound: no room, %u frames dropped\n", n - free_frames);
			told = 1;
		}
		n = free_frames;
	}
	if (n > 0)
		write_frames(samples, n);
}

/* What the output's buffer holds, in the NeXT's bytes (4 a frame), once it
   is over half full; 0 until then (Audio_Output_Queue_Size()). */
static int output_queued(void)
{
	unsigned free_frames, used;
	if (out_rate == 0)
		return 0;
	free_frames = host_sound_free_frames();
	used = free_frames < out_latency ? out_latency - free_frames : 0;
	if (used <= out_latency / 2)
		return 0;
	return (int) ((uint64_t) used * NEXT_RATE / out_rate) * 4;
}

/* ---- A test tone ------------------------------------------------------------ */

void bmnext_test_tone(int next_way)
{
	int16_t chunk[PLAY_CHUNK * 2];
	unsigned rate = next_way ? NEXT_RATE : out_rate;
	unsigned total, done = 0, i;
	double phase = 0.0, step;
	if (out_rate == 0)
		return;
	total = rate * 2;
	step = 2.0 * M_PI * 440.0 / rate;
	printf("BMNeXT: test tone, 440 Hz at %u Hz%s\n", rate,
	       next_way ? " (the NeXT's way)" : "");
	while (done < total) {
		unsigned n = total - done > PLAY_CHUNK ? PLAY_CHUNK : total - done;
		if (next_way ? output_queued() > 0
		             : host_sound_free_frames() < n) {
			host_sleep_us(1000);
			continue;
		}
		for (i = 0; i < n; i++) {
			int16_t v = (int16_t) (sin(phase) * 12000.0);
			chunk[i * 2] = chunk[i * 2 + 1] = v;
			phase += step;
			if (phase > 2.0 * M_PI)
				phase -= 2.0 * M_PI;
		}
		if (next_way)
			play_chunk(chunk, n);
		else
			write_frames(chunk, n);
		done += n;
	}
}

/* ---- Previous's audio functions ---------------------------------------------- */

void Audio_Output_Init(int channels, int freq)
{
	if (channels != 2 || freq != NEXT_RATE)
		printf("BMNeXT: sound at %d Hz, %d channel(s)? Expected 44100, 2\n", freq, channels);
}

void Audio_Output_UnInit(void)
{
	playing = false;
}

void Audio_Output_Enable(bool bEnable)
{
	playing = bEnable && ConfigureParams.Sound.bEnableSound;
}

/* LEN bytes of 16-bit big endian stereo. */
void Audio_Output_Queue_Put(uint8_t *data, int len)
{
	int16_t frames[PLAY_CHUNK * 2];
	int n = len / 4;
	if (!playing || out_rate == 0)
		return;
	while (n > 0) {
		int m = n > PLAY_CHUNK ? PLAY_CHUNK : n, i;
		for (i = 0; i < m * 2; i++)
			frames[i] = (int16_t) ((data[i * 2] << 8) | data[i * 2 + 1]);
		play_chunk(frames, (unsigned) m);
		data += m * 4;
		n -= m;
	}
}

void Audio_Output_Queue_Flush(void)
{
}

void Audio_Output_Queue_Clear(void)
{
}

/* What is waiting to be played, in the NeXT's bytes (4 a frame), once the
   output's buffer is over half full, as Previous's SDL version has it (0
   until then). The NeXT's sound DMA sends a sample every 8 us, three times
   faster than they play (snd.c), and waits while this isn't 0: so the
   sound keeps pace with the output, whatever speed the NeXT runs at. */
int Audio_Output_Queue_Size(void)
{
	return playing ? output_queued() : 0;
}

/* ---- Recording: silence ---------------------------------------------------- */

void Audio_Input_InitAndEnable(int channels, int freq)
{
}

void Audio_Input_UnInit(void)
{
}

int Audio_Input_Buffer_Get(int16_t *sample)
{
	*sample = 0;
	return 0;
}

int Audio_Input_Buffer_Size(void)
{
	return 0;
}

void Audio_DSP_InitAndEnable(int channels, int freq)
{
}

void Audio_DSP_UnInit(void)
{
}

int Audio_DSP_Buffer_Get(int16_t *sample)
{
	*sample = 0;
	return 0;
}
