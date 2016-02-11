/*
 * by @lazykuna
 */

#ifdef WAV_MUSIC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "SDL_mutex.h"
#include "SDL_endian.h"
#include "SDL_timer.h"

#include "SDL_mixer.h"
#include "load_wav.h"

#define RIFF_HEADER		0x46464952
#define WAVE_HEADER		0x45564157
#define FMT_CHUNK		0x20746D66
#define DATA_CHUNK		0x61746164
#define FMT_BYTE_SIZE	16
#define FMT_ID			1
#define STRICT_CHECK	0

static int sdl_read_int(SDL_RWops *fp) {
	int r = 0;
	int r_ = SDL_RWread(fp, &r, 4, 1);
	assert(r_ == 4);
	return r;
}

static short sdl_read_short(SDL_RWops *fp) {
	short r = 0;
	int r_ = SDL_RWread(fp, &r, 2, 1);
	assert(r_ == 1);
	return r;
}

static void sdl_read_skip(SDL_RWops *fp, unsigned int len) {
	int r_ = SDL_RWseek(fp, len, RW_SEEK_CUR);
	assert(r_ != 0);
}






Uint16 read_24bit_wav(SDL_RWops *fp) {
	char tmp[4] = { 0, };
	SDL_RWread(fp, tmp, 1, 3);
	tmp[3] = (char)(tmp[2] & 0x80 ? 0xFF : 0x00);
	return *((Uint32*)tmp) / 256;
}

Uint16 read_16bit_wav(SDL_RWops *fp) {
	char tmp[2] = { 0, };
	SDL_RWread(fp, tmp, 1, 2);
	return *((Uint16*)tmp);
}

Uint16 read_8bit_wav(SDL_RWops *fp) {
	char tmp = 0;
	SDL_RWread(fp, &tmp, 1, 1);
	return (tmp - 128) * 256;
}

Uint16 read_bit_wav(SDL_RWops *fp, int bit) {
	switch (bit) {
	case 24:
		return read_24bit_wav(fp);
	case 16:
		return read_16bit_wav(fp);
	case 8:
		return read_8bit_wav(fp);
	default:
		assert(0);
	}
}

Uint16 read_4bit_first(Uint8 byte) {
	return (int)(byte / 16) * 16 * 256;
}

Uint16 read_4bit_last(Uint8 byte) {
	return (byte % 16) * 16 * 256;
}






#define ERROR\
		spec = 0;\
		goto done;

