/*
 * A possible library for FDM S1 interfacing
 * Andrea Montefusco IW0HDV
 */
#if !defined __ELAD_FDMS1_H__
#define __ELAD_FDMS1_H__

#if defined __cplusplus
extern "C" {
#endif

struct fdms1_t;
typedef struct fdms1_t *FDMS1_T;
typedef void * (*FDMS1_CB) (unsigned char *, int len, void *user_param);

enum ELAD_FDMS1_RC
{ 
    EFDMS1_NOERROR      = 0, 
    EFDMS1_NOINIT       = 1,
    EFDMS1_NOEMEM       = 2,
    EFDMS1_USBERR       = 3,
    FDMS1_EFPGA         = 4,
    FDMS1_INVALIDSHARED = 5,
    FDMS1_EINVPARAM     = 6,
    FDMS1_DEVNOTFOUND   = 7
};

int fdms1_init (int sample_rate, FDMS1_T *);
int fdms1_deinit (FDMS1_T);

int fdms1_open (FDMS1_T, FDMS1_CB, void *usr_param);
int fdms1_close (FDMS1_T);

int fdms1_set_frequency (FDMS1_T, long frequency);
int fdms1_set_attenuator (FDMS1_T, int enable);
int fdms1_get_bytes_transferred (FDMS1_T);

const char *fdms1_get_error_msg (int err_code);

#if defined __cplusplus
};
#endif

#endif

