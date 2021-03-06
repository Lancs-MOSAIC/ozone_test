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
#include <time.h>
#include <errno.h>

#include "rtl-sdr.h"
#include <fftw3.h>
#include "calcontrol.h"
#include "common.h"
#include "recthread.h"
#include "signalproc.h"
#include "rtldongle.h"
#include "config.h"

timer_t watchdog;

void watchdog_handler(int sig)
{
  fprintf(stderr, "Watchdog timer expired! Exiting.\n");
  exit(EXIT_FAILURE);
}

void watchdog_reset(void)
{
  struct itimerspec its;
 
  its.it_value.tv_sec = watchdog_timeout;
  its.it_value.tv_nsec = 0;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;
  if (timer_settime(watchdog, 0, &its, NULL) != 0)
    perror("timer_settime");
}

int watchdog_init(void)
{

  struct sigaction sa;
  sigset_t ss;

  sigemptyset(&ss);
  sa.sa_handler = watchdog_handler;
  sa.sa_mask = ss;
  sa.sa_flags = 0;
  if (sigaction(SIGALRM, &sa, NULL) != 0)
    return 1;

  /* create timer, requesting SIGALRM on expiry */

  return timer_create(CLOCK_REALTIME, NULL, &watchdog);
}

int main(int argc, char *argv[])
{
  FILE *calfp;
  pthread_t rthread;
  float *fft_win;
  int r, n, opt;
  int conf_read = 0;
  pthread_barrier_t cal_on_barrier;
  pthread_barrier_t cal_rec_done_barrier;
  pthread_barrier_t cal_off_barrier;
  pthread_barrier_t sig_rec_done_barrier;
  pthread_mutex_t outfile_mutex = PTHREAD_MUTEX_INITIALIZER;
  uint64_t time_stamp;


  while ((opt = getopt(argc, argv, "f:")) != -1) {
    switch (opt) {
      case 'f':
	read_config(optarg);
	conf_read = 1;
	break;
      default:
	fprintf(stderr, "Usage: ozonespec [-f <config file>]\n");
	return 1;
    }
  }

  if (!conf_read)
    read_config(NULL);

  if (num_channels < 1) {
    fprintf(stderr, "No channels defined!\n");
    return 1;
  }

  if (watchdog_init() != 0) {
    perror("Could not initialise watchdog timer");
    return 1;
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
  r = pthread_barrier_init(&cal_on_barrier, NULL, num_channels + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(cal_on_barrier): %s\n", strerror(r));
    return 1;
  }

  r = pthread_barrier_init(&cal_rec_done_barrier, NULL, num_channels + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(cal_rec_done_barrier): %s\n",
	    strerror(r));
    return 1;
  }

  r = pthread_barrier_init(&cal_off_barrier, NULL, num_channels + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(cal_off_barrier): %s\n", strerror(r));
    return 1;
  }

  r = pthread_barrier_init(&sig_rec_done_barrier, NULL, num_channels + 1);
  if (r != 0) {
    fprintf(stderr, "pthread_barrier_init(sig_rec_done_barrier): %s\n",
	    strerror(r));
    return 1;
  }

  for (n = 0; n < num_channels; n++) {

    /* Start a recorder thread */
    struct rec_thread_context *ctx = malloc(sizeof(struct rec_thread_context));
    if (ctx == NULL) {
      fprintf(stderr, "Failed to allocate rec thread context\n");
      return 1;
    }

    strcpy(ctx->dongle_sn, &dongle_sns[n][0]);

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

  /* Set realtime scheduling */

  struct sched_param spar;
  spar.sched_priority = RT_PRIO_MAIN;
  r = sched_setscheduler(getpid(), SCHED_FIFO, &spar);
  if (r != 0)
    perror("  main_thread: Failed to set RT scheduling");

  /* Calibrator control loop */

  for (;;) {

    fprintf(stderr, "  main_thread: calibrator on\n");
    set_cal_state(calfp, 1);

    time_stamp = (uint64_t)time(NULL);

    watchdog_reset();

    r = pthread_barrier_wait(&cal_on_barrier);

    fprintf(stderr, "  main_thread: waiting for rec threads\n");
    r = pthread_barrier_wait(&cal_rec_done_barrier);

    if (!keep_cal_on) {
      fprintf(stderr, "  main_thread: calibrator off\n");
      set_cal_state(calfp, 0);
    } else
      fprintf(stderr, "  main_thread: calibrator remains on\n");

    r = pthread_barrier_wait(&cal_off_barrier);

    fprintf(stderr, "  main_thread: waiting for sig rec to finish\n");
    r = pthread_barrier_wait(&sig_rec_done_barrier);

  }

  fclose(calfp);

  return 0;
}
