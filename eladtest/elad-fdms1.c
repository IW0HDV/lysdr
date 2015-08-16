/*
 * A possible library for FDM S1 interfacing
 * Andrea Montefusco IW0HDV
 *
 *
 * Compile with: 
 *
 * gcc -g -Wall -D__TEST_MODULE__  elad-fdms1.c  -ldl -lusb-1.0 -lc -lm -lfdms1-hw-ctrl  -o elad_test 
 *
 */

#include <stdlib.h>  // required for malloc
#include <stdio.h>   // fprintf
#include <string.h>  // memset
#include <dlfcn.h>
#include <math.h>
#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <mqueue.h>

#include "elad-fdms1.h"

typedef int (* FDMS1_HW_INIT)(libusb_device_handle *);

// functions in FDM S1 control shared library
extern void set_en_ext_io_ATT20 (libusb_device_handle *deviceHandle, int *d_en_ext_io_ATT20 );
extern int  OpenHW (libusb_device_handle *deviceHandle, long sample_rate);
extern int  SetHWLO (libusb_device_handle *, long *);
extern void StartFIFO ( libusb_device_handle* deviceHandle );
extern void StopFIFO ( libusb_device_handle* deviceHandle );

struct fdms1_t {
   char shl_name[BUFSIZ];
   void *shl_handle;
   FDMS1_HW_INIT init;

   libusb_context *context;
   libusb_device_handle *dev_handle; 
   uint16_t vendor_id;
   uint16_t product_id;
   libusb_device_handle *device_handle;

   int sample_rate;

   pthread_t usb_thread_id;
   FDMS1_CB usr_cb;
   void *usr_param;
   long total_bytes_received; 

   int fClose;
};

#define IQ_SIZE  8

#define QUEUE_NAME "/fdms1usb"

#define NSPB 1024

static int initialized = 0;

int fdms1_init (int sample_rate, struct fdms1_t **pp)
{
    double x, y, z;

    FDMS1_T p = (FDMS1_T)malloc (sizeof(struct fdms1_t));
    if (!(p)) {
       *pp = 0;
       return EFDMS1_NOEMEM;
    } else {

       *pp = p; 
    }
    x = 1.0, y = 2.0;	
    y = z = pow(x,y);

    memset (p, sizeof (FDMS1_T), 0);
    p->vendor_id  = 0x1721;
    p->product_id = 0x0610;
    p->sample_rate = sample_rate;

    int rc = libusb_init ( &(p->context) );
	
    if (rc == 0 && p->context != 0) {
       fprintf (stderr, "Context: [%p]\n", p->context);
       libusb_set_debug (p->context,3); 
    } else {
       fprintf (stderr, "Error in libusb_init: [%d] %s\n", rc, libusb_error_name (rc));
       return 255;
    }
    p->dev_handle = libusb_open_device_with_vid_pid ( p->context, p->vendor_id, p->product_id ); 

    if (p->dev_handle == 0) {
       fprintf (stderr, "Error in libusb_open_device_with_vid_pid\n");
       fprintf (stderr, "Check FDMS1 is properly connected and turned on.\n");
       return FDMS1_DEVNOTFOUND;
    } else {
       fprintf (stdout, "libusb_open_device_with_vid_pid OK\n");
    }	

    if ( libusb_kernel_driver_active (p->dev_handle, 0) ){ 
       fprintf (stdout, "Device busy...detaching...\n"); 
       rc = libusb_detach_kernel_driver (p->dev_handle, 0);
       if (rc < 0) {
          fprintf (stderr, "Cannot detach driver: [%d] %s\n", rc, libusb_error_name (rc));
          return EFDMS1_USBERR;
       } 
    } else 
       fprintf(stdout, "Device free from kernel, continue...\n"); 
 
    rc = libusb_claim_interface (p->dev_handle, 0); //claim interface 0 (the first) of device (mine had jsut 1)
    if (rc < 0) {
       fprintf (stderr, "Cannot claim interface: [%d] %s\n", rc, libusb_error_name (rc));
       return (200);
    }
    fprintf (stdout, "Claimed Interface\n");

    // now the libusb is ready
    
    // 
    // now go ahead for Elad provided shared abracadabra 
    //

    //dlopen(NULL,RTLD_NOW|RTLD_GLOBAL);

    // Build the shared lib name, for example using snprintf() 
    snprintf (p->shl_name, sizeof(p->shl_name), "libfdms1_hw_init_%d.so.1.0", sample_rate);

    // load the shared library (the shared library must be in a sys path or full path name must be given) 
    p->shl_handle = dlopen (p->shl_name, RTLD_NOW|RTLD_GLOBAL);
    if (p->shl_handle == NULL) {
       printf("dlopen error: %s\n\n", dlerror());
       return (1);
    }
    
    /* fetch the init function */
    p->init = (FDMS1_HW_INIT) dlsym (p->shl_handle, "fdms1_hw_init");

    if (p->init == NULL) {
        fprintf (stderr, "Function not found in library\n\n");
        return FDMS1_INVALIDSHARED;
    } else {
   	/* Call the init, now that works ! */
        fprintf (stdout, "Loading FPGA, please wait ....."); fflush (stdout);
        int rc = p->init (p->dev_handle);  // The function returns 0 on error.
        if (rc) { 
           fprintf (stdout, "\rFPGA successfully loaded.                      \n");
        } else {
           fprintf (stderr, "FATAL error loading FPGA image: %d\n", rc);
           return FDMS1_EFPGA;
        } 
    }
    initialized = 1;
   
    return EFDMS1_NOERROR;    
}


