#ifndef _WAV_PLAYER_H
#define _WAV_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

void play_wav_file( const char *file_name, const char *device );
void terminate_last_one();
void close_wav_player();

#ifdef __cplusplus
}
#endif

#endif  // _WAV_PLAYER_H

