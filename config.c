#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "config.h"
#include "common.h"

#define CONF_FILE "ozonespec.conf"
#define BUF_LEN 128

char dongle_sns[MAX_NUM_CHANNELS][MAX_SN_LEN];
int num_channels = 0;
int vsrt_num = 0;
char data_dir[_POSIX_PATH_MAX] = ".";

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
