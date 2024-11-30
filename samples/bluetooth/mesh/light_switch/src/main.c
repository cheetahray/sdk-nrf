/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic mesh light switch sample
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"
#include "uart1.h"
	
static void bt_ready(int err)
{
	if (err) {
		//sprintf(rayray, "Bluetooth init failed (err %d)\n", err);
		return;
	}

	//printf("Bluetooth initialized\n");

	dk_leds_init();
	dk_buttons_init(NULL);

	err = bt_mesh_init(bt_mesh_dk_prov_init(), model_handler_init());
	if (err) {
		//printf(rayray, "Initializing mesh failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_BT_MESH_LOW_POWER)) {
		bt_mesh_lpn_set(true);
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* This will be a no-op if settings_load() loaded provisioning info */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	//sprintf(rayray, "Mesh initialized\n");
}

int main(void)
{
	int err;
	//sprintf(rayray ,"Initializing...\n");

	err = bt_enable(bt_ready);
	if (err) {
		err = 9;//sprintf(rayray, "Bluetooth init failed (err %d)\n", err);
	}

	sensormain();
	uint8_t* raytry = getSensorValue(Noise);
	raytry[0] = 6;
	return 0;
}
