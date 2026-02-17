#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(LossTest_3rd_02s)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
# NORDIC SDK APP END
target_sources(app PRIVATE src/losstst_svc.c)
target_sources(app PRIVATE src/ext_scr_svc.c)
target_sources(app PRIVATE src/int_spec_uart_svc.c)
target_sources(app PRIVATE src/int_spec_uart_svc.h)
target_sources(app PRIVATE src/nrf52txpwr.c)
target_sources(app PRIVATE src/nrf52txpwr.h)


zephyr_linker_sources(SECTIONS sections-rom.ld)
#zephyr_iterable_section(NAME my_section KVMA RAM_REGION GROUP RODATA_REGION SUBALIGN ${CONFIG_LINKER_ITERABLE_SUBALIGN})
zephyr_iterable_section(NAME KVMA RAM_REGION GROUP RODATA_REGION SUBALIGN ${CONFIG_LINKER_ITERABLE_SUBALIGN})
