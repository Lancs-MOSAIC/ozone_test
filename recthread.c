/*
 * Data recorder thread
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fftw3.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "rtl-sdr.h"
#include "recthread.h"
#include "common.h"
#include "compthread.h"
#include "rtldongle.h"
#include "signalproc.h"
#include "calcontrol.h"
#include "config.h"

#define HEADER_MAGIC 0xa9e4b8b4
#define HEADER_VERSION 4

#define CALFREQ 1320000000 /* actual calibrator frequency */
#define LINEFREQ 1322454500 /* actual line frequency */
//#define LINEFREQ 1322754500 /* line + 300 kHz for testing */
//#define LINEFREQ CALFREQ
#define CALRXFREQ CALFREQ

#define READ_SIZE (16384 * 256)
#define NUM_BLOCKS 4
#define SIG_SIZE (NUM_BLOCKS * READ_SIZE)
#define NUM_SIG_SPEC 8
#define MAX_IN_QUEUE_LEN 3
#define MAX_SIG_LEVEL_SAMPLES 10000

/* Write data to file
 * This function is NOT thread-safe and calls MUST be
 * protected by a mutex!
 */

void write_file(struct rec_thread_context *ctx, uint64_t time_stamp,
		double freq_err, int spec_out_int[2],
		float *cal_spec_buf, float *spec_out_buf,
		int32_t max_sig_level)
{
  static FILE *fp = NULL;
  static uint64_t current_day = 0;
  char filename[_POSIX_PATH_MAX];
  const uint32_t hdr_magic = HEADER_MAGIC;
  const uint32_t samp_rate = SAMPLERATE;
  const uint32_t fft_len = FFT_LEN;
  const uint32_t hdr_version = HEADER_VERSION;
  const double line_freq = LINEFREQ;
  struct tm *tms;
  time_t t;

  uint64_t wanted_day = time_stamp - (time_stamp % 86400);

  if (fp != NULL) {
    /* if currently open file is not the right one, close it */
    if (current_day != wanted_day) {
      fclose(fp);
      fp = NULL;
    }
  }

  if (fp == NULL) {
    /* open the wanted file */
    t = (time_t)wanted_day;
    tms = gmtime(&t);
    snprintf(filename, _POSIX_PATH_MAX, "%s/%04d%02d%02d_s%03d.ozo",
	     data_dir, 1900 + tms->tm_year, tms->tm_mon + 1, tms->tm_mday,
	     vsrt_num);
    fp = fopen(filename, "a");
    if (fp == NULL) {
      fprintf(stderr, "Could not open file %s\n", filename);
      return;
    }
    fprintf(stderr, "Opened file %s\n", filename);
    current_day = wanted_day;
  }

  

  if (fwrite(&hdr_magic, sizeof(hdr_magic), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out magic value\n");

  if (fwrite(&hdr_version, sizeof(hdr_version), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out header version\n");

  uint32_t rec_len = 3 * FFT_LEN * sizeof(float) + sizeof(hdr_magic)
    + sizeof(hdr_version) + sizeof(rec_len)
    + sizeof(time_stamp) + sizeof(freq_err)
    + 2 * sizeof(int) + sizeof(samp_rate)
    + sizeof(fft_len) + sizeof(ctx->channel) + MAX_SN_LEN
    + sizeof(line_freq) + sizeof(vsrt_num) + MAX_STATION_NAME
    + sizeof(max_sig_level);

  if (fwrite(&rec_len, sizeof(rec_len), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out record length\n");

  if (fwrite(&time_stamp, sizeof(time_stamp), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out timestamp\n");

  if (fwrite(&freq_err, sizeof(freq_err), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out freq err\n");

  if (fwrite(spec_out_int, 2 * sizeof(int), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out int factors\n");

  if (fwrite(&samp_rate, sizeof(samp_rate), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out sample rate\n");

  if (fwrite(&fft_len, sizeof(fft_len), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out FFT length\n");

  if (fwrite(&ctx->channel, sizeof(ctx->channel), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out channel number\n");

  if (fwrite(ctx->dongle_sn, MAX_SN_LEN, 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out serial number\n");

  if (fwrite(&line_freq, sizeof(line_freq), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out line freq.\n");
 
  if (fwrite(&vsrt_num, sizeof(vsrt_num), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out VSRT number\n");

  if (fwrite(station_name, MAX_STATION_NAME, 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out station name\n");

  if (fwrite(&max_sig_level, sizeof(max_sig_level), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out max sig level\n");

  if (fwrite(cal_spec_buf, FFT_LEN * sizeof(float), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out cal spectrum\n");

  if (fwrite(spec_out_buf, 2 * FFT_LEN * sizeof(float), 1, fp) != 1)
    fprintf(stderr, "WARNING: could not write out sig spectra\n");

  fflush(fp);

}


void *rec_thread(void *ptarg)
{

  struct rec_thread_context *ctx;
  int n, r, n_read;
  double freq_err;
  uint32_t line_rx_freq;
  pthread_t cthread;
  struct comp_thread_context cctx;
  int in_queue_in_ptr = 0, out_queue_out_ptr = 0;
  fftwf_complex *fftin;
  fftwf_complex *fftout;
  fftwf_plan fplan;
  float spec_out_buf[2 * FFT_LEN];
  int spec_out_int[2];
  uint64_t time_stamp;
  int data_buf_idx;
  pthread_mutex_t in_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t in_queue_cond = PTHREAD_COND_INITIALIZER;
  int in_queue_len = 0;
  pthread_mutex_t out_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t out_queue_cond = PTHREAD_COND_INITIALIZER;
  int out_queue_len = 0;
  int32_t max_sig_level;

  fprintf(stderr, "  rec_thread: thread started\n");

  ctx = (struct rec_thread_context *)ptarg;

  if (READ_SIZE % (2 * FFT_LEN) != 0)
    fprintf(stderr, "  rec_thread: WARNING: dongle read length is not a multiple of FFT length\n");

  if (SIG_SIZE % (2 * FFT_LEN) != 0)
    fprintf(stderr, "  rec_thread: WARNING: signal length is not a multiple of FFT length\n");

  /* Allocate data buffers */

  uint8_t *data_buf = malloc(SIG_SIZE * MAX_IN_QUEUE_LEN);
  if (data_buf == NULL)
    return NULL;

  int *data_buf_sig_len = malloc(MAX_IN_QUEUE_LEN * sizeof(int));
  if (data_buf_sig_len == NULL)
    return NULL;

  uint8_t *cal_data_buf = malloc(READ_SIZE);
  if (cal_data_buf == NULL)
    return NULL;

  float *cal_spec_buf = malloc(FFT_LEN * sizeof(float));
  if (cal_spec_buf == NULL)
    return NULL;

  float *sig_spec_buf = malloc(FFT_LEN * NUM_SIG_SPEC * 2 * sizeof(float));
  if (sig_spec_buf == NULL)
    return NULL;

  int *sig_spec_int = malloc(NUM_SIG_SPEC * 2 * sizeof(int));
  if (sig_spec_int == NULL)
    return NULL;

  if ((fplan = init_fft(&fftin, &fftout)) == NULL)
    return NULL;

  /* Create computational thread */

  cctx.in_queue_mutex_p = &in_queue_mutex;
  cctx.in_queue_cond_p = &in_queue_cond;
  cctx.in_queue_len_p = &in_queue_len;
  cctx.out_queue_mutex_p = &out_queue_mutex;
  cctx.out_queue_cond_p = &out_queue_cond;
  cctx.out_queue_len = &out_queue_len;
  cctx.max_in_queue_len = MAX_IN_QUEUE_LEN;
  cctx.sig_size = SIG_SIZE;
  cctx.data_buf = data_buf;
  cctx.data_buf_sig_len = data_buf_sig_len;
  cctx.num_sig_spec = NUM_SIG_SPEC;
  cctx.sig_spec_buf = sig_spec_buf;
  cctx.sig_spec_int = sig_spec_int;
  
  r = pthread_create(&cthread, NULL, comp_thread, (void *)&cctx);
  if (r != 0) {
    fprintf(stderr, "pthread_create(): %s", strerror(r));
    return NULL;
  }

  /* set RT scheduling for this thread */

  struct sched_param spar;
  spar.sched_priority = 99;
  r = pthread_setschedparam(pthread_self(), SCHED_FIFO, &spar);
  if (r != 0) {
    fprintf(stderr, "WARNING: could not set RT scheduling: %s\n", \
	    strerror(r));
  }

  while (1) {

    max_sig_level = 0;

    set_frequency(ctx->dev, CALRXFREQ);
    
    /* Clear signal data buffer: 127 corresponds to zero signal */

    memset(cal_data_buf, 127, sizeof(cal_data_buf)); 

    fprintf(stderr, "  rec_thread: waiting for cal on\n");
    r = pthread_barrier_wait(ctx->cal_on_barrier);

    fprintf(stderr, "  rec_thread: recording cal\n");

    time_stamp = *(ctx->time_stamp);

    rtlsdr_reset_buffer(ctx->dev); /* flush any old signal away */

    r = rtlsdr_read_sync(ctx->dev, cal_data_buf, READ_SIZE, &n_read);
    if (r < 0)
      fprintf(stderr, "WARNING: rtlsdr_read_sync() failed\n");
    if (n_read != READ_SIZE)
      fprintf(stderr, "WARNING: received wrong number of samples (%d)\n", \
	      n_read);

    fprintf(stderr, "  rec_thread: waiting for cal to finish\n");
    r = pthread_barrier_wait(ctx->cal_rec_done_barrier);
  
    fprintf(stderr, "  Calculating spectrum... ");
    calc_spectrum(cal_data_buf, READ_SIZE, cal_spec_buf, NULL, \
		  ctx->fft_win, fplan, fftin, fftout);
    fprintf(stderr, "Done.\n");

    freq_err = find_freq_error(cal_spec_buf, SAMPLERATE, CALRXFREQ, CALFREQ);

    fprintf(stderr, "  rec_thread: waiting for cal off\n");
    r = pthread_barrier_wait(ctx->cal_off_barrier);

    for (int scount = 0; scount < 2 * NUM_SIG_SPEC; scount++) {

      if ((scount % 2) == 0) {

	/* tune above line frequency */
	
	line_rx_freq = (uint32_t)((double)LINEFREQ + (double)(SAMPLERATE / 4) \
				  + freq_err);
      } else {
	/* tune below line frequency */

	line_rx_freq = (uint32_t)((double)LINEFREQ - (double)(SAMPLERATE / 4) \
				 + freq_err);
      }

      set_frequency(ctx->dev, line_rx_freq);

      fprintf(stderr, "  rec_thread: recording signal %d, %d\n", scount,
	      in_queue_in_ptr);

      rtlsdr_reset_buffer(ctx->dev); /* flush any cal signal away */

      /* Check for space in queue, wait if full */

      r = pthread_mutex_lock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(r));
	return NULL;
      }

      while (in_queue_len == MAX_IN_QUEUE_LEN) {
	fprintf(stderr, "  rec_thread: waiting for space in queue\n");
	r = pthread_cond_wait(&in_queue_cond, &in_queue_mutex);
	if (r != 0) {
	  fprintf(stderr, "  rec_thread: pthread_cond_wait(in_queue): %s\n",
		  strerror(r));
	  return NULL;
	}
      }

      r = pthread_mutex_unlock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(r));
	return NULL;
      }

      /* Clear signal data buffer: 127 corresponds to zero signal */

      memset(&data_buf[in_queue_in_ptr * SIG_SIZE], 127, SIG_SIZE);

      data_buf_sig_len[in_queue_in_ptr] = 0;

      data_buf_idx = 0;

      for(n = 0; n < NUM_BLOCKS; n++) {

	r = rtlsdr_read_sync(ctx->dev, &data_buf[in_queue_in_ptr * SIG_SIZE
			    + data_buf_idx], READ_SIZE, &n_read);
	if (r < 0)
	  fprintf(stderr, "WARNING: rtlsdr_read_sync() failed\n");
	if (n_read != READ_SIZE)
	  fprintf(stderr, "WARNING: received wrong number of samples (%d)\n", \
		  n_read);
	if ((n_read % 2) != 0) {
	  fprintf(stderr, "WARNING: odd number of samples received!\n");
	  n_read++; /* preserve real/imaginary alignment */
	}

	data_buf_idx += n_read;
	data_buf_sig_len[in_queue_in_ptr] += n_read;

      }

      /* Use first part of recorded signal to monitor level */

      int samps_to_check = data_buf_sig_len[in_queue_in_ptr] 
                           > MAX_SIG_LEVEL_SAMPLES ? MAX_SIG_LEVEL_SAMPLES
                           : data_buf_sig_len[in_queue_in_ptr];

      for (n = 0; n < samps_to_check; n++) {
	int32_t x = (int32_t)data_buf[in_queue_in_ptr * SIG_SIZE + n] - 127;
        if (abs(x) > max_sig_level)
	  max_sig_level = x;
      }
        

      r = pthread_mutex_lock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(r));
	return NULL;
      }

      in_queue_len++;
      in_queue_in_ptr = (in_queue_in_ptr + 1) % MAX_IN_QUEUE_LEN;

      r = pthread_mutex_unlock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(r));
	return NULL;
      }

      
      r = pthread_cond_signal(&in_queue_cond);
      if (r != 0) {
	fprintf(stderr, "pthread_cond_signal: %s\n", strerror(r));
	return NULL;
      }
      

    }

    memset(spec_out_buf, 0, 2 * FFT_LEN * sizeof(float));
    memset(spec_out_int, 0, 2 * sizeof(int));

    /* Read signal spectra from queue and store them */

    for (int scount = 0; scount < 2 * NUM_SIG_SPEC; scount++) {

      r = pthread_mutex_lock(&out_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(r));
	return NULL;
      }

      while (out_queue_len == 0) {
	fprintf(stderr, "  rec_thread: waiting for data\n");
	r = pthread_cond_wait(&out_queue_cond, &out_queue_mutex);
	if (r != 0) {
	  fprintf(stderr, "pthread_cond_wait: %s\n", strerror(r));
	  return NULL;
	}
      }

      out_queue_len--;

      r = pthread_mutex_unlock(&out_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(r));
	return NULL;
      }

      /* integrate spectra */

      int n = scount % 2;
      spec_out_int[n] += sig_spec_int[out_queue_out_ptr];
      for (int k = 0; k < FFT_LEN; k++) {
	spec_out_buf[n * FFT_LEN + k] += \
	  sig_spec_buf[FFT_LEN * out_queue_out_ptr + k];
      }

      out_queue_out_ptr = (out_queue_out_ptr + 1) % (2 * NUM_SIG_SPEC);


    }

    /* normalise spectra */
    for (int k =0; k < 2; k++) {
      for (int n = 0; n < FFT_LEN; n++) {
	spec_out_buf[k * FFT_LEN + n] /= 
		((float)spec_out_int[k] * (float)FFT_LEN * (float)FFT_LEN);
      }
    }


    /*  writing to output file protected by mutex  */

    r = pthread_mutex_lock(ctx->outfile_mutex);
    if (r != 0) {
      fprintf(stderr, "pthread_mutex_lock(ctx->outfile_mutex): %s\n",
	      strerror(r));
      return NULL;
    }

    write_file(ctx, time_stamp, freq_err, spec_out_int,
	       cal_spec_buf, spec_out_buf, max_sig_level);

    r = pthread_mutex_unlock(ctx->outfile_mutex);
    if (r != 0) {
      fprintf(stderr, "pthread_mutex_unlock(ctx->outfile_mutex): %s\n",
	      strerror(r));
      return NULL;
    }

    fprintf(stderr, "  recthread %d: max signal level = %d\n",
            ctx->channel, max_sig_level); 

    r = pthread_barrier_wait(ctx->sig_rec_done_barrier);

  }

  
  rtlsdr_close(ctx->dev);

  return NULL;
}
