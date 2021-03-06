/*
 * timerHandler.h
 *
 *  Created on: 2018. 7. 24.
 *      Author: james
 */

#ifndef PLATFORMHANDLER_TIMERHANDLER_H_
#define PLATFORMHANDLER_TIMERHANDLER_H_

#include "stm32f10x_tim.h"

void Timer_Configuration(void);
void Timer2_ISR(void);


////////////////////////////////////////
uint32_t getNow(void);
uint32_t getDevtime(void);
void setDevtime(uint32_t timeval_sec);
uint32_t millis(void);
////////////////////////////////////////

uint32_t getDeviceUptime_day(void);
uint32_t getDeviceUptime_hour(void);
uint8_t  getDeviceUptime_min(void);
uint8_t  getDeviceUptime_sec(void);
uint16_t getDeviceUptime_msec(void);

void set_phylink_time_check(uint8_t enable);
uint32_t get_phylink_downtime(void);

void set_runled_msec(uint16_t setmsec);
uint16_t get_runled_msec(void);

#endif /* PLATFORMHANDLER_TIMERHANDLER_H_ */
