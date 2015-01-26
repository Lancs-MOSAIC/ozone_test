/*
 * Computational thread
 */

#ifndef _COMPTHREAD_H
#define _COMPTHREAD_H

#include <pthread.h>
#include <stdint.h>

struct comp_thread_context {

  /* mutexes and condition variables */
  pthread_mutex_t *in_queue_mutex_p;
  pthread_cond_t *in_queue_cond_p;
  int *in_queue_len_p;
  pthread_mutex_t *out_queue_mutex_p;
  pthread_cond_t *out_queue_cond_p;
  int *out_queue_len;

  /* input data buffers */
  int max_in_queue_len;
  int sig_size;
  uint8_t *data_buf;
  int *data_buf_sig_len;

  /* output data buffers */
  int num_sig_spec;
  float *sig_spec_buf;
  int *sig_spec_int;

};


void *comp_thread(void *ptarg);

#endif /* _COMPTHREAD_H */

