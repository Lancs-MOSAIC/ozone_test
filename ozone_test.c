#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>

#include "rtl-sdr.h"
#include <fftw3.h>

#define GPIOPATH "/sys/class/gpio/gpio60"
#define GPIODIR "/direction"
#define GPIOVAL "/value"

#define SAMPLERATE 2500000
#define CALFREQ 1320000000 /* actual calibrator frequency */
#define LINEFREQ 1322454500 /* actual line frequency */
//#define LINEFREQ CALFREQ
#define CALRXFREQ 1319000000


int dongle_debug = 1;

#define READ_SIZE (16384 * 256)
#define NUM_BLOCKS 5
#define SIG_SIZE (NUM_BLOCKS * READ_SIZE)
#define NUM_SIG_SPEC 8
#define FFT_LEN 1024
#define MAX_IN_QUEUE_LEN 3

uint8_t data_buf[SIG_SIZE * MAX_IN_QUEUE_LEN];
uint8_t cal_data_buf[READ_SIZE];
float cal_spec_buf[FFT_LEN];
float sig_spec_buf[FFT_LEN * NUM_SIG_SPEC * 2];
float convtab[256];

pthread_mutex_t in_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t in_queue_cond = PTHREAD_COND_INITIALIZER;
int in_queue_len = 0;
pthread_mutex_t out_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t out_queue_cond = PTHREAD_COND_INITIALIZER;
int out_queue_len = 0;

FILE *init_cal_control(void)
{ 
  char fname[256];
  FILE *fp;
  int ret;

  /* Set GPIO direction */

  strncpy(fname, GPIOPATH, sizeof(fname) - 1);
  strncat(fname, GPIODIR, sizeof(fname) - sizeof(GPIODIR));
  fp = fopen(fname, "w");
  if(fp == NULL)
    perror(fname);
  else {
    ret = fputs("out\n",fp);
    if(ret == EOF)
      perror(fname);
    fclose(fp);
  }

  /* Open control file */

  strncpy(fname, GPIOPATH, sizeof(fname) - 1);
  strncat(fname, GPIOVAL, sizeof(fname) - sizeof(GPIODIR));
  fp = fopen(fname, "w");
  if(fp == NULL)
    perror(fname);

  return fp;
 
}

void set_cal_state(FILE *fp, int state)
{
  int ret;
  
  if (fp == NULL)
    return;

  if (state != 0)
    ret = fputs("1\n", fp);
  else
    ret = fputs("0\n", fp);
  fflush(fp);

  if (ret == EOF)
    perror("set_cal_state");
  
}

int set_frequency(rtlsdr_dev_t *dev, uint32_t freq)
{
  int r;
  uint32_t actual_freq;

  r = rtlsdr_set_center_freq(dev, freq);
  if (r < 0)
    fprintf(stderr, "WARNING: Failed to set center freq.\n");
  else {
    if (dongle_debug) {
      actual_freq = rtlsdr_get_center_freq(dev);
      fprintf(stderr, "  Tuned to %u Hz (wanted %u Hz).\n", actual_freq, freq);
    }
  }

  return r;
 
}

