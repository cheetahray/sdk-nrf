/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Model handler
 */

#ifndef MODEL_HANDLER_H__
#define MODEL_HANDLER_H__

#include <zephyr/bluetooth/mesh.h>

#ifdef __cplusplus
extern "C" {
#endif
#define sensorFarFarFarFar
//#define sensorIIIFarFarFarFar
const struct bt_mesh_comp *model_handler_init(void);

int sensor_message(char *content);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_HANDLER_H__ */
