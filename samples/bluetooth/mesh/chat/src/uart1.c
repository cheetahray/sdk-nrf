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

#include <zephyr/logging/log.h>

#define MSG_SIZE 64
#define MOD_SIZE 8
#define TYPE_SIZE 5
#define PRINT_SIZE 256
// CRC-16-CCITT 多項式 0x1021
#define CRC16_POLY 0xA001
#define CRC16_INIT 0xFFFF

#define CONFIG_BT_NUS_UART_BUFFER_SIZE 40
#define CONFIG_BT_NUS_UART_RX_WAIT_TIME 50000

#define UART_BUF_SIZE CONFIG_BT_NUS_UART_BUFFER_SIZE
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_WAIT_FOR_RX CONFIG_BT_NUS_UART_RX_WAIT_TIME

#define LOG_MODULE_NAME farfarfarfar
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#ifdef brobao
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

const int modSize[] = {
    2,
    8,
    8,
    8,
	8
};

const int recSize[] = {
    6,
    7,
    7,
    7,
	37
};

char recv_buf[MSG_SIZE];
//Todo
//1. 06從AT來
//2. 計算 CRC16 
uint8_t sensorSendArr[TYPE_SIZE][MOD_SIZE] = {
	{'A',  'T',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x06, 0x03, 0x00, 0x00, 0x00, 0x01, 0x85, 0xBD},
	{0x06, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD4, 0x7D},
	{0x06, 0x03, 0x00, 0x07, 0x00, 0x01, 0x34, 0x7C},
	{0x06, 0x03, 0x00, 0x00, 0x00, 0x10, 0x34, 0x7C}
};
	
const struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart1));

// struct uart_config uart_cfg = {
// 	.baudrate = 9600,
// 	.parity = UART_CFG_PARITY_NONE,
// 	.stop_bits = UART_CFG_STOP_BITS_1,
// 	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
// 	.data_bits = UART_CFG_DATA_BITS_8,
// };

struct uart_data_t {
	void *fifo_reserved;
	uint8_t data[UART_BUF_SIZE];
	uint16_t len;
};

static struct k_work_delayable uart_work;

#if CONFIG_BT_NUS_UART_ASYNC_ADAPTER
UART_ASYNC_ADAPTER_INST_DEFINE(async_adapter);
#else
static const struct device *const async_adapter;
#endif

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

struct uart_data_t *tx;
struct uart_data_t *rx;
	
void send_str(const struct device *uart, uint8_t *str, int msg_len)
{

	if (tx) {
		memcpy(tx->data, str, msg_len);
		tx->len = msg_len;
	} else {
		k_free(rx);
		return -ENOMEM;
	}

	int err = uart_tx(uart2, tx->data, tx->len, SYS_FOREVER_MS);
	if (err) {
		k_free(rx);
		k_free(tx);
		LOG_ERR("Cannot display welcome message (err: %d)", err);
		
	}

	return err;

	// for (int i = 0; i < msg_len; i++) {
	// 	uart_poll_out(uart, str[i]);
	// }
	
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
	//snprintf(send_buf, MSG_SIZE, "Hello from device %s, num %d", uart2->name, i);
	send_str(uart2, byteArray, msg_len);
	/* Wait some time for the messages to arrive to the second uart. */
	k_sleep(K_MSEC(150));
	// recv_str(uart2, recv_buf);
}

