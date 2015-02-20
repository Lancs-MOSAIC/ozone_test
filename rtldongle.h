/*
 * RTL2832U dongle support functions
 */

#ifndef _RTLDONGLE_H
#define _RTLDONGLE_H

#include "rtl-sdr.h"
#include <stdint.h>

int set_frequency(rtlsdr_dev_t *dev, uint32_t freq);
rtlsdr_dev_t *init_dongle(char *sernum);

#endif /* _RTLDONGLE_H */