rtlsdr_dev_t *init_dongle(void)
{
    int r;
    int i = 0;
    int gain = 496;

    uint32_t dev_index = 0;
    uint32_t frequency = 1320100000;
    uint32_t samp_rate = SAMPLERATE;
    int device_count;
    char vendor[256], product[256], serial[256];

    rtlsdr_dev_t *dev = NULL;

    device_count = rtlsdr_get_device_count();
    if (!device_count) {
        fprintf(stderr, "No supported devices found.\n");
        return NULL;
    }

    if (dongle_debug) {
      fprintf(stderr, "Found %d device(s):\n", device_count);
      for (i = 0; i < device_count; i++) {
        rtlsdr_get_device_usb_strings(i, vendor, product, serial);
        fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
      }
    }

    if (dongle_debug)
      fprintf(stderr, "Using device %d: %s\n", dev_index, \
	      rtlsdr_get_device_name(dev_index));

    r = rtlsdr_open(&dev, dev_index);
    if (r < 0) {
        fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
        return NULL;
    }

    /* Set the sample rate */
    r = rtlsdr_set_sample_rate(dev, samp_rate);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to set sample rate.\n");

    /* Set the frequency */
    set_frequency(dev, frequency);

    if (0 == gain) {
        /* Enable automatic gain */
        r = rtlsdr_set_tuner_gain_mode(dev, 0);
        if (r < 0)
            fprintf(stderr, "WARNING: Failed to enable automatic gain.\n");
    } else {
        /* Enable manual gain */
        r = rtlsdr_set_tuner_gain_mode(dev, 1);
        if (r < 0)
            fprintf(stderr, "WARNING: Failed to enable manual gain.\n");

        /* Set the tuner gain */
        r = rtlsdr_set_tuner_gain(dev, gain);
        if (r < 0)
            fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
        else {
            if (dongle_debug)
	      fprintf(stderr, "Tuner gain set to %f dB.\n", gain / 10.0);
	}
    }

    r = rtlsdr_reset_buffer(dev);
    if (r < 0)
      fprintf(stderr, "WARNING: rtlsdr_reset_buffer() failed\n");

    return dev;
}

void calc_spectrum(uint8_t *signal, int sig_len, float *spec_buf, \
		   fftwf_plan fplan, fftwf_complex *fftin, \
		   fftwf_complex *fftout)
{
  float f;
  int nspec, n, k, idx;

  f = (float)sig_len / 2.0 / (float)FFT_LEN;
  if (f != floorf(f))
    fprintf(stderr, "WARNING: signal length not divisble by FFT length\n");

  nspec = (int)floorf(f);

  memset(spec_buf, 0, FFT_LEN * sizeof(float));

  for (n = 0; n < nspec; n++) {

    /* Copy signal into FFT buffer, converting format */
    for (k = 0; k < FFT_LEN; k++) {
      idx = 2 * (FFT_LEN * n + k);
      fftin[k][0] = convtab[signal[idx]];
      fftin[k][1] = convtab[signal[idx + 1]];
    }

    fftwf_execute(fplan);

    /* Accumulate power spectrum */

    for (k = 0; k < FFT_LEN; k++)
      spec_buf[k] += fftout[k][0] * fftout[k][0] + fftout[k][1] * fftout[k][1];

  }

}


fftwf_plan init_fft(fftwf_complex **inbuf, fftwf_complex **outbuf)
{
  fftwf_plan fplan;
  
  *inbuf = fftwf_alloc_complex(FFT_LEN);
  if(*inbuf == NULL) {
    fprintf(stderr, "Failed to allocate FFT input buffer\n");
    return NULL;
  }

  *outbuf = fftwf_alloc_complex(FFT_LEN);
  if(*outbuf == NULL) {
   fprintf(stderr, "Failed to allocate FFT output buffer\n");
   return NULL;
  }

  fplan = fftwf_plan_dft_1d(FFT_LEN, *inbuf, *outbuf, FFTW_FORWARD, \
			    FFTW_MEASURE);

  return fplan;
}

void init_convtab(void)
{
  int n;

  for (n = 0; n < 256; n++)
    convtab[n] = (float)n - 127.0;
}

double find_freq_error(float *calspec, double samplerate, double centfreq,
		       double calfreq)
{
  float max_pow = 0;
  int max_idx, n;
  double freqerr;

  max_idx = 0;

  /* find peak power */
  for (n = 0; n < FFT_LEN; n++)
    if (calspec[n] > max_pow) {
      max_pow = calspec[n];
      max_idx = n;
    }

  if (max_idx >= FFT_LEN / 2)
    max_idx = max_idx - FFT_LEN;

  freqerr = (double)max_idx * samplerate / (double)FFT_LEN;
  freqerr = (centfreq + freqerr) - calfreq;

  fprintf(stderr, "  Frequency error %.0f Hz\n", freqerr);

  return freqerr;
}