uint8_t* getSensorValue(enum SenseType type)
{
	memset(recv_buf, 0, MSG_SIZE);
	getValue(recv_buf, sensorSendArr[type], modSize[type]);	
    return recv_buf;
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);

	static size_t aborted_len;
	struct uart_data_t *buf;
	static uint8_t *aborted_buf;
	static bool disable_req;

	switch (evt->type) {
	case UART_TX_DONE:
		LOG_DBG("UART_TX_DONE");
		if ((evt->data.tx.len == 0) ||
		    (!evt->data.tx.buf)) {
			return;
		}

		if (aborted_buf) {
			buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
					   data[0]);
			aborted_buf = NULL;
			aborted_len = 0;
		} else {
			buf = CONTAINER_OF(evt->data.tx.buf, struct uart_data_t,
					   data[0]);
		}

		k_free(buf);

		buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
		if (!buf) {
			return;
		}

		if (uart_tx(uart2, buf->data, buf->len, SYS_FOREVER_MS)) {
			LOG_WRN("Failed to send data over UART");
		}

		break;

	case UART_RX_RDY:
		LOG_DBG("UART_RX_RDY");
		buf = CONTAINER_OF(evt->data.rx.buf, struct uart_data_t, data[0]);
		buf->len += evt->data.rx.len;

		if (disable_req) {
			return;
		}
		else {
			memcpy(recv_buf, evt->data.rx.buf, recSize[TYPE_SIZE-1]);
			disable_req = true;
			uart_rx_disable(uart2);
		}

		if ((evt->data.rx.buf[buf->len - 1] == '\n') ||
		    (evt->data.rx.buf[buf->len - 1] == '\r')) {
			disable_req = true;
			uart_rx_disable(uart2);
		}

		break;

	case UART_RX_DISABLED:
		LOG_DBG("UART_RX_DISABLED");
		disable_req = false;

		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
			k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
			return;
		}

		uart_rx_enable(uart2, buf->data, sizeof(buf->data),
			       UART_WAIT_FOR_RX);

		break;

	case UART_RX_BUF_REQUEST:
		LOG_DBG("UART_RX_BUF_REQUEST");
		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
			uart_rx_buf_rsp(uart2, buf->data, sizeof(buf->data));
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
		}

		break;

	case UART_RX_BUF_RELEASED:
		LOG_DBG("UART_RX_BUF_RELEASED");
		buf = CONTAINER_OF(evt->data.rx_buf.buf, struct uart_data_t,
				   data[0]);

		if (buf->len > 0) {
			k_fifo_put(&fifo_uart_rx_data, buf);
		} else {
			k_free(buf);
		}

		break;

	case UART_TX_ABORTED:
		LOG_DBG("UART_TX_ABORTED");
		if (!aborted_buf) {
			aborted_buf = (uint8_t *)evt->data.tx.buf;
		}

		aborted_len += evt->data.tx.len;
		buf = CONTAINER_OF((void *)aborted_buf, struct uart_data_t,
				   data);

		uart_tx(uart2, &buf->data[aborted_len],
			buf->len - aborted_len, SYS_FOREVER_MS);

		break;

	default:
		break;
	}
}

static void uart_work_handler(struct k_work *item)
{
	struct uart_data_t *buf;

	buf = k_malloc(sizeof(*buf));
	if (buf) {
		buf->len = 0;
	} else {
		LOG_WRN("Not able to allocate UART receive buffer");
		k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
		return;
	}

	uart_rx_enable(uart2, buf->data, sizeof(buf->data), UART_WAIT_FOR_RX);
}

static bool uart_test_async_api(const struct device *dev)
{
	const struct uart_driver_api *api =
			(const struct uart_driver_api *)dev->api;

	return (api->callback_set != NULL);
}

int sensormain(void)
{
	int err;
	int pos;
	
	// int rc;
	//char send_buf[MSG_SIZE];
	
	//uart_cfg.baudrate = 9600;
	//printk("\nChanging baudrate of both uart devices to %d!\n\n", uart_cfg.baudrate);

	// rc = uart_configure(uart2, &uart_cfg);
	// if (rc) {
	// 	printk("Could not configure device %s", uart2->name);
	// }

	if (!device_is_ready(uart2)) {
		return -ENODEV;
	}

	rx = k_malloc(sizeof(*rx));
	if (rx) {
		rx->len = 0;
	} else {
		return -ENOMEM;
	}

	k_work_init_delayable(&uart_work, uart_work_handler);

	if (IS_ENABLED(CONFIG_BT_NUS_UART_ASYNC_ADAPTER) && !uart_test_async_api(uart2)) {
		/* Implement API adapter */
		uart_async_adapter_init(async_adapter, uart2);
		uart2 = async_adapter;
	}

	err = uart_callback_set(uart2, uart_cb, NULL);
	if (err) {
		k_free(rx);
		LOG_ERR("Cannot initialize UART callback");
		return err;
	}

	tx = k_malloc(sizeof(*tx));

	err = uart_rx_enable(uart2, rx->data, sizeof(rx->data), UART_WAIT_FOR_RX);
	if (err) {
		LOG_ERR("Cannot enable uart reception (err: %d)", err);
		/* Free the rx buffer only because the tx buffer will be handled in the callback */
		k_free(rx);
	}

	return err;

}

