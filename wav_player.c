/** 
 * Function: Play a wave file.
 * Author  : Civen
 * Date    : 2018-11-12
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#define STRUCT_PACKED __attribute__((packed))

#ifndef bool
typedef enum _bool_
{
    false = 0,
    true = 1
}BOOL;
#define bool BOOL
#endif

typedef struct _wav_header
{
    char  riff[4];    // "RIFF"
    int   len;
    char  wave[4];    // "WAVE"
    char  format[4];  // "fmt "
    int   size;
    short format_tag;
    short channels;
    int   samples_per_second;
    int   average_bits_per_sample;
    short block_aligned;
    short bits_per_sample;
    char  data[4];    // "data"
    int   sample_length;
}STRUCT_PACKED WAV_HEADER;

typedef struct _pcm_size_info
{
    snd_pcm_sframes_t pcm_buffer_size;
    snd_pcm_sframes_t pcm_period_size;
}STRUCT_PACKED PCM_SIZE_INFO;

static bool g_terminate_flag = false;
static char g_pcm_device[16] = { "default" };  /* "plughw:0,0" */
static snd_pcm_t *g_pcm_dev_handle = NULL;
static pthread_mutex_t g_mutex_lock = PTHREAD_MUTEX_INITIALIZER;

static bool isWavFormat( FILE *fp, WAV_HEADER *header )
{
    int len = 0;

    if ( (NULL == fp) || (NULL == header) )
    {
        return false;
    }

    fseek( fp, 0, SEEK_SET );
    len = fread( header, 1, sizeof(WAV_HEADER), fp );

    if ( sizeof(WAV_HEADER) > len )
    {
        return false;
    }

    if ( (0 != strncasecmp("RIFF", header->riff, 4)) \
         || (0 != strncasecmp("WAVE", header->wave, 4)) \
         || (0 != strncasecmp("fmt", header->format, 3)) \
         || (0 != strncasecmp("data", header->data, 4)) \
       )
    {
        return false;
    }

    return true; 
}