void *comp_thread(void *ptarg)
{
  fftwf_plan cfplan;
  fftwf_complex *cfftin, *cfftout;
  int r, in_queue_out_ptr = 0, out_queue_in_ptr = 0;

  fprintf(stderr, "  comp_thread: computation thread alive\n");

  cfplan = init_fft(&cfftin, &cfftout);
  if (cfplan == NULL) {
    fprintf(stderr, "  comp_thread: failed to initialise FFT\n");
    return NULL;
  }



  while (1) {

    /* Check input queue and wait if nothing to process */

    r = pthread_mutex_lock(&in_queue_mutex);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_lock: %s\n", strerror(r));
      return NULL;
    }

    while (in_queue_len == 0) {
      fprintf(stderr, "  comp_thread: waiting for data\n");
      r = pthread_cond_wait(&in_queue_cond, &in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "  comp_thread: pthread_cond_wait: %s\n", strerror(r));
	return NULL;
      }
    }

    r = pthread_mutex_unlock(&in_queue_mutex);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_unlock: %s\n", strerror(r));
      return NULL;
    }

    /* Process a block of signal */

    fprintf(stderr, "  comp_thread: calculating spectrum (%d)\n", 
	    in_queue_out_ptr);

    calc_spectrum(&data_buf[in_queue_out_ptr * SIG_SIZE], SIG_SIZE, \
		  &sig_spec_buf[out_queue_in_ptr * FFT_LEN], \
		  cfplan, cfftin, cfftout);

    in_queue_out_ptr = (in_queue_out_ptr + 1) % MAX_IN_QUEUE_LEN;
    out_queue_in_ptr = (out_queue_in_ptr + 1) % (NUM_SIG_SPEC * 2);

    /* Indicate that data has been removed from input queue and
       added to output queue
    */

    r = pthread_mutex_lock(&in_queue_mutex);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_lock: %s\n", strerror(r));
      return NULL;
    }

    in_queue_len--;

    r = pthread_mutex_unlock(&in_queue_mutex);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_unlock: %s\n", strerror(r));
      return NULL;
    }

    r = pthread_cond_signal(&in_queue_cond);
    if (r != 0) {
      fprintf(stderr, "pthread_cond_signal: %s\n", strerror(r));
      return NULL;
    }

    r = pthread_mutex_lock(&out_queue_mutex);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_lock: %s\n", strerror(r));
      return NULL;
    }

    out_queue_len++;

    r = pthread_mutex_unlock(&out_queue_mutex);
    if (r != 0) {
      fprintf(stderr, "  comp_thread: pthread_mutex_unlock: %s\n", strerror(r));
      return NULL;
    }

    r = pthread_cond_signal(&out_queue_cond);
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


