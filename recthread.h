/*
 * Data recorder thread
 */

#ifndef _RECTHREAD_H
#define _RECTHREAD_H

#include <pthread.h>

struct rec_thread_context {
  float *fft_win; /* FFT window coefficients */
  char *dongle_sn; /* Serial number of dongle to use */
  pthread_barrier_t *cal_on_barrier;
  pthread_barrier_t *cal_rec_done_barrier;
  pthread_barrier_t *cal_off_barrier;
  pthread_barrier_t *sig_rec_done_barrier;
};


void *rec_thread(void *ptarg);

#endif /* _RECTHREAD_H */
