/*
 sdr.c
 
 */

#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>

#include "sdr.h"

#define MAX_FILTER_SIZE 10000
 
static int indexFilter;
static int sizeFilter = FFT_SIZE;


FFT_DATA *filter;

static double cFilterI[MAX_FILTER_SIZE];	// Digital filter coefficients for receive
static double cFilterQ[MAX_FILTER_SIZE];	// Digital filter coefficients
static double bufFilterI[MAX_FILTER_SIZE];	// Digital filter sample buffer
static double bufFilterQ[MAX_FILTER_SIZE];	// Digital filter sample buffer

void make_filter(float rate, int N, float bw, float centre) {
    // rate is currently 48000 by default
    // N - filter length
    // bw = bandwidth
    // centre = Fc
    
    // blow away N
    N=FFT_SIZE;

    fftw_plan plan;

    float K = bw * N / rate;
    float w;
    complex z;
    complex impulse[FFT_SIZE];
    int i=0, k;
    float tune = 2.0 * M_PI * centre / rate;
    
    for (k=-N/2; k<=N/2; k++) {
        if (k==0) z=(float)K/N;
        else z=1.0/N*sin(M_PI*k*K/N)/sin(M_PI*k/N);
        // apply a windowing function.  I can't hear any difference...
        //w = 0.5 + 0.5 * cos(2.0 * M_PI * k / N); // Hanning window
        w = 0.42 + 0.5 * cos(2.0 * M_PI * k / N) + 0.08 * cos(4. * M_PI * k / N); // Blackman window
        //w=0.5; // No window
        z *= w; 
        z *= 2*cexp(-1*I * tune * k);
        cFilterI[i] = creal(z);
        cFilterQ[i] = cimag(z);
        filter->samples[i]= z;   // store the complex value, too
        i++;
    }
    // now for the clever bit
   fftw_execute(filter->plan);
   //for (i=0; i<N; i++) {
  //     printf("%d %f\n",i,filter->out[i]);
  // }
}

int sdr_process(SDR_DATA *sdr) {
    // actually do the SDR bit
    int i, j, k;
    double y, accI, accQ;
    complex c;
    FFT_DATA *fft = sdr->fft;
    FFT_DATA *fft_out = sdr->fft_out;
    float agcGain = sdr->agcGain;
    float agcPeak = 0;//sdr->agcPeak;

    // remove DC
    for (i = 0; i < sdr->size; i++) {       // DC removal; R.G. Lyons page 553
                c = sdr->iqSample[i] + sdr->dc_remove * 0.95;
                sdr->iqSample[i] = c - sdr->dc_remove;
                sdr->dc_remove = c;
    }


    // shift frequency
    for (i = 0; i < sdr->size; i++) {
		sdr->iqSample[i] *= sdr->loVector;
    	sdr->loVector *= sdr->loPhase;
	}

    // copy this frame to FFT
    for (i=0; i<sdr->size; i++) {
        fft->samples[i] = sdr->iqSample[i];
    }
    fft->status = READY;
    fftw_execute(fft->plan);
    
    for (i=0; i<sdr->fft_size; i++) {
        fft_out->samples[i] = fft->out[i] * cabs(filter->out[i]);
    }

    fftw_execute(fft_out->plan);
    for (i=0; i<sdr->size; i++) {
        sdr->iqSample[i] = fft_out->out[i];
        
    }
    
   // printf("%f %f\n", creal(fft_out->out[40]), creal(fft_out->samples[40]));
/*
    // shift frequency
    for (i = 0; i < sdr->size; i++) {
		sdr->iqSample[i] *= sdr->loVector;
    	sdr->loVector *= sdr->loPhase;
	}
*/
    // this demodulates LSB
    for (i=0; i<sdr->size; i++) {
	    y = creal(sdr->iqSample[i])+cimag(sdr->iqSample[i]);
        sdr->output[i] = y/10;
    }

}

void fft_setup(SDR_DATA *sdr) {
    sdr->fft = (FFT_DATA *)malloc(sizeof(FFT_DATA));
    sdr->fft_size = FFT_SIZE;
    FFT_DATA *fft = sdr->fft;
    
    fft->samples = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sdr->fft_size);
    fft->out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sdr->fft_size);
    fft->plan = fftw_plan_dft_1d(sdr->fft_size, fft->samples, fft->out, FFTW_FORWARD, FFTW_ESTIMATE);
	fft->status = EMPTY;
	fft->index = 0;
	
	sdr->fft_out = (FFT_DATA *)malloc(sizeof(FFT_DATA));
    sdr->fft_out->samples = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sdr->fft_size);
    sdr->fft_out->out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sdr->fft_size);
    sdr->fft_out->plan = fftw_plan_dft_1d(sdr->fft_size, sdr->fft_out->samples, sdr->fft_out->out, FFTW_BACKWARD, FFTW_ESTIMATE);

	filter = (FFT_DATA *)malloc(sizeof(FFT_DATA));
    filter->samples = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sdr->fft_size);
    filter->out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sdr->fft_size);
    filter->plan = fftw_plan_dft_1d(sdr->fft_size, filter->samples, filter->out, FFTW_FORWARD, FFTW_ESTIMATE);

}

void fft_teardown(SDR_DATA *sdr) {
    FFT_DATA *fft = sdr->fft;
    fftw_destroy_plan(fft->plan);
    fftw_free(fft->samples);
    fftw_free(fft->out);
    free(sdr->fft);
    
    fft = sdr->fft_out;
    fftw_destroy_plan(fft->plan);
    fftw_free(fft->samples);
    fftw_free(fft->out);
    free(sdr->fft_out);

    fft = filter;
    fftw_destroy_plan(fft->plan);
    fftw_free(fft->samples);
    fftw_free(fft->out);
    free(sdr->fft_out);
    
    
}

