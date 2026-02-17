/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/libc-hooks.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#include <zephyr/settings/settings.h>

#include <zephyr/sys/byteorder.h>

//#include <dk_buttons_and_leds.h>

#include <stdlib.h>
#include <ctype.h>

#include "nrf52txpwr.h"
//#include "async_spec_uart_svc.h"
#include "int_spec_uart_svc.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>


//#include <mpsl_fem_power_model.h>
#include <mpsl_fem_protocol_api.h>

#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/version.h>
#include <ncs_version.h>
#include <ncs_commit.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/regulator/nrf5x.h>
#include <hal/nrf_power.h>
#include <zephyr/console/console.h>
#include <zephyr/console/tty.h>

#include "losstst_svc.h"

#if (1)//(DT_PROP(DT_INST(0, nordic_nrf5x_regulator), regulator_initial_mode)== NRF5X_REG_MODE_DCDC)
int impl_func_dcdc(int req)
{
	int retval=-1;
	if(0==req) {
		retval=(int)nrf_power_dcdcen_get(NRF_POWER);
	}
	else if(0<req) {
		nrf_power_dcdcen_set(NRF_POWER,1);
		retval=1;
	}
	else {
		nrf_power_dcdcen_set(NRF_POWER,0);
		retval=0;
	}
	return retval;
}
#else
#define impl_func_dcdc NULL
#endif

// 1:switching/0:linear/-1:unknown func( 0:enquire/1:switching/-1:linear)
int (*func_dcdc)(int)=impl_func_dcdc; 

//const struct device *spec_uart0 = DEVICE_DT_GET(DT_NODELABEL(uart0));
const struct device *spec_uart1 = DEVICE_DT_GET(DT_NODELABEL(uart1));
const struct gpio_dt_spec loc_dt_dio[]={GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),dio_gpios,0),
										GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),dio_gpios,1),
										GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),dio_gpios,2),
										GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),dio_gpios,3)};
#define GPIO_SIG0 &loc_dt_dio[0]
#define GPIO_SIG1 &loc_dt_dio[1]
#define GPIO_SIG2 &loc_dt_dio[2]
#define GPIO_SIG3 &loc_dt_dio[3]


// Core Spec. 5.4, page 1375.
#define BT_GAP_ADV_FAST_INT_CODED_MIN_1		(((unsigned int)90  * 16) /10)  /* TGAP(adv_fast_interval1_coded) */
#define BT_GAP_ADV_FAST_INT_CODED_MAX_1		(((unsigned int)180 * 16) /10)
#define BT_GAP_ADV_FAST_INT_CODED_MIN_2		(((unsigned int)300 * 16) /10)  /* TGAP(adv_fast_interval2_coded) */
#define BT_GAP_ADV_FAST_INT_CODED_MAX_2		(((unsigned int)450 * 16) /10)
#define BT_GAP_ADV_SLOW_INT_CODED_MIN		(((unsigned int)3000* 16) /10)  /* TGAP(adv_slow_interval_coded) */
#define BT_GAP_ADV_SLOW_INT_CODED_MAX		(((unsigned int)3600* 16) /10)
#define BT_GAP_INIT_CONN_INT_CODED_MIN      (((unsigned int)90  * 16) /10)  /* TGAP(initial_conn_interval_coded) */
#define BT_GAP_INIT_CONN_INT_CODED_MAX      (((unsigned int)150 * 16) /10)



typedef struct __attribute__((__packed__))
{
	unsigned m7:1;
	unsigned m5:1;
	unsigned m3:1;
	unsigned m1:1;
	unsigned m0:1;
	unsigned m2:1;
	unsigned m4:1;
	unsigned m6:1;

	unsigned m15:1;
	unsigned m13:1;
	unsigned m11:1;
	unsigned m9:1;
	unsigned m8:1;
	unsigned m10:1;
	unsigned m12:1;
	unsigned m14:1;
}ADAPT_DIP_SW_ST;

typedef union __attribute__((__packed__))
{
	ADAPT_DIP_SW_ST DIP_SW_MAP;
	struct {
		unsigned immd_SND:1;
		unsigned immd_RCV:1;
		unsigned NUM:3; //0..7: 500,1000,2000,5000,10000,20000,50000,50000
		unsigned TX_ATT:3;

		unsigned SCANNER:1;
		unsigned PHY_S8:1;
		unsigned PHY_1M:1;
		unsigned ADV_PARAM:4;
		unsigned :1;
	};
	uint16_t u16_val;
} CFG_SWITCH_ST;

