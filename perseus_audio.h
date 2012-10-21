#if !defined __PERSEUS_AUDIO_H__
#define      __PERSEUS_AUDIO_H__

struct PerseusAudio * perseus_audio_open   (int core_bandwidth)  ;
int perseus_audio_close    (void) ;
int perseus_audio_write_sr (float* left_samples,float* right_samples) ;
int perseus_audio_write_2  (float* left_samples,float* right_samples) ;
int perseus_audio_write_3  (float* left_samples,float* right_samples) ;


#include <samplerate.h>
#include <portaudio.h>

struct PerseusAudio  {

    int CHANNELS;       /* 1 = mono 2 = stereo */
    int SAMPLES_PER_BUFFER;

    int   DECIM_FACT;
    float SAMPLE_RATE;
    int   CORE_BANDWIDTH;

    PaStream* stream;

    SRC_STATE *sr_state;
    SRC_DATA  *sr_data;

};

extern struct PerseusAudio *ppa;


#endif