static
void *usb_thread (void *param)
{
    struct fdms1_t *p = (struct fdms1_t *) param;
    int i;
    int timeout = 2000; // in ms
    
    int n_tmo = 0;
    unsigned char ibuf [512*8];

    StartFIFO (p->dev_handle);
    fprintf (stderr, ">>>> StartFIFO\n");

    for (i=0; n_tmo < 3 && !p->fClose; ++i) {
       int transferred = 0;

       int rc = libusb_bulk_transfer ( p->dev_handle,
                                       (6 | LIBUSB_ENDPOINT_IN),
                                       ibuf, sizeof(ibuf),
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
               p->total_bytes_received += transferred;
               if (p->usr_cb) p->usr_cb ((unsigned char *)ibuf, sizeof(ibuf), p->usr_param);
           } else {
               fprintf (stderr, "******************** Error in libusb_bulk_transfer: %d\n", transferred);
               continue;
           }
      
       }
       
    }
    StopFIFO (p->dev_handle);
    fprintf (stderr, "******************** FIFO stopped, usb thread exiting....\n");
    return 0;
}



int fdms1_open (struct fdms1_t *p, FDMS1_CB cb, void *usr_param)
{
    
    if (p) {
        int s;
        int rc;

        fprintf (stderr, ">>>> %d \n", p->sample_rate);

        rc = OpenHW (p->dev_handle, p->sample_rate);

        if (rc == 1) {
           fprintf (stderr, ">>>> OpenHW returns: %d, control library successfully opened.\n", rc);
        } else {
           fprintf (stderr, "FATAL: OpenHW returns: %d\n", rc);
           return (255);
        }
        p->total_bytes_received = 0;
        p->usr_cb = cb;
        p->usr_param = usr_param;
        // start an auxiliary thread that receives samples and 
        // execute the sdr_dsp
	s = pthread_create(&(p->usb_thread_id), 0, &usb_thread, p);
  
        if (s) {
           perror ("fdms1_connect, thread not started");
           return 1;
        }
        return 0;
    } else {
        return FDMS1_EINVPARAM;
    }
}

int fdms1_close (struct fdms1_t *p)
{    
    if (p) {
       void *pexit;
       p->fClose = 1;
       pthread_join (p->usb_thread_id, &pexit);
       libusb_close (p->dev_handle);
       return 0;
    } else {
       return FDMS1_EINVPARAM;
    }
}


