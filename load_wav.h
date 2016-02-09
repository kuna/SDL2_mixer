/*
 * modified by @lazykuna
 */

#ifdef WAV_MUSIC
/* Don't call this directly; use Mix_LoadWAV_RW() for now. */
SDL_AudioSpec *Mix_LoadWAV_RW_(SDL_RWops *src, int freesrc,
	SDL_AudioSpec *spec, Uint8 **audio_buf, Uint32 *audio_len);
#endif
