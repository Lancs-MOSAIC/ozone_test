#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "config.h"
#include "common.h"

#define CONF_FILE "ozonespec.conf"
#define BUF_LEN 128
#define WATCHDOG_TIMEOUT 180
#define LINEFREQ 1322454500 /* actual line frequency */
//#define LINEFREQ 1322754500 /* line + 300 kHz for testing */
//#define LINEFREQ CALFREQ

char dongle_sns[MAX_NUM_CHANNELS][MAX_SN_LEN];
int num_channels = 0;
int vsrt_num = 0;
char data_dir[_POSIX_PATH_MAX] = ".";
char station_name[MAX_STATION_NAME] = "Test";
int watchdog_timeout = WATCHDOG_TIMEOUT;
int keep_cal_on = 0;
double line_freq = LINEFREQ;

void parse_config(char *key, char *val)
{

  if (strcmp(key, "DONGLE") == 0) {
    if (num_channels < MAX_NUM_CHANNELS) {
      strncpy(&dongle_sns[num_channels][0], val, MAX_SN_LEN);
      num_channels++;
    }
    else {
      fprintf(stderr, "Too many channels defined!\n");
    }

  }
  else if (strcmp(key, "VSRTNUM") == 0) {
    vsrt_num = atoi(val);
  }
  else if (strcmp(key, "DATADIR") == 0) {
    strncpy(data_dir, val, _POSIX_PATH_MAX);
  }
  else if (strcmp(key, "STATNAME") == 0) {
    if (strlen(val) > 12)
      fprintf(stderr, "Warning: station name > 12 characters\n");
    strncpy(station_name, val, MAX_STATION_NAME);
  }
  else if (strcmp(key, "WATCHDOGTIME") == 0) {
    watchdog_timeout = atoi(val);
    if (watchdog_timeout < WATCHDOG_TIMEOUT) {
      fprintf(stderr, "Watchdog time-out (%d) too short, setting to minimum (%d)\n",
	      watchdog_timeout, WATCHDOG_TIMEOUT);
      watchdog_timeout = WATCHDOG_TIMEOUT;
    }
  }
  else if (strcmp(key, "VCALSTAYON") == 0) {
    keep_cal_on = atoi(val);
    if ((keep_cal_on != 0) && (keep_cal_on != 1)) {
      fprintf(stderr, "VCALSTAYON must be 0 or 1. Setting to 0.\n");
      keep_cal_on = 0;
    }
  }
  else if (strcmp(key, "FLINE") == 0) {
    line_freq = atof(val) * 1.0E6;
    if ((line_freq < 0) || (line_freq > 2.5E9)) {
      fprintf(stderr, "Line frequency out of range. Setting to default.\n");
      line_freq = LINEFREQ;
    }
  }
}

int read_config(char *conf_file)
{
  FILE *fp;
  char buf[BUF_LEN], key[BUF_LEN], val[BUF_LEN];
  char *file_name;

  file_name = conf_file != NULL ? conf_file : CONF_FILE;

  fp = fopen(file_name, "r");
  if (fp == NULL) {
    fprintf(stderr, "Cannot open %s\n", file_name);
    return 1;
  }

  fprintf(stderr, "Reading configuration from %s\n", file_name);

  while (fgets(buf, BUF_LEN, fp) != NULL) {

     if ((buf[0] != '*') && (buf[0] != '#') && (buf[0] != '\n')) {
       if (sscanf(buf, "%s %s", key, val) >= 1)
	 parse_config(key, val);
       else
	 fprintf(stderr, "Format error in %s\n", file_name);
     }

  }

  for (int k = 0; k < num_channels; k++)
    fprintf(stderr, "Channel %d: %s\n", k, &dongle_sns[k][0]);

  fclose(fp);
  return 0;
}
