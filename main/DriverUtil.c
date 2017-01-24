/*
 * DriverUtil.c
 *
 *  Created on: 2017.1.23
 *      Author: Jack
 */
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

const char *DU_TAG = "WM8978";

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */


/*
 * DevAddr 器件的7位地址
 */
uint8_t  IIC_Write_One_Byte(uint8_t DevAddr,uint8_t RegAddr,uint8_t Data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    int ret;

    ret = i2c_master_start(cmd);

    if (ret != ESP_OK) {
    	ESP_LOGE(DU_TAG, "i2c_master_start failed! ret:%d", ret);

        return ret;
    }

    ret = i2c_master_write_byte(cmd, DevAddr & 0xFE, ACK_CHECK_EN);

    if (ret != ESP_OK) {
    	ESP_LOGE(DU_TAG, "i2c_master_write_byte failed! ret:%d" , ret);

        return ret;
    }

    //i2c_master_write_byte(cmd, DevAddr << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, RegAddr, ACK_CHECK_EN);


    if (ret != ESP_OK) {
    	ESP_LOGE(DU_TAG, "i2c_master_write_byte failed 2! ret:%d" , ret);

        return ret;
    }

    i2c_master_write_byte(cmd, Data, ACK_CHECK_EN);

    if (ret != ESP_OK) {
    	ESP_LOGE(DU_TAG, "i2c_master_write_byte failed 3! ret:%d" , ret);

        return ret;
    }

    ret =  i2c_master_stop(cmd);

    if (ret != ESP_OK) {
    	ESP_LOGE(DU_TAG, "i2c_master_stop failed! ret:%d" , ret);

        return ret;
    }

    ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);

    if (ret != ESP_OK) {
    	ESP_LOGE(DU_TAG, "i2c_master_cmd_begin failed! ret:%d" , ret);

        return ret;
    }

    i2c_cmd_link_delete(cmd);

//    if (ret != ESP_OK) {
//    	ESP_LOGE(DU_TAG, "I2C write failed! DevAddr:%x regAddr:%d, ret:%d!", DevAddr & 0xFE, RegAddr, ret);
//
//        return ret;
//    }

    return ESP_OK;
}
