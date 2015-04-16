/*
 * Common configuration, etc.
 */

#ifndef _COMMON_H
#define _COMMON_H

#define M_PI 3.14159265358979323846 /* not defined in C99! */

#define SAMPLERATE 1800000
#define FFT_LEN 768

#define MAX_SN_LEN 16

#define MAX_NUM_CHANNELS 8

/* Realtime scheduling priorities */

#define RT_PRIO_MAIN 50 /* used by main thread, which has the watchdog */
#define RT_PRIO_REC (RT_PRIO_MAIN - 1) /* recorder threads */

#endif /* _COMMON_H */

