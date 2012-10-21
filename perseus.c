#include <perseus-sdr.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "perseus.h"
#include "perseus_audio.h"

static int num_perseus = 0;
static perseus_descr *pPd = 0;
static eeprom_prodid prodid;
static int CORE_BANDWIDTH = 48000;

typedef union {
	struct __attribute__((__packed__)) {
		int32_t	i;
		int32_t	q;
		} iq;
	struct __attribute__((__packed__)) {
		uint8_t		i1;
		uint8_t		i2;
		uint8_t		i3;
		uint8_t		i4;
		uint8_t		q1;
		uint8_t		q2;
		uint8_t		q3;
		uint8_t		q4;
		};
} iq_sample;

static int counter = 0;


#define BUFFER_SIZE 1024

float input_buffer[BUFFER_SIZE*2];
float output_buffer[BUFFER_SIZE*2];
int samples;

// 2^24 / 2 -1 = 8388607.0

//#define SCALE_FACTOR 8388607.0
#define SCALE_FACTOR 2147483647.00000000000000000000

#define ADC_CLIP 0x00000001

int user_data_callback(void *buf, int buf_size, void *extra)
{
	// The buffer received contains 24-bit IQ samples (6 bytes per sample)
	// Here as a demonstration we save the received IQ samples as 32 bit 
	// (msb aligned) integer IQ samples.

	// At the maximum sample rate (2 MS/s) the hard disk should be capable
	// of writing data at a rate of at least 16 MB/s (almost 1 GB/min!)

	uint8_t	*samplebuf 	= (uint8_t*)buf;
	//FILE *fout 			= (FILE*)extra;
	int nSamples 		= buf_size/6;
	int k;
    int adc_clip = 0;
	iq_sample s;
	//perseus_descr *pPd  = (perseus_descr *)extra;
    sdr_data_t *sdr = (sdr_data_t *)extra;

    //fprintf (stderr, ">>>>>>>>>>>>> %s\n", __FUNCTION__);

	s.i1 = s.q1 = 0;

	for (k=0; k < nSamples; k++) {
		s.i2 = *samplebuf++;
		s.i3 = *samplebuf++;
		s.i4 = *samplebuf++;
		s.q2 = *samplebuf++;
		s.q3 = *samplebuf++;
		s.q4 = *samplebuf++;

		//fwrite(&s.iq, 1, sizeof(iq_sample), fout);

        input_buffer[samples]             = ((float) (s.iq.q)) / SCALE_FACTOR;
        input_buffer[samples+BUFFER_SIZE] = ((float) (s.iq.i)) / SCALE_FACTOR;
//        pRec->input_buffer[pRec->samples]             = (float) 0;
//        pRec->input_buffer[pRec->samples+BUFFER_SIZE] = (float) 0;

        if (adc_clip == 0) adc_clip = s.iq.i & ADC_CLIP; 

        if ((counter++ % 204800) == 0) {
           fprintf (stderr, ">>>>>>>>>>>>>> %s: k: %d i: %d q: %d\n", __FUNCTION__, k, s.iq.i, s.iq.q);  
           fprintf (stderr, ">>>>>>>>>>>>>> LSB first: i: %02x%02x%02x%02x q: %02x%02x%02x%02x\n",
                    s.i1, s.i2, s.i3, s.i4, s.q1, s.q2, s.q3, s.q4 );  
           fprintf (stderr, ">>>>>>>>>>>>>> i: %f q: %f\n", 
                    input_buffer[samples], input_buffer[samples+BUFFER_SIZE]);
           fflush(stderr);
        }

		samples++;

     // if(timing) {
     //     sample_count++;
     //     if(sample_count==sample_rate) {
     //         ftime(&end_time);
     //         fprintf(stderr,"%d samples in %ld ms\n",sample_count,((end_time.time*1000)+end_time.millitm)-((start_time.time*1000)+start_time.millitm));
     //         sample_count=0;
     //         ftime(&start_time);
     // }
     // }

        // when we have enough samples send them to the clients
        if(samples==BUFFER_SIZE) {
			int i;
            // send I/Q data to clients
            //fprintf (stderr, "%s: sending data.\n", __FUNCTION__);


//            send_IQ_buffer(pRec);

			for(i = 0; i < samples; i++) {
				// uncomment whichever is appropriate
//				sdr->iqSample[i] =     input_buffer[i] + 
//                                   I * input_buffer[i+BUFFER_SIZE]; // I on left
				sdr->iqSample[i] =     input_buffer[i+BUFFER_SIZE] + 
                                   I * input_buffer[i];               // I on right
				//	sdr->iqSample[i] = qq[i] + I * ii[i]; // I on right
			}

			// actually run the SDR for a frame
			sdr_process(sdr);

			// copy the frames to the output

			//
			// AUDIO
			//
            float ls[4096]; float rs[4096];

            for (i=0; i < BUFFER_SIZE; ++i) {
                ls[i] =  sdr->output[i];
                rs[i] =  sdr->output[i];
            } 

            perseus_audio_write_sr (ls, rs);

			//for(i = 0; i < nframes; i++) {
			//	L[i]=sdr->output[i];
			//	R[i]=sdr->output[i];
			//}
            samples=0;
        }


	}
    return 0;
}