static bool set_hwparams( const WAV_HEADER header, PCM_SIZE_INFO *info )
{
    int resample = 1;
    int err = 0, dir = 0;
    unsigned int rate = 0;
    unsigned int buffer_time = 0, period_time = 0;
    snd_pcm_uframes_t size = 0;
    snd_pcm_hw_params_t *pcm_hwparams = NULL;

    if ( NULL == info )
    {
        return false;
    }

    /* open PCM device for playback. */
    if ( NULL == g_pcm_dev_handle )
    {
        err = snd_pcm_open( &g_pcm_dev_handle, g_pcm_device, SND_PCM_STREAM_PLAYBACK, 0 ); 

        if ( 0 > err )
        {
            printf( " *** Error: Playback open error: %s\n", snd_strerror(err) );

            return false;
        }
    }

    snd_pcm_hw_params_alloca( &pcm_hwparams );

    if ( NULL == pcm_hwparams )
    {
        printf( " *** Error: snd_pcm_hw_params_alloca() error.\n" );

        return false;
    }

    /* choose all parameters */
    err = snd_pcm_hw_params_any( g_pcm_dev_handle, pcm_hwparams );

    if ( 0 > err )
    {
        printf( " *** Error: Broken configuration for playback: no configurations available: %s\n", snd_strerror(err) );

        return false;
    }

    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample( g_pcm_dev_handle, pcm_hwparams, resample );

    if ( 0 > err )
    {
        printf( " *** Error: Resampling setup failed for playback: %s\n", snd_strerror(err) );

        return false;
    }

    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access( g_pcm_dev_handle, pcm_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED );

    if ( 0 > err )
    {
        printf( " *** Error: Access type not available for playback: %s\n", snd_strerror(err) );

        return false;
    }

    err = -1;

    /* set the sample format */
    switch ( header.bits_per_sample >> 3 )
    {
        case 1:
        {
            err = snd_pcm_hw_params_set_format( g_pcm_dev_handle, pcm_hwparams, SND_PCM_FORMAT_U8 );

            break;
        }

        case 2:
        {
            err = snd_pcm_hw_params_set_format( g_pcm_dev_handle, pcm_hwparams, SND_PCM_FORMAT_S16_LE );

            break;
        }

        case 3:
        {
            err = snd_pcm_hw_params_set_format( g_pcm_dev_handle, pcm_hwparams, SND_PCM_FORMAT_S24_LE );

            break;
        }

        default:
        {
            err = -1;

            break;
        }
    }

    if ( 0 > err )
    {
        printf( " *** Error: Sample format not available for playback: %s\n", snd_strerror(err) );

        return false;
    }

    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels( g_pcm_dev_handle, pcm_hwparams, header.channels );

    if ( 0 > err )
    {
        printf( " *** Error: Channels count (%i) not available for playbacks: %s\n", header.channels, snd_strerror(err) );

        return false;
    }

    rate = header.samples_per_second;

    /* set the stream rate */
    err = snd_pcm_hw_params_set_rate_near( g_pcm_dev_handle, pcm_hwparams, &rate, &dir );

    if ( 0 > err )
    {
        printf( " *** Error: Rate %iHz not available for playback: %s\n", rate, snd_strerror(err) );

        return false;
    }

    /* set the buffer time */
    err = snd_pcm_hw_params_get_buffer_time_max( pcm_hwparams, &buffer_time, &dir );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to get buffer time for playback: %s\n", snd_strerror(err) );

        return false;
    }

    err = snd_pcm_hw_params_set_buffer_time_near( g_pcm_dev_handle, pcm_hwparams, &buffer_time, &dir );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err) );

        return false;
    }

    err = snd_pcm_hw_params_get_buffer_size( pcm_hwparams, &size );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to get buffer size for playback: %s\n", snd_strerror(err) );

        return false;
    }

    info->pcm_buffer_size = size;

    /* set the period time */
    err = snd_pcm_hw_params_get_period_time_max( pcm_hwparams, &period_time, &dir );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to get buffer time for playback: %s\n", snd_strerror(err) );

        return false;
    }

    err = snd_pcm_hw_params_set_period_time_near( g_pcm_dev_handle, pcm_hwparams, &period_time, &dir );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err) );

        return false;
    }

    err = snd_pcm_hw_params_get_period_size( pcm_hwparams, &size, &dir );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to get period size for playback: %s\n", snd_strerror(err) );

        return false;
    }

    info->pcm_period_size = size;

    /* write the parameters to device */
    err = snd_pcm_hw_params( g_pcm_dev_handle, pcm_hwparams );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to set hw params for playback: %s\n", snd_strerror(err) );

        return false;
    }
    
    return true; 
}


static bool set_swparams( const PCM_SIZE_INFO info )
{
    int err = 0, period_event = 0;
    snd_pcm_sw_params_t *pcm_swparams = NULL;

    snd_pcm_sw_params_alloca( &pcm_swparams );

    if ( NULL == pcm_swparams )
    {
        printf( " *** Error: snd_pcm_sw_params_alloca() error.\n" );

        return false;
    }

    /* get the current swparams */
    err = snd_pcm_sw_params_current( g_pcm_dev_handle, pcm_swparams );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to determine current swparams for playback: %s\n", snd_strerror(err) );

        return false;
    }

    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold( g_pcm_dev_handle, pcm_swparams, (info.pcm_buffer_size / info.pcm_period_size) * info.pcm_period_size );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to set start threshold mode for playback: %s\n", snd_strerror(err) );

        return false;
    }

    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min( g_pcm_dev_handle, pcm_swparams, period_event ? info.pcm_buffer_size : info.pcm_period_size );

    if ( 0 > err )
    {
        printf( " *** Error: Unable to set avail min for playback: %s\n", snd_strerror(err) );

        return false;
    }

    /* enable period events when requested */
    if ( period_event )
    {
        err = snd_pcm_sw_params_set_period_event( g_pcm_dev_handle, pcm_swparams, 1 );

        if ( 0 > err )
        {
            printf( " *** Error: Unable to set period event: %s\n", snd_strerror(err) );

            return false;
        }
    }

    /* write the parameters to the playback device */
    err = snd_pcm_sw_params( g_pcm_dev_handle, pcm_swparams );

    if (  0 > err )
    {
        printf( " *** Error: Unable to set sw params for playback: %s\n", snd_strerror(err) );

        return false;
    }

    return true;
}


