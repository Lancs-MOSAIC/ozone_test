/*
 * RTL2832U dongle support functions
 */

#include "rtldongle.h"
#include "common.h"
#include <stdio.h>

int dongle_debug = 1;

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

rtlsdr_dev_t *init_dongle(char *sernum)
{
    int r;
    int gain = 496;

    uint32_t dev_index = 0;
    uint32_t frequency = 1320100000;
    uint32_t samp_rate = SAMPLERATE;

    rtlsdr_dev_t *dev = NULL;

    dev_index = rtlsdr_get_index_by_serial(sernum);

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

