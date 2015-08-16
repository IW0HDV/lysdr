
#include <unistd.h>  // for sleep
#include <time.h>
#include <stdio.h>
#include "elad-fdms1.h"
#include <stdlib.h>

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