static int xrun_recovery( int err )
{
    if ( -EPIPE == err)
    {    /* under-run */
        err = snd_pcm_prepare( g_pcm_dev_handle );

        if ( 0 > err )
        {
            printf( " *** Error: Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err) );
        }

        return 0;
    }
    else if ( -ESTRPIPE == err )
    {
            while ( -EAGAIN == (err = snd_pcm_resume(g_pcm_dev_handle)) )
            {
                sleep( 1 );       /* wait until the suspend flag is released */
            }

            if ( 0 > err )
            {
                err = snd_pcm_prepare( g_pcm_dev_handle );

                if ( 0 > err )
                {
                    printf( " *** Error: Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err) );
                }
            }

            return 0;
    }

    return err;
}


static void play_wav_raw_data( FILE *fp, const PCM_SIZE_INFO info, const int block_size )
{
    int err = 0;
    int size = info.pcm_period_size * block_size;
    char *wav_data_buffer = NULL;

    if ( NULL == fp )
    {
        return;
    }

    wav_data_buffer = (char *)malloc( size );

    if ( NULL == wav_data_buffer )
    {
        return;
    }

    while ( !g_terminate_flag )
    {
        memset( wav_data_buffer, 0, size );
        err = fread( wav_data_buffer, 1, size, fp );

        if ( g_terminate_flag || (0 >= err) )
        {
            break;
        }

        while ( 0 > (err = snd_pcm_writei(g_pcm_dev_handle, wav_data_buffer, info.pcm_period_size)) )
        {
            usleep( 1000 );

            if ( g_terminate_flag )
            {
                break;
            }

            if ( -EAGAIN == err )
            {
                continue;
            }
            else if ( 0 > err )
            {
                if ( 0 > xrun_recovery(err) )
                {
                    printf( " *** Error: snd_pcm_writei() error: %s\n", snd_strerror(err) );
                }

                break;  // skip one period
            }
        }
    }

    if ( NULL != wav_data_buffer )
    {
        free( wav_data_buffer );
        wav_data_buffer = NULL;
    }

    g_terminate_flag = false;

    return; 
}


/************************************************************/


void close_wav_player()
{
    if ( NULL != g_pcm_dev_handle )
    {
        snd_pcm_drain( g_pcm_dev_handle );
        snd_pcm_close( g_pcm_dev_handle );
        g_pcm_dev_handle = NULL;
    }

    return;
}


void terminate_last_one()
{
    if ( !g_terminate_flag )
    {
        int counter = 0;

        g_terminate_flag = true;

        while ( 100 > counter )
        {
            counter++;
            usleep( 100 );

            if ( !g_terminate_flag )
            {
                break;
            }
        }
    }

    return;
}


void play_wav_file( const char *file_name, const char *device )
{
    FILE *fp = NULL;
    WAV_HEADER header;
    PCM_SIZE_INFO info;

    if ( NULL == file_name )
    {
        puts( " *** Error: 'file_name' is NULL." );

        return;
    }

    //pthread_mutex_lock( &g_mutex_lock );

    fp = fopen( file_name, "rb" );

    if ( NULL == fp )
    {
        //pthread_mutex_unlock( &g_mutex_lock );
        printf( " *** Error: Open '%s' error.", file_name );

        return;
    }

    if ( !isWavFormat(fp, &header) )
    {
        fclose( fp );
        //pthread_mutex_unlock( &g_mutex_lock );
        printf( " *** Error: '%s' format error.", file_name );

        return;
    }

    if ( NULL != device )
    {
        memset( g_pcm_device, 0, 16 );
        snprintf( g_pcm_device, 16, "%s", device );
    }

    if ( !set_hwparams(header, &info) ) 
    {
        fclose( fp );
        //pthread_mutex_unlock( &g_mutex_lock );
        puts( " *** Error: Setting of hwparams failed." );

        return;
    }

    if ( !set_swparams(info) )
    {
        fclose( fp );
        //pthread_mutex_unlock( &g_mutex_lock );
        puts( " *** Error: Setting of swparams failed." );

        return;
    }

    g_terminate_flag = false;
    play_wav_raw_data( fp, info, header.block_aligned );
    fclose( fp );
    //pthread_mutex_unlock( &g_mutex_lock );

    return; 
}

