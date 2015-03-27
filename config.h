#ifndef _CONFIG_H
#define _CONFIG_H

#include <limits.h>
#include "common.h"

extern char dongle_sns[MAX_NUM_CHANNELS][MAX_SN_LEN];
extern int num_channels;
extern int vsrt_num;
extern char data_dir[_POSIX_PATH_MAX];

int read_config(char *conf_file);

#endif /* _CONFIG_H */
