/*
 * Data recorder thread
 */

#ifndef _RECTHREAD_H
#define _RECTHREAD_H

struct rec_thread_context {
  FILE *calfp; /* calibrator control */
  float *fft_win; /* FFT window coefficients */
  char *dongle_sn; /* Serial number of dongle to use */
};


void *rec_thread(void *ptarg);

#endif /* _RECTHREAD_H */
