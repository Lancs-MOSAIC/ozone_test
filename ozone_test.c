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
#include "rtldongle.h"

#define NUM_CHANNELS 2

int main(int argc, char *argv[])
{
  FILE *calfp;
  pthread_t rthread;
  float *fft_win;
  int r, n;
  pthread_barrier_t cal_on_barrier;
  pthread_barrier_t cal_rec_done_barrier;
  pthread_barrier_t cal_off_barrier;
  pthread_barrier_t sig_rec_done_barrier;
  pthread_mutex_t outfile_mutex = PTHREAD_MUTEX_INITIALIZER;
  uint64_t time_stamp;

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

  /* Initialise barriers for calibration synchronisation */
  r = pthread_barrier_init(&cal_on_barrier, NULL, NUM_CHANNELS + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(cal_on_barrier): %s\n", strerror(r));
    return 1;
  }

  r = pthread_barrier_init(&cal_rec_done_barrier, NULL, NUM_CHANNELS + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(cal_rec_done_barrier): %s\n",
	    strerror(r));
    return 1;
  }

  r = pthread_barrier_init(&cal_off_barrier, NULL, NUM_CHANNELS + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(cal_off_barrier): %s\n", strerror(r));
    return 1;
  }

  r = pthread_barrier_init(&sig_rec_done_barrier, NULL, NUM_CHANNELS + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(sig_rec_done_barrier): %s\n",
	    strerror(r));
    return 1;
  }

  for (n = 0; n < NUM_CHANNELS; n++) {

    /* Start a recorder thread */
    struct rec_thread_context *ctx = malloc(sizeof(struct rec_thread_context));
    if (ctx == NULL) {
      fprintf(stderr, "Failed to allocate rec thread context\n");
      return 1;
    }

    sprintf(ctx->dongle_sn, "SPEARS%04d", n + 1);

    ctx->fft_win = fft_win;
    ctx->dev = init_dongle(ctx->dongle_sn);
    if (ctx->dev == NULL) {
      fprintf(stderr, "Failed to init dongle %s\n", ctx->dongle_sn);
      return 1;
    }
    ctx->channel = n;
    ctx->time_stamp = &time_stamp;
    ctx->cal_on_barrier = &cal_on_barrier;
    ctx->cal_rec_done_barrier = &cal_rec_done_barrier;
    ctx->cal_off_barrier = &cal_off_barrier;
    ctx->sig_rec_done_barrier = &sig_rec_done_barrier;
    ctx->outfile_mutex = &outfile_mutex;

    r = pthread_create(&rthread, NULL, rec_thread, (void *)ctx);
    if (r != 0) {
      fprintf(stderr, "pthread_create(rec_thread): %s", strerror(r));
      return 1;
    }

  }

  /* Calibrator control loop */

  for (;;) {

    fprintf(stderr, "  main_thread: calibrator on\n");
    set_cal_state(calfp, 1);

    time_stamp = (uint64_t)time(NULL);

    r = pthread_barrier_wait(&cal_on_barrier);

    fprintf(stderr, "  main_thread: waiting for rec threads\n");
    r = pthread_barrier_wait(&cal_rec_done_barrier);

    fprintf(stderr, "  main_thread: calibrator off\n");
    set_cal_state(calfp, 0);

    r = pthread_barrier_wait(&cal_off_barrier);

    fprintf(stderr, "  main_thread: waiting for sig rec to finish\n");
    r = pthread_barrier_wait(&sig_rec_done_barrier);

  }

  fclose(calfp);

  return 0;
}
