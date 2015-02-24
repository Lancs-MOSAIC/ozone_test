#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdlib.h>

#include "rtl-sdr.h"
#include <fftw3.h>
#include "calcontrol.h"
#include "common.h"
#include "recthread.h"
#include "signalproc.h"


int main(int argc, char *argv[])
{
  FILE *calfp;
  pthread_t rthread;
  struct rec_thread_context ctx;
  float *fft_win;
  int r;

  if (argc < 2) {
    fprintf(stderr, "Usage: ozone_test <dongle serial number>\n");
    return 1;
  }

  r = mlockall(MCL_CURRENT | MCL_FUTURE);
  if (r != 0) {
    perror("Could not lock memory");
  }

  init_convtab();

  fft_win = malloc(FFT_LEN * sizeof(float));
  if (fft_win == NULL) {
    fprintf(stderr, "Failed to allocate space for FFT window\n");
    return 1;
  }

  init_window(fft_win, FFT_LEN);

  if ((calfp = init_cal_control()) == NULL)
    return 1;

  /* Start a recorder thread */

  ctx.calfp = calfp;
  ctx.fft_win = fft_win;
  ctx.dongle_sn = argv[1];

  r = pthread_create(&rthread, NULL, rec_thread, (void *)&ctx);
  if (r != 0) {
    fprintf(stderr, "pthread_create(rec_thread): %s", strerror(r));
    return 1;
  }

  r = pthread_join(rthread, NULL);
  if (r != 0) {
    fprintf(stderr, "pthread_join(rec_thread): %s", strerror(r));
    return 1;
  }

  fclose(calfp);

  return 0;
}
