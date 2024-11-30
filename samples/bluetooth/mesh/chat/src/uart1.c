/*
 * @brief Native TTY UART sample
 *
 * Copyright (c) 2023 Marko Sagadin
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/crc.h>
#include "uart1.h"
#include "model_handler.h"
#include <stdio.h>
#include <string.h>

#define MSG_SIZE 16
#define MOD_SIZE 8
#define TYPE_SIZE 4
#define PRINT_SIZE 256
// CRC-16-CCITT 多項式 0x1021
#define CRC16_POLY 0xA001
#define CRC16_INIT 0xFFFF
#ifdef sensor485
enum SenseType {
	ID,
    Temperature,     // 默認值 1
    Humidity,     // 默認值 2
    Noise    // 默認值 3
};

// 定義對應的字串陣列
const char *SenseStr[] = {
    "ID",
    "Temperature",
    "Humidity",
    "Noise"
};

char recv_buf[MSG_SIZE];
//Todo
//1. 06從AT來
//2. 計算 CRC16 
uint8_t sensorSendArr[TYPE_SIZE][MOD_SIZE] = {
	{'A',  'T',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x06, 0x03, 0x00, 0x00, 0x00, 0x01, 0x85, 0xBD},
	{0x06, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD4, 0x7D},
	{0x06, 0x03, 0x00, 0x07, 0x00, 0x01, 0x34, 0x7C}
};
	
const struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart1));

struct uart_config uart_cfg = {
	.baudrate = 9600,
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	.data_bits = UART_CFG_DATA_BITS_8,
};

void send_str(const struct device *uart, uint8_t *str, int msg_len)
{
	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart, str[i]);
	}
	
	//printk("Device %s sent: \"%s\"\n", uart->name, "Ray");
}

void recv_str(const struct device *uart, char *str)
{
	char *head = str;
	char c;

	while (!uart_poll_in(uart, &c)) {
		*head++ = c;
	}
	//*head = '\0';
	
	//printk("Device %s received: \"%s\"\n", uart->name, "Ray");
}

void getValue(uint8_t* recv_buf, uint8_t* byteArray, int msg_len)
{
	for (int ii = 0; ii < MSG_SIZE; ii++) {
	   recv_buf[ii] = 0;
	}

	//snprintf(send_buf, MSG_SIZE, "Hello from device %s, num %d", uart2->name, i);
	send_str(uart2, byteArray, msg_len);
	/* Wait some time for the messages to arrive to the second uart. */
	k_sleep(K_MSEC(100));
	recv_str(uart2, recv_buf);

	k_sleep(K_MSEC(1000));
}

uint8_t* getSensorValue(enum SenseType type)
{
	int msg_len = (type ==  ID ? 2 : 8);
    getValue(recv_buf, sensorSendArr[type], msg_len);	
    return recv_buf;
}

int sensormain(void)
{
	int rc;
	//char send_buf[MSG_SIZE];
	
	//uart_cfg.baudrate = 9600;
	//printk("\nChanging baudrate of both uart devices to %d!\n\n", uart_cfg.baudrate);

	rc = uart_configure(uart2, &uart_cfg);
	if (rc) {
		printk("Could not configure device %s", uart2->name);
	}

	return 0;
}

float calregisters(uint8_t DF1, uint8_t DF2)
{
	return (((float)(DF1<<8)+(float)(DF2))/10.0);
}

void calculateSensorValue(char* str, enum SenseType type)
{
	//2. 溫濕度噪音提出去自成函數
	uint8_t* ret = getSensorValue(type);
			
	float adc_read = calregisters((*(ret+3)),(*(ret+4)));

	char *tmpSign = (adc_read < 0) ? "-" : "";
	float tmpVal = (adc_read < 0) ? -adc_read : adc_read;

	int tmpInt1 = tmpVal;                  // Get the integer (678).
	float tmpFrac = tmpVal - tmpInt1;      // Get fraction (0.0123).
	int tmpInt2 = trunc(tmpFrac * 10000);  // Turn into integer (123).

	// Print as parts, note that you need 0-padding for fractional bit.
	sprintf (str, "%s = %s%d.%04d\n", SenseStr[type], tmpSign, tmpInt1, tmpInt2);
			
}

void uart_out(void)
{
	int err;
	char str[PRINT_SIZE];
	
	sensormain();

	while (1) {
		k_sleep(K_MSEC(1000));

		uint8_t* ret = getSensorValue(ID);

		if(strlen(ret) > 0)
		{
			memset(str, 0, PRINT_SIZE);
			// Print as parts, note that you need 0-padding for fractional bit.
			sprintf (str, "%s\n", ret);
			
			//1. 06從AT來，如果沒反應就不執行 sensor
			int sensorId = atoi(ret+3);
			for(int ii = 1; ii < TYPE_SIZE; ii++) {
				sensorSendArr[ii][0] = sensorId;
				uint16_t crc = crc16_reflect(CRC16_POLY, CRC16_INIT, sensorSendArr[ii], MOD_SIZE - 2);
				sensorSendArr[ii][7] = (crc >> 8) & 0xFF;
   		 		sensorSendArr[ii][6] = crc & 0xFF;
				calculateSensorValue((str+strlen(str)), ii);
			}
			
			// 怎麼從 main process 去發動傳送 mesh 還要研究一下，目前看起來只能從 shell 來
			// 應該是從這個地方下 chat
			err = sensor_message(str);
			if (err) {
				printk("Failed to send message: %d", err);
			}
		}

	}
}
#endif
