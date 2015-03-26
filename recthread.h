/*
 * Data recorder thread
 */

#ifndef _RECTHREAD_H
#define _RECTHREAD_H

#include <pthread.h>
#include <stdint.h>
#include "rtl-sdr.h"
#include "common.h"


struct rec_thread_context {
  float *fft_win; /* FFT window coefficients */
  rtlsdr_dev_t *dev; /* librtlsdr device for dongle to use */
  int32_t channel; /* channel number */
  char dongle_sn[MAX_SN_LEN]; /* dongle serial number */
  uint64_t *time_stamp;
  pthread_barrier_t *cal_on_barrier;
  pthread_barrier_t *cal_rec_done_barrier;
  pthread_barrier_t *cal_off_barrier;
  pthread_barrier_t *sig_rec_done_barrier;
  pthread_mutex_t *outfile_mutex;
};


void *rec_thread(void *ptarg);

#endif /* _RECTHREAD_H */
