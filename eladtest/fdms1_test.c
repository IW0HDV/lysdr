/*
    fdms1_test.c

    Copyright (C) 2013, 2014, 2015 - Andrea Montefusco, IW0HDV

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.  

    #
    # install the ELAD libraries (32 bit)
    #
    wget  http://sdr.eladit.com/FDM-S1%20Sampler/Linux/libfdms1_1.0-1_i386.deb
    sudo dpkg -i ./libfdms1_1.0-1_i386.deb
    sudo ln -s /usr/local/lib/libfdms1_hw_ctrl.so.1.0 /usr/local/lib/libfdms1-hw-ctrl.so
    #
    # install the ELAD libraries (64 bit)
    #
    wget http://sdr.eladit.com/FDM-S1%20Sampler/Linux/libfdms1_1.0-1_amd64.deb
    sudo dpkg -i ./libfdms1_1.0-1_amd64.deb
    sudo ln -s /usr/local/lib64/libfdms1_hw_ctrl.so.1.0 /usr/local/lib/libfdms1-hw-ctrl.so
    #
    sudo ldconfig
    # 
    # compile and test
    #
    gcc -Wall fdms1_test.c  -ldl -lusb-1.0 -lc -lm -lfdms1-hw-ctrl -o fdms1_test \
    && ./fdms1_test 192000 \
    && echo ""


 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <math.h>
#include <libusb-1.0/libusb.h>
#include <time.h>
#include <unistd.h>


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

int main(int argc, char **argv) {
   double x, y, z;
   char shl_name[BUFSIZ];
   void *shl_handle;
   int sr;

   FDMS1_HW_INIT init = 0;
	
   libusb_context *context = 0;
   libusb_device_handle *dev_handle; 
   uint16_t vendor_id =  0x1721;
   uint16_t product_id = 0x0610;
 	
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
   x = 1.0, y = 2.0;	
   z = pow(x,y);

   if ( libusb_kernel_driver_active (dev_handle, 0) ){ 
      printf("Device busy...detaching...\n"); 
      libusb_detach_kernel_driver (dev_handle, 0); 
   } else 
      printf("Device free from kernel, continue...\n"); 
 
   rc = libusb_claim_interface (dev_handle, 0); //claim interface 0 (the first) of device (mine had jsut 1)
   if (rc < 0) {
      fprintf (stderr, "Cannot claim interface: [%d] %s\n", rc, libusb_error_name (rc));
      exit (200);
   }
   fprintf (stdout, "Claimed Interface\n");


   /* 
    * sample rate from the first command line parameter
    */
   if ( (argc != 2 || (sscanf (argv[1], "%d", &sr) != 1)) && (sr == 192000 || sr == 384000 || sr == 768000 || sr == 1536000)) {
   	printf("Usage: %s [192000|384000|768000|1536000]\n\n",argv[0]);
   	exit(255);
    }

   //dlopen(NULL,RTLD_NOW|RTLD_GLOBAL);

   // Build the shared lib name, for example using snprintf() 
   snprintf (shl_name, sizeof(shl_name), "libfdms1_hw_init_%d.so.1.0", sr);
   // load the shared library (the shared library must be in a sys path or full path name must be given) 
   shl_handle = dlopen (shl_name, RTLD_NOW|RTLD_GLOBAL);
   if (shl_handle == NULL) {
      printf("dlopen error: %s\n\n", dlerror());
      exit(1);
    }
    
   /* fetch the init function */
   init = (FDMS1_HW_INIT) dlsym (shl_handle, "fdms1_hw_init");

   if (init == NULL) {
       fprintf (stderr, "Function not found in library\n\n");
       exit (254);
   } else {
       /* Call the init... */
       fprintf (stdout, "Loading FPGA, please wait ....."); fflush (stdout);
       int rc = init(dev_handle);  // The function returns 0 on error.

       if (rc) { 
          fprintf (stdout, "\rFPGA successfully loaded.                      \n"); 
       } else {
          fprintf (stderr, "FATAL error loading FPGA image: %d\n", rc);
          exit (252);
       } 
    }

   // 
   // Open Hardware and set the attenuator and local osc
   //
    {

       int d_en_ext_io_ATT20 = 1;
       
       int rc = OpenHW (dev_handle, sr);

       if (rc == 1) {
          fprintf (stderr, ">>>> OpenHW returns: %d, control library successfully opened.\n", rc);
            
       } else {
          fprintf (stderr, "FATAL: OpenHW returns: %d\n", rc);
          exit (255);
       }

       set_en_ext_io_ATT20 (dev_handle, &d_en_ext_io_ATT20 );
       
       {
           long freq = 7050000;
           int rc = SetHWLO (dev_handle, &freq);
           fflush (stdout); fflush (stderr);
           if (rc == 1) {
               fflush (stdout); fflush (stderr);
               fprintf (stderr, ">>>> set_HWLO returns: %d, frequency set to %ld.\n", rc, freq);
           } else {
               fprintf (stderr, "ERROR: set_HWLO returns: %d\n", rc);
           }
       }

   }

   //
   // Start the FIFO and read some data
   //
   {
       int i = 0;
       int timeout = 2000; // in ms
       unsigned char buf [8192];
       long total_bytes_received = 0;
       int n_tmo = 0;
       struct timespec  time_start, time_end, time_diff;
       long double diff_s ;
       long unsigned int ns;

       StartFIFO (dev_handle);
       fprintf (stderr, ">>>> StartFIFO\n");
 
       clock_gettime (CLOCK_REALTIME, &time_start);

       for (; total_bytes_received  < 1024000 && n_tmo < 3; ) {
          int transferred = 0;

          int rc = libusb_bulk_transfer ( dev_handle,
                                  (6 | LIBUSB_ENDPOINT_IN),
                                  buf, sizeof(buf),
                                          &transferred,
                                          timeout
                                        ); 	              
          if (rc) {
              fprintf (stderr, "Error in libusb_bulk_transfer: [%d] %s\n", rc, libusb_error_name (rc));
              if (rc == -7) { //TIMEOUT
                 n_tmo ++;
              } else 
                 break;

          } else {
              ++i;
              if (transferred >0) total_bytes_received += transferred;
          }
          usleep (10000);
       }

       // compute the sample per second
       clock_gettime(CLOCK_REALTIME, &time_end);
       time_diff = diff(time_start, time_end);
       diff_s = time_diff.tv_sec + (time_diff.tv_nsec/1E9) ;
       ns = total_bytes_received / 8;
       fprintf (stderr, "****** Total byte(s) received: %ld in %d operation(s)\n", total_bytes_received, i);
       fprintf (stderr, "Samples received: %lu, %.1Lf kS/s\n", ns, ((double)ns / (diff_s)/1E3) );

       fprintf (stderr, ">>>> StopFIFO\n");
       StopFIFO (dev_handle);

       for (i=0; i < 256; ++i) {
            fprintf (stderr, "%02X%02X%02X%02X ", buf[i], buf[i+1],buf[i+2],buf[i+3]);
       }
   }


   fprintf (stderr, "Exiting....\n");
   dlclose(shl_handle);	
   fprintf (stderr, "Closing USB device.\n");
   libusb_close (dev_handle);
   fprintf (stderr, "Freeing context...\n");
   libusb_exit (context);
   return 0;
}
