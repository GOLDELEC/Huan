/*
 * DriverUtil.h
 *
 *  Created on: 2017.1.23
 *      Author: Jack
 */


#ifndef __DRIVER_UTIL_H
#define __DRIVER_UTIL_H

#include "stdint.h"

uint8_t IIC_Write_One_Byte(uint8_t DevAddr,uint8_t RegAddr,uint8_t Data);

#endif
