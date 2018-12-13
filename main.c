#include <stdio.h>
#include "wav_player.h"

int main( int argc, char *argv[] )
{
    play_wav_file( "test1.wav", NULL );
    play_wav_file( "test2.wav", NULL );
    play_wav_file( "test3.wav", NULL );
    close_wav_player();

    return 0;
}
