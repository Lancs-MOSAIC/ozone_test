/*
 * Computational thread
 */

#include <pthread.h>
#include <fftw3.h>
#include <stdio.h>
#include "compthread.h"
#include "signalproc.h"
#include "common.h"
#include <string.h>

void *comp_thread(void *ptarg)
{
  fftwf_plan cfplan;
  fftwf_complex *cfftin, *cfftout;
  int r, in_queue_out_ptr = 0, out_queue_in_ptr = 0;
  struct comp_thread_context *ctx;

  ctx = (struct comp_thread_context *)ptarg;

  fprintf(stderr, "  comp_thread: computation thread alive\n");

  cfplan = init_fft(&cfftin, &cfftout);
  if (cfplan == NULL) {
    fprintf(stderr, "  comp_thread: failed to initialise FFT\n");
    return NULL;
  }



  while (1) {

    /* Check input queue and wait if nothing to process */

    r = pthread_mutex_lock(ctx->in_queue_mutex_p);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_lock: %s\n", strerror(r));
      return NULL;
    }

    while (*(ctx->in_queue_len_p) == 0) {
      fprintf(stderr, "  comp_thread: waiting for data\n");
      r = pthread_cond_wait(ctx->in_queue_cond_p, ctx->in_queue_mutex_p);
      if (r != 0) {
	fprintf(stderr, "  comp_thread: pthread_cond_wait: %s\n", strerror(r));
	return NULL;
      }
    }

    r = pthread_mutex_unlock(ctx->in_queue_mutex_p);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_unlock: %s\n", strerror(r));
      return NULL;
    }

    /* Process a block of signal */

    fprintf(stderr, "  comp_thread: calculating spectrum (%d)\n", 
	    in_queue_out_ptr);

    calc_spectrum(&ctx->data_buf[in_queue_out_ptr * ctx->sig_size],
		  ctx->data_buf_sig_len[in_queue_out_ptr],
		  &ctx->sig_spec_buf[out_queue_in_ptr * FFT_LEN],
		  &ctx->sig_spec_int[out_queue_in_ptr], NULL,
		  cfplan, cfftin, cfftout);

    in_queue_out_ptr = (in_queue_out_ptr + 1) % ctx->max_in_queue_len;
    out_queue_in_ptr = (out_queue_in_ptr + 1) % (ctx->num_sig_spec * 2);

    /* Indicate that data has been removed from input queue and
       added to output queue
    */

    r = pthread_mutex_lock(ctx->in_queue_mutex_p);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_lock: %s\n", strerror(r));
      return NULL;
    }

    (*(ctx->in_queue_len_p))--;

    r = pthread_mutex_unlock(ctx->in_queue_mutex_p);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_unlock: %s\n", strerror(r));
      return NULL;
    }

    r = pthread_cond_signal(ctx->in_queue_cond_p);
    if (r != 0) {
      fprintf(stderr, "pthread_cond_signal: %s\n", strerror(r));
      return NULL;
    }

    r = pthread_mutex_lock(ctx->out_queue_mutex_p);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_lock: %s\n", strerror(r));
      return NULL;
    }

    (*(ctx->out_queue_len))++;

    r = pthread_mutex_unlock(ctx->out_queue_mutex_p);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_unlock: %s\n", strerror(r));
      return NULL;
    }

    r = pthread_cond_signal(ctx->out_queue_cond_p);
    if (r != 0) {
      fprintf(stderr, "pthread_cond_signal: %s\n", strerror(r));
      return NULL;
    }

  }

  fftwf_free(cfftin);
  fftwf_free(cfftout);
  fftwf_destroy_plan(cfplan);

  return NULL;

}



