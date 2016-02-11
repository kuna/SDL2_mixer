#include "convertfreq.h"

#include <stdlib.h>

#include "SDL_mutex.h"
#include "SDL_endian.h"
#include "SDL_timer.h"

#include "SDL_mixer.h"

int read_bit(int byte, int sign, Uint8** buf) {
	int r = 0;
	if (byte == 1) {
		r = *(*buf);
		if (sign) r = (char)r;
	}
	else if (byte == 2) {
		r = *(*(Uint16**)buf);
		if (sign) r = (short)r;
	}
	else if (byte == 4) {
		r = *(*(Uint32**)buf);
	}
	(*buf) += byte;
	return r;
}

void write_bit(int byte, int sign, Uint8** buf, Uint32 val) {
	if (byte == 1) {
		*(*(Uint8**)buf) = val;
	}
	else if (byte == 2) {
		*(*(Uint16**)buf) = val;
	}
	else if (byte == 4) {
		*(*(Uint32**)buf) = val;
	}
	(*buf) += byte;
}

#define READBIT read_bit(byte_, sign_, &buf_);
#define WRITEBIT(v) write_bit(byte_, sign_, &newbuf_, v);
void AudioFreqConvert(Mix_Chunk *chunk, int channels_, int format_, int freq_org, int freq_target) {
	int bit_ = 16;
	switch (format_) {
	case AUDIO_S32:
	case AUDIO_F32:
		bit_ = 32;
		break;
	case AUDIO_S16:
	case AUDIO_U16:
		bit_ = 16;
		break;
	case AUDIO_S8:
	case AUDIO_U8:
		bit_ = 8;
		break;
	}
	int sign_ = 1;
	switch (format_) {
	case AUDIO_U16:
	case AUDIO_U8:
		sign_ = 0;
		break;
	}
	int byte_ = bit_ / 8;

	double mul_freq = (double)freq_target / freq_org;
	Uint32 samples_ = chunk->alen / channels_ / byte_;	// in: Uint8 data
	Uint32 newlen_ = (int)(samples_ * mul_freq) * channels_ * byte_;
	Uint8* newbuf_ = (Uint8*)SDL_malloc(newlen_);
	Uint8* newbuf_org = newbuf_;
	Uint8* buf_ = (Uint8*)chunk->abuf;
	int lprev_ = 0, lcur_ = 0, rprev_ = 0, rcur_ = 0;
	int audio_pos = 0;

	lprev_ = READBIT;
	if (channels_ == 2)
		rprev_ = READBIT;
	for (int i = 1; i < samples_; i++) {
		lcur_ = READBIT;
		if (channels_ == 2)
			rcur_ = READBIT;
		while ((i - 1) * mul_freq <= audio_pos && audio_pos <= i * mul_freq) {
			double prev_mul = (i * mul_freq - audio_pos) / mul_freq;
			double next_mul = (audio_pos - (i - 1) * mul_freq) / mul_freq;
			WRITEBIT(lcur_ * next_mul + lprev_ * prev_mul);
			if (channels_ == 2) {
				WRITEBIT(rcur_ * next_mul + rprev_ * prev_mul);
			}
			audio_pos++;
		}
		lprev_ = lcur_;
		rprev_ = rcur_;
	}

	SDL_free(chunk->abuf);
	chunk->alen = newlen_;
	chunk->abuf = newbuf_org;
}