float calregisters(uint8_t DF1, uint8_t DF2)
{
	return (((float)(DF1<<8)+(float)(DF2))/10.0);
}

bool calculateSensorValue(char* str, enum SenseType type)
{
	//2. 溫濕度噪音提出去自成函數
	getSensorValue(type);
	bool bolret = (recv_buf[0] != 0);
	if(bolret)
	{
		float adc_read = calregisters((*(recv_buf+3)),(*(recv_buf+4)));

		char *tmpSign = (adc_read < 0) ? "-" : "";
		float tmpVal = (adc_read < 0) ? -adc_read : adc_read;

		int tmpInt1 = tmpVal;                  // Get the integer (678).
		float tmpFrac = tmpVal - tmpInt1;      // Get fraction (0.0123).
		int tmpInt2 = trunc(tmpFrac * 10000);  // Turn into integer (123).

		// Print as parts, note that you need 0-padding for fractional bit.
		sprintf (str, "%s = %s%d.%04d\n", SenseStr[type], tmpSign, tmpInt1, tmpInt2);
	}		
	return bolret;
			
}

bool getSensorRaw(uint8_t* raw, enum SenseType type, uint8_t len)
{
	//2. 溫濕度噪音提出去自成函數
	getSensorValue(type);
	bool bolret = (recv_buf[0] != 0);
	if(bolret)
	{
		memcpy(raw, recv_buf, len);
	}
	return bolret;		
}

void uart_out(void)
{
	int err;
	char str[PRINT_SIZE];
	bool getId = false;
	uint8_t* ret = NULL;
	sensormain();

	while (1) {
		memset(str, 0, PRINT_SIZE);
			
		if(false == getId)
		{
			k_sleep(K_MSEC(1500));
			ret = getSensorValue(ID);
			if(strlen(ret) > 0)
			{
				// Print as parts, note that you need 0-padding for fractional bit.
				sprintf (str, "%s\n", ret);
				//1. 06從AT來，如果沒反應就不執行 sensor
				int sensorId = atoi(ret+3);
				for(int ii = 1; ii < TYPE_SIZE; ii++) {
					sensorSendArr[ii][0] = sensorId;
					uint16_t crc = crc16_reflect(CRC16_POLY, CRC16_INIT, sensorSendArr[ii], MOD_SIZE - 2);
					sensorSendArr[ii][7] = (crc >> 8) & 0xFF;
					sensorSendArr[ii][6] = crc & 0xFF;
				}
			}
			
		}
		
        if(strlen(ret) > 0)
		{
			
#ifdef modbus
			getId = getSensorRaw(str, TYPE_SIZE-1, recSize[TYPE_SIZE-1]);
			err = sensor_message_len(str, recSize[TYPE_SIZE-1]);
			k_sleep(K_MSEC(1500));
#else
			for(int ii = 1; ii < TYPE_SIZE; ii++) {
				getId = calculateSensorValue((str+strlen(str)), ii);
			}
			err = sensor_message(str);
			k_sleep(K_MSEC(1000));
#endif
		}
        else
        {
            // Print as parts, note that you need 0-padding for fractional bit.
			sprintf (str, "%s\n", "No bro bao 485 sensor.");
			getId = false;
			// 怎麼從 main process 去發動傳送 mesh 還要研究一下，目前看起來只能從 shell 來
			// 應該是從這個地方下 chat
			err = sensor_message(str);
        }

        
		if (err) {
			printk("Failed to send message: %d", err);
		}

	}
}
#endif
