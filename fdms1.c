#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <math.h>
#include <libusb-1.0/libusb.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>

struct timespec diff (struct timespec start, struct timespec end)
{
	struct timespec temp;
	if ((end.tv_nsec - start.tv_nsec) < 0) {
		temp.tv_sec  = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	} else {
		temp.tv_sec  = end.tv_sec  - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp;
}

// functions in FDM S1 control shared library
extern void set_en_ext_io_ATT20 (libusb_device_handle *deviceHandle, int *d_en_ext_io_ATT20 );
extern int  OpenHW (libusb_device_handle *deviceHandle, long sample_rate);
extern int  SetHWLO (libusb_device_handle *, long *);
extern void StartFIFO ( libusb_device_handle* deviceHandle );
extern void StopFIFO ( libusb_device_handle* deviceHandle );

typedef int (* FDMS1_HW_INIT)(libusb_device_handle *);

#include "fdms1.h"
#include "perseus_audio.h"

static FDMS1_HW_INIT init = 0;
	
static libusb_context *context = 0;
static libusb_device_handle *dev_handle; 
static uint16_t vendor_id =  0x1721;
static uint16_t product_id = 0x0610;
static int sr = 192000;

#define NSPB 1024


struct _TD {
    libusb_device_handle *dev_handle;
    long total_bytes_received; 
    sdr_data_t *sdr;
} td;

pthread_t usb_thread_id;

pthread_t rx_thread_id;

typedef union {

	struct __attribute__((__packed__)) {
		float	i;
		float	q;
		} iqf;
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


#define SCALE_FACTOR		2147483647.00000000000000000000
#define SCALE_FACTOR_24		((2^23)-1)

#define IQ_SIZE  8

#define QUEUE_NAME "/fdms1usb"


    

static
void *usb_thread (void *param)
{
    struct _TD *p = (struct _TD *) param;
    int i;
    int timeout = 2000; // in ms
    
    int n_tmo = 0;
    mqd_t mqxx = -1;
    struct mq_attr attr;
    

   struct timespec  time_start; //, time_end, time_diff;
//    long double diff_s ;
//    long unsigned int ns;
    static float ibuf  [NSPB*2];
    int   n = 0;

 
    /* initialize the queue attributes */
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(ibuf);
    attr.mq_curmsgs = 0;

   

    StartFIFO (p->dev_handle);
    fprintf (stderr, ">>>> StartFIFO\n");
 
    clock_gettime (CLOCK_REALTIME, &time_start);

    

    for (i=0; n_tmo < 3; ++i) {
       int transferred = 0;

       int rc = libusb_bulk_transfer ( p->dev_handle,
                                       (6 | LIBUSB_ENDPOINT_IN),
                                       (unsigned char *)ibuf, sizeof(ibuf),
                                       &transferred,
                                       timeout
                                     ); 	              
       if (rc) {
           fprintf (stderr, "Error in libusb_bulk_transfer: [%d] %s\n", rc, libusb_error_name (rc));
           if ( rc == -7 ) { //TIMEOUT
              n_tmo ++;
           } else 
              break;

       } else {
           if ( transferred > 0 ) {
               int rc;
               p->total_bytes_received += transferred;

               if (mqxx == -1) {
                   mqxx = mq_open (QUEUE_NAME, O_WRONLY);
                   if (mqxx == -1)     {
                      perror ("********* Failed to open the queue: ");
                      break;
                   }
               }
               rc = mq_send (mqxx, (char *)ibuf, sizeof(ibuf), 0);
               if (rc == -1) {
                  fprintf(stderr, "%s:%d: msg length: %d, ", __func__, __LINE__, sizeof(ibuf)); 
                  perror("sending message"); 
                  break;
               } //else 
                 //   fprintf (stderr, ">>>>>> sent %d msg\n", sizeof(ibuf));

           } else {
               fprintf (stderr, "******************** Error in libusb_bulk_transfer: %d\n", transferred);
               continue;
           }
      
       }
       
    }
    mq_close (mqxx);
	fprintf (stderr, "******************** usb thread exiting....\n");
}


static
void *rx_thread (void *param)
{
    int timeout = 2000; // in ms
    mqd_t mq;
    struct timespec  time_start, time_end, time_diff;
    long double diff_s ;
    int nsample_in_buf = 0;
    static unsigned char buf  [NSPB*IQ_SIZE];
    static float input_buffer  [NSPB*2];
    struct _TD *p = (struct _TD *) param;
	int n_msg = 0;
	unsigned char flag;
    struct mq_attr attr;

    /* initialize the queue attributes */
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof (buf);
    attr.mq_curmsgs = 0;

    /* create the message queue */
    mq = mq_open (QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
    if (mq == -1)     {
        perror ("******** Failed to create the queue: ");
        fprintf (stderr, "MSG SIZE: %d\n", sizeof (buf));
        return 0;
    } else {       
        fprintf (stderr, "QUEUE: MSG SIZE: %d\n", sizeof (buf));
    }



    int s = pthread_create(&usb_thread_id, 0, &usb_thread, (void *)p);
    if (s) {
       perror ("rx_thread, thread not started");
       return 0;
    }
    
    clock_gettime (CLOCK_REALTIME, &time_start);
    fprintf (stderr, ">>>>>>> entering rx loop.\n"); 
	flag = 0;
    for (;;) {
        ssize_t bytes_read;
        int j,x,i;
        
        /* receive the message */
        bytes_read = mq_receive (mq, (char *)buf, sizeof (buf), NULL);
        if ( bytes_read <= 0) { 
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); 
            perror("Receiving data from queue"); 
            break;
        } else {
			n_msg ++;
			if (n_msg >> 10) flag = (++flag) & 0x07, printf ("%d %d\n", n_msg, flag), fflush (stdout), n_msg = 0;

            //fprintf(stderr, ">>>>> %s:%d: received %d\n", __func__, __LINE__, bytes_read); 

		      unsigned char *pb;
              for (i=0, pb=&buf[0]; i < bytes_read; ) { 
     
				if ( flag == 0 ) {
				  //
				  // conversione a int 32 e poi a float con shift e somma aritmetica (SENZA normalizzazione +1.0/-1.0)
				  //
                  int ii = 0;
                  int qq = 0;

                  uint8_t q0 = buf[i++];
                  uint8_t q1 = buf[i++];
                  uint8_t q2 = buf[i++];
                  uint8_t q3 = buf[i++];
           
                  uint8_t i0 = buf[i++];
                  uint8_t i1 = buf[i++];
                  uint8_t i2 = buf[i++];
                  uint8_t i3 = buf[i++];
           
                  ii = (int)((signed char)i3 << 24) + (int)((unsigned char)i2 << 16) + (int)((unsigned char)i1 << 8) + (int)((unsigned char)i0);

                  qq = (int)((signed char)q3 << 24) + (int)((unsigned char)q2 << 16) + (int)((unsigned char)q1 << 8) + (int)((unsigned char)q0);
                  

                  input_buffer[nsample_in_buf]      = (float)ii; // / SCALE_FACTOR;
                  input_buffer[nsample_in_buf+NSPB] = (float)qq; // / SCALE_FACTOR;

				} else if (flag == 1) {
				  //
				  // conversione a int 32 e poi a float con shift e OR (con normalizzazione +1.0/-1.0)
				  //

                  int ii = 0;
                  int qq = 0;

                  uint8_t q0 = buf[i++];
                  uint8_t q1 = buf[i++];
                  uint8_t q2 = buf[i++];
                  uint8_t q3 = buf[i++];
           
                  uint8_t i0 = buf[i++];
                  uint8_t i1 = buf[i++];
                  uint8_t i2 = buf[i++];
                  uint8_t i3 = buf[i++];
           
                  ii = (i3 << 24) | (i2 << 16) | (i1 << 8) | i0;
                  qq = (q3 << 24) | (q2 << 16) | (q1 << 8) | q0;

                  input_buffer[nsample_in_buf]      = (((float)ii) / SCALE_FACTOR_24);
                  input_buffer[nsample_in_buf+NSPB] = (((float)qq) / SCALE_FACTOR_24);

				} else if (flag == 2) {
				  //
				  // conversione a int 32 e poi a float con union (architecture dependent !!!) (CON normalizzazione +1.0/-1.0)
				  //

				  iq_sample *s = (iq_sample *)&(buf[i]);
				  i += 8;

                  input_buffer[nsample_in_buf]      = (((float) (s->iq.q)) / SCALE_FACTOR_24);
                  input_buffer[nsample_in_buf+NSPB] = (((float) (s->iq.i)) / SCALE_FACTOR_24);

				} else if (flag == 3) {
				  //
				  // conversione a int 32 e poi a float con shift e somma aritmetica (CON normalizzazione +1.0/-1.0)
				  // il MSB e' ignorato
				  //
                  int ii = 0;
                  int qq = 0;
			  
				  ii   = (int)((unsigned char)(buf[i++]))		;
                  ii  += (int)((unsigned char)(buf[i++])) << 8 ;
                  ii  += (int)((  signed char)(buf[i++])) << 16;
				  i++;
                  qq   = (int)((unsigned char)(buf[i++]))	   ;
                  qq  += (int)((unsigned char)(buf[i++])) << 8 ;
                  qq  += (int)((  signed char)(buf[i++])) << 16;
   				  i++;

                  input_buffer[nsample_in_buf]      = (float)qq/8388607.0;
                  input_buffer[nsample_in_buf+NSPB] = (float)ii/8388607.0;

				} else if (flag >= 4) {
				  //
				  // conversione a int 32 e poi a float con shift e somma aritmetica (CON normalizzazione +1.0/-1.0)
				  // il MSB e' ignorato, identica alla precedente ma con puntatore invece di array
				  //
				
                  int ii = 0;
                  int qq = 0;

                  ii   = (int)((unsigned char)(*pb++))      ;
                  ii  += (int)((unsigned char)(*pb++)) << 8 ;
                  ii  += (int)((  signed char)(*pb++)) << 16;
				  pb++;
                  qq   = (int)((unsigned char)(*pb++))      ;
                  qq  += (int)((unsigned char)(*pb++)) << 8 ;
                  qq  += (int)((  signed char)(*pb++)) << 16;
                  pb++;
				  i += 8;

                  input_buffer[nsample_in_buf]      = (float)qq/8388607.0;
                  input_buffer[nsample_in_buf+NSPB] = (float)ii/8388607.0;

			  }
              nsample_in_buf ++ ;

              if (nsample_in_buf == NSPB) { 

			     for (j = 0; j < NSPB; j++) {
				    p->sdr->iqSample[j] =     input_buffer[j+NSPB] + 
                                              I * input_buffer[j];               // I on right
				 }

                 //fprintf (stderr, "DSP processing: %d\n", j);
			     
			     // actually run the SDR for a frame
			     sdr_process(p->sdr);
                 

			     //
			     // AUDIO
			     //
                 float ls[NSPB*4]; float rs[NSPB*4];
                 
                 for (x = 0; x < NSPB; ++x) {
                     ls[x] =  p->sdr->output[x];
                     rs[x] =  p->sdr->output[x];
                 }
 
                 //fprintf (stderr, "audio processing: %d\n", x);
			     
                 perseus_audio_write_sr (rs, ls);

                 nsample_in_buf = 0;
               }
            }
        } // 
    }
    mq_close (mq);
    mq_unlink (QUEUE_NAME);
	fprintf (stderr, "******************** rx thread exiting....\n");
    return 0;
}


int fdms1_connect(sdr_data_t *sdr, gboolean ci, gboolean co)
{
    int s;
    int rc = OpenHW (dev_handle, sr);

    if (rc == 1) {
       fprintf (stderr, ">>>> OpenHW returns: %d, control library successfully opened.\n", rc);
    } else {
       fprintf (stderr, "FATAL: OpenHW returns: %d\n", rc);
       return (255);
    }

    {
       //int d_en_ext_io_ATT20 = 0;
       //set_en_ext_io_ATT20 (dev_handle, &d_en_ext_io_ATT20 );
    }
    td.dev_handle = dev_handle;
    td.total_bytes_received = 0;
    td.sdr = sdr;
    // start an auxiliary thread that receives samples and 
    // execute the sdr_dsp
	s = pthread_create(&rx_thread_id, 0, &rx_thread, &td);
  
    if (s) {
       perror ("fdms1_connect, thread not started");
       return 1;
    }
    

    return 0;
}

int fdms1_start(sdr_data_t *sdr, int bandwidth)
{
	double x, y, z;
	
    x = 1.0, y = 2.0;	
    z = pow(x,y);
	
    char shl_name[BUFSIZ];
    void *shl_handle;
    sr = bandwidth;

    //FDMS1_HW_INIT init = 0;
	
    //libusb_context *context = 0;
    //libusb_device_handle *dev_handle; 
    //uint16_t vendor_id =  0x1721;
    //uint16_t product_id = 0x0610;
	// save some info in the SDR
	sdr->size = NSPB;
	sdr->sample_rate = sr; // jack_get_sample_rate(client);
	sdr->iqSample = g_new0(complex, sdr->size);
	sdr->output = g_new0(double, sdr->size); 	

    fprintf (stderr, ">>>>>>>>>>>>>>>>>>>>>>>>>> %d %d %p %p\n", sdr->size, sdr->sample_rate, sdr->iqSample, sdr->output
            );

    int rc = libusb_init ( &context );
	
    if (rc == 0 && context != 0) {
       libusb_set_debug(context,3); 
    } else {
       fprintf (stderr, "Error in libusb_init: [%d] %s\n", rc, libusb_error_name (rc));
       exit (1);
    }
    dev_handle = libusb_open_device_with_vid_pid ( context, vendor_id, product_id ); 

    if (dev_handle == 0) {
       fprintf (stderr, "Error in libusb_open_device_with_vid_pid\n");
       fprintf (stderr, "Check FDMS1 is properly connected and turned on.\n");
       exit (255);
    } else {
       fprintf (stderr, "libusb_open_device_with_vid_pid OK\n");
    }	

    if ( libusb_kernel_driver_active (dev_handle, 0) ){ 
       printf("Device busy...detaching...\n"); 
       libusb_detach_kernel_driver (dev_handle, 0); 
    } else 
       printf("Device free from kernel, continue...\n"); 
 
    rc = libusb_claim_interface (dev_handle, 0); //claim interface 0 (the first) of device (mine had jsut 1)
    if (rc < 0) {
       fprintf (stderr, "Cannot claim interface: [%d] %s\n", rc, libusb_error_name (rc));
       return (200);
    }
    fprintf (stdout, "Claimed Interface\n");




    //dlopen(NULL,RTLD_NOW|RTLD_GLOBAL);

    // Build the shared lib name, for example using snprintf() 
    snprintf (shl_name, sizeof(shl_name), "libfdms1_hw_init_%d.so.1.0", sr);

    // load the shared library (the shared library must be in a sys path or full path name must be given) 
    shl_handle = dlopen (shl_name, RTLD_NOW|RTLD_GLOBAL);
    if (shl_handle == NULL) {
       printf("dlopen error: %s\n\n", dlerror());
       return (1);
    }
    
    /* fetch the init function */
    init = (FDMS1_HW_INIT) dlsym (shl_handle, "fdms1_hw_init");

    if (init == NULL) {
        fprintf (stderr, "Function not found in library\n\n");
        exit (254);
    } else {
   	    /* Call the init... but do not expect that works...*/
        fprintf (stdout, "Loading FPGA, please wait ....."); fflush (stdout);
   	    int rc = init(dev_handle);  // The function returns 0 on error.

        if (rc) { 
           fprintf (stdout, "\rFPGA successfully loaded.                      \n");
           //
           //
           // attempt to open an audio stream on a local audio card
           //
	       ppa = perseus_audio_open (bandwidth);
           return 0; 
        } else {
           fprintf (stderr, "FATAL error loading FPGA image: %d\n", rc);
           return (252);
        } 
    }
}



int fdms1_stop(sdr_data_t *sdr)
{
	fprintf (stderr, ">>>> StopFIFO\n");
    StopFIFO (dev_handle);
    return 0;
}

int fdms1_update_freq(sdr_data_t *sdr)
{
    long freq = sdr->centre_freq;
    int rc = SetHWLO (dev_handle, &freq);
    fflush (stdout); fflush (stderr);
    if (rc == 1) {
	    fflush (stdout); fflush (stderr);
        fprintf (stderr, ">>>> set_HWLO returns: %d, frequency set to %ld.\n", rc, freq);
    } else {
        fprintf (stderr, "ERROR: set_HWLO returns: %d\n", rc);
    }
    return rc;
}

/* vim: set noexpandtab ai ts=4 sw=4 tw=4: */