SDL_AudioSpec *Mix_LoadWAV_RW_(SDL_RWops *src, int freesrc,
	SDL_AudioSpec *spec, Uint8 **audio_buf, Uint32 *audio_len)
{
	/*
	 * only process in case of 32000hz / 48000Hz (not 44100 / 22050 / 11025Hz)
	 * part of codes were refered from bmx2wav/bmx2ogg project.
	 */

	//
	// these part is already checked before, so don't do error checking
	//

	// RIFF header
	int riffheader = sdl_read_int(src);
#if 0
	if (riffheader != RIFF_HEADER)
	{ ERROR }
#endif

	// file size
	sdl_read_int(src);

	// WAVE header
	int wavheader = sdl_read_int(src);
#if 0
	if (wavheader != WAVE_HEADER)
	{ ERROR }
#endif

	// skip until met fmt chunk
	for (;;) {
		if (sdl_read_int(src) == FMT_CHUNK) {
			break;
		}
		sdl_read_skip(src, sdl_read_int(src));
	}

	// fmt byte size
	int fmt_byte_size = sdl_read_int(src);

	// fmt id
	if (sdl_read_short(src) != FMT_ID)
	{ ERROR }

	// channel count
	int channel_count = sdl_read_short(src);
	if (channel_count != 1 && channel_count != 2)
	{ ERROR }

	// frequency
	int frequency = sdl_read_int(src);

	// byte per second
	int byte_per_second = sdl_read_int(src);

	// block size
	int block_size = sdl_read_short(src);

	// bit rate
	int bit_rate = sdl_read_short(src);
	if (bit_rate != 4 && 
		bit_rate != 8 && 
		bit_rate != 16 && 
		bit_rate != 24 && 
		bit_rate != 32)
	{ ERROR }

	// check once again (valid block size?)
	// as it's too minor problem, we'll gonna skip this
#if STRICT_CHECK
	if (block_size != (bit_rate / 8) * channel_count)
	{ ERROR }
	if (byte_per_second != frequency * block_size)
	{ ERROR }
#endif

	//
	// if bit is general one (8 / 16 / 32)
	// then use default SDL loader
	// otherwise (24 bit), load it by using this module.
	//
	if (bit_rate == 8 || bit_rate == 16 || bit_rate == 32) {
		SDL_RWseek(src, 0, RW_SEEK_SET);
		return SDL_LoadWAV_RW(src, freesrc, spec, audio_buf, audio_len);
	}

	// skip extended format byte
	if (fmt_byte_size - FMT_BYTE_SIZE > 0) {
		sdl_read_skip(src, fmt_byte_size - FMT_BYTE_SIZE);
	}

	// skip until met data chunk
	for (;;) {
		if (sdl_read_int(src) == DATA_CHUNK) {
			break;
		}
		sdl_read_skip(src, sdl_read_int(src));
	}

	// data size
	int data_size = sdl_read_int(src);

	// data size check
	if (data_size % ((bit_rate / 8) * channel_count) != 0) {
#if STRICT_CHECK
		// some WAVE file doesn't returns valid size
		// better fix it rather then returning ERROR
		ERROR
#else
		data_size -= data_size % ((bit_rate / 8) * channel_count);
#endif
	}

	//
	// get data size strictly by checking file size
	//
	Sint64 filetotsize = SDL_RWsize(src);
	Sint64 filepos = SDL_RWtell(src);
	if (filetotsize - filepos < data_size) data_size = filetotsize - filepos;

	//
	// now we know data size, so allocate buffer
	//
	*audio_buf = NULL;
	*audio_len = 0;
	memset(spec, '\0', sizeof(SDL_AudioSpec));

	//
	// convert it to 16bit, always,
	//
	spec->format = AUDIO_S16;
	spec->channels = channel_count;
	spec->freq = frequency;
	spec->samples = 4096; /* buffer size */

	int samples = data_size / (bit_rate / 8) / channel_count;

	/* x2 : we'll going to interpret audio data as 16bit */
	*audio_len = spec->size = samples * spec->channels * 2;
	*audio_buf = (Uint8 *)SDL_malloc(*audio_len);
	if (*audio_buf == NULL)
		goto done;

	//
	// all metadata were gathered... start to read WAV file
	// store raw if freq is 44100 / 22050 / 11025Hz.
	//
	int pos = 0;
	int mul = 2 * spec->channels;

	//
	// before continue, if wav file is 4 bit per sample,
	// then extend it into 16 bit per sample.
	//
#define READBIT read_bit_wav(src, bit_rate)
	if (bit_rate == 4) {
		// it's 2 sample per a byte, actually.
		bit_rate = 8;
		Uint16* write_buf = *audio_buf;
		for (int i = 0; i < samples / 2 * spec->channels; i++) {
			Uint16 _t = READBIT;
			*(write_buf++) = read_4bit_first(_t);
			*(write_buf++) = read_4bit_last(_t);
		}
	}
	else {
		for (int i = 0; i < samples; i++) {
			int bufpos = i * mul;
			*((Uint16*)(*audio_buf + bufpos)) = READBIT;
			if (spec->channels == 2)
				*((Uint16*)(*audio_buf + bufpos + 2)) = READBIT;
		}
	}

	/*
	 * cleanup
	 */
done:
	if (freesrc && src) {
		SDL_RWclose(src);
	}

	return(spec);
}

#endif