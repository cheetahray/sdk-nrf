/*
 * Copyright (c) 2018 Peter Bigot Consulting, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include "light.h"
#include "model_handler.h"
#ifdef sensorIIIFarFarFarFar
#define PRINT_SIZE 256
#endif
int ambient_light(void)
{
#ifdef sensorIIIFarFarFarFar
	const struct device *const dev = DEVICE_DT_GET_ONE(rohm_bh1750);
	struct sensor_value val;
	uint32_t lum = 0U;

	char str[PRINT_SIZE];
    
	if (!device_is_ready(dev)) {
		printk("Device %s is not ready\n", dev->name);
		return 0;
	}

	k_sleep(K_MSEC(15000));
	while (1) {
		if (sensor_sample_fetch_chan(dev, SENSOR_CHAN_LIGHT) != 0) {
			printk("sensor: sample fetch fail.\n");
			return 0;
		}

		if (sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &val) != 0) {
			printk("sensor: channel get fail.\n");
			return 0;
		}

		lum = val.val1;
		sprintf(str, "sensor: lum reading: %d\n", lum);

		// 怎麼從 main process 去發動傳送 mesh 還要研究一下，目前看起來只能從 shell 來
		// 應該是從這個地方下 chat
		int err = sensor_message(str);
		if (err) {
			printk("Failed to send message: %d", err);
		}

		k_sleep(K_MSEC(4000));
	}
#endif
	return 0;
}
