#ifndef _CONFIG_H
#define _CONFIG_H

#include <limits.h>
#include "common.h"

#define MAX_STATION_NAME 16

extern char dongle_sns[MAX_NUM_CHANNELS][MAX_SN_LEN];
extern int num_channels;
extern int vsrt_num;
extern char data_dir[_POSIX_PATH_MAX];
extern char station_name[MAX_STATION_NAME];
extern int watchdog_timeout;
extern int keep_cal_on;
extern double line_freq;
extern float dongle_gains[MAX_NUM_CHANNELS];

int read_config(char *conf_file);

#endif /* _CONFIG_H */
