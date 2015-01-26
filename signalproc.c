/*
 * Signal processing functions
 */

#include "signalproc.h"
#include "common.h"
#include <math.h>
#include <string.h>

float convtab[256];

void calc_spectrum(uint8_t *signal, int sig_len, float *spec_buf,
		   int *num_spec, float *win,
		   fftwf_plan fplan, fftwf_complex *fftin,
		   fftwf_complex *fftout)
{
  float f;
  int nspec, n, k, idx;

  f = (float)sig_len / 2.0 / (float)FFT_LEN;
  if (f != floorf(f))
    fprintf(stderr, "WARNING: signal length not divisible by FFT length\n");

  nspec = (int)floorf(f);

  memset(spec_buf, 0, FFT_LEN * sizeof(float));

  for (n = 0; n < nspec; n++) {

    /* Copy signal into FFT buffer, converting format */
    for (k = 0; k < FFT_LEN; k++) {
      idx = 2 * (FFT_LEN * n + k);
      if (win == NULL) {
	fftin[k][0] = convtab[signal[idx]];
	fftin[k][1] = convtab[signal[idx + 1]];
      } else {
	fftin[k][0] = win[k] * convtab[signal[idx]];
	fftin[k][1] = win[k] * convtab[signal[idx + 1]];
      }
    }

    fftwf_execute(fplan);

    /* Accumulate power spectrum */

    for (k = 0; k < FFT_LEN; k++)
      spec_buf[k] += fftout[k][0] * fftout[k][0] + fftout[k][1] * fftout[k][1];

  }

  if (num_spec != NULL)
    *num_spec = nspec;

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

  /* Convert 8-bit offset sample to floating point
   * with full-scale = 1
   */

  for (n = 0; n < 256; n++)
    convtab[n] = ((float)n - 127.0) / 127.0;
}

void init_window(float *win, int len)
{
  int n;
  float t;

  for (n = 0; n < len; n++) {

    t = 2 * M_PI * ((float)n - (float)(len-1) / 2.0) / (float)len;
    win[n] = 1.0 + 2.0*sqrt(5.0/9.0)*cos(t);
  }

}

double find_freq_error(float *calspec, double samplerate, double centfreq,
		       double calfreq)
{
  float max_pow = 0;
  int max_idx, max_idx_p, max_idx_m, n;
  double freqerr, f_interp;

  max_idx = 0;

  /* find peak power */
  for (n = 0; n < FFT_LEN; n++)
    if (calspec[n] > max_pow) {
      max_pow = calspec[n];
      max_idx = n;
    }

  /* quadratic interpolation of peak frequency */

  max_idx_p = (max_idx + 1) % FFT_LEN;
  max_idx_m = max_idx - 1;
  if (max_idx_m < 0)
    max_idx_m = max_idx_m + FFT_LEN;

  f_interp = 0.5 * (calspec[max_idx_m] - calspec[max_idx_p]) /
    (calspec[max_idx_m] - 2*calspec[max_idx] + calspec[max_idx_p]);

  /* convert from FFT bin to actual frequency */

  freqerr = (double)max_idx;

  if (freqerr >= FFT_LEN / 2)
    freqerr = freqerr - (double)FFT_LEN;

  freqerr += f_interp;
  freqerr = freqerr * samplerate / (double)FFT_LEN;
  freqerr = (centfreq + freqerr) - calfreq;

  fprintf(stderr, "  Frequency error %.0f Hz (f_interp = %.2f)\n",
	  freqerr, f_interp);

  return freqerr;
}