CFG_SWITCH_ST cfg_switch;
bool task_ENVMON,task_SENDER, task_SCANNER, task_NUMCAST, task_delay;


#if(DTM_ADAPT_BRD)


#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/devicetree/io-channels.h>
const struct spi_dt_spec bus_spi1_dev0_dt=SPI_DT_SPEC_GET(DT_NODELABEL(slv0)
															,(SPI_WORD_SET(8)
																|SPI_OP_MODE_MASTER
																|SPI_MODE_CPOL|SPI_MODE_CPHA
																|SPI_TRANSFER_MSB
																|SPI_LINES_SINGLE)
															,0);

uint16_t dip_sw_map,dip_sw_chgd;
uint8_t spi_buf_context[2];
struct spi_buf spibuf={.buf=spi_buf_context,.len=2};
struct spi_buf_set rcvbuf={.buffers=&spibuf,.count=1};
bool bus_spi1_rdy;

void ext_sio_init(void)
{
	if(spi_is_ready_dt(&bus_spi1_dev0_dt))
		bus_spi1_rdy=true, spi_read_dt(&bus_spi1_dev0_dt,&rcvbuf);
}

void ext_sio_read(void)
{
	static uint16_t previous_val;
	uint16_t current_val;
	spi_read_dt(&bus_spi1_dev0_dt,&rcvbuf);
	current_val=*(uint16_t *)spi_buf_context;
	if(previous_val!=current_val) {
		previous_val=current_val;
		return;
	}
	uint16_t omap=0;
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m0)?0:BIT(0);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m1)?0:BIT(1);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m2)?0:BIT(2);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m3)?0:BIT(3);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m4)?0:BIT(4);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m5)?0:BIT(5);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m6)?0:BIT(6);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m7)?0:BIT(7);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m8)?0:BIT(8);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m9)?0:BIT(9);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m10)?0:BIT(10);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m11)?0:BIT(11);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m12)?0:BIT(12);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m13)?0:BIT(13);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m14)?0:BIT(14);
	omap|=(((ADAPT_DIP_SW_ST*)spi_buf_context)->m15)?0:BIT(15);
	dip_sw_chgd|=dip_sw_map^omap;
	dip_sw_map=omap;
}

