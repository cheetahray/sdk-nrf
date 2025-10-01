/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

#include "pawr_common.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

// 外部函數聲明
extern int pawr_central_init(void);

int main(void)
{
	printk("PAwR Broadcaster/Observer Simulation Starting...\n");
    
    // 根據編譯配置決定角色

	// Central 角色 (broadcaster)
    int err = pawr_central_init();
    if (err) {
        printk("Failed to initialize PAwR Central (err %d)\n", err);
        return err;
    }
    printk("Running as PAwR Central (Broadcaster)\n");

	// 主循環
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}