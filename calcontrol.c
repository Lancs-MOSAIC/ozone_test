/*
 * Calibrator control functions
 */

#include "calcontrol.h"
#include <string.h>

#define GPIOPATH "/sys/class/gpio/gpio60"
#define GPIODIR "/direction"
#define GPIOVAL "/value"

FILE *init_cal_control(void)
{ 
  char fname[256];
  FILE *fp;
  int ret;

  /* Set GPIO direction */

  strncpy(fname, GPIOPATH, sizeof(fname) - 1);
  strncat(fname, GPIODIR, sizeof(fname) - sizeof(GPIODIR));
  fp = fopen(fname, "w");
  if(fp == NULL)
    perror(fname);
  else {
    ret = fputs("out\n",fp);
    if(ret == EOF)
      perror(fname);
    fclose(fp);
  }

  /* Open control file */

  strncpy(fname, GPIOPATH, sizeof(fname) - 1);
  strncat(fname, GPIOVAL, sizeof(fname) - sizeof(GPIODIR));
  fp = fopen(fname, "w");
  if(fp == NULL)
    perror(fname);

  return fp;
 
}

void set_cal_state(FILE *fp, int state)
{
  int ret;
  
  if (fp == NULL)
    return;

  if (state != 0)
    ret = fputs("1\n", fp);
  else
    ret = fputs("0\n", fp);
  fflush(fp);

  if (ret == EOF)
    perror("set_cal_state");
  
}

