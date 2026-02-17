
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <hal/nrf_radio.h>
#include <mpsl_tx_power.h>

extern int32_t txpwr_set(mpsl_tx_power_t pwr);
