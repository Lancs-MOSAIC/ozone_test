/*
 * Data recorder thread
 */

#ifndef _RECTHREAD_H
#define _RECTHREAD_H

#include <pthread.h>
#include "rtl-sdr.h"

#define MAX_SN_LEN 16

struct rec_thread_context {
  float *fft_win; /* FFT window coefficients */
  rtlsdr_dev_t *dev; /* librtlsdr device for dongle to use */
  pthread_barrier_t *cal_on_barrier;
  pthread_barrier_t *cal_rec_done_barrier;
  pthread_barrier_t *cal_off_barrier;
  pthread_barrier_t *sig_rec_done_barrier;
  pthread_mutex_t *outfile_mutex;
};


void *rec_thread(void *ptarg);

#endif /* _RECTHREAD_H */
