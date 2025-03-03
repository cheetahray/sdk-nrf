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
#define brobao
#define modbus
#define staticId
#ifdef __cplusplus
extern "C" {
#endif

const struct bt_mesh_comp *model_handler_init(void);

int sensor_message(char *content);
int sensor_message_len(char *content, uint8_t len);
uint16_t get_local_node_id();
void split_uint16_to_uint8(uint16_t input, uint8_t *high, uint8_t *low);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_HANDLER_H__ */
