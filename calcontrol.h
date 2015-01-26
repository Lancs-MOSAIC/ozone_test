/*
 * Calibrator control functions
 */

#ifndef _CALCONTROL_H
#define _CALCONTROL_H

#include <stdio.h>

FILE *init_cal_control(void);
void set_cal_state(FILE *fp, int state);


#endif /* _CALCONTROL_H */