int fdms1_deinit (struct fdms1_t *p)
{       
   if (!p) return FDMS1_EINVPARAM;
   fprintf (stderr, "Closing shared library....\n");
   dlclose(p->shl_handle);	
   fprintf (stderr, "Exiting USB library... %p\n", p->context);
   libusb_exit (p->context);
   fprintf (stderr, "Freeing opaque type object...\n");
   free (p);
   return 0;
}

int fdms1_set_attenuator (struct fdms1_t *p, int att)
{
    if (p) {
       int d_en_ext_io_ATT20 = (att ? 1 : 0);
       set_en_ext_io_ATT20 (p->dev_handle, &d_en_ext_io_ATT20 );
       return 0;
    } else {
       return FDMS1_EINVPARAM;
    }
}

int fdms1_set_frequency (struct fdms1_t *p, long frequency)
{
    if (p) {
        int rc = SetHWLO (p->dev_handle, &frequency);
        fflush (stdout); fflush (stderr);
        if (rc == 1) {
           return 0;
        } else {
           return FDMS1_EINVPARAM;
        }
    } else {
        return FDMS1_EINVPARAM;
    }
}


struct err_msg {
    int code;
    char *msg;
} err_table [] =
{
    { EFDMS1_NOERROR, "no error" },
    { EFDMS1_NOINIT,  "library non initialized" },
    { EFDMS1_NOEMEM,  "memory allocation failed" },
    { EFDMS1_USBERR,  "libusb error" },
    { FDMS1_EFPGA,    "error loading FPGA" },
    { FDMS1_INVALIDSHARED, "ubnable to locate or load FPGA shared library" },
    { FDMS1_EINVPARAM, "bad parameter" },
    { FDMS1_DEVNOTFOUND, "Device not found" },
};

const char *fdms1_get_error_msg (int err_code)
{
    int i;
    for (i = 0; i < ( sizeof(err_table)/sizeof(err_table[0])) ; ++i)
        if (err_table[i].code == err_code) return err_table[i].msg;
    return "unknown error code";
}

int fdms1_get_bytes_transferred (struct fdms1_t *p)
{    
    if (p) {
       return p->total_bytes_received;
    } else {
       return FDMS1_EINVPARAM;
    }
}

#if defined __TEST_MODULE__

#include <unistd.h>  // for sleep
#include <time.h>


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



#define CHECK(x) \
    do { \
        int rc = 0; \
        if ((rc = x)) { \
            fprintf(stderr, "%s:%d: [%d]: %s\n", __func__, __LINE__, rc, fdms1_get_error_msg(rc)); \
            exit(-1); \
        } \
    } while (0) \


struct _CBD {
    int i;

} cb_data ;

void * usr_cb (unsigned char *buf, int len, void *user_param)
{
    return 0;
}

int main (void)
{
    FDMS1_T  fdms1;
    struct _CBD cbd = {0};

    int i = 0;
    struct timespec  time_start, time_end, time_diff;
    long double diff_s ;
    long unsigned int ns;

    CHECK ( fdms1_init (384000, &fdms1) );


    CHECK ( fdms1_open (fdms1, usr_cb, (void *)&cbd));

    clock_gettime (CLOCK_REALTIME, &time_start);


    sleep (10);


    CHECK ( fdms1_close (fdms1) ) ;

       // compute the sample per second
       clock_gettime(CLOCK_REALTIME, &time_end);
       time_diff = diff(time_start, time_end);
       diff_s = time_diff.tv_sec + (time_diff.tv_nsec/1E9) ;
       ns = fdms1_get_bytes_transferred (fdms1) / 8;
       fprintf (stderr, "****** Total byte(s) received: %d in %d operation(s)\n", fdms1_get_bytes_transferred (fdms1), i);
       fprintf (stderr, "Samples received: %lu, %.1Lf kS/s\n", ns, ((double)ns / (diff_s)/1E3) );

    CHECK ( fdms1_deinit (fdms1) ) ;
    return 0;
}



#endif

