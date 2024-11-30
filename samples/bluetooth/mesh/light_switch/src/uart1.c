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
#include "uart1.h"
#include <stdio.h>
#include <string.h>

#define MSG_SIZE 16
#define MOD_SIZE 8

char recv_buf[MSG_SIZE];
//Todo
//1. 06從AT來
//2. 計算 CRC16 
uint8_t sensorSendArr[3][MOD_SIZE] = {
	{0x06, 0x03, 0x00, 0x00, 0x00, 0x01, 0x85, 0xBD},
	{0x06, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD4, 0x7D},
	{0x06, 0x03, 0x00, 0x07, 0x00, 0x01, 0x34, 0x7C}
};
	
const struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart0));

struct uart_config uart_cfg = {
	.baudrate = 9600,
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	.data_bits = UART_CFG_DATA_BITS_8,
};

void send_str(const struct device *uart, uint8_t *str)
{
	int msg_len = MOD_SIZE;//strlen(str);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart, str[i]);
	}
	
	//sprintf(rayray, "Device %s sent: \"%s\"\n", uart->name, "Ray");
}

void recv_str(const struct device *uart, uint8_t *str)
{
	char *head = str;
	char c;

	while (!uart_poll_in(uart, &c)) {
		*head++ = c;
	}
	*head = '\0';
	
	//sprintf(rayray, "Device %s received: \"%s\"\n", uart->name, "Ray");
}

void getValue(uint8_t* recv_buf, uint8_t* byteArray)
{
	for (int ii = 0; ii < MSG_SIZE; ii++) {
	   recv_buf[ii] = 0;
	}

	//snprintf(send_buf, MSG_SIZE, "Hello from device %s, num %d", uart2->name, i);
	send_str(uart2, byteArray);
	/* Wait some time for the messages to arrive to the second uart. */
	k_sleep(K_MSEC(100));
	recv_str(uart2, recv_buf);

	k_sleep(K_MSEC(1000));
}

uint8_t* getSensorValue(enum SenseType type)
{
    getValue(recv_buf, sensorSendArr[type]);	
    return recv_buf;
}

int sensormain(void)
{
	int rc;
	//char send_buf[MSG_SIZE];
	
	//uart_cfg.baudrate = 115200;
	//sprintf(rayray, "\nChanging baudrate of both uart devices to %d!\n\n", uart_cfg.baudrate);

	rc = uart_configure(uart2, &uart_cfg);
	//if (rc) {
	//	sprintf(rayray, "Could not configure device %s", uart2->name);
	//}

	return 0;
}