static void unidir_spi_work_handle(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(unidir_spi_work, unidir_spi_work_handle);

static void unidir_spi_work_handle(struct k_work *work)
{
	if(!bus_spi1_rdy)
		ext_sio_init();
	else
		ext_sio_read();
	
	k_work_schedule(&unidir_spi_work,Z_TIMEOUT_MS(250));
}


int8_t get_cfg_tx_pwr(void) { return (CONFIG_BT_CTLR_TX_PWR_ANTENNA-(4*cfg_switch.TX_ATT)); }
uint16_t get_cfg_total_num_idx(void) { return cfg_switch.NUM; }
int8_t get_cfg_interval_idx(void) { return MIN(cfg_switch.ADV_PARAM,10); }
bool poll_cfg_switch(void)
{
	bool result=false;
	if(dip_sw_chgd) cfg_switch.u16_val=dip_sw_map, result=true;
	return result;
}

void flush_cfg_flag(void)
{
	dip_sw_chgd=0;
}

#else
void ext_sio_init(void){return;}
void ext_sio_read(void){return;}
int8_t get_cfg_tx_pwr(void) { return CONFIG_BT_CTLR_TX_PWR_ANTENNA; }
uint16_t get_cfg_total_num_idx(void) { return 1; }
int8_t get_cfg_interval_idx(void) { return 0; }
bool poll_cfg_switch(void) {return false;}
void flush_cfg_flag(void) {;}
#endif


#if defined(CONFIG_UART_CONSOLE)
#if(0)
int hook_getchar(void)
{
	return console_getchar();
}
void stdin_listener_init(void)
{
	console_init();
	__stdin_hook_install(hook_getchar);
}
#else
static char stdin_array[128];
static int stdin_put,stdin_get;
static struct k_thread stdin_listener_thread;
K_THREAD_STACK_DEFINE(stdin_listener_stack,512);
static int hook_getchar(void)
{
	int retval;
	if(stdin_put==stdin_get) {
		return EOF;
	}
	retval=stdin_array[(ARRAY_SIZE(stdin_array)-1)&(stdin_get++)];
	return retval;
}
static void stdin_listener(void * p1, void * p2, void * p3)
{
	while(1) {
		int val_getchar=console_getchar();
		if(ARRAY_SIZE(stdin_array)>(unsigned int)(stdin_put-stdin_get)) {
			stdin_array[(ARRAY_SIZE(stdin_array)-1)&(stdin_put++)]=val_getchar;
		}
	}
}
void stdin_listener_init(void)
{
    k_tid_t tid_p = k_thread_create(&stdin_listener_thread,
       stdin_listener_stack,
       K_THREAD_STACK_SIZEOF(stdin_listener_stack),
       stdin_listener,NULL,NULL,NULL,
       0,0,
       (k_timeout_t){.ticks=0} // K_MSEC(100)
    );


	console_init();
	// zephyr-sdk\arm-zephyr-eabi\picolibc\arm-zephyr-eabi\sys-include\stdio.h
	// zephyr-sdk\arm-zephyr-eabi\picolibc\include\stdio.h
	// struct __file;
	__stdin_hook_install(hook_getchar);

    k_thread_name_set(tid_p,"stdin_listener");
}
#endif
#endif // CONFIG_UART_CONSOLE



#ifdef CONFIG_SOC_DCDC_NRF52X
  #define SOC_REGULATOR_INFO S
#else
  #define SOC_REGULATOR_INFO L
#endif

#ifndef CLONE_CHAR
  #define CLONE_CHAR X
#endif

#define TO_STRING(arg) _to_string(arg)
#define _to_string(arg) #arg 
#define DEVICE_NAME_SUPPLEMENT "(%03u," TO_STRING(SOC_REGULATOR_INFO) ",%s," TO_STRING(CONFIG_BT_CTLR_TX_PWR_ANTENNA) ")"
#define PHY_CHK_MASK		(BT_LE_ADV_OPT_EXT_ADV|BT_LE_ADV_OPT_NO_2M|BT_LE_ADV_OPT_CODED)
#define PHY_CHK_1M(PARM)	(((((PARM->options)^(BT_LE_ADV_OPT_EXT_ADV|BT_LE_ADV_OPT_NO_2M))&PHY_CHK_MASK)==0)||((((PARM->options))&PHY_CHK_MASK)==0))
#define PHY_CHK_1M2M(PARM)	((((PARM->options)^(BT_LE_ADV_OPT_EXT_ADV))&PHY_CHK_MASK)==0)
#define PHY_CHK_S8(PARM)	((((PARM->options)^(BT_LE_ADV_OPT_EXT_ADV|BT_LE_ADV_OPT_NO_2M|BT_LE_ADV_OPT_CODED))&PHY_CHK_MASK)==0)
#define PHY_STR(PARM)		(PHY_CHK_1M(PARM))?"1M":(PHY_CHK_1M2M(PARM))?"1M/2M":(PHY_CHK_S8(PARM))?"S8":"?"

int update_adv(uint8_t index , const struct bt_le_adv_param *adv_parm , struct bt_data *adv_data , const struct bt_le_ext_adv_start_param *adv_start_param);
void blocking_adv(uint8_t);



extern void	extscr_init(void);

void TOGGLE_SIG0(void)
{
	gpio_pin_toggle_dt(GPIO_SIG0);
}
void TOGGLE_SIG1(void)
{
	gpio_pin_toggle_dt(GPIO_SIG1);
}
void TOGGLE_SIG2(void)
{
	gpio_pin_toggle_dt(GPIO_SIG2);
}
void TOGGLE_SIG3(void)
{
	gpio_pin_toggle_dt(GPIO_SIG3);
}


struct uart_config spec_uart_cfg = {.baudrate=115200,
									.parity=UART_CFG_PARITY_NONE,      // zephyr/drivers/uart.h, enum uart_config_parity
									.stop_bits=UART_CFG_STOP_BITS_2,   // zephyr/drivers/uart.h, enum uart_config_stop_bits
									.data_bits=UART_CFG_DATA_BITS_8,   // zephyr/drivers/uart.h, enum uart_config_data_bits
									.flow_ctrl=UART_CFG_FLOW_CTRL_NONE // zephyr/drivers/uart.h, enum uart_config_flow_control
									};


// retval 1:dip-switch changed
//		  2:ext-scr task_trigger
//		 -2:ext-scr task_abort
int is_re_sche(int update)
{
	int result=0;
	//static int8_t extscr_tgr_stamp;
	//int8_t tgr_val=ext_scr_task_tgr(0);
	static int16_t extscr_tgr_stamp;
	int16_t tgr_val=MAX(sender_task_tgr(0),MAX(scanner_task_tgr(0),MAX(numcst_task_tgr(0),envmon_task_tgr(0))));
	if(0==extscr_tgr_stamp && 0!=tgr_val) {
		result=2;
		if(update) extscr_tgr_stamp=tgr_val;
		flush_cfg_flag();
	} 
	else if(0!=extscr_tgr_stamp && 0==tgr_val) {
		result=-2;
		if(update) extscr_tgr_stamp=tgr_val;
		flush_cfg_flag();
	} 
	else if(poll_cfg_switch()) {
		result=1;
		if(update) {
			extscr_tgr_stamp=0;
			flush_cfg_flag();
			sender_task_tgr(-sender_task_tgr(0));
			scanner_task_tgr(-scanner_task_tgr(0));
			numcst_task_tgr(-numcst_task_tgr(0));
			envmon_task_tgr(-envmon_task_tgr(0));
		}
	}
	
	return result;
}


struct TEST_PARM round_test_parm;

bool tst_sender_abort(void)
{
	return is_re_sche(0);
}
bool tst_scanner_abort(void)
{
	return is_re_sche(0);
}
bool tst_numcast_abort(void)
{
	return is_re_sche(0);
}
bool tst_envmon_abort(void)
{
	return is_re_sche(0);
}
//void tst_txpower_setup(int8_t val)
//{
//	txpwr_set(val);
//}



#include <mpsl_temp.h>

void load_parm_cfg(void)
{
	round_test_parm.txpwr=enum_txpower(0);
	round_test_parm.count_idx=enum_totalnum_idx(0);
	round_test_parm.interval_idx=enum_adv_interval_idx(0);
	round_test_parm.envmon_abort=tst_envmon_abort;
	round_test_parm.sender_abort=tst_sender_abort;
	round_test_parm.scanner_abort=tst_scanner_abort;
	round_test_parm.numcast_abort=tst_numcast_abort;
	round_test_parm.phy_2m=get_cfg_phy_sel(0);
	round_test_parm.phy_1m=get_cfg_phy_sel(1);
	round_test_parm.phy_s8=get_cfg_phy_sel(2);
	round_test_parm.phy_ble4=get_cfg_phy_sel(3);
	round_test_parm.inhibit_ch37=!get_cfg_ch37();
	round_test_parm.inhibit_ch38=!get_cfg_ch38();
	round_test_parm.inhibit_ch39=!get_cfg_ch39();
	round_test_parm.non_ANONYMOUS=get_cfg_NON_ANONYMOUS();
	round_test_parm.ignore_rcv_resp=get_uni_cast_method();
}

void load_parm_dipswitch(void)
{
	printk("CFG_sw ADV_PARM(%u) PHY_1M(%u) PHY_S8(%u) SCANNER(%u)\n       TX_ATT(%u) NUM(%u) immd_RCV(%u) immd_SND(%u)\n"
		,cfg_switch.ADV_PARAM,cfg_switch.PHY_1M,cfg_switch.PHY_S8,cfg_switch.SCANNER,cfg_switch.TX_ATT,cfg_switch.NUM,cfg_switch.immd_RCV,cfg_switch.immd_SND);
	round_test_parm.txpwr=get_cfg_tx_pwr();
	round_test_parm.count_idx=get_cfg_total_num_idx();
	round_test_parm.interval_idx=get_cfg_interval_idx();
	round_test_parm.envmon_abort=tst_envmon_abort;
	round_test_parm.sender_abort=tst_sender_abort;
	round_test_parm.scanner_abort=tst_scanner_abort;
	round_test_parm.numcast_abort=tst_numcast_abort;
	round_test_parm.phy_2m=(cfg_switch.PHY_1M)?true:false;
	round_test_parm.phy_1m=(cfg_switch.PHY_1M)?true:false;
	round_test_parm.phy_s8=(cfg_switch.PHY_1M)?true:false;
	round_test_parm.phy_ble4=false;
	round_test_parm.inhibit_ch37=true;
	round_test_parm.inhibit_ch38=true;
	round_test_parm.inhibit_ch39=true;
	round_test_parm.non_ANONYMOUS=true;
	round_test_parm.ignore_rcv_resp=false;
}

int main(void)
{
  #if defined(CONFIG_UART_CONSOLE)
	stdin_listener_init();
	__stdout_hook_install(console_putchar);
  #endif
	int err;

	ext_sio_init();
	ext_sio_read();
	
	for(uint8_t idx=0;idx<ARRAY_SIZE(loc_dt_dio);idx++) {
		int retval=gpio_pin_configure_dt(&loc_dt_dio[idx],GPIO_OUTPUT|loc_dt_dio[idx].dt_flags);
	}

	losstst_init();
	
	spec_uart_init(spec_uart1,&spec_uart_cfg,NULL,0,UART_CTRL_FULL_DUPLEX);
	spec_uart_rcv_begin();
	extscr_init();
  #if(DTM_ADAPT_BRD)
	k_work_schedule(&unidir_spi_work,Z_TIMEOUT_MS(500));
  #endif

	int64_t uptime_64_barrier=2000+k_uptime_get();

	while(uptime_64_barrier> k_uptime_get()) if(k_can_yield()) k_yield();

	bool re_sche;
	
	for (;;) {
		if(!task_ENVMON && !task_SCANNER && !task_SENDER && !task_NUMCAST) {
			if(!poll_cfg_switch()) {
				if(sender_task_tgr(0)) task_ENVMON=false ,task_SENDER=true, task_SCANNER=false, task_NUMCAST=false, load_parm_cfg();
				else if(scanner_task_tgr(0)) task_ENVMON=false ,task_SENDER=false, task_SCANNER=true, task_NUMCAST=false, load_parm_cfg();
				else if(numcst_task_tgr(0)) task_ENVMON=false ,task_SENDER=false, task_SCANNER=false, task_NUMCAST=true, load_parm_cfg();
				else if(envmon_task_tgr(0)) task_ENVMON=true ,task_SENDER=false, task_SCANNER=false, task_NUMCAST=false, load_parm_cfg();
				else task_ENVMON=false ,task_SENDER=false, task_SCANNER=false, task_NUMCAST=false;
				task_delay=false;
			}
			else if(0!=DTM_ADAPT_BRD && poll_cfg_switch()) {
				load_parm_dipswitch();
				
				if(cfg_switch.immd_SND && !cfg_switch.immd_RCV)
					task_delay=false, task_SENDER=true, task_SCANNER=false;
				
				else if(!cfg_switch.immd_SND && cfg_switch.immd_RCV)
					task_delay=false, task_SENDER=false, task_SCANNER=true;
				
				else if(cfg_switch.immd_SND || cfg_switch.immd_RCV)
					task_delay=false, task_SENDER=false, task_SCANNER=false;
				
				else if(!cfg_switch.SCANNER)
					task_delay=true, task_SENDER=true, task_SCANNER=false;

				else if(cfg_switch.SCANNER)
					task_delay=true, task_SENDER=false, task_SCANNER=true;

				else
					task_delay=false, task_SENDER=false, task_SCANNER=false;
				
				flush_cfg_flag();
			}


			if(task_SCANNER) {
				blocking_adv(0);
				blocking_adv(1);
				blocking_adv(2);
				blocking_adv(3);
				scanner_setup(&round_test_parm);
			}
			else if(task_SENDER) {
				blocking_adv(0);
				blocking_adv(1);
				blocking_adv(2);
				blocking_adv(3);
				sender_setup(&round_test_parm);
			}
			else if(task_NUMCAST) {
				blocking_adv(0);
				blocking_adv(1);
				blocking_adv(2);
				blocking_adv(3);
				numcast_setup(&round_test_parm);
				is_re_sche(true);
				continue;
			}
			else if(task_ENVMON) {
				blocking_adv(0);
				blocking_adv(1);
				blocking_adv(2);
				blocking_adv(3);
				envmon_setup(&round_test_parm);
				is_re_sche(true);
				continue;
			}
			is_re_sche(true);
			uptime_64_barrier=1000+k_uptime_get();
			do {
				if(k_can_yield()) k_yield();
				if(true==(re_sche=is_re_sche(false))) break;
			} while(uptime_64_barrier > k_uptime_get());
			if(re_sche) { task_SCANNER=task_SENDER=task_NUMCAST=false; continue; }


			if(!task_SCANNER && !task_SENDER) continue;
			
			update_adv(3,NULL,NULL,NULL);

			if(task_delay)
				uptime_64_barrier=((task_SCANNER)?1000:20000)+k_uptime_get();
			else
				uptime_64_barrier=((task_SCANNER)?1000:3000)+k_uptime_get();
			
			do {
				if(k_can_yield()) k_yield();
				if(true==(re_sche=is_re_sche(false))) break;
			} while(uptime_64_barrier > k_uptime_get());

			if(re_sche) { task_SCANNER=task_SENDER=task_NUMCAST=false; continue; }
			
		}

		if(task_ENVMON) {
			int err=losstst_envmon();
			if(0>=err) {
				task_ENVMON=false;
				envmon_task_tgr(-envmon_task_tgr(0));
			}
		}
		else if(task_SENDER) {
			int err=losstst_sender();
			if(0>=err) {
				task_SENDER=false;
				sender_task_tgr(-sender_task_tgr(0));
			}
		}
		else if(task_SCANNER) {
			int err=losstst_scanner();
			if(0>=err) {
				task_SCANNER=false;
				scanner_task_tgr(-scanner_task_tgr(0));
			}
		}
		else if(task_NUMCAST) {
			int err=losstst_numcast();
			if(0>=err) {
				task_NUMCAST=false;
				sender_task_tgr(-sender_task_tgr(0));
			}
		}

		if(k_can_yield()) k_yield();
		#if(0)
		{
			static int64_t elapsed;
			static int64_t uptime_64_barrier;
			elapsed+=k_uptime_delta(&uptime_64_barrier);
			if(60000<=elapsed) {elapsed=0, thread_analyzer_print(0);

			//typedef void (*k_thread_user_cb_t)(const struct k_thread *thread, void *user_data);
			// void k_thread_foreach(k_thread_user_cb_t user_cb, void *user_data);
			k_thread_user_cb_t ask_prio(const struct k_thread *thread, void *user_data);
			k_thread_foreach(ask_prio,NULL);}
		}
		#endif
	}
}


int dump_to_str(char **dest, uint8_t **surc, uint32_t map_addr, size_t len)
{
//         1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
//1   5    0    5    0    5    0    5    0    5    0    5    0    5    0    5    0
//AAAAAAAA   00 11 22 33 44 55 66 77 - 88 99 aa bb cc dd ee ff   01234567 89abcdef
	
	if(NULL==dest || NULL==surc) { /*errno=EINVAL;*/ return -1; }
	if(NULL==*dest /*|| NULL==*surc*/) { /*errno=EINVAL;*/ return -1; }
	char hx;
	char * dst_p=*dest;
	uint8_t * src_p=*surc;
	char cbuf[17];
	char *chr_p=cbuf;
	uint32_t map_row=map_addr&0xFFFFFFF0u;
	int counter=0;
	
	hx='0'+(0x0F&(map_row>>28)), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	hx='0'+(0x0F&(map_row>>24)), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	hx='0'+(0x0F&(map_row>>20)), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	hx='0'+(0x0F&(map_row>>16)), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	hx='0'+(0x0F&(map_row>>12)), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	hx='0'+(0x0F&(map_row>>8 )), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	hx='0'+(0x0F&(map_row>>4 )), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	hx='0'+(0x0F&(map_row>>0 )), *(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
	//*(dst_p++)=' ',
	*(dst_p++)=' ',
	*(dst_p++)=' ', *(dst_p++)=' ';
	
	do
	{
		if(map_addr!=map_row || 0== len)
		{
			*(dst_p++)=' ', *(dst_p++)=' ', *(dst_p++)=' ', *(chr_p++)=' ';
			if(8==(0x0F & ++map_row))
			{
				*(dst_p++)=' ', *(dst_p++)=' ', *(chr_p++)=' ';
			}
		}
		else
		{
			unsigned char src=*src_p;
			hx='0'+(0x0F&(src>>4 )),*(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
			hx='0'+(0x0F&(src>>0 )),*(dst_p++)=(hx>'9')?hx+'A'-'9'-1:hx;
			*(dst_p++)=' ';
		#if(__GNUC__)
			if(isprint(src)) *(chr_p++)=src; else *(chr_p++)='.';
		#elif(1)
			if((0==(0x80&src)) && (0!=isprint(src))) *(chr_p++)=src; else *(chr_p++)='.';// for keil (errata)
		#else
			if(0!=isprint(0x7F&src)) *(chr_p++)=src; else *(chr_p++)='.';// for keil (errata)
		#endif
			src_p++;
			map_addr++;
			counter++;
			len--;
			if(8==(0x0F & ++map_row))
			{
				*(dst_p++)='-', *(dst_p++)=' ', *(chr_p++)=' ';
			}
		}
	}while(0!=(0x0F&map_row));
	chr_p=cbuf;
	*(dst_p++)=' ', *(dst_p++)=' ';
#if(1)
	memcpy(dst_p,cbuf,sizeof(cbuf));
	dst_p+=sizeof(cbuf);
#else
	*(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++),
	*(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++),
	*(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++),
	*(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++), *(dst_p++)=*(chr_p++),
	*(dst_p++)=*(chr_p++);
#endif
	//*(dst_p++)='\r',
	*(dst_p++)='\n', *dst_p='\0';
	*dest=dst_p;
	*surc=src_p;
	return counter;
}


const uint16_t TBL_CRC_16_xmodem_MSB[]=
{
// 0x00
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
	0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
	0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
// 0x20
	0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
	0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
	0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
// 0x40
	0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
	0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
	0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
// 0x60
	0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
	0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
	0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
// 0x80
	0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
	0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
	0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
// 0xA0
	0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
	0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
	0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
// 0xC0
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
	0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
	0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
// 0xE0
	0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
	0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
	0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
	0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};


const uint16_t TBL_CRC_16_ccitt_LSB[]=
{
// 0x00
	0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
	0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
	0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
	0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
// 0x20
	0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
	0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
	0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
	0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
// 0x40
	0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
	0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
	0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
	0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
// 0x60
	0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
	0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
	0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
	0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
// 0x80
	0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
	0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
	0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
	0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
// 0xA0
	0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
	0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
	0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
	0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
// 0xC0
	0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
	0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
	0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
	0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
// 0xE0
	0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
	0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
	0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
	0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78
};


const uint16_t TBL_CRC_16_ibm_LSB[]=
{
// 0x00
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
// 0x20
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
// 0x40
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
// 0x60
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
// 0x80
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
// 0xA0
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
// 0xC0
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
// 0xE0
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};


const uint8_t TBL_CRC_8_MSB[]=
{
// 0x00
        0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97, 0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
        0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4, 0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
        0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11, 0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
        0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52, 0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
// 0x40
        0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA, 0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
        0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9, 0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
        0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C, 0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
        0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F, 0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
// 0x80
        0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED, 0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
        0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE, 0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
        0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B, 0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
        0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
// 0xC0
        0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0, 0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
        0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93, 0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
        0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
		0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15,0x3B, 0x0A, 0x59, 0x68,0xFF, 0xCE, 0x9D, 0xAC
};


const uint8_t TBL_CRC_8_LSB[]=
{
// 0x00
	0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83, 0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
	0x9D, 0xC3, 0x21, 0x7F, 0xFC, 0xA2, 0x40, 0x1E, 0x5F, 0x01, 0xE3, 0xBD, 0x3E, 0x60, 0x82, 0xDC,
	0x23, 0x7D, 0x9F, 0xC1, 0x42, 0x1C, 0xFE, 0xA0, 0xE1, 0xBF, 0x5D, 0x03, 0x80, 0xDE, 0x3C, 0x62,
	0xBE, 0xE0, 0x02, 0x5C, 0xDF, 0x81, 0x63, 0x3D, 0x7C, 0x22, 0xC0, 0x9E, 0x1D, 0x43, 0xA1, 0xFF,
// 0x40
	0x46, 0x18, 0xFA, 0xA4, 0x27, 0x79, 0x9B, 0xC5, 0x84, 0xDA, 0x38, 0x66, 0xE5, 0xBB, 0x59, 0x07,
	0xDB, 0x85, 0x67, 0x39, 0xBA, 0xE4, 0x06, 0x58, 0x19, 0x47, 0xA5, 0xFB, 0x78, 0x26, 0xC4, 0x9A,
	0x65, 0x3B, 0xD9, 0x87, 0x04, 0x5A, 0xB8, 0xE6, 0xA7, 0xF9, 0x1B, 0x45, 0xC6, 0x98, 0x7A, 0x24,
	0xF8, 0xA6, 0x44, 0x1A, 0x99, 0xC7, 0x25, 0x7B, 0x3A, 0x64, 0x86, 0xD8, 0x5B, 0x05, 0xE7, 0xB9,
// 0x80
	0x8C, 0xD2, 0x30, 0x6E, 0xED, 0xB3, 0x51, 0x0F, 0x4E, 0x10, 0xF2, 0xAC, 0x2F, 0x71, 0x93, 0xCD,
	0x11, 0x4F, 0xAD, 0xF3, 0x70, 0x2E, 0xCC, 0x92, 0xD3, 0x8D, 0x6F, 0x31, 0xB2, 0xEC, 0x0E, 0x50,
	0xAF, 0xF1, 0x13, 0x4D, 0xCE, 0x90, 0x72, 0x2C, 0x6D, 0x33, 0xD1, 0x8F, 0x0C, 0x52, 0xB0, 0xEE,
	0x32, 0x6C, 0x8E, 0xD0, 0x53, 0x0D, 0xEF, 0xB1, 0xF0, 0xAE, 0x4C, 0x12, 0x91, 0xCF, 0x2D, 0x73,
// 0xC0
	0xCA, 0x94, 0x76, 0x28, 0xAB, 0xF5, 0x17, 0x49, 0x08, 0x56, 0xB4, 0xEA, 0x69, 0x37, 0xD5, 0x8B,
	0x57, 0x09, 0xEB, 0xB5, 0x36, 0x68, 0x8A, 0xD4, 0x95, 0xCB, 0x29, 0x77, 0xF4, 0xAA, 0x48, 0x16,
	0xE9, 0xB7, 0x55, 0x0B, 0x88, 0xD6, 0x34, 0x6A, 0x2B, 0x75, 0x97, 0xC9, 0x4A, 0x14, 0xF6, 0xA8,
	0x74, 0x2A, 0xC8, 0x96, 0x15, 0x4B, 0xA9, 0xF7, 0xB6, 0xE8, 0x0A, 0x54, 0xD7, 0x89, 0x6B, 0x35
};


uint16_t crc_xmodem(uint16_t crc, uint8_t data)
{
	// x^16 + x^12 + x^5 + x^0 , msb first
	// NORMAL	0x1021  <----
	// REVERSED	0x8408

	data^=(crc>>8);
	crc<<=8;
	return crc^TBL_CRC_16_xmodem_MSB[data];
}


uint16_t crc_16_ccitt(uint16_t crc, uint8_t data)
{
	// x^16 + x^12 + x^5 + x^0 , lsb first
	// NORMAL	0x1021
	// REVERSED	0x8408  <----

	data^=crc;
	crc>>=8;
	return crc^TBL_CRC_16_ccitt_LSB[data];
}


uint16_t crc_16(uint16_t crc, uint8_t data)
{
	// x^16 + x^15 + x^2 + x^0 , lsb first
	// NORMAL	0x8005
	// REVERSED	0xA001  <----

	data^=crc;
	crc>>=8;
	return crc^TBL_CRC_16_ibm_LSB[data];
}


uint8_t crc_8(uint8_t crc, uint8_t data)
{
	// x^8 + x^5 + x^4 + x^0 , lsb first
	// NORMAL	0x31
	// REVERSED	0x8C  <----

	return TBL_CRC_8_LSB[crc^data];
}


uint16_t crc_xmodem_stream(uint16_t init,ptrdiff_t pointer,size_t len)
{
	while(len--){init=crc_xmodem(init,*(uint8_t *)pointer);(uint8_t *)pointer++;}
	return init;
}


uint16_t crc_16_ccitt_stream(uint16_t init,ptrdiff_t pointer,size_t len)
{
	while(len--){init=crc_16_ccitt(init,*(uint8_t *)pointer);(uint8_t *)pointer++;}
	return init;
}


uint16_t crc_16_stream(uint16_t init,ptrdiff_t pointer,size_t len)
{
	while(len--){init=crc_16(init,*(uint8_t *)pointer);(uint8_t *)pointer++;}
	return init;
}


uint8_t crc_8_stream(uint8_t init, ptrdiff_t pointer, size_t len)
{
	while(len--){init=crc_8(init,*(uint8_t *)pointer);(uint8_t *)pointer++;}
	return init;
}


uint16_t chksum_stream(uint16_t init,ptrdiff_t pointer,size_t len)
{
	while(len--) { init+=*(uint8_t *)pointer; (uint8_t *)pointer++; }
	return init;
}


uint8_t crc_8_msb(uint8_t crc, uint8_t data)
{
	// x^8 + x^5 + x^4 + x^0 , lsb first
	// NORMAL	0x31  <----
	// REVERSED	0x8C

	/*uint8_t i;
	uint8_t tmp;
	tmp=crc^data;
	for(i=0;i<8;i++){
		if(0!=(tmp&0x80)){
			tmp<<=1;
			tmp^=0x31;
		}
		else{
			tmp<<=1;
		}
	}
	return tmp;*/
	return TBL_CRC_8_MSB[crc^data];
}


uint8_t crc_8_msb_stream(uint8_t init, ptrdiff_t pointer, size_t len)
{
	while(len--){init=crc_8_msb(init,*(uint8_t *)pointer);(uint8_t *)pointer++;}
	return init;
}