int perseus_start(sdr_data_t *sdr, int bandwidth)
{
    CORE_BANDWIDTH = bandwidth;

	// save some info in the SDR
	sdr->size = BUFFER_SIZE;
	sdr->sample_rate = CORE_BANDWIDTH; // jack_get_sample_rate(client);
	sdr->iqSample = g_new0(complex, sdr->size);
	sdr->output = g_new0(double, sdr->size);

	//perseus_set_debug(debug_level);
    

	// Check how many Perseus receivers are connected to the system
	num_perseus = perseus_init();
	if (num_perseus==0) {
		fprintf (stderr, "No Perseus receiver(s) detected\n");
		perseus_exit();
		return ~0;
	} else {
		fprintf (stderr, " Perseus receiver(s) found\n" );
	}

	// Open the first one...
	if ((pPd = perseus_open(0))==NULL) {
		printf("error: %s\n", perseus_errorstr());
        return ~0;
    }

	// Download the standard firmware to the unit
	printf("Downloading firmware...\n");
	if (perseus_firmware_download(pPd,NULL)<0) {
		printf("firmware download error: %s", perseus_errorstr());
        perseus_close (pPd);
        return ~0;
    }

	// Dump some information about the receiver (S/N and HW rev)
	if (pPd->is_preserie == TRUE) 
		printf("The device is a preserie unit");
	else
		if (perseus_get_product_id(pPd,&prodid)<0) 
			printf("get product id error: %s", perseus_errorstr());
		else
			printf("Receiver S/N: %05d-%02hX%02hX-%02hX%02hX-%02hX%02hX - HW Release:%hd.%hd\n",
					(uint16_t) prodid.sn, 
					(uint16_t) prodid.signature[5],
					(uint16_t) prodid.signature[4],
					(uint16_t) prodid.signature[3],
					(uint16_t) prodid.signature[2],
					(uint16_t) prodid.signature[1],
					(uint16_t) prodid.signature[0],
					(uint16_t) prodid.hwrel,
					(uint16_t) prodid.hwver);

	// Configure the receiver for 2 MS/s operations
	printf("Configuring FPGA...\n");

	switch (CORE_BANDWIDTH) {
		case 48000:
			if (perseus_set_sampling_rate(pPd, 48000)<0) {
				printf("fpga configuration error: %s\n", perseus_errorstr());
				perseus_close (pPd);
				return ~0;
			}
		break;
		case 95000:
			if (perseus_set_sampling_rate(pPd, 95000)<0) {
				printf("fpga configuration eCORE_BANDWIDTHrror: %s\n", perseus_errorstr());
				perseus_close (pPd);
				return ~0;
			}
		break;
		case 96000:
			if (perseus_set_sampling_rate(pPd, 96000)<0) {
				printf("fpga configuration error: %s\n", perseus_errorstr());
				perseus_close (pPd);
				return ~0;
			}
		break;
		case 125000:
			if (perseus_set_sampling_rate(pPd, 125000)<0) {
				printf("fpga configuration error: %s\n", perseus_errorstr());
				perseus_close (pPd);
				return ~0;
			}
		break;
		case 192000:
			if (perseus_set_sampling_rate(pPd, 192000)<0) {
				printf("fpga configuration error: %s\n", perseus_errorstr());
				perseus_close (pPd);
				return ~0;
			}
		break;
		case 250000:
			if (perseus_set_sampling_rate(pPd, 250000)<0) {
				printf("fpga configuration error: %s\n", perseus_errorstr());
				perseus_close (pPd);
				return ~0;
			}
		break;
		case 500000:
			if (perseus_set_sampling_rate(pPd, 500000)<0) {
				printf("fpga configuration error: %s\n", perseus_errorstr());
				perseus_close (pPd);
				return ~0;
			}
		break;
        case 1000000:
            if (perseus_set_sampling_rate(pPd, 1000000)<0) {
                printf("fpga configuration error: %s\n", perseus_errorstr());
                perseus_close (pPd);
                return ~0;
            }
        break;
        case 2000000:
            if (perseus_set_sampling_rate(pPd, 2000000)<0) {
                printf("fpga configuration error: %s\n", perseus_errorstr());
                perseus_close (pPd);
                return ~0;
            }
        break;

		default:
			printf("No suitable bandwith: %d\n",CORE_BANDWIDTH );
            return ~0;
	}
    

//   // Cycle attenuator leds on the receiver front panel
//   // just to see if they indicate what they shoud
//   perseus_set_attenuator(pPd, PERSEUS_ATT_0DB);
//   sleep(1);
//   perseus_set_attenuator(pPd, PERSEUS_ATT_10DB);
//   sleep(1);
//   perseus_set_attenuator(pPd, PERSEUS_ATT_20DB);
//   sleep(1);
//   perseus_set_attenuator(pPd, PERSEUS_ATT_30DB);
//   sleep(1);
//   perseus_set_attenuator(pPd, PERSEUS_ATT_0DB);
//   sleep(1);

	// Enable ADC Dither, Disable ADC Preamp
	perseus_set_adc(pPd, TRUE, FALSE);

	// Do the same cycling test with the WB front panel led.
	// Enable preselection filters (WB_MODE Off)
	//perseus_set_ddc_center_freq(pPd, 7050000.000, 1);
//   sleep(1);
//   // Disable preselection filters (WB_MODE On)
//   perseus_set_ddc_center_freq(pPd, 7000000.000, 0);
//   sleep(1);
//   // Re-enable preselection filters (WB_MODE Off)
//   perseus_set_ddc_center_freq(pPd, 7000000.000, 1);


    // disable the attenuator
    perseus_set_attenuator(pPd, PERSEUS_ATT_0DB);

    //
    //
    // attempt to open an audio stream on a local audio card
    //perseus_audio_open (CORE_BANDWIDTH);
	//
    ppa = perseus_audio_open(CORE_BANDWIDTH);

    return 0;
}

int perseus_stop(sdr_data_t *sdr)
{
    printf("perseus_stop: Quitting...\n");
    perseus_stop_async_input(pPd);
    perseus_close(pPd);

    return 0;
}


int perseus_connect(sdr_data_t *sdr, gboolean ci, gboolean co)
{
	samples = 0;
	if ( perseus_start_async_input ( pPd, 16320, user_data_callback, sdr ) < 0 ) {
		printf("start async input error: %s\n", perseus_errorstr());
		return ~0;
	} else {
		printf("start async input: %s\n", "STARTED");
        return 0;
	}

}

int perseus_update_freq(sdr_data_t *sdr)
{
	if (sdr) {
		perseus_set_ddc_center_freq(pPd, sdr->centre_freq, 1);
		return ~0;
	}
	return 0;
}



/* vim: set noexpandtab ai ts=4 sw=4 tw=4: */

