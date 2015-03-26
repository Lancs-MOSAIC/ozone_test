#ifndef _CONFIG_H
#define _CONFIG_H

#include "common.h"

extern char dongle_sns[MAX_NUM_CHANNELS][MAX_SN_LEN];
extern int num_channels;

int read_config(void);

#endif /* _CONFIG_H */
