/*
 * gaincheck
 * 
 * Check appropriate dongle gain setting by reporting
 * RMS amplitude
 */

#include "common.h"
#include "rtl-sdr.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>


#define RX_FREQUENCY 1320000000
#define READ_SIZE 65536

int quit = 0;

void sigint_handler(int arg)
{
    quit = 1;
}


void print_usage(void)
{
    fprintf(stderr, "Usage: gaincheck [options] <dongle serial number>\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -l         list available gains\n");
    fprintf(stderr, "    -g <gain>  set gain to <gain> dB\n");
}

void list_gains(rtlsdr_dev_t *dev)
{
    /* Enumerate tuner gains */

    int *gain_tab;
    int num_gains;
    num_gains = rtlsdr_get_tuner_gains(dev, NULL);
    gain_tab = malloc(num_gains * sizeof(int));
    if (gain_tab == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return;
    }

    rtlsdr_get_tuner_gains(dev, gain_tab);

    fprintf(stderr, "Device supports the following gains (dB):\n");
    for (int i = 0; i < num_gains; i++) {
        fprintf(stderr, "%.1f ", (double)gain_tab[i] / 10.0);
    }
    fprintf(stderr, "\n");

    free(gain_tab);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    int do_list_gains = 0;
    int wanted_gain = INT_MIN;
    int opt;
    while ((opt = getopt(argc, argv, "lg:")) != -1) {
        double gain;
        
        switch (opt) {
            case 'l':
                /* list available gains */
                do_list_gains = 1;
                break;
            case 'g':
                /* set gain to given value */
                gain = strtod(optarg, NULL);
                gain = round(gain * 10);
                wanted_gain = (int)gain;
                break;
            default:
                /* unknown option */
                print_usage();
                return 1;
        }
    }

    if (optind >= argc) {
        print_usage();
        return 1;
    }

    const char *dev_name = argv[optind];

    int dev_index = rtlsdr_get_index_by_serial(dev_name);

    if (dev_index < 0) {
        fprintf(stderr, "No dongle with serial number %s found.\n", dev_name);
        return 1;
    }
   
    fprintf(stderr, "Dongle %s is device #%d\n", dev_name, dev_index);
 
    rtlsdr_dev_t *dev;

    int r = rtlsdr_open(&dev, dev_index);

    if (r < 0) {

        fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);

        return 1;
    }

    if (do_list_gains) {
        list_gains(dev);
        rtlsdr_close(dev);
        return 0;
    }

    /* Set the sample rate */
    r = rtlsdr_set_sample_rate(dev, SAMPLERATE);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to setsample sample rate.\n");

    /* Set the frequency */
    r = rtlsdr_set_center_freq(dev, RX_FREQUENCY);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to set frequency.\n");

    /* Enable manual gain */
    r = rtlsdr_set_tuner_gain_mode(dev, 1);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to enable manual gain.\n");

    /* Set the tuner gain */
    if (wanted_gain == INT_MIN) {
        fprintf(stderr, "Not setting gain\n");
    } else {
        r = rtlsdr_set_tuner_gain(dev, wanted_gain);
        if (r < 0)
            fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
        else {
            int actual_gain = rtlsdr_get_tuner_gain(dev);
            if (actual_gain != wanted_gain) {
                fprintf(stderr, "Gain %.1f dB requested but actual gain is %.1f dB\n",
                        (double)wanted_gain / 10, (double)actual_gain / 10);
            }
        }
    }

    r = rtlsdr_reset_buffer(dev);
    if (r < 0)
      fprintf(stderr, "WARNING: rtlsdr_reset_buffer() failed\n");

    struct sigaction sa  = {.sa_handler = sigint_handler};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    unsigned char data[READ_SIZE];
    int n_samples;

    fprintf(stderr, "Reporting signal power. Press Ctrl+C to quit.\n");
    while (!quit) {

        r = rtlsdr_read_sync(dev, data, READ_SIZE, &n_samples);
        if (r < 0)
            fprintf(stderr, "WARNING: error when reading samples\n");

        if (n_samples != READ_SIZE)
            fprintf(stderr, "WARNING: did not read all data requested\n");
    
        /* Samples are interleaved real, imag and offset by 127 */

        float rms = 0;
        for (int i = 0; i < n_samples; i++) {
            float x = (float)data[i] - 127;
            rms += x * x;
        }
        rms = sqrtf(rms / (float)(n_samples / 2));

        fprintf(stderr, "RMS amplitude %5.1f (%6.1f dBFS)\n", rms, 20.0 * log10f(rms / 127));

        struct timespec ts = {.tv_sec = 1, .tv_nsec = 0};
        nanosleep(&ts, NULL);

    }

    fprintf(stderr, "Closing device.\n");

    rtlsdr_close(dev);

    return 0;
}
