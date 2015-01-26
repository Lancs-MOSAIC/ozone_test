/*
 * Signal processing functions
 */

#ifndef _SIGNALPROC_H
#define _SIGNALPROC_H

#include <fftw3.h>
#include <stdint.h>

void calc_spectrum(uint8_t *signal, int sig_len, float *spec_buf,
		   int *num_spec, float *win,
		   fftwf_plan fplan, fftwf_complex *fftin,
		   fftwf_complex *fftout);

fftwf_plan init_fft(fftwf_complex **inbuf, fftwf_complex **outbuf);

void init_convtab(void);

void init_window(float *win, int len);

double find_freq_error(float *calspec, double samplerate,
                       double centfreq, double calfreq);

#endif /* _SIGNALPROC_H */