int main(void)
{
  FILE *calfp;
  rtlsdr_dev_t *dev;
  int n, r, n_read, write_data = 0;
  double freq_err;
  uint32_t line_rx_freq;
  pthread_t cthread;
  int in_queue_in_ptr = 0, out_queue_out_ptr = 0;
  fftwf_complex *fftin;
  fftwf_complex *fftout;
  fftwf_plan fplan;

  write_data = !isatty(STDOUT_FILENO);
  if (!write_data)
    fprintf(stderr, "stdout is a terminal, not writing data\n");

  init_convtab();

  if ((fplan = init_fft(&fftin, &fftout)) == NULL)
    return 1;

  if ((dev = init_dongle()) == NULL)
    return 1;

  if ((calfp = init_cal_control()) == NULL)
    return 1;

  r = pthread_create(&cthread, NULL, comp_thread, NULL);
  if (r != 0) {
    fprintf(stderr, "pthread_create(): %s", strerror(r));
    return 1;
  }

  /* set RT scheduling for this thread */

  struct sched_param spar;
  spar.sched_priority = 99;
  r = pthread_setschedparam(pthread_self(), SCHED_FIFO, &spar);
  if (r != 0) {
    fprintf(stderr, "WARNING: could not set RT scheduling: %s\n", \
	    strerror(r));
  }

  r = mlockall(MCL_CURRENT | MCL_FUTURE);
  if (r != 0) {
    perror("Could not lock memory");
  }


  while (1) {

    set_frequency(dev, CALRXFREQ);

    memset(cal_data_buf, 0, sizeof(cal_data_buf)); 

    fprintf(stderr, "  Calibrator on\n");
    set_cal_state(calfp, 1);

    rtlsdr_reset_buffer(dev); /* flush any old signal away */

    r = rtlsdr_read_sync(dev, cal_data_buf, READ_SIZE, &n_read);
    if (r < 0)
      fprintf(stderr, "WARNING: rtlsdr_read_sync() failed\n");
    if (n_read != READ_SIZE)
      fprintf(stderr, "WARNING: received wrong number of samples (%d)\n", \
	      n_read);

    fprintf(stderr, "  Calibrator off\n");
    set_cal_state(calfp, 0);

    fprintf(stderr, "  Calculating spectrum... ");
    calc_spectrum(cal_data_buf, READ_SIZE, cal_spec_buf, fplan, fftin, fftout);
    fprintf(stderr, "Done.\n");

    if (write_data) {
      if (fwrite(cal_spec_buf, sizeof(cal_spec_buf), 1, stdout) != 1)
	fprintf(stderr, "Error writing data to stdout\n");
    }

    freq_err = find_freq_error(cal_spec_buf, SAMPLERATE, CALRXFREQ, CALFREQ);

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

      set_frequency(dev, line_rx_freq);

      fprintf(stderr, "  main_thread: recording signal %d, %d\n", scount,
	      in_queue_in_ptr);

      memset(&data_buf[in_queue_in_ptr * SIG_SIZE], 0, SIG_SIZE);

      rtlsdr_reset_buffer(dev); /* flush any cal signal away */

      /* Check for space in queue, wait if full */

      r = pthread_mutex_lock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(r));
	return 1;
      }

      while (in_queue_len == MAX_IN_QUEUE_LEN) {
	fprintf(stderr, "main_thread: waiting for space in queue\n");
	r = pthread_cond_wait(&in_queue_cond, &in_queue_mutex);
	if (r != 0) {
	  fprintf(stderr, "  main_thread: pthread_cond_wait(in_queue): %s\n",
		  strerror(r));
	  return 1;
	}
      }

      r = pthread_mutex_unlock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(r));
	return 1;
      }

      for(n = 0; n < NUM_BLOCKS; n++) {

	r = rtlsdr_read_sync(dev, &data_buf[in_queue_in_ptr * SIG_SIZE
					    + n * READ_SIZE],
			     READ_SIZE, &n_read);
	if (r < 0)
	  fprintf(stderr, "WARNING: rtlsdr_read_sync() failed\n");
	if (n_read != READ_SIZE)
	  fprintf(stderr, "WARNING: received wrong number of samples (%d)\n", \
		  n_read);    

      }

      r = pthread_mutex_lock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(r));
	return 1;
      }

      in_queue_len++;
      in_queue_in_ptr = (in_queue_in_ptr + 1) % MAX_IN_QUEUE_LEN;

      r = pthread_mutex_unlock(&in_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(r));
	return 1;
      }

      
      r = pthread_cond_signal(&in_queue_cond);
      if (r != 0) {
	fprintf(stderr, "pthread_cond_signal: %s\n", strerror(r));
	return 1;
      }
      

    }


    /* Read signal spectra from queue and store them */

    for (int scount = 0; scount < 2 * NUM_SIG_SPEC; scount++) {

      r = pthread_mutex_lock(&out_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(r));
	return 1;
      }

      while (out_queue_len == 0) {
	fprintf(stderr, "  main_thread: waiting for data\n");
	r = pthread_cond_wait(&out_queue_cond, &out_queue_mutex);
	if (r != 0) {
	  fprintf(stderr, "pthread_cond_wait: %s\n", strerror(r));
	  return 1;
	}
      }

      out_queue_len--;

      r = pthread_mutex_unlock(&out_queue_mutex);
      if (r != 0) {
	fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(r));
	return 1;
      }

      if (write_data) {

	if (fwrite(sig_spec_buf + FFT_LEN * out_queue_out_ptr,
		   FFT_LEN * sizeof(float), 1, stdout) != 1)
	  fprintf(stderr, "WARNING: could not write out data\n");

      }

      out_queue_out_ptr = (out_queue_out_ptr + 1) % (2 * NUM_SIG_SPEC);


    }

  }

  
  fclose(calfp);

  rtlsdr_close(dev);

  return 0;
}
