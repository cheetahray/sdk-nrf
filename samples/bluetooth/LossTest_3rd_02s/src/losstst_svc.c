

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include <zephyr/settings/settings.h>
#include <sdc_hci_vs.h>

#include <stdlib.h>
#include <ctype.h>

#include "losstst_svc.h"
#include "ui_resource_code.h"

#define GET_SYM_DECL(CFG_SYM) uint32_t get_GSYM_ ##CFG_SYM(void) __attribute__((weak));
#define GET_SYM_VAL(CFG_SYM) get_GSYM_ ##CFG_SYM()
#define GET_SYM_IMPL(CFG_SYM) const uint32_t CFG_SYM __attribute__((weak)); uint32_t get_GSYM_ ##CFG_SYM (void) { return (uint32_t)&CFG_SYM ; }

GET_SYM_DECL(CONFIG_BT_EXT_ADV_MAX_ADV_SET)
GET_SYM_DECL(CONFIG_BT_DEVICE_NAME_MAX)
GET_SYM_DECL(CONFIG_BT_DEVICE_NAME)

#define BT_GAP_SCAN_FAST_INTERVAL_CODED         0x0120  /* 180 ms    */

#define VALUE_ADV_INT_MIN_0 30  //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1)
#define VALUE_ADV_INT_MAX_0 60  //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1)
#define PARAM_ADV_INT_MIN_0 (((unsigned int)VALUE_ADV_INT_MIN_0*16)/10)
#define PARAM_ADV_INT_MAX_0 (((unsigned int)VALUE_ADV_INT_MAX_0*16)/10)

#define VALUE_ADV_INT_MIN_1 60 
#define VALUE_ADV_INT_MAX_1 120 
#define PARAM_ADV_INT_MIN_1 (((unsigned int)VALUE_ADV_INT_MIN_1*16)/10)
#define PARAM_ADV_INT_MAX_1 (((unsigned int)VALUE_ADV_INT_MAX_1*16)/10)

#define VALUE_ADV_INT_MIN_2 90  //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1_coded)
#define VALUE_ADV_INT_MAX_2 180 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1_coded)
#define PARAM_ADV_INT_MIN_2 (((unsigned int)VALUE_ADV_INT_MIN_2*16)/10)
#define PARAM_ADV_INT_MAX_2 (((unsigned int)VALUE_ADV_INT_MAX_2*16)/10)

#define VALUE_ADV_INT_MIN_3 100 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2)
#define VALUE_ADV_INT_MAX_3 150 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2)
#define PARAM_ADV_INT_MIN_3 (((unsigned int)VALUE_ADV_INT_MIN_3*16)/10)
#define PARAM_ADV_INT_MAX_3 (((unsigned int)VALUE_ADV_INT_MAX_3*16)/10)

#define VALUE_ADV_INT_MIN_4 200 
#define VALUE_ADV_INT_MAX_4 300 
#define PARAM_ADV_INT_MIN_4 (((unsigned int)VALUE_ADV_INT_MIN_4*16)/10)
#define PARAM_ADV_INT_MAX_4 (((unsigned int)VALUE_ADV_INT_MAX_4*16)/10)

#define VALUE_ADV_INT_MIN_5 300 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2_coded)
#define VALUE_ADV_INT_MAX_5 450 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2_coded)
#define PARAM_ADV_INT_MIN_5 (((unsigned int)VALUE_ADV_INT_MIN_5*16)/10)
#define PARAM_ADV_INT_MAX_5 (((unsigned int)VALUE_ADV_INT_MAX_5*16)/10)

#define VALUE_ADV_INT_MIN_6 500 
#define VALUE_ADV_INT_MAX_6 650 
#define PARAM_ADV_INT_MIN_6 (((unsigned int)VALUE_ADV_INT_MIN_6*16)/10)
#define PARAM_ADV_INT_MAX_6 (((unsigned int)VALUE_ADV_INT_MAX_6*16)/10)

#define VALUE_ADV_INT_MIN_7 750 
#define VALUE_ADV_INT_MAX_7 950 
#define PARAM_ADV_INT_MIN_7 (((unsigned int)VALUE_ADV_INT_MIN_7*16)/10)
#define PARAM_ADV_INT_MAX_7 (((unsigned int)VALUE_ADV_INT_MAX_7*16)/10)

#define VALUE_ADV_INT_MIN_8 1000 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval)
#define VALUE_ADV_INT_MAX_8 1200 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval)
#define PARAM_ADV_INT_MIN_8 (((unsigned int)VALUE_ADV_INT_MIN_8*16)/10)
#define PARAM_ADV_INT_MAX_8 (((unsigned int)VALUE_ADV_INT_MAX_8*16)/10)

#define VALUE_ADV_INT_MIN_9 2000 
#define VALUE_ADV_INT_MAX_9 2400 
#define PARAM_ADV_INT_MIN_9 (((unsigned int)VALUE_ADV_INT_MIN_9*16)/10)
#define PARAM_ADV_INT_MAX_9 (((unsigned int)VALUE_ADV_INT_MAX_9*16)/10)

#define VALUE_ADV_INT_MIN_10 3000 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval_coded)
#define VALUE_ADV_INT_MAX_10 3600 //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval_coded)
#define PARAM_ADV_INT_MIN_10 (((unsigned int)VALUE_ADV_INT_MIN_10*16)/10)
#define PARAM_ADV_INT_MAX_10 (((unsigned int)VALUE_ADV_INT_MAX_10*16)/10)

#define LOSS_TEST_BURST_COUNT 250ul

#define VALUE_ADV_INT_MIN_n1 20
#define VALUE_ADV_INT_MAX_n1 40
#define PARAM_ADV_INT_MIN_n1 (((unsigned int)VALUE_ADV_INT_MIN_n1*16)/10)
#define PARAM_ADV_INT_MAX_n1 (((unsigned int)VALUE_ADV_INT_MAX_n1*16)/10)

#define VALUE_ADV_INT_MIN_n2 12
#define VALUE_ADV_INT_MAX_n2 25
#define PARAM_ADV_INT_MIN_n2 (((unsigned int)VALUE_ADV_INT_MIN_n2*16)/10)
#define PARAM_ADV_INT_MAX_n2 (((unsigned int)VALUE_ADV_INT_MAX_n2*16)/10)

#define VALUE_ADV_INT_MIN_n3 10
#define VALUE_ADV_INT_MAX_n3 20
#define PARAM_ADV_INT_MIN_n3 (((unsigned int)VALUE_ADV_INT_MIN_n3*16)/10)
#define PARAM_ADV_INT_MAX_n3 (((unsigned int)VALUE_ADV_INT_MAX_n3*16)/10)

#define FIXED_BT_LE_ADV_PARAM(_id, _options, _int_min, _int_max, _peer) \
	&((const struct bt_le_adv_param) { \
		.id = _id, \
		.sid = 0, \
		.secondary_max_skip = 0, \
		.options = (_options), \
		.interval_min = (_int_min), \
		.interval_max = (_int_max), \
		.peer = (_peer), \
	})

#define BT4_ADV_OPT_CLR_MASK (BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_ANONYMOUS | BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_NO_2M | BT_LE_ADV_OPT_CODED )
#define ADV_OPT_IDX_0 (BT_LE_ADV_OPT_NONE \
	| BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_ANONYMOUS | BT_LE_ADV_OPT_EXT_ADV \
	)

#define ADV_OPT_IDX_1 (BT_LE_ADV_OPT_NONE \
	| BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_ANONYMOUS | BT_LE_ADV_OPT_EXT_ADV \
	| BT_LE_ADV_OPT_NO_2M \
	)

#define ADV_OPT_IDX_2 (BT_LE_ADV_OPT_NONE \
	| BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_ANONYMOUS | BT_LE_ADV_OPT_EXT_ADV \
	| BT_LE_ADV_OPT_NO_2M \
	| BT_LE_ADV_OPT_CODED \
	)

#define ADV_OPT_IDX_3 (BT_LE_ADV_OPT_NONE \
	| BT_LE_ADV_OPT_USE_IDENTITY \
	)
	
// man_id
#ifndef MANUFACTURER_ID
  #define MANUFACTURER_ID   ((uint16_t)0xFFFF)
#endif

// form_id
#ifndef LOSS_TEST_FORM_ID
  #define LOSS_TEST_FORM_ID ((uint16_t)0xBAAB)
#endif

#define CHK_UPDATE_ADV_PROCDURE 0

static const uint16_t value_interval[][2]={
	{VALUE_ADV_INT_MIN_0, VALUE_ADV_INT_MAX_0}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1)
	{VALUE_ADV_INT_MIN_1, VALUE_ADV_INT_MAX_1}, 
	{VALUE_ADV_INT_MIN_2, VALUE_ADV_INT_MAX_2}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1_coded)
	{VALUE_ADV_INT_MIN_3, VALUE_ADV_INT_MAX_3}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2)
	{VALUE_ADV_INT_MIN_4, VALUE_ADV_INT_MAX_4},
	{VALUE_ADV_INT_MIN_5, VALUE_ADV_INT_MAX_5}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2_coded)
	{VALUE_ADV_INT_MIN_6, VALUE_ADV_INT_MAX_6},
	{VALUE_ADV_INT_MIN_7, VALUE_ADV_INT_MAX_7},
	{VALUE_ADV_INT_MIN_8, VALUE_ADV_INT_MAX_8}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval)
	{VALUE_ADV_INT_MIN_9, VALUE_ADV_INT_MAX_9},
	{VALUE_ADV_INT_MIN_10,VALUE_ADV_INT_MAX_10} //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval_coded)
	//,{VALUE_ADV_INT_MIN_n1, VALUE_ADV_INT_MAX_n1}
	//,{VALUE_ADV_INT_MIN_n2, VALUE_ADV_INT_MAX_n2}
	//,{VALUE_ADV_INT_MIN_n3, VALUE_ADV_INT_MAX_n3}
};

static const uint16_t param_interval[][2]={
	{PARAM_ADV_INT_MIN_0, PARAM_ADV_INT_MAX_0}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1)
	{PARAM_ADV_INT_MIN_1, PARAM_ADV_INT_MAX_1}, 
	{PARAM_ADV_INT_MIN_2, PARAM_ADV_INT_MAX_2}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1376 ; TGAP(adv_fast_interval1_coded)
	{PARAM_ADV_INT_MIN_3, PARAM_ADV_INT_MAX_3}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2)
	{PARAM_ADV_INT_MIN_4, PARAM_ADV_INT_MAX_4},
	{PARAM_ADV_INT_MIN_5, PARAM_ADV_INT_MAX_5}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_fast_interval2_coded)
	{PARAM_ADV_INT_MIN_6, PARAM_ADV_INT_MAX_6},
	{PARAM_ADV_INT_MIN_7, PARAM_ADV_INT_MAX_7},
	{PARAM_ADV_INT_MIN_8, PARAM_ADV_INT_MAX_8}, //BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval)
	{PARAM_ADV_INT_MIN_9, PARAM_ADV_INT_MAX_9},
	{PARAM_ADV_INT_MIN_10,PARAM_ADV_INT_MAX_10},//BLUETOOTH CORE SPEC 5.4, Vol 3, Part C, p.1377 ; TGAP(adv_slow_interval_coded)
	{PARAM_ADV_INT_MIN_10,PARAM_ADV_INT_MAX_10},
	{PARAM_ADV_INT_MIN_10,PARAM_ADV_INT_MAX_10},
	{PARAM_ADV_INT_MIN_10,PARAM_ADV_INT_MAX_10},
	{PARAM_ADV_INT_MIN_10,PARAM_ADV_INT_MAX_10},
	{PARAM_ADV_INT_MIN_10,PARAM_ADV_INT_MAX_10}
//	,{PARAM_ADV_INT_MIN_n1, PARAM_ADV_INT_MAX_n1}
//	,{PARAM_ADV_INT_MIN_n2, PARAM_ADV_INT_MAX_n2}
//	,{PARAM_ADV_INT_MIN_n3, PARAM_ADV_INT_MAX_n3}
};


static const struct bt_le_adv_param *non_connectable_adv_param_peek[] ={
	//IDX_0 , TX side
	FIXED_BT_LE_ADV_PARAM(0,
		BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_NO_2M | BT_LE_ADV_OPT_EXT_ADV,
		BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL),
	//IDX_1 , RX side
	FIXED_BT_LE_ADV_PARAM(0,
		BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_NO_2M | BT_LE_ADV_OPT_EXT_ADV,
		BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL)
};

static const struct bt_le_adv_param *non_connectable_adv_param_x[][4] ={
	{	// group 0
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_0, PARAM_ADV_INT_MAX_0, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_0, PARAM_ADV_INT_MAX_0, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_0, PARAM_ADV_INT_MAX_0, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_0, PARAM_ADV_INT_MAX_0, NULL)},
	{	// group 1
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_1, PARAM_ADV_INT_MAX_1, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_1, PARAM_ADV_INT_MAX_1, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_1, PARAM_ADV_INT_MAX_1, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_1, PARAM_ADV_INT_MAX_1, NULL)},
	{	// group 2
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_2, PARAM_ADV_INT_MAX_2, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_2, PARAM_ADV_INT_MAX_2, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_2, PARAM_ADV_INT_MAX_2, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_2, PARAM_ADV_INT_MAX_2, NULL)},
	{	// group 3
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_3, PARAM_ADV_INT_MAX_3, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_3, PARAM_ADV_INT_MAX_3, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_3, PARAM_ADV_INT_MAX_3, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_3, PARAM_ADV_INT_MAX_3, NULL)},
	{	// group 4
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_4, PARAM_ADV_INT_MAX_4, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_4, PARAM_ADV_INT_MAX_4, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_4, PARAM_ADV_INT_MAX_4, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_4, PARAM_ADV_INT_MAX_4, NULL)},
	{	// group 5
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_5, PARAM_ADV_INT_MAX_5, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_5, PARAM_ADV_INT_MAX_5, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_5, PARAM_ADV_INT_MAX_5, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_5, PARAM_ADV_INT_MAX_5, NULL)},
	{	// group 6
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_6, PARAM_ADV_INT_MAX_6, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_6, PARAM_ADV_INT_MAX_6, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_6, PARAM_ADV_INT_MAX_6, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_6, PARAM_ADV_INT_MAX_6, NULL)},
	{	// group 7
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_7, PARAM_ADV_INT_MAX_7, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_7, PARAM_ADV_INT_MAX_7, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_7, PARAM_ADV_INT_MAX_7, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_7, PARAM_ADV_INT_MAX_7, NULL)},
	{	// group 8
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_8, PARAM_ADV_INT_MAX_8, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_8, PARAM_ADV_INT_MAX_8, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_8, PARAM_ADV_INT_MAX_8, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_8, PARAM_ADV_INT_MAX_8, NULL)},
	{	// group 9
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_9, PARAM_ADV_INT_MAX_9, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_9, PARAM_ADV_INT_MAX_9, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_9, PARAM_ADV_INT_MAX_9, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_9, PARAM_ADV_INT_MAX_9, NULL)},
	{	// group 10
		//IDX_0, IDX_1, IDX_2, IDX_3
		 FIXED_BT_LE_ADV_PARAM(1,ADV_OPT_IDX_0, PARAM_ADV_INT_MIN_10, PARAM_ADV_INT_MAX_10, NULL)
		,FIXED_BT_LE_ADV_PARAM(2,ADV_OPT_IDX_1, PARAM_ADV_INT_MIN_10, PARAM_ADV_INT_MAX_10, NULL)
		,FIXED_BT_LE_ADV_PARAM(3,ADV_OPT_IDX_2, PARAM_ADV_INT_MIN_10, PARAM_ADV_INT_MAX_10, NULL)
		,FIXED_BT_LE_ADV_PARAM(4,ADV_OPT_IDX_3, PARAM_ADV_INT_MIN_10, PARAM_ADV_INT_MAX_10, NULL)}
};

static const struct bt_le_ext_adv_start_param p_adv_default_start_param[]=BT_LE_EXT_ADV_START_DEFAULT;
static const struct bt_le_ext_adv_start_param p_adv_finit_start_param[]=BT_LE_EXT_ADV_START_PARAM(300,0);
static const struct bt_le_ext_adv_start_param p_adv_1sec_start_param[]=BT_LE_EXT_ADV_START_PARAM(100,0);
static const struct bt_le_ext_adv_start_param p_adv_5sec_start_param[]=BT_LE_EXT_ADV_START_PARAM(500,0);
static const struct bt_le_ext_adv_start_param p_adv_burst_start_param[]=BT_LE_EXT_ADV_START_PARAM(0,LOSS_TEST_BURST_COUNT);
static const uint8_t p_common_adv_flags[]={BT_LE_AD_NO_BREDR/*|BT_LE_AD_GENERAL*/};

typedef struct __attribute__((__packed__)) {
	uint16_t  node;
	uint8_t  pri_phy;
	uint8_t  sec_phy;
	int8_t   tx_pwr;
	uint16_t flow;
	uint16_t subtotal;
	int16_t  rssi;
	int16_t  rssi_upper;
	int16_t  rssi_lower;
	unsigned det_sender:1;
	unsigned dump_rcvinfo:1;
	unsigned complete:1;
	unsigned notified:1;
} RCV_RECORD;

typedef struct __attribute__((__packed__)) {
	RCV_RECORD rec;
	int rssi_acc;
	int rssi_idx;
} RCV_STAMP;

typedef union {
	uint8_t u8_val;
	struct __attribute__((__packed__)) {
		unsigned initialzed:1;
		unsigned update_param:1;
		unsigned set_data:1;
		unsigned start:1;
		unsigned stop:1;
	};
} EXT_ADV_STATUS;

typedef struct __attribute__((__packed__)) {
	int64_t expired_tm;
	int8_t rssi;
} REC_RSSI_STAMP;

typedef struct __attribute__((__packed__)) {
	uint16_t man_id;  // 0xFFFF
	uint16_t form_id; // 0
	int16_t  pre_cnt; // 
	uint16_t flw_cnt; // 10 hrtbt/min
	struct __attribute__((scalar_storage_order("big-endian"))) {
		uint64_t eui_64 ;
	} ;
} DEVICE_INFO_ST;

typedef struct __attribute__((__packed__)) {
	uint16_t man_id;  // 0xFFFF
	uint16_t form_id; // 0
	int16_t  pre_cnt; // 
	uint16_t flw_cnt; // 10 hrtbt/min
	struct __attribute__((scalar_storage_order("big-endian"))) {
		uint32_t eui_64 ;
	} ;
} DEVICE_INFOlet_ST;

typedef struct __attribute__((__packed__)) {
	DEVICE_INFO_ST device_info;
	uint8_t tail[10];
} DEVICE_INFO_BTv4_ST;

typedef struct __attribute__((__packed__)) {
	DEVICE_INFOlet_ST device_info;
	uint8_t tail[10];
} DEVICE_INFOlet_BTv4_ST;

typedef struct __attribute__((__packed__)) {
	uint16_t man_id;  // 0xFFFF
	uint16_t form_id; // 0
	uint16_t number_cast_form[4];
} NUMCAST_INFO_ST;

typedef struct __attribute__((packed)) {
	int8_t sv;
	int8_t pv;
} SV_PV_PWR_ST;

struct DEV_FOUND_PARM_ST {
	uint16_t flw_cnt;
	union {
		struct {
			unsigned  step_flag :2;
			unsigned  step_special_stream :3;
			unsigned  step_devnm :2;
			unsigned  :7;
			unsigned  step_fail :1;
			unsigned  step_success :1;
		};
		struct {
			unsigned :14;
			unsigned step_completed:2;
		};
		uint16_t step_raw;
	};
	struct bt_hci_evt_le_ext_advertising_info * adv_info_p;
	void * temp_ptr;
} static dev_chr;

static int8_t losstst_task_status(int8_t TGR_VAL) __attribute__((__noinline__));
static void env_rssi_calc(void) __attribute__((__noinline__));
static void numcst_rssi_calc(int64_t) __attribute__((__noinline__));
bool get_cfg_phy_sel(uint8_t idx) __attribute__((__noinline__));
static void numcast_packet_evt(uint8_t idx, DEVICE_INFO_ST * form_p, uint64_t * numcast_p, int8_t rssi) __attribute__((__noinline__));
static void rssi_avg_procedure(RCV_STAMP * stamp_p, int16_t rssi_val) __attribute__((__noinline__));
static void rssi_idx_init(RCV_STAMP * stamp_p, int16_t rssi_val) __attribute__((__noinline__));
static char * rssi_toa(int16_t rssi,char * str_p);
static char * txpwr_toa(int8_t pwr,char * str_p);
int update_adv(uint8_t index , const struct bt_le_adv_param *adv_parm , struct bt_data *adv_data , const struct bt_le_ext_adv_start_param *adv_start_param);
static void loss_tst_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info);
static void sender_peek_msg(void);
static void scanner_peek_msg(void);
static void passive_scan_method(int8_t); //0:phy_all, 1:phy_1m, 2:,phy_coded
extern bool rc_msg_incomming(void *,size_t,bt_addr_le_t *);
extern bool rm_msg_incomming(void *,size_t,bt_addr_le_t *);
bool rc_msg_outgoing(void *,size_t);
bool rc_rush_msg_outgoing(void *,size_t);



static const struct bt_le_ext_adv_cb private_adv_cb = {
	.sent = loss_tst_sent_cb
};
static const uint16_t enum_total_num[]={500,1000,2000,5000,10000,20000,50000};
static REC_RSSI_STAMP numcst_rssi_rec[32];
static REC_RSSI_STAMP env_rssi_rec[4][256];
static uint8_t numcst_rssi_idx;
static uint16_t env_rssi_idx[4];
static bool svc_init_success;
static uint8_t num_adv_set;
static RCV_RECORD rec_sets[4];
static RCV_STAMP rcv_stamp[4];
static EXT_ADV_STATUS ext_adv_status[5 /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/]={{.u8_val=0},{.u8_val=0},{.u8_val=0},{.u8_val=0},{.u8_val=0}};//{0,0,0,0};

static bool sndr_abort_flag[4]={false,false,false,false};
static char rssi_str[3][5];
static char tx_pwr_str[5];
static uint16_t sub_total_snd_1m;
static uint16_t sub_total_snd_2m;
static uint16_t sub_total_snd_s8;
static uint16_t sub_total_snd_ble4;
static uint16_t sub_total_rcv[4];
static uint64_t number_cast_val;
static uint64_t number_cast_rxval;
static bool number_cast_auto;
static int64_t numcst_phy_stamp_tm[4];
static uint8_t numcst_src_node[2];
static int16_t precnt_rcv[4];
static uint16_t round_total_num;
static int8_t remote_tx_pwr[4];
static int8_t round_tx_pwr;
static uint8_t round_adv_param_index;
static bool scanner_inactive;
static int8_t peek_rcv_rssi[4][3]={{INT8_MAX,INT8_MAX,INT8_MAX},{INT8_MAX,INT8_MAX,INT8_MAX},{INT8_MAX,INT8_MAX,INT8_MAX},{INT8_MAX,INT8_MAX,INT8_MAX}};

static volatile bool ack_remote_resp[4];
static char peek_msg_str[4][64];
static uint32_t adv_param_mask[2];

#define ADV_DBG_LOG_ENABLE 1
#define ADV_DBG_ONLY_IDX0 1

#if ADV_DBG_LOG_ENABLE
static uint32_t adv_dbg_last_options[5] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
static uint8_t adv_dbg_last_ad_len[5] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const char *adv_dbg_last_tag[5] = { NULL, NULL, NULL, NULL, NULL };

static const char *adv_dbg_phy_name(uint8_t index)
{
	if (index == 0) return "1M/2M";
	if (index == 1) return "1M";
	if (index == 2) return "S8";
	if (index == 3) return "BLE4";
	if (index == 4) return "PEEK";
	return "?";
}

static uint8_t adv_dbg_compute_ad_len(struct bt_data *adv_data)
{
	uint8_t ad_len = 0;

	if (NULL == adv_data) return 0;
	while (0 != (adv_data + ad_len)->data_len) {
		ad_len++;
	}

	return ad_len;
}

static const char *adv_dbg_pdu_hint(uint32_t options, uint8_t ad_len)
{
	if (options & BT_LE_ADV_OPT_ANONYMOUS) return "extended";
	if (options & BT_LE_ADV_OPT_CODED) return "extended";
	if (options & BT_LE_ADV_OPT_NO_2M) return "extended";
	if (!(options & BT_LE_ADV_OPT_EXT_ADV)) return "legacy";
	if (ad_len > 2) return "extended";
	return "legacy_or_extended";
}

static void adv_dbg_log_options_if_changed(const char *tag, uint8_t index, uint32_t options, struct bt_data *adv_data)
{
	#if ADV_DBG_ONLY_IDX0
	if (index != 0) return;
	#endif

	if (index >= ARRAY_SIZE(adv_dbg_last_options)) return;

	uint8_t ad_len = adv_dbg_compute_ad_len(adv_data);
	if (adv_dbg_last_options[index] == options
		&& adv_dbg_last_ad_len[index] == ad_len
		&& adv_dbg_last_tag[index] == tag) return;

	adv_dbg_last_options[index] = options;
	adv_dbg_last_ad_len[index] = ad_len;
	adv_dbg_last_tag[index] = tag;
	const char *pdu_hint = adv_dbg_pdu_hint(options, ad_len);
	bool pdu_is_extended = (options & BT_LE_ADV_OPT_ANONYMOUS)
		|| (options & BT_LE_ADV_OPT_CODED)
		|| (options & BT_LE_ADV_OPT_NO_2M)
		|| ((options & BT_LE_ADV_OPT_EXT_ADV) && (ad_len > 2));

	printf("[ADVDBG] %s idx=%u phy=%s opt=0x%08lx ext=%u anon=%u id=%u no2m=%u coded=%u ad_items=%u pdu_hint=%s\n",
		tag,
		index,
		adv_dbg_phy_name(index),
		(unsigned long)options,
		(options & BT_LE_ADV_OPT_EXT_ADV) ? 1u : 0u,
		(options & BT_LE_ADV_OPT_ANONYMOUS) ? 1u : 0u,
		(options & BT_LE_ADV_OPT_USE_IDENTITY) ? 1u : 0u,
		(options & BT_LE_ADV_OPT_NO_2M) ? 1u : 0u,
		(options & BT_LE_ADV_OPT_CODED) ? 1u : 0u,
		ad_len,
		pdu_hint);

	if (!pdu_is_extended) {
		printf("[ADVWARN] %s idx=%u phy=%s opt=0x%08lx pdu_hint=%s (possible legacy path)\n",
			tag,
			index,
			adv_dbg_phy_name(index),
			(unsigned long)options,
			pdu_hint);
	}
}
#endif

static bool cfg_phy_sel[4]={true,false,false,false};
static bool cfg_inhibit_ch37,cfg_inhibit_ch38,cfg_inhibit_ch39;
static bool cfg_non_ANONYMOUS;
static int8_t cfg_interval_sel_idx;
static int8_t cfg_totalnum_sel_idx;
static bool uni_cast_method;

static bool ignore_rcv_resp;
static bool round_phy_sel[4]={false,false,false,false};
static bool inhibit_ch37,inhibit_ch38,inhibit_ch39;
static bool non_ANONYMOUS;


typedef bool (*envmon_task_abort)(void);
typedef bool (*sender_task_abort)(void);
typedef bool (*scanner_task_abort)(void);
typedef bool (*numcast_task_abort)(void);
//typedef void (*txpower_setup)(int8_t);
static envmon_task_abort envmon_abort_p;
static sender_task_abort sender_abort_p;
static scanner_task_abort scanner_abort_p;
static numcast_task_abort numcast_abort_p;
//static txpower_setup txpower_setup_p;


// scr input val
static uint16_t xmt_ratio_val[4][2];
static uint16_t rcv_ratio_val[4][2];
static int8_t rcv_rssi_val[4][3];
static int8_t rcv_state_val[4];
static int8_t snd_state_val[4];
static uint32_t rcv_stats[4];
static uint32_t env_stats[4];
static uint16_t sndr_id;
static int8_t sndr_txpower;



const static char *adv_typ[]={
	(char *){"ADV_IND"},        // Scannable and connectable advertising.                      BT_GAP_ADV_TYPE_ADV_IND         = 0x00 
	(char *){"ADV_DIRECT_IND"}, // Directed connectable advertising.                           BT_GAP_ADV_TYPE_ADV_DIRECT_IND  = 0x01 
	(char *){"ADV_SCAN_IND"},   // Non-connectable and scannable advertising.                  BT_GAP_ADV_TYPE_ADV_SCAN_IND    = 0x02 
	(char *){"ADV_NONCONN_IND"},// Non-connectable and non-scannable advertising.              BT_GAP_ADV_TYPE_ADV_NONCONN_IND = 0x03 
	(char *){"SCAN_RSP"},       // Additional advertising data requested by an active scanner. BT_GAP_ADV_TYPE_SCAN_RSP        = 0x04 
	(char *){"EXT_ADV"},        // Extended advertising, see advertising properties.           BT_GAP_ADV_TYPE_EXT_ADV         = 0x05 
	(char *){"UNKNOWN"}
};

const static char *pri_phy_typ[]={
	(char *){"NA"},  // 0x00 
	(char *){"1M"},  // 0x01
	(char *){"NA"},  // 0x02 
	(char *){"S8"},  // 0x03 
	(char *){"S2"},  // 0x04 
	(char *){"NA"}   // 0x05 
};

const static char *sec_phy_typ[]={
	(char *){"NA"},  // 0x00 
	(char *){"1M"},  // 0x01 
	(char *){"2M"},  // 0x02 
	(char *){"S8"},  // 0x03 
	(char *){"S2"},  // 0x04 
	(char *){"NA"}   // 0x05 
};

const static char *le_addr_sub_type[]={
	(char *){"NRES_PRI"}, // 0x0 Non-resolvable private address
	(char *){"RES_PRI"},  // 0x1 Resolvable private address
	(char *){"UNKNOWN"},  // 0x2 Reserved for future use
	(char *){"STATIC"},   // 0x3 Static device address
};

const static char *peek_sndpkt_btv4_form=(const char *){"\xff\xffSND:%03u P:%s%s R:%u/%u T:%d"};
const static char *peek_sndpkt_form=(const char *){"\xff\xffSND:%03u P:%s/%s R:%u/%u T:%d"};
const static char *peek_rcvpkt_btv4_form=(const char *){"\xff\xffRCV:%03u P:%s%s R:%u/%u S:%s(%s..%s) T:%s"};
const static char *peek_rcvpkt_form=(const char *){"\xff\xffRCV:%03u P:%s/%s R:%u/%u S:%s(%s..%s) T:%s"};
const static char *log_rcvpkt_form=(const char *){"RCV:%03u P:%s/%s R:%d/%u S:%s(%s..%s) T:%s"};
const static char *log_sender_form=(const char *){"SENDER:%03u P:%s/%s R:%d/%u S:%s(%s..%s) T:%s"};

static unsigned char adv_dev_nm[5 /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/][1+CONFIG_BT_DEVICE_NAME_MAX];
static NUMCAST_INFO_ST numcast_info_form={MANUFACTURER_ID,LOSS_TEST_FORM_ID};
static uint16_t * const p_number_cast_form=(uint16_t *)&numcast_info_form.number_cast_form;

static volatile DEVICE_INFO_ST device_info_form[4]={
	{MANUFACTURER_ID,LOSS_TEST_FORM_ID,INT16_MIN,255},
	{MANUFACTURER_ID,LOSS_TEST_FORM_ID,INT16_MIN,255},
	{MANUFACTURER_ID,LOSS_TEST_FORM_ID,INT16_MIN,255},
	{MANUFACTURER_ID,LOSS_TEST_FORM_ID,INT16_MIN,255}};
static volatile DEVICE_INFO_BTv4_ST device_info_bt4_form={.device_info={MANUFACTURER_ID,LOSS_TEST_FORM_ID,INT16_MIN,255}};
static DEVICE_INFO_BTv4_ST numcast_bt4_form;
static DEVICE_INFO_ST remote_resp_form[4]={{.man_id=0},{.man_id=0},{.man_id=0},{.man_id=0}};

static struct bt_le_ext_adv *ext_adv[5 /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/];
static struct bt_data ratio_test_data_set[5 /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/][8];
static struct bt_data number_cast_data_set[2][4];
static struct bt_data resp_burst_end_data[4];
static struct bt_data remote_ctrl_data[4];

const int8_t sender_tgr=1;
const int8_t scanner_tgr=2;
const int8_t numcst_tgr=3;
const int8_t envmon_tgr=4;

static int8_t losstst_task_val;

static SV_PV_PWR_ST txpwr_setval[2][20];
static uint8_t txpwr_idx=ARRAY_SIZE(txpwr_setval[0]);
static int8_t env_rssi[4][3];
static int64_t numcst_rssi_rec_tm;
static int8_t numcst_rssi[3];

static bt_addr_le_t device_id_array[CONFIG_BT_ID_MAX];
static bool rc_party;
static bt_addr_le_t remote_ctrl_party;

static int8_t losstst_task_tgr(int8_t set,int8_t TGR_VAL)
{
	if(TGR_VAL!=losstst_task_val && 0==set) return 0;
	if(0==losstst_task_val && 0<set) losstst_task_val=TGR_VAL;
	else if(TGR_VAL==losstst_task_val && -TGR_VAL==set) losstst_task_val=0;
	return losstst_task_val;
}
static int8_t losstst_task_status(int8_t TGR_VAL)
{
	if(0==losstst_task_val) return 0; // idle
	if(TGR_VAL==losstst_task_val) return 1; // running
	return 2; // blocking	
}

int8_t numcst_task_tgr(int8_t set) { return losstst_task_tgr( set, numcst_tgr  ); }
int8_t numcst_task_status(void) { return losstst_task_status( numcst_tgr ); }
int8_t scanner_task_tgr(int8_t set){ return losstst_task_tgr( set, scanner_tgr ); }
int8_t scanner_task_status(void){ return losstst_task_status( scanner_tgr); }
int8_t sender_task_tgr(int8_t set) { return losstst_task_tgr( set, sender_tgr  ); }
int8_t sender_task_status(void) { return losstst_task_status( sender_tgr ); }
int8_t envmon_task_tgr(int8_t set) { return losstst_task_tgr( set, envmon_tgr  ); }
int8_t envmon_task_status(void) { return losstst_task_status( envmon_tgr ); }

static int enum_txpwr_pv_sort(const SV_PV_PWR_ST * parma, const SV_PV_PWR_ST * parmb)
{
	return (parmb->pv - parma->pv);
}
static int enum_txpwr_sv_sort(const SV_PV_PWR_ST * parma, const SV_PV_PWR_ST * parmb)
{
	return (parmb->sv - parma->sv);
}
static void init_txpwr_setval(void)
{
	txpwr_idx=0;
	uint8_t idx=0;
	memset(txpwr_setval,INT8_MIN,sizeof(txpwr_setval));
	sdc_hci_cmd_vs_zephyr_write_tx_power_t param_txpwr;
	sdc_hci_cmd_vs_zephyr_write_tx_power_return_t return_txpwr;
	param_txpwr=(sdc_hci_cmd_vs_zephyr_write_tx_power_t)
		{.handle_type=SDC_HCI_VS_TX_POWER_HANDLE_TYPE_ADV,
		.handle=0,
		.tx_power_level=CONFIG_BT_CTLR_TX_PWR_ANTENNA};
	do {
		sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
		if(param_txpwr.tx_power_level<return_txpwr.selected_tx_power) break;
	  #if(0)
		if(return_txpwr.selected_tx_power==param_txpwr.tx_power_level) {
			idx++;
			txpwr_setval[0][idx].pv=return_txpwr.selected_tx_power;
			txpwr_setval[0][idx].sv=param_txpwr.tx_power_level;
		}
	  #else
		if(txpwr_setval[0][idx].pv!=return_txpwr.selected_tx_power) {
			idx++;
			txpwr_setval[0][idx].pv=return_txpwr.selected_tx_power;
			txpwr_setval[0][idx].sv=param_txpwr.tx_power_level;
		} else {
			txpwr_setval[0][idx].sv=param_txpwr.tx_power_level;
		}
	  #endif
		param_txpwr.tx_power_level--;
	} while (idx<(ARRAY_SIZE(txpwr_setval[0])-1));
	memcpy(txpwr_setval[1],txpwr_setval[0],sizeof(txpwr_setval[0]));
	qsort(txpwr_setval[0],idx+1,sizeof(SV_PV_PWR_ST), (__compar_fn_t)enum_txpwr_pv_sort );
	qsort(txpwr_setval[1],idx+1,sizeof(SV_PV_PWR_ST), (__compar_fn_t)enum_txpwr_sv_sort );
}

int8_t get_txpower_sv(int8_t process_value)
{
	if(txpwr_idx>=ARRAY_SIZE(txpwr_setval[0])) init_txpwr_setval();
	uint8_t idx=0;
	while(txpwr_setval[0][idx].pv>process_value) idx++;
	if(INT8_MIN==txpwr_setval[0][idx].pv) idx--;
	return txpwr_setval[0][idx].sv;
}

int8_t get_txpower_pv(int8_t set_value)
{
	if(txpwr_idx>=ARRAY_SIZE(txpwr_setval[0])) init_txpwr_setval();
	uint8_t idx=0;
	while(txpwr_setval[1][idx].sv>set_value) idx++;
	if(INT8_MIN==txpwr_setval[1][idx].sv) idx--;
	return txpwr_setval[1][idx].pv;
}

int8_t get_txpower_effect(int8_t tx_power_level)
{
	if(txpwr_idx>=ARRAY_SIZE(txpwr_setval[0])) init_txpwr_setval();
	uint8_t idx=0;
	while(txpwr_setval[0][idx].pv>tx_power_level) idx++;
	if(INT8_MIN==txpwr_setval[0][idx].pv) idx--;
	return txpwr_setval[0][idx].pv;
}

int8_t enum_txpower(int8_t dir)
{
	if(txpwr_idx>=ARRAY_SIZE(txpwr_setval[0])) init_txpwr_setval();

	if(0<dir) {
		if(INT8_MAX==dir) txpwr_idx=0;
		else if(0>((int8_t)(txpwr_idx=txpwr_idx-1))) {
			uint8_t idx=0;
			while(txpwr_setval[0][idx].pv!=INT8_MIN) idx++;
			txpwr_idx=idx-1;
		}
		//printf("LN%u, txpwr_idx %d\n",__LINE__,txpwr_idx);
	}
	else if(0>dir) { 
		if(INT8_MIN==dir) {
			uint8_t idx=0;
			while(txpwr_setval[0][idx].pv!=INT8_MIN) idx++;
			txpwr_idx=idx-1;
		}
		else if(INT8_MIN==txpwr_setval[0][++txpwr_idx].pv) txpwr_idx=0;
		//printf("LN%u, txpwr_idx %d\n",__LINE__,txpwr_idx);
	}
	
	return txpwr_setval[0][txpwr_idx].pv;
}


uint8_t enum_adv_interval_idx(int8_t dir) // index func( 1:next/ -1:previous/ 0:current)
{
	if(0<dir) cfg_interval_sel_idx++;
	else if(0>dir) cfg_interval_sel_idx--;

	if(cfg_interval_sel_idx>=(int8_t)ARRAY_SIZE(value_interval))
		cfg_interval_sel_idx=(INT8_MAX==dir)?(ARRAY_SIZE(value_interval)-1):0;
	else if(0>cfg_interval_sel_idx)
		cfg_interval_sel_idx=(INT8_MIN==dir)?0:(ARRAY_SIZE(value_interval)-1);
	
	return cfg_interval_sel_idx;
}
uint16_t adv_interval_upper(uint8_t idx)
{
	return (idx<ARRAY_SIZE(value_interval))?value_interval[idx][1]:UINT16_MAX;
}

uint16_t adv_interval_lower(uint8_t idx)
{
	return (idx<ARRAY_SIZE(value_interval))?value_interval[idx][0]:UINT16_MAX;
}


uint16_t xmt_ratio_lower(uint8_t idx) // cnt func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?xmt_ratio_val[idx][0]:0;
}
uint16_t xmt_ratio_upper(uint8_t idx) // cnt func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?xmt_ratio_val[idx][1]:0;
}

uint16_t rcv_ratio_lower(uint8_t idx) // cnt func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?rcv_ratio_val[idx][0]:0;
}
uint16_t rcv_ratio_upper(uint8_t idx) // cnt func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?rcv_ratio_val[idx][1]:0;
}

int8_t rcv_rssi_average(uint8_t idx) // rssi func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?rcv_rssi_val[idx][0]:0;
}
int8_t rcv_rssi_lower(uint8_t idx) // rssi func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?rcv_rssi_val[idx][1]:0;
}
int8_t rcv_rssi_upper(uint8_t idx) // rssi func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?rcv_rssi_val[idx][2]:0;
}


static void env_rssi_calc(void)
{
	int64_t expire_tm=k_uptime_get();
	for(int phy_idx=0; phy_idx<ARRAY_SIZE(env_rssi_rec); phy_idx++) {
		int16_t cnt=0;
		int32_t avg=0;
		int8_t lower=20;
		int8_t upper=-127;
		for(int idx=0; idx<ARRAY_SIZE(env_rssi_rec[0]); idx++) {
			int64_t rec_tm=env_rssi_rec[phy_idx][idx].expired_tm;
			int8_t rec_rssi=env_rssi_rec[phy_idx][idx].rssi;
			if(expire_tm<=rec_tm) {
				avg=avg+rec_rssi, cnt=cnt+1;
				lower=MIN(lower,rec_rssi);
				upper=MAX(upper,rec_rssi);
			}
		}
		if(cnt) avg/=cnt;
		env_rssi[phy_idx][0]=avg;
		env_rssi[phy_idx][1]=(20==lower)?0:lower;
		env_rssi[phy_idx][2]=(-127==upper)?0:upper;
	}
}
int8_t envmon_rssi_average(uint8_t idx)
{
	return (idx>=ARRAY_SIZE(env_rssi))?0:env_rssi[idx][0];
}
int8_t envmon_rssi_lower(uint8_t idx)
{
	return (idx>=ARRAY_SIZE(env_rssi))?0:env_rssi[idx][1];
}
int8_t envmon_rssi_upper(uint8_t idx)
{
	return (idx>=ARRAY_SIZE(env_rssi))?0:env_rssi[idx][2];
}


static void numcst_rssi_calc(int64_t tm_stamp)
{
	if(abs(numcst_rssi_rec_tm-tm_stamp)<50) return;
	numcst_rssi_rec_tm=tm_stamp;
	int8_t cnt=0;
	int16_t avg=0;
	int8_t lower=20;
	int8_t upper=-127;
	for(int idx=0; idx<ARRAY_SIZE(numcst_rssi_rec); idx++) {
		int64_t rec_tm=numcst_rssi_rec[idx].expired_tm;
		int8_t rec_rssi=numcst_rssi_rec[idx].rssi;
		if(numcst_rssi_rec_tm<=rec_tm) {
			avg=avg+rec_rssi, cnt=cnt+1;
			lower=MIN(lower,rec_rssi);
			upper=MAX(upper,rec_rssi);
		}
	}
	if(cnt) avg/=cnt;
	numcst_rssi[0]=avg;
	numcst_rssi[1]=(20==lower)?0:lower;
	numcst_rssi[2]=(-127==upper)?0:upper;
}
int8_t numcst_rssi_average(void)
{
	return numcst_rssi[0];
}
int8_t numcst_rssi_lower(void)
{
	return numcst_rssi[1];
}
int8_t numcst_rssi_upper(void)
{
	return numcst_rssi[2];
}
int16_t numcst_setval(uint8_t field, int16_t setval)
{
	int16_t retval;
	if(4<=field) retval=INT16_MIN;
	else if(0>setval) {
		retval=*(field+p_number_cast_form);
	} else if(1000>setval) {
		retval=*(field+p_number_cast_form)=setval;
	} else {
		if(1000<=(retval=*(field+p_number_cast_form)+(setval-1000))) retval-=1000;
		*(field+p_number_cast_form)=retval;
	}
	return retval;
}
int16_t numcst_rxval(uint8_t field)
{
	uint16_t *number_cast_rx=(uint16_t *)&number_cast_rxval;
	return *(field+number_cast_rx);
}

int8_t rcv_state_mark(uint8_t idx) // mark func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?rcv_state_val[idx]:0;
}
int8_t rcv_state_progress(uint8_t idx) // mark func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	int8_t retval=(4>idx)?rcv_state_val[idx]:0;
	int16_t progress=precnt_rcv[idx];
	if(1==retval && -1==progress) retval=2;
	return retval;
}
int8_t snd_state_mark(uint8_t idx) // mark func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?snd_state_val[idx]:0;
}

uint32_t rcv_stats_val(uint8_t idx) // stats-cumulate func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?rcv_stats[idx]:0;
}
uint32_t env_stats_val(uint8_t idx) // stats-cumulate func(0:1m-2m / 1:1m-1m / 2:s8-s8 / 3:blev4)
{
	return (4>idx)?env_stats[idx]:0;
}
void env_stats_clr(void)
{
	memset(env_stats,0,sizeof(env_stats));
	memset(env_rssi,0,sizeof(env_rssi));
	memset(env_rssi_rec,0,sizeof(env_rssi_rec));
}

uint16_t sender_id(void)
{
	return sndr_id;
}
uint8_t sender_id_upper(void)
{
	return 0xFF&(sndr_id>>8);
}
uint8_t sender_id_lower(void)
{
	return sndr_id;
}

int8_t sender_txpower(void)
{
	return sndr_txpower;
}

extern int (*func_dcdc)(int);
int chg_soc_dcdc(void)
{
	if(NULL!=func_dcdc) {
		if(0<func_dcdc(0)) func_dcdc(-1); else func_dcdc(1);
		return func_dcdc(0);
	}
	return -1;
}
int get_soc_dcdc(void)
{
	if(NULL!=func_dcdc) return func_dcdc(0);
	return 2;
}

bool get_uni_cast_method(void)
{
	return (uni_cast_method)?1:0;
}
void chg_uni_cast_method(void)
{
	uni_cast_method=!uni_cast_method;
}

bool get_number_cast_auto(void)
{
	return (number_cast_auto)?1:0;
}
bool chg_number_cast_auto(void)
{
	number_cast_auto=!number_cast_auto;
	return (number_cast_auto)?1:0;
}

uint8_t numcst_src_id_upper(void)
{
	return numcst_src_node[0];
}
uint8_t numcst_src_id_lower(void)
{
	return numcst_src_node[1];
}

uint8_t enum_totalnum_idx(int8_t dir)
{
	if(0<dir) cfg_totalnum_sel_idx++;
	else if(0>dir) cfg_totalnum_sel_idx--;

	if(cfg_totalnum_sel_idx>=(int8_t)ARRAY_SIZE(enum_total_num))
		cfg_totalnum_sel_idx=(INT8_MAX==dir)?(ARRAY_SIZE(enum_total_num)-1):0;
	else if(0>cfg_totalnum_sel_idx)
		cfg_totalnum_sel_idx=(INT8_MIN==dir)?0:(ARRAY_SIZE(enum_total_num)-1);
	
	return cfg_totalnum_sel_idx;
}
uint16_t enum_totalnum(int8_t dir)
{
	return enum_total_num[enum_totalnum_idx(dir)];
}

void chk_cfg_phy_sel(void)
{
	if(!cfg_phy_sel[0] && !cfg_phy_sel[1] && !cfg_phy_sel[2]) cfg_phy_sel[0]=true;
}
bool get_cfg_phy_sel(uint8_t idx)
{
	chk_cfg_phy_sel();
	if(3!=idx && cfg_phy_sel[3]) return false;
    return (cfg_phy_sel[idx])?1:0;
}
bool chg_cfg_phy2m(void)
{
	cfg_phy_sel[0]=!cfg_phy_sel[0];
	return get_cfg_phy_sel(0);
}
bool chg_cfg_phy1m(void)
{
	cfg_phy_sel[1]=!cfg_phy_sel[1];
	return get_cfg_phy_sel(1);
}
bool chg_cfg_phy8s(void)
{
	cfg_phy_sel[2]=!cfg_phy_sel[2];
	return get_cfg_phy_sel(2);
}
bool chg_cfg_phyBLEv4(void)
{
	cfg_phy_sel[3]=!cfg_phy_sel[3];
	return get_cfg_phy_sel(3);
}
bool get_cfg_phy2m(void)
{
	return get_cfg_phy_sel(0);
}
bool get_cfg_phy1m(void)
{
	return get_cfg_phy_sel(1);
}
bool get_cfg_phy8s(void)
{
	return get_cfg_phy_sel(2);
}
bool get_cfg_phyBLEv4(void)
{
	return get_cfg_phy_sel(3);
}
bool chg_cfg_ch37(void)
{
	cfg_inhibit_ch37=!cfg_inhibit_ch37;
	if(cfg_inhibit_ch37 && cfg_inhibit_ch38 && cfg_inhibit_ch39) cfg_inhibit_ch39=false;
	return (!cfg_inhibit_ch37)?1:0;
}
bool chg_cfg_ch38(void)
{
	cfg_inhibit_ch38=!cfg_inhibit_ch38;
	if(cfg_inhibit_ch37 && cfg_inhibit_ch38 && cfg_inhibit_ch39) cfg_inhibit_ch39=false;
	return (!cfg_inhibit_ch38)?1:0;
}
bool chg_cfg_ch39(void)
{
	cfg_inhibit_ch39=!cfg_inhibit_ch39;
	if(cfg_inhibit_ch37 && cfg_inhibit_ch38 && cfg_inhibit_ch39) cfg_inhibit_ch39=false;
	return (!cfg_inhibit_ch39)?1:0;
}
bool get_cfg_ch37(void)
{
	return (!cfg_inhibit_ch37)?1:0;
}
bool get_cfg_ch38(void)
{
	return (!cfg_inhibit_ch38)?1:0;
}
bool get_cfg_ch39(void)
{
	return (!cfg_inhibit_ch39)?1:0;
}
bool chg_cfg_ANONYMOUS(void)
{
	cfg_non_ANONYMOUS=!cfg_non_ANONYMOUS;
	return (!cfg_non_ANONYMOUS && !get_cfg_phyBLEv4())?1:0;
}
bool get_cfg_ANONYMOUS(void)
{
	return (!cfg_non_ANONYMOUS && !get_cfg_phyBLEv4())?1:0;
}
bool get_cfg_NON_ANONYMOUS(void)
{
	return (cfg_non_ANONYMOUS || get_cfg_phyBLEv4())?1:0;
}
uint8_t node_id_upper(void)
{
	return *(unsigned char *)(1+(unsigned char *)(NRF_FICR->DEVICEADDR));
}
uint8_t node_id_lower(void)
{
	return *(unsigned char *)(NRF_FICR->DEVICEADDR);
}


static void loss_tst_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info)
{
	uint8_t index=bt_le_ext_adv_get_index(adv);

	if(num_adv_set /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/>index) {
		ext_adv_status[index].stop=1;
		if(ARRAY_SIZE(sndr_abort_flag)>index && sndr_abort_flag[index]) {
			sndr_abort_flag[index]=false;
			if(200>=device_info_form[index].flw_cnt)
				device_info_form[index].flw_cnt*=LOSS_TEST_BURST_COUNT;
			else
				device_info_form[index].flw_cnt=256;
			
			if(3==index)
				device_info_bt4_form.device_info=device_info_form[3];
			
			update_adv(index,NULL,ratio_test_data_set[index],p_adv_5sec_start_param);
		}
		if(/*rc_party &&*/ 4==index) {
			extern void rc_msg_out_cb(void);
			rc_msg_out_cb();
		}
	}
}


void blocking_adv(uint8_t index)
{
  #if(CHK_UPDATE_ADV_PROCDURE)
	int err=
  #endif
	bt_le_ext_adv_stop(ext_adv[index]);
  #if(CHK_UPDATE_ADV_PROCDURE)
	if(err) printf("%s(%u) LN%u, err %d (%s)\n",__func__,index,__LINE__,err,strerror(-err));
  #endif
	ext_adv_status[index].u8_val=1;
}


int update_adv(uint8_t index , const struct bt_le_adv_param *adv_parm , struct bt_data *adv_data , const struct bt_le_ext_adv_start_param *adv_start_param)
{
    if(!svc_init_success) return -1;
	int retval=0;
    
	if(num_adv_set /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/<=index) return -EINVAL;
	if('\0'==*adv_dev_nm[0]) {
		sprintf(adv_dev_nm[0] , "LossTst(%03u)" , (unsigned char)NRF_FICR->DEVICEADDR[0]);
		sprintf(adv_dev_nm[1] , "LossTst(%03u)" , (unsigned char)NRF_FICR->DEVICEADDR[0]);
		sprintf(adv_dev_nm[2] , "LossTst(%03u)" , (unsigned char)NRF_FICR->DEVICEADDR[0]);
		sprintf(adv_dev_nm[3] , "LossTst%03u" , (unsigned char)NRF_FICR->DEVICEADDR[0]);
		sprintf(adv_dev_nm[4]
			, CONFIG_BT_DEVICE_NAME /*"Turnkey LossTest"*/ "(PEEK %03u)"
			, (unsigned char)NRF_FICR->DEVICEADDR[0]);
		
		*((uint64_t *)p_number_cast_form)=*((uint64_t *)NRF_FICR->DEVICEADDR);
		for(int idx=0; idx<ARRAY_SIZE(numcast_info_form.number_cast_form); idx++) p_number_cast_form[idx]%=1000;
		number_cast_data_set[0][0]=(struct bt_data){.type=BT_DATA_FLAGS,
												.data_len=1,
												.data=p_common_adv_flags};
		number_cast_data_set[0][1]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
												.data_len=sizeof(DEVICE_INFO_ST),
												.data=(const uint8_t *)&device_info_form[index]};
		number_cast_data_set[0][2]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,.data=(uint8_t *)&numcast_info_form,.data_len=sizeof(numcast_info_form)};

		number_cast_data_set[1][0]=(struct bt_data){.type=BT_DATA_FLAGS,
												.data_len=1,
												.data=p_common_adv_flags};
		number_cast_data_set[1][1]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
												.data_len=sizeof(DEVICE_INFO_BTv4_ST),
												.data=(const uint8_t *)&numcast_bt4_form};
	}

//	// if(1==CONFIG_BT_SETTINGS) ..... WEAR .. Flash Memory 
//	bt_set_name(adv_dev_nm[index]);
	if(0==ext_adv_status[index].initialzed) {
		if(3>=index) {
			bt_le_ext_adv_create(non_connectable_adv_param_x[3][index]
							, &private_adv_cb
							, &ext_adv[index]
							);
		} else {
			bt_le_ext_adv_create(non_connectable_adv_param_peek[0]
							, &private_adv_cb
							, &ext_adv[index]
							);
		}

		/*#include <host/hci_core.h>
		struct bt_le_ext_adv *adv_ptr=ext_adv[index];
		adv_ptr->id=index;*/
		ext_adv_status[index].initialzed=1, ext_adv_status[index].update_param=1;
	}

	if(/*3*/4>=index && NULL!=adv_parm) {
		if(ext_adv_status[index].update_param) bt_le_ext_adv_stop(ext_adv[index]);
	  #if ADV_DBG_LOG_ENABLE
		adv_dbg_log_options_if_changed("update_param", index, adv_parm->options, adv_data);
	  #endif
		
		int err=bt_le_ext_adv_update_param(ext_adv[index],adv_parm);
	  #if(CHK_UPDATE_ADV_PROCDURE)
		if(err) printf("%s LN%u, err %d (%s)\n",__func__,__LINE__,err,strerror(-err));
	  #endif
		if(0==retval && 0!=err) retval=err;
		ext_adv_status[index].update_param=1;
	} else if(3>=index && !ext_adv_status[index].update_param) {
		bt_le_ext_adv_update_param(ext_adv[index],non_connectable_adv_param_x[3][index]);
		ext_adv_status[index].update_param=1;
	} else if(4==index && !ext_adv_status[index].update_param) {
		int err=bt_le_ext_adv_update_param(ext_adv[index],non_connectable_adv_param_peek[0]);
	  #if(CHK_UPDATE_ADV_PROCDURE)
		if(err) printf("%s LN%u, err %d (%s)\n",__func__,__LINE__,err,strerror(-err));
	  #endif
		if(0==retval && 0!=err) retval=err;
		ext_adv_status[index].update_param=1;
	}

	if(NULL!=adv_data) {
		uint8_t ad_len=0;
		while(0!=(adv_data+ad_len)->data_len) {
			ad_len++;
		}
		
		if(num_adv_set /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/>index) {
			int err=bt_le_ext_adv_set_data(ext_adv[index], adv_data, ad_len, NULL, 0);
		  #if(CHK_UPDATE_ADV_PROCDURE)
			if(err) printf("%s LN%u, err %d (%s)\n",__func__,__LINE__,err,strerror(-err));
		  #endif
			if(0==retval && 0!=err) retval=err;
			ext_adv_status[index].set_data=1;
		}
	} else if(2>=index) {
		ratio_test_data_set[index][0]=(struct bt_data){.type=BT_DATA_FLAGS,
														.data_len=1,
														.data=p_common_adv_flags};
		ratio_test_data_set[index][1]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
														.data_len=sizeof(DEVICE_INFO_ST),
														.data=(const uint8_t *)&device_info_form[index]};
		ratio_test_data_set[index][2]=(struct bt_data){.type=BT_DATA_NAME_COMPLETE,.data=adv_dev_nm[index],.data_len=strlen(adv_dev_nm[index])};
		bt_le_ext_adv_set_data(ext_adv[index], ratio_test_data_set[index], 3, NULL, 0);
		ext_adv_status[index].set_data=1;
	} else if(3==index) {
		ratio_test_data_set[index][0]=(struct bt_data){.type=BT_DATA_FLAGS,
														.data_len=1,
														.data=p_common_adv_flags};
		ratio_test_data_set[index][1]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
														.data_len=sizeof(DEVICE_INFO_BTv4_ST),
														.data=(const uint8_t *)&device_info_bt4_form};
		memcpy((uint8_t *)device_info_bt4_form.tail,adv_dev_nm[3],sizeof(device_info_bt4_form.tail));
		bt_le_ext_adv_set_data(ext_adv[index], ratio_test_data_set[index], 2, NULL, 0);
		ext_adv_status[index].set_data=1;
	} else {
		ratio_test_data_set[4][0]=(struct bt_data){.type=BT_DATA_FLAGS,
														.data_len=1,
														.data=p_common_adv_flags};
		ratio_test_data_set[4][1]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
														.data_len=strlen(peek_msg_str[0]),
														.data=peek_msg_str[0]};
		ratio_test_data_set[4][2]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
														.data_len=strlen(peek_msg_str[1]),
														.data=peek_msg_str[1]};
		ratio_test_data_set[4][3]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
														.data_len=strlen(peek_msg_str[2]),
														.data=peek_msg_str[2]};
		ratio_test_data_set[4][4]=(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
														.data_len=strlen(peek_msg_str[3]),
														.data=peek_msg_str[3]};
		ratio_test_data_set[4][5]=(struct bt_data){.type=BT_DATA_NAME_COMPLETE,.data=adv_dev_nm[4],.data_len=strlen(adv_dev_nm[4])};
		bt_le_ext_adv_set_data(ext_adv[4], ratio_test_data_set[4], 6, NULL, 0);
		ext_adv_status[4].set_data=1;
	}
	
	if(ext_adv_status[index].start && ext_adv_status[index].stop) ext_adv_status[index].start=0, ext_adv_status[index].stop=0;

	if(0==ext_adv_status[index].start || NULL!=adv_start_param) {
		if(3>=index) {
			if(NULL!=adv_start_param)
				bt_le_ext_adv_start(ext_adv[index] , adv_start_param);
			else
				bt_le_ext_adv_start(ext_adv[index] , p_adv_default_start_param);
			
			ext_adv_status[index].start=1;
		} else {
			if(NULL!=adv_start_param) {
				int err=bt_le_ext_adv_start(ext_adv[index] , adv_start_param);
			  #if(CHK_UPDATE_ADV_PROCDURE)
				if(err) printf("%s LN%u, err %d (%s)\n",__func__,__LINE__,err,strerror(-err));
			  #endif
				if(0==retval && 0!=err) retval=err;
			}
			else {
				int err=bt_le_ext_adv_start(ext_adv[index] , p_adv_default_start_param);
			  #if(CHK_UPDATE_ADV_PROCDURE)
				if(err) printf("%s LN%u, err %d (%s)\n",__func__,__LINE__,err,strerror(-err));
			  #endif
				if(0==retval && 0!=err) retval=err;
			}

			ext_adv_status[index].start=1;
		}

	}
	
	return retval;
}


static void sender_peek_msg(void)
{
	sprintf(peek_msg_str[0],peek_sndpkt_form
						,(unsigned char)NRF_FICR->DEVICEADDR[0]
						,pri_phy_typ[1] ,sec_phy_typ[2]
						,sub_total_snd_2m, (round_phy_sel[0])?round_total_num:0
						,round_tx_pwr);
	*(uint16_t *)(peek_msg_str[0])=MANUFACTURER_ID;

	sprintf(peek_msg_str[1],peek_sndpkt_form
						,(unsigned char)NRF_FICR->DEVICEADDR[0]
						,pri_phy_typ[1] ,sec_phy_typ[1]
						,sub_total_snd_1m, (round_phy_sel[1])?round_total_num:0
						,round_tx_pwr);
	*(uint16_t *)(peek_msg_str[1])=MANUFACTURER_ID;

	sprintf(peek_msg_str[2],peek_sndpkt_form
						,(unsigned char)NRF_FICR->DEVICEADDR[0]
						,pri_phy_typ[3] ,sec_phy_typ[3]
						,sub_total_snd_s8, (round_phy_sel[2])?round_total_num:0
						,round_tx_pwr);
	*(uint16_t *)(peek_msg_str[2])=MANUFACTURER_ID;

	sprintf(peek_msg_str[3],peek_sndpkt_btv4_form
						,(unsigned char)NRF_FICR->DEVICEADDR[0]
						,"BLE" ,"v4"
						,sub_total_snd_ble4, (round_phy_sel[3])?round_total_num:0
						,round_tx_pwr);
	*(uint16_t *)(peek_msg_str[3])=MANUFACTURER_ID;
}


void sender_finit(void)
{
	struct bt_le_adv_param work_adv_param;

	for(int idx=0; idx<4 ;idx++) {
		if(round_phy_sel[idx]) {
			sndr_abort_flag[idx]=true;
			device_info_form[idx].pre_cnt=INT16_MAX;
			if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
			work_adv_param=*non_connectable_adv_param_x[3][idx];
			work_adv_param.options|=adv_param_mask[1];
			work_adv_param.options&=~adv_param_mask[0];
			update_adv(idx, &work_adv_param, ratio_test_data_set[idx], p_adv_finit_start_param);
		}
	}

	if(!rc_party) update_adv(4,NULL,NULL,NULL);
}


void sender_setup(struct TEST_PARM * parm_p)
{
    if(!svc_init_success) return;
	uint16_t total_num=enum_total_num[parm_p->count_idx];
	round_tx_pwr=parm_p->txpwr;
	round_adv_param_index=parm_p->interval_idx;
	round_phy_sel[0]=parm_p->phy_2m, round_phy_sel[1]=parm_p->phy_1m, round_phy_sel[2]=parm_p->phy_s8, round_phy_sel[3]=parm_p->phy_ble4;
	device_info_form[0].pre_cnt=INT16_MIN; device_info_form[0].flw_cnt=0;
	device_info_form[1].pre_cnt=INT16_MIN; device_info_form[1].flw_cnt=0;
	device_info_form[2].pre_cnt=INT16_MIN; device_info_form[2].flw_cnt=0;
	device_info_form[3].pre_cnt=INT16_MIN; device_info_form[3].flw_cnt=0;
	device_info_bt4_form.device_info=device_info_form[3];
	sub_total_snd_2m = sub_total_snd_1m = sub_total_snd_s8 = sub_total_snd_ble4 =0;
	memset(xmt_ratio_val,0,sizeof(xmt_ratio_val));
	round_total_num=total_num;
	if(round_phy_sel[0]) xmt_ratio_val[0][1]=total_num;
	if(round_phy_sel[1]) xmt_ratio_val[1][1]=total_num;
	if(round_phy_sel[2]) xmt_ratio_val[2][1]=total_num;
	if(round_phy_sel[3]) xmt_ratio_val[3][1]=total_num;
	ignore_rcv_resp = parm_p->ignore_rcv_resp;
	sender_peek_msg();
	envmon_abort_p=parm_p->envmon_abort;
	sender_abort_p=parm_p->sender_abort;
	scanner_abort_p=parm_p->scanner_abort;
	numcast_abort_p=parm_p->numcast_abort;
	sdc_hci_cmd_vs_zephyr_write_tx_power_t param_txpwr;
	sdc_hci_cmd_vs_zephyr_write_tx_power_return_t return_txpwr;
	param_txpwr=(sdc_hci_cmd_vs_zephyr_write_tx_power_t){.handle_type=SDC_HCI_VS_TX_POWER_HANDLE_TYPE_ADV, .handle=0, .tx_power_level=get_txpower_sv(get_txpower_effect(parm_p->txpwr))};
	sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	//printf("cfg_val %d, eff_val %d, set_val %d, res_val %d\n",parm_p->txpwr,get_txpower_effect(parm_p->txpwr),param_txpwr.tx_power_level,return_txpwr.selected_tx_power);
	param_txpwr.handle=1, sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=2, sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=3, sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	if(round_phy_sel[0]) update_adv(0,NULL,NULL,NULL);
	if(round_phy_sel[1]) update_adv(1,NULL,NULL,NULL);
	if(round_phy_sel[2]) update_adv(2,NULL,NULL,NULL);
	if(round_phy_sel[3]) update_adv(3,NULL,NULL,NULL);
	inhibit_ch37=parm_p->inhibit_ch37;
	inhibit_ch38=parm_p->inhibit_ch38;
	inhibit_ch39=parm_p->inhibit_ch39;
	non_ANONYMOUS=parm_p->non_ANONYMOUS;
	adv_param_mask[0]=((non_ANONYMOUS)?BT_LE_ADV_OPT_ANONYMOUS:0);
	adv_param_mask[1]=(((inhibit_ch37)?BT_LE_ADV_OPT_DISABLE_CHAN_37:0) | ((inhibit_ch38)?BT_LE_ADV_OPT_DISABLE_CHAN_38:0) | ((inhibit_ch39)?BT_LE_ADV_OPT_DISABLE_CHAN_39:0) | ((non_ANONYMOUS)?BT_LE_ADV_OPT_USE_IDENTITY:0));
	#if ADV_DBG_LOG_ENABLE
	printf("[ADVDBG] sender_setup phy={2m:%u,1m:%u,s8:%u,ble4:%u} non_anon=%u mask_clr=0x%08lx mask_set=0x%08lx\n",
		round_phy_sel[0] ? 1u : 0u,
		round_phy_sel[1] ? 1u : 0u,
		round_phy_sel[2] ? 1u : 0u,
		round_phy_sel[3] ? 1u : 0u,
		non_ANONYMOUS ? 1u : 0u,
		(unsigned long)adv_param_mask[0],
		(unsigned long)adv_param_mask[1]);
	#endif
	printf("Packet Loss Test (node %03u) **** SND SIDE ****\n",(unsigned char)NRF_FICR->DEVICEADDR[0]);
	passive_scan_method(0);
}


void scanner_setup(struct TEST_PARM * parm_p)
{
    if(!svc_init_success) return;
	round_tx_pwr=parm_p->txpwr;
	round_adv_param_index=parm_p->interval_idx;
	round_phy_sel[0]=parm_p->phy_2m, round_phy_sel[1]=parm_p->phy_1m, round_phy_sel[2]=parm_p->phy_s8, round_phy_sel[3]=parm_p->phy_ble4;
	sub_total_snd_2m = sub_total_snd_1m = sub_total_snd_s8 = sub_total_snd_ble4 =0;
	sub_total_rcv[0] = sub_total_rcv[1] = sub_total_rcv[2] = sub_total_rcv[3] =0;
	ignore_rcv_resp = parm_p->ignore_rcv_resp;
	memset(rec_sets,0,sizeof(rec_sets));
	scanner_peek_msg();
	envmon_abort_p=parm_p->envmon_abort;
	sender_abort_p=parm_p->sender_abort;
	scanner_abort_p=parm_p->scanner_abort;
	numcast_abort_p=parm_p->numcast_abort;
	sdc_hci_cmd_vs_zephyr_write_tx_power_t param_txpwr;
	sdc_hci_cmd_vs_zephyr_write_tx_power_return_t return_txpwr;
	param_txpwr=(sdc_hci_cmd_vs_zephyr_write_tx_power_t){.handle_type=SDC_HCI_VS_TX_POWER_HANDLE_TYPE_ADV, .handle=0, .tx_power_level=get_txpower_sv(get_txpower_effect(parm_p->txpwr))};
	sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=1, sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=2, sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=3, sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	inhibit_ch37=parm_p->inhibit_ch37;
	inhibit_ch38=parm_p->inhibit_ch38;
	inhibit_ch39=parm_p->inhibit_ch39;
	non_ANONYMOUS=parm_p->non_ANONYMOUS;
	scanner_inactive=true;
	printf("Packet Loss Test (node %03u) **** RCV SIDE ****\n",(unsigned char)NRF_FICR->DEVICEADDR[0]);
	passive_scan_method(0);
}


void numcast_setup(struct TEST_PARM * parm_p)
{
    if(!svc_init_success) return;
	round_adv_param_index=parm_p->interval_idx;
	round_phy_sel[0]=parm_p->phy_2m, round_phy_sel[1]=parm_p->phy_1m, round_phy_sel[2]=parm_p->phy_s8, round_phy_sel[3]=parm_p->phy_ble4;
	number_cast_val=*(uint64_t *)p_number_cast_form;
	envmon_abort_p=parm_p->envmon_abort;
	sender_abort_p=parm_p->sender_abort;
	scanner_abort_p=parm_p->scanner_abort;
	numcast_abort_p=parm_p->numcast_abort;
	sdc_hci_cmd_vs_zephyr_write_tx_power_t param_txpwr;
	sdc_hci_cmd_vs_zephyr_write_tx_power_return_t return_txpwr;
	param_txpwr=(sdc_hci_cmd_vs_zephyr_write_tx_power_t){.handle_type=SDC_HCI_VS_TX_POWER_HANDLE_TYPE_ADV, .handle=0, .tx_power_level=get_txpower_sv(get_txpower_effect(parm_p->txpwr))};
	uint8_t err=sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=1;
	err=sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=2;
	err=sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);
	param_txpwr.handle=3;
	err=sdc_hci_cmd_vs_zephyr_write_tx_power(&param_txpwr, &return_txpwr);

	inhibit_ch37=parm_p->inhibit_ch37;
	inhibit_ch38=parm_p->inhibit_ch38;
	inhibit_ch39=parm_p->inhibit_ch39;
	non_ANONYMOUS=parm_p->non_ANONYMOUS;
	adv_param_mask[0]=((non_ANONYMOUS)?BT_LE_ADV_OPT_ANONYMOUS:0);
	adv_param_mask[1]=(((inhibit_ch37)?BT_LE_ADV_OPT_DISABLE_CHAN_37:0) | ((inhibit_ch38)?BT_LE_ADV_OPT_DISABLE_CHAN_38:0) | ((inhibit_ch39)?BT_LE_ADV_OPT_DISABLE_CHAN_39:0) | ((non_ANONYMOUS)?BT_LE_ADV_OPT_USE_IDENTITY:0));

	blocking_adv(0);
	blocking_adv(1);
	blocking_adv(2);
	blocking_adv(3);

	uint8_t scan_method=(round_phy_sel[2]&&(round_phy_sel[3]||round_phy_sel[1]||round_phy_sel[0]))?0:((round_phy_sel[2])?2:1);
	//uint8_t scan_method=(round_phy_sel[2]&&(round_phy_sel[3]||round_phy_sel[1]||round_phy_sel[0]))?0:((round_phy_sel[2])?((rc_party)?0:2):1);
	passive_scan_method((2==scan_method && rc_party)?0:scan_method);

	number_cast_rxval=UINT64_MAX;
	number_cast_auto=false;
}


void envmon_setup(struct TEST_PARM * parm_p)
{
	blocking_adv(0);
	blocking_adv(1);
	blocking_adv(2);
	blocking_adv(3);
	passive_scan_method(0);
	env_stats_clr();
}


int losstst_init(void)
{
	int err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d, %s)\n", err, strerror(-err));
		return err;
	}
    num_adv_set=GET_SYM_VAL(CONFIG_BT_EXT_ADV_MAX_ADV_SET);
    if(5>num_adv_set) {
        printk("error CONFIG_BT_EXT_ADV_MAX_ADV_SET < 5\n");
        svc_init_success=false;
        return -1;
    }

	err=settings_load();

	size_t id_sz=CONFIG_BT_ID_MAX;

	bt_id_get(device_id_array,&id_sz);
	
	if(CONFIG_BT_ID_MAX!=id_sz) {
		int loop;
		for(loop=0; (loop+id_sz)!=CONFIG_BT_ID_MAX; loop++) bt_id_create(NULL,NULL);
		id_sz=CONFIG_BT_ID_MAX;
		bt_id_get(device_id_array,&id_sz);
	}

	resp_burst_end_data[0]=
			(struct bt_data){.type=BT_DATA_FLAGS,
			.data_len=1,
			.data=p_common_adv_flags};
	resp_burst_end_data[1]=
			(struct bt_data){.type=BT_DATA_MANUFACTURER_DATA,
			.data_len=sizeof(DEVICE_INFO_ST)};
	resp_burst_end_data[2]=(struct bt_data){.type=0,.data=NULL,.data_len=0};

	remote_ctrl_data[0]=
			(struct bt_data){.type=BT_DATA_FLAGS,
			.data_len=1,
			.data=p_common_adv_flags};
    remote_ctrl_data[2]=(struct bt_data){.type=0,.data=NULL,.data_len=0};
	
    svc_init_success=true;
    
	sender_peek_msg();
	
	device_info_form[0].eui_64	= device_info_form[1].eui_64
								= device_info_form[2].eui_64
								= device_info_form[3].eui_64
								= device_info_bt4_form.device_info.eui_64
								= (*(uint64_t *)NRF_FICR->DEVICEADDR);
	for (int i=0;i<num_adv_set /*CONFIG_BT_EXT_ADV_MAX_ADV_SET*/;i++)
		update_adv(i,NULL,NULL,BT_LE_EXT_ADV_START_PARAM((4==i)?0:18000,0));
	
	passive_scan_method(0);
    return 0;
}


int losstst_sender(void)
{
    if(!svc_init_success) return -1;
	int retval=1;
	int16_t lc_pre_cnt;
	uint16_t sub_phy0,sub_phy1,sub_phy2,sub_phy3;
	bool lc_phy_sel[4]={false,false,false,false};
	bool abort;
	int64_t uptime_64_barrier,period_msec,pitch_msec;
	struct bt_le_adv_param work_adv_param;

	sub_phy0=(round_phy_sel[0])?sub_total_snd_2m:round_total_num;
	sub_phy1=(round_phy_sel[1])?sub_total_snd_1m:round_total_num;
	sub_phy1=MIN(sub_phy1,sub_phy0);
	sub_phy2=(round_phy_sel[2])?sub_total_snd_s8:round_total_num;
	sub_phy3=(round_phy_sel[3])?sub_total_snd_ble4:round_total_num;

	if(sub_phy1<=sub_phy2) {
		lc_phy_sel[0]=round_phy_sel[0];
		lc_phy_sel[1]=round_phy_sel[1];
		lc_phy_sel[3]=round_phy_sel[3];
	} else {
		lc_phy_sel[2]=round_phy_sel[2];
	}

	// sd1
	if((lc_phy_sel[0] && sub_total_snd_2m<round_total_num)
		||(lc_phy_sel[1] && sub_total_snd_1m<round_total_num)
		||(lc_phy_sel[2] && sub_total_snd_s8<round_total_num)
		||(lc_phy_sel[3] && sub_total_snd_ble4<round_total_num)) {
		// sd2
		uptime_64_barrier=k_uptime_get();
		lc_pre_cnt=-3;

		for(int idx=0; idx<=3; idx++) {  // PRE_BURST_GROGRESS init
			if(lc_phy_sel[idx]) {
				device_info_form[idx].pre_cnt=lc_pre_cnt;
				device_info_form[idx].flw_cnt++;
				if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
			}
		}

		do {
			for(int idx=0; idx<=3; idx++) {
				if(lc_phy_sel[idx]) {
					work_adv_param=*non_connectable_adv_param_x[3][idx];
					work_adv_param.options|=adv_param_mask[1];
					work_adv_param.options&=~adv_param_mask[0];
				  #if ADV_DBG_LOG_ENABLE
					adv_dbg_log_options_if_changed("pre_burst", idx, work_adv_param.options, ratio_test_data_set[idx]);
				  #endif
					if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
					update_adv(idx,&work_adv_param,ratio_test_data_set[idx],p_adv_default_start_param);
					snd_state_val[idx]=1;
				}
			}

			uptime_64_barrier+=1000;
			while(uptime_64_barrier > k_uptime_get()) {
				if(k_can_yield()) k_yield();
				if(NULL==sender_abort_p) {}
				else if(true==(abort=(sender_abort_p)())) break;
			}
			
			lc_pre_cnt++;

			for(int idx=0; idx<=3; idx++) { // PRE_BURST_GROGRESS incress
				if(lc_phy_sel[idx]) {
					device_info_form[idx].pre_cnt=lc_pre_cnt;
					if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
				}
			}
		
		} while(!abort && 0!=lc_pre_cnt);
		
		if(abort) {
			snd_state_val[0]=snd_state_val[1]=snd_state_val[2]=snd_state_val[3]=0;
			sender_finit();
			return -1;
		}

		blocking_adv(0); // PRE_BURST_GROGRESS end
		blocking_adv(1);
		blocking_adv(2);
		blocking_adv(3);

		// sd3
		period_msec=LOSS_TEST_BURST_COUNT*value_interval[round_adv_param_index][1];
		int32_t period_sec=1+period_msec/1000;

		for(int idx=0; idx<=3; idx++) { // RUN_BURST init
			if(lc_phy_sel[idx]) device_info_form[idx].pre_cnt=period_sec;
			if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
		}
		
		uptime_64_barrier+=100;
		while(uptime_64_barrier > k_uptime_get()) { // wait 100ms for BURST
			if(k_can_yield()) k_yield();
			if(NULL==sender_abort_p) {}
			else if(true==(abort=(sender_abort_p)())) break;
		}
		
		if(abort) {
			snd_state_val[0]=snd_state_val[1]=snd_state_val[2]=snd_state_val[3]=0;
			sender_finit();
			return -1;
		}

		for(int idx=0; idx<=3; idx++) { // RUN_BURST start
			if(lc_phy_sel[idx]) {
				work_adv_param=*non_connectable_adv_param_x[round_adv_param_index][idx];
				work_adv_param.options|=adv_param_mask[1];
				work_adv_param.options&=~adv_param_mask[0];
			  #if ADV_DBG_LOG_ENABLE
				adv_dbg_log_options_if_changed("run_burst", idx, work_adv_param.options, ratio_test_data_set[idx]);
			  #endif
				if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
				update_adv(idx,&work_adv_param,ratio_test_data_set[idx],p_adv_burst_start_param);
				snd_state_val[idx]=2;
			}
		}

		uptime_64_barrier+=period_msec;
		pitch_msec=1000ul+k_uptime_get();
		while(((lc_phy_sel[0]&&!ext_adv_status[0].stop) // listen for BURST duration
				||(lc_phy_sel[1]&&!ext_adv_status[1].stop)
				||(lc_phy_sel[2]&&!ext_adv_status[2].stop)
				||(lc_phy_sel[3]&&!ext_adv_status[3].stop))
			&&(uptime_64_barrier> (period_msec=k_uptime_get()))) {
			if(k_can_yield()) k_yield();
			if(NULL==sender_abort_p) {}
			else if(true==(abort=(sender_abort_p)())) break;
			if(period_msec>=pitch_msec) {
				pitch_msec+=1000;
				period_sec--;

				for(int idx=0; idx<=3; idx++) {
					if(lc_phy_sel[idx]) {
						device_info_form[idx].pre_cnt=period_sec;
						if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
						update_adv(idx,NULL,ratio_test_data_set[idx],NULL);
					}
				}
			}
		}
		
		if(abort) {
			snd_state_val[0]=snd_state_val[1]=snd_state_val[2]=snd_state_val[3]=0;
			sender_finit();
			return -1;
		}

		// sd4
		ack_remote_resp[0]=ack_remote_resp[1]=ack_remote_resp[2]=ack_remote_resp[3]=false; // for receive remote response

		for(int idx=0; idx<=3; idx++) {// POST_BURST_GROGRESS
			if(lc_phy_sel[idx]) {
				device_info_form[idx].pre_cnt=0;
				work_adv_param=*non_connectable_adv_param_x[3][idx];
				work_adv_param.options|=adv_param_mask[1];
				work_adv_param.options&=~adv_param_mask[0];
				if(3==idx) device_info_bt4_form.device_info=device_info_form[3];
				update_adv(idx,&work_adv_param,ratio_test_data_set[idx],p_adv_default_start_param);
				switch(idx) {
					case 0: xmt_ratio_val[idx][0]=(sub_total_snd_2m+=LOSS_TEST_BURST_COUNT); break;
					case 1: xmt_ratio_val[idx][0]=(sub_total_snd_1m+=LOSS_TEST_BURST_COUNT); break;
					case 2: xmt_ratio_val[idx][0]=(sub_total_snd_s8+=LOSS_TEST_BURST_COUNT); break;
					case 3: xmt_ratio_val[idx][0]=(sub_total_snd_ble4+=LOSS_TEST_BURST_COUNT);
				}
				snd_state_val[idx]=3;
			}
		}

		sender_peek_msg();
		if(!rc_party) update_adv(4,NULL,NULL,NULL);
		
		if(lc_phy_sel[0]) printf("%s\n",peek_msg_str[0]+2);
		if(lc_phy_sel[1]) printf("%s\n",peek_msg_str[1]+2);
		if(lc_phy_sel[2]) printf("%s\n",peek_msg_str[2]+2);
		if(lc_phy_sel[3]) printf("%s\n",peek_msg_str[3]+2);

		uptime_64_barrier+=100;
		while(((lc_phy_sel[0]&&!ack_remote_resp[0])
				||(lc_phy_sel[1]&&!ack_remote_resp[1])
				||(lc_phy_sel[2]&&!ack_remote_resp[2])
				||(lc_phy_sel[3]&&!ack_remote_resp[3]) || ignore_rcv_resp)
			&&(uptime_64_barrier > k_uptime_get())) {
			if(k_can_yield()) k_yield();
			if(NULL==sender_abort_p) {}
			else if(true==(abort=(sender_abort_p)())) break;
		}
		
		if(abort) {
			snd_state_val[0]=snd_state_val[1]=snd_state_val[2]=snd_state_val[3]=0;
			sender_finit();
			return -1;
		}

		snd_state_val[0]=snd_state_val[1]=snd_state_val[2]=snd_state_val[3]=0;

		if(lc_phy_sel[0] && sub_total_snd_2m>=round_total_num) {
			printf("SND:%u P:%s/%s Complete\n"
					,(unsigned char)NRF_FICR->DEVICEADDR[0]
					,pri_phy_typ[1] ,sec_phy_typ[2]);

			device_info_form[0].pre_cnt=INT16_MAX;
			update_adv(0, NULL, ratio_test_data_set[0], p_adv_default_start_param);
		}

		if(lc_phy_sel[1] && sub_total_snd_1m>=round_total_num) {
			printf("SND:%u P:%s/%s Complete\n"
					,(unsigned char)NRF_FICR->DEVICEADDR[0]
					,pri_phy_typ[1] ,sec_phy_typ[1]);

			device_info_form[1].pre_cnt=INT16_MAX;
			update_adv(1, NULL, ratio_test_data_set[1], p_adv_default_start_param);
		}

		if(lc_phy_sel[2] && sub_total_snd_s8>=round_total_num) {
			printf("SND:%u P:%s/%s Complete\n"
					,(unsigned char)NRF_FICR->DEVICEADDR[0]
					,pri_phy_typ[3] ,sec_phy_typ[3]);
			
			device_info_form[2].pre_cnt=INT16_MAX; 
			update_adv(2, NULL, ratio_test_data_set[2], p_adv_default_start_param);
		}

		if(lc_phy_sel[3] && sub_total_snd_ble4>=round_total_num) {
			printf("SND:%u P:BLEv4 Complete\n"
					,(unsigned char)NRF_FICR->DEVICEADDR[0]);
			
			device_info_form[3].pre_cnt=INT16_MAX; 
			device_info_bt4_form.device_info=device_info_form[3];
			update_adv(3, NULL, ratio_test_data_set[3], p_adv_default_start_param);
		}

		uptime_64_barrier=500+k_uptime_get();
		while(uptime_64_barrier > k_uptime_get()) if(k_can_yield()) k_yield();
		uptime_64_barrier+=500;
		while(uptime_64_barrier > k_uptime_get()) {
			if(k_can_yield()) k_yield();
			if(NULL==sender_abort_p) {}
			else if(true==(abort=(sender_abort_p)())) break;
		}
	} else {
		sender_finit();
		retval=0;
	}
	return retval;
}

static char rcv_msg_str[3][80];
int losstst_scanner(void)
{
    if(!svc_init_success) return -1;
	int retval=1;
	int64_t period_msec,uptime_64_barrier;
	bool abort=false;
	static int8_t round_scan_method;
	static int8_t next_scan_method=0;
	static int16_t assign;
	static int64_t cntdn=0;
	static int64_t complete_mark,complete_elapse;
	static bool phy_mark[4];
	static int64_t hrtbt,hrtbt_stamp;
	static bool first_round;

    if(scanner_inactive) {
        memset(rcv_stamp,0,sizeof(rcv_stamp));
        scanner_inactive=false;
        memset(rcv_ratio_val,0,sizeof(rcv_ratio_val));
		round_scan_method=(round_phy_sel[2]&&(round_phy_sel[3]||round_phy_sel[1]||round_phy_sel[0]))?0:((round_phy_sel[2])?2:1);
		//round_scan_method=(round_phy_sel[2]&&(round_phy_sel[3]||round_phy_sel[1]||round_phy_sel[0]))?0:((round_phy_sel[2])?((rc_party)?0:2):1);
		memset(rcv_stats,0,sizeof(rcv_stats));
		memset(rcv_ratio_val,0,sizeof(rcv_ratio_val));
		memset(rcv_rssi_val,0,sizeof(rcv_rssi_val));
		first_round=true;
    }
	passive_scan_method((2==round_scan_method && rc_party)?0:round_scan_method);
	period_msec=LOSS_TEST_BURST_COUNT*value_interval[round_adv_param_index][1];
	if(round_scan_method) period_msec*=2; else period_msec+=3000;

	if(0==hrtbt_stamp) hrtbt_stamp=k_uptime_get();
	else if(
		((NULL!=scanner_abort_p)&&(true==(abort=(scanner_abort_p)())))
		||(MAX(((round_scan_method)?30000:10000),period_msec*((first_round)?5:1))<(hrtbt+=k_uptime_delta(&hrtbt_stamp)))) {
		hrtbt = hrtbt_stamp=0;
		passive_scan_method(0);
		return -1;
	}

	blocking_adv(0);
	blocking_adv(1);
	blocking_adv(2);
	blocking_adv(3);

	if(0>(assign=precnt_rcv[0]) && round_phy_sel[0]) {
		if(INT16_MIN!=assign) next_scan_method=1, cntdn=precnt_rcv[0]*-1000L;
	} else if(0>(assign=precnt_rcv[1]) && round_phy_sel[1]) {
		if(INT16_MIN!=assign) next_scan_method=1, cntdn=precnt_rcv[1]*-1000L;
	} else if(0>(assign=precnt_rcv[2]) && round_phy_sel[2]) {
		if(INT16_MIN!=assign) next_scan_method=2, cntdn=precnt_rcv[2]*-1000L;	
	} else if(0>(assign=precnt_rcv[3]) && round_phy_sel[3]) {
		if(INT16_MIN!=assign) next_scan_method=1, cntdn=precnt_rcv[3]*-1000L;	
	} else if(0<(assign=precnt_rcv[0]) && round_phy_sel[0]) {
		if(INT16_MAX!=assign) next_scan_method=1;
	} else if(0<(assign=precnt_rcv[1]) && round_phy_sel[1]) {
		if(INT16_MAX!=assign) next_scan_method=1;
	} else if(0<(assign=precnt_rcv[2]) && round_phy_sel[2]) {
		if(INT16_MAX!=assign) next_scan_method=2;
	} else if(0<(assign=precnt_rcv[3]) && round_phy_sel[3]) {
		if(INT16_MAX!=assign) next_scan_method=1;
	} else {
		if((!rec_sets[0].flow && !rec_sets[1].flow && !rec_sets[2].flow && !rec_sets[3].flow)) {}
		else if((rec_sets[0].complete || !rec_sets[0].flow)
			&& ( rec_sets[1].complete || !rec_sets[1].flow)
			&& ( rec_sets[2].complete || !rec_sets[2].flow)
			&& ( rec_sets[3].complete || !rec_sets[3].flow)) {
			hrtbt = hrtbt_stamp=0;
			retval=0;
			passive_scan_method(0);
		}
		return retval;
	}
	if(0==next_scan_method) return retval;
	first_round=false;

	if(INT16_MAX==precnt_rcv[0] && INT16_MAX==precnt_rcv[1] && INT16_MAX==precnt_rcv[2] && INT16_MAX==precnt_rcv[3]) {
		if(0==complete_mark)
			complete_mark=k_uptime_get();
		else if(10000<(complete_elapse+=k_uptime_delta(&complete_mark))) {
			printf("RCV_Task completed\n");
			passive_scan_method(0);
			return 0;
		}
	}
	else complete_elapse=complete_mark=0;

	if(!rc_party) blocking_adv(4);

	period_msec+=cntdn;
	
	uptime_64_barrier=period_msec+k_uptime_get();

	passive_scan_method(next_scan_method);

	cntdn=0;
	phy_mark[0]=phy_mark[1]=phy_mark[2]=phy_mark[3]=false;
	while(uptime_64_barrier > k_uptime_get()) {
		if('\0'!=*rcv_msg_str[0]) {
			do { printf("%s\n",rcv_msg_str[0]); } while(0); *rcv_msg_str[0]='\0';
		}
		if('\0'!=*rcv_msg_str[1]) {
			do { printf("%s\n",rcv_msg_str[1]); } while(0); *rcv_msg_str[1]='\0';
		}
		if('\0'!=*rcv_msg_str[2]) {
			do { printf("%s\n",rcv_msg_str[2]); } while(0); *rcv_msg_str[2]='\0';
		}
		if(NULL==scanner_abort_p) {}
		else if(true==(abort=(scanner_abort_p)())) {
			rcv_state_val[0]=rcv_state_val[1]=rcv_state_val[2]=rcv_state_val[3]=0;
			break;
		}
		
		if(1==next_scan_method) {
			if(0>(precnt_rcv[0])) { if(INT16_MIN!=precnt_rcv[0]) { phy_mark[0]=true; rcv_state_val[0]=1; } }
			else if(0<(precnt_rcv[0])) { phy_mark[0]=true; rcv_state_val[0]=2; }
			else if(0==(precnt_rcv[0])) { if(phy_mark[0]) rcv_state_val[0]=3; }

			if(0>(precnt_rcv[1])) { if(INT16_MIN!=precnt_rcv[1]) { phy_mark[1]=true; rcv_state_val[1]=1; } }
			else if(0<(precnt_rcv[1])) { phy_mark[1]=true; rcv_state_val[1]=2; }
			else if(0==(precnt_rcv[1])) { if(phy_mark[1]) rcv_state_val[1]=3; }

			if(0>(precnt_rcv[3])) { if(INT16_MIN!=precnt_rcv[3]) { phy_mark[3]=true; rcv_state_val[3]=1; } }
			else if(0<(precnt_rcv[3])) { phy_mark[3]=true; rcv_state_val[3]=2; }
			else if(0==(precnt_rcv[3])) { if(phy_mark[3]) rcv_state_val[3]=3; }
			
			rcv_state_val[2]=0;
		}

		if(2==next_scan_method) {
			if(0>precnt_rcv[2]) { if(INT16_MIN!=precnt_rcv[2]) phy_mark[2]=true, rcv_state_val[2]=1; }
			if(0<precnt_rcv[2]) { phy_mark[2]=true; rcv_state_val[2]=2; }
			if(0==precnt_rcv[2]) { if(phy_mark[2]) rcv_state_val[2]=3; }
			rcv_state_val[0]=rcv_state_val[1]=rcv_state_val[3]=0;
		}

		for(int idx=0; idx<=3; idx++) {
			if(phy_mark[idx] && rec_sets[idx].complete) {
				abort=true;
				break;
			}
			if(phy_mark[idx] && 0==precnt_rcv[idx]) {
				if(!ignore_rcv_resp) {
					resp_burst_end_data[1].data=(const uint8_t *)&remote_resp_form[idx];
					update_adv(idx,NULL,resp_burst_end_data,p_adv_1sec_start_param);
				}
				
				rcv_state_val[idx]=0;
				phy_mark[idx]=false;
			}
		}

		if(abort) break;
		
			
		if(!phy_mark[0] && !phy_mark[1] && !phy_mark[2] && !phy_mark[3]) {
			if(0==cntdn) cntdn=800+k_uptime_get();
			else if(cntdn<k_uptime_get()) break;
		}
		if(k_can_yield()) k_yield();
	}
	if(phy_mark[0]) precnt_rcv[0]=0;
	if(phy_mark[1]) precnt_rcv[1]=0;
	if(phy_mark[2]) precnt_rcv[2]=0;
	if(phy_mark[3]) precnt_rcv[3]=0;
	rcv_state_val[0]=rcv_state_val[1]=rcv_state_val[2]=rcv_state_val[3]=0;
	hrtbt=hrtbt_stamp=0;
	if(abort) retval=-1;

	scanner_peek_msg();
	if(!rc_party) update_adv(4, NULL, NULL, p_adv_default_start_param);
	//passive_scan_method((0<retval)?round_scan_method:0);
	passive_scan_method(((0>=retval)||(2==round_scan_method && rc_party))?0:round_scan_method);
	return retval;
}


bool numcast_phy_mark(uint8_t idx)
{
	if(3<idx) return false;
	int64_t msec_tm=k_uptime_get();
	return (numcst_phy_stamp_tm[idx]>msec_tm)?true:false;

}
int losstst_numcast(void)
{
	static bool cast_auto;
	if((numcast_abort_p)()) {
		cast_auto=false;
		number_cast_rxval=UINT64_MAX;
		blocking_adv(0);
		update_adv(0,NULL,NULL,p_adv_1sec_start_param);
		blocking_adv(1);
		update_adv(1,NULL,NULL,p_adv_1sec_start_param);
		blocking_adv(2);
		update_adv(2,NULL,NULL,p_adv_1sec_start_param);
		blocking_adv(3);
		update_adv(3,NULL,NULL,p_adv_1sec_start_param);
		passive_scan_method(0);
		return 0;
	}
	
	if(number_cast_val!=*(uint64_t *)p_number_cast_form || cast_auto!=number_cast_auto) {
		number_cast_val=*(uint64_t *)p_number_cast_form;
		cast_auto=number_cast_auto;
		struct bt_le_ext_adv_start_param start_param={0,(cast_auto)?0:10};
		struct bt_le_adv_param work_adv_param;
		for(int idx=0; idx<4; idx++) {
			blocking_adv(idx);
			work_adv_param=*non_connectable_adv_param_x[round_adv_param_index][idx];
			work_adv_param.options|=adv_param_mask[1];
			work_adv_param.options&=~adv_param_mask[0];
			if(round_phy_sel[idx]) {
				if(3==idx) {
					numcast_bt4_form=device_info_bt4_form;
					*((uint16_t *)numcast_bt4_form.tail)=UINT16_MAX;
					*((uint64_t *)(2+numcast_bt4_form.tail))=number_cast_val;
					update_adv(idx,&work_adv_param/*non_connectable_adv_param_x[round_adv_param_index][3]*/,number_cast_data_set[1],&start_param);
				}
				else
					update_adv(idx,&work_adv_param/*non_connectable_adv_param_x[round_adv_param_index][idx]*/,number_cast_data_set[0],&start_param);
			}
		}
	}
	numcst_rssi_calc(k_uptime_get());
	return 1;
}

int losstst_envmon(void)
{
	env_rssi_calc();
	return (0!=envmon_task_tgr(0))?1:0;
}
/*
k_thread_user_cb_t ask_prio(const struct k_thread *thread, void *user_data)
{
	K_KERNEL_STACK_ARRAY_DECLARE(z_interrupt_stacks, CONFIG_MP_MAX_NUM_CPUS, CONFIG_ISR_STACK_SIZE);
	printf("thread::name:%s\n        prio:%3d,   stack:%08x..%08x,   stack_sz:%04x\n",thread->name,thread->base.prio,thread->stack_info.start,thread->stack_info.start+thread->stack_info.size-1,thread->stack_info.size);
	//if(NULL!=cb_local_var[0]) {
	//	printf("uart_cb var:: %p %p %p %p %p\n",cb_local_var[0],cb_local_var[1],cb_local_var[2],cb_local_var[3],cb_local_var[4]);cb_local_var[0]=NULL;
	//	printf("z_interrupt_stacks:: %p..%p, sz %u\n",z_interrupt_stacks,(uint32_t)z_interrupt_stacks+sizeof(z_interrupt_stacks)-1,sizeof(z_interrupt_stacks));
	//}
}
*/


static void scanner_peek_msg(void)
{
	sprintf(peek_msg_str[0],peek_rcvpkt_form
						,(uint8_t)rec_sets[0].node
						,pri_phy_typ[rec_sets[0].pri_phy] ,sec_phy_typ[rec_sets[0].sec_phy]
						,sub_total_rcv[0], LOSS_TEST_BURST_COUNT*rec_sets[0].flow
						,rssi_toa(peek_rcv_rssi[0][0],rssi_str[0])
						,rssi_toa(peek_rcv_rssi[0][1],rssi_str[1])
						,rssi_toa(peek_rcv_rssi[0][2],rssi_str[2])
						,txpwr_toa(remote_tx_pwr[0],tx_pwr_str));
	*(uint16_t *)(peek_msg_str[0])=MANUFACTURER_ID;

	sprintf(peek_msg_str[1],peek_rcvpkt_form
						,(uint8_t)rec_sets[1].node
						,pri_phy_typ[rec_sets[1].pri_phy] ,sec_phy_typ[rec_sets[1].sec_phy]
						,sub_total_rcv[1], LOSS_TEST_BURST_COUNT*rec_sets[1].flow
						,rssi_toa(peek_rcv_rssi[1][0],rssi_str[0])
						,rssi_toa(peek_rcv_rssi[1][1],rssi_str[1])
						,rssi_toa(peek_rcv_rssi[1][2],rssi_str[2])
						,txpwr_toa(remote_tx_pwr[1],tx_pwr_str));
	*(uint16_t *)(peek_msg_str[1])=MANUFACTURER_ID;

	sprintf(peek_msg_str[2],peek_rcvpkt_form
						,(uint8_t)rec_sets[2].node
						,pri_phy_typ[rec_sets[2].pri_phy] ,sec_phy_typ[rec_sets[2].sec_phy]
						,sub_total_rcv[2], LOSS_TEST_BURST_COUNT*rec_sets[2].flow
						,rssi_toa(peek_rcv_rssi[2][0],rssi_str[0])
						,rssi_toa(peek_rcv_rssi[2][1],rssi_str[1])
						,rssi_toa(peek_rcv_rssi[2][2],rssi_str[2])
						,txpwr_toa(remote_tx_pwr[2],tx_pwr_str));
	*(uint16_t *)(peek_msg_str[2])=MANUFACTURER_ID;

	sprintf(peek_msg_str[3],peek_rcvpkt_btv4_form
						,(uint8_t)rec_sets[3].node
						,"BLE" ,"v4"
						,sub_total_rcv[3], LOSS_TEST_BURST_COUNT*rec_sets[3].flow
						,rssi_toa(peek_rcv_rssi[3][0],rssi_str[0])
						,rssi_toa(peek_rcv_rssi[3][1],rssi_str[1])
						,rssi_toa(peek_rcv_rssi[3][2],rssi_str[2])
						,txpwr_toa(remote_tx_pwr[3],tx_pwr_str));
	*(uint16_t *)(peek_msg_str[3])=MANUFACTURER_ID;
}

static char * rssi_toa(int16_t rssi,char * str_p)
{
	if(NULL==str_p) return (char *){"\0"};
	if(INT8_MAX<=rssi || INT8_MIN>=rssi)
		*str_p='\0';
	else
		__itoa(rssi,str_p,10);
	return str_p;
}


static char * txpwr_toa(int8_t pwr,char * str_p)
{
	if(NULL==str_p) return (char *){"\0"};
	if(INT8_MAX==pwr)
		*str_p='\0';
	else
		__itoa(pwr,str_p,10);
	return str_p;
}


static void numcast_packet_evt(uint8_t idx, DEVICE_INFO_ST * form_p, uint64_t * numcast_p, int8_t rssi)
{
	if(3<idx) return;

	//E: ***** USAGE FAULT *****
	//E:   Unaligned memory access
	//..........
	//E: Faulting instruction address (r15/pc): 0x0001e790
	//E: >>> ZEPHYR FATAL ERROR 31: Unknown error on CPU 0
	//E: Current thread: 0x20002ab8 (BT RX WQ)
	// if idx==3 , fault...
	//number_cast_rxval=*numcast_p;

	memcpy(&number_cast_rxval,numcast_p,sizeof(uint64_t));
	int64_t tm_expire=k_uptime_get()+5000;
	numcst_phy_stamp_tm[idx]=tm_expire;
	numcst_src_node[0]=*(6+offsetof(DEVICE_INFO_ST,eui_64)+(uint8_t *)form_p);
	numcst_src_node[1]=*(7+offsetof(DEVICE_INFO_ST,eui_64)+(uint8_t *)form_p);
	REC_RSSI_STAMP loc_rec={.expired_tm=tm_expire,.rssi=rssi};
	
	numcst_rssi_rec[(ARRAY_SIZE(numcst_rssi_rec)-1)&numcst_rssi_idx++]=loc_rec;
}


static void rssi_avg_procedure(RCV_STAMP * stamp_p, int16_t rssi_val)
{
	stamp_p->rec.rssi_upper=MAX(rssi_val,stamp_p->rec.rssi_upper);
	stamp_p->rec.rssi_lower=MIN(rssi_val,stamp_p->rec.rssi_lower);
	stamp_p->rssi_acc+=rssi_val;
	stamp_p->rssi_idx+=1;
	stamp_p->rec.rssi=stamp_p->rssi_acc/stamp_p->rssi_idx;
}
static void rssi_idx_init(RCV_STAMP * stamp_p, int16_t rssi_val)
{
	stamp_p->rec.rssi=rssi_val;
	stamp_p->rec.rssi_upper=INT16_MIN;
	stamp_p->rec.rssi_lower=INT16_MAX;
	stamp_p->rssi_acc=rssi_val;
	stamp_p->rssi_idx=1;
}
static void tst_form_packet_rcv(struct bt_hci_evt_le_ext_advertising_info * info_p, DEVICE_INFO_ST * form_p)
{
	RCV_STAMP rcv_stamp_lc={.rec.rssi_upper=INT16_MIN,.rec.rssi_lower=INT16_MAX};
	uint8_t index;
	uint16_t subtotal;
	int16_t info_rssi;
	bool sndinfo_output_req=false;
	bool rcvinfo_output_req=false;
	//char rcv_msg_str[80];

	rcv_stamp_lc.rec.node   =0xFFFF&form_p->eui_64;
	rcv_stamp_lc.rec.pri_phy=info_p->prim_phy;
	rcv_stamp_lc.rec.sec_phy=info_p->sec_phy;
	info_rssi=(20<info_p->rssi)?-128:info_p->rssi;

	if(1==rcv_stamp_lc.rec.pri_phy && 2==rcv_stamp_lc.rec.sec_phy) index=0;
	else if(1==rcv_stamp_lc.rec.pri_phy && 1==rcv_stamp_lc.rec.sec_phy) index=1;
	else if(3==rcv_stamp_lc.rec.pri_phy && 3==rcv_stamp_lc.rec.sec_phy) index=2;
	else if(1==rcv_stamp_lc.rec.pri_phy && 0==rcv_stamp_lc.rec.sec_phy) index=3;
	else return;

	if(0!=sender_task_tgr(0)) {
		if(0==memcmp((const char *)&device_info_form[index],form_p,sizeof(DEVICE_INFO_ST))) {
			ack_remote_resp[index]=true;
		}
		return;
	}

	if(0==scanner_task_tgr(0) || scanner_inactive || !round_phy_sel[index]) return;

	rcv_stamp_lc.rec.tx_pwr =info_p->tx_power;
	rcv_stamp_lc.rec.flow   =form_p->flw_cnt;

	if(201<rcv_stamp_lc.rec.flow) {}
	else if(INT16_MIN==form_p->pre_cnt) { // sender config-preset progress
		if(0!=memcmp(&rcv_stamp[index],&rcv_stamp_lc,sizeof(rcv_stamp_lc.rec.flow)+offsetof(RCV_RECORD,flow))) {
			rcv_stamp_lc.rec.subtotal=0;
			rcv_stamp_lc.rec.det_sender=1;
			rssi_idx_init(&rcv_stamp_lc,info_rssi);
			sndinfo_output_req=true;
		} else {
			rcv_stamp_lc.rec.det_sender=1;
			rssi_avg_procedure(&rcv_stamp_lc,info_rssi);
		}
		
		rcv_stamp[index]=rcv_stamp_lc;
		rec_sets[index]=rcv_stamp[index].rec;
		subtotal=sub_total_rcv[index]=0;
	} else if(0<form_p->pre_cnt && INT16_MAX!=form_p->pre_cnt) { // burst counting
		subtotal=++sub_total_rcv[index];
		rcv_ratio_val[index][0]=subtotal;
		rcv_ratio_val[index][1]=LOSS_TEST_BURST_COUNT*rcv_stamp_lc.rec.flow;
		precnt_rcv[index]=form_p->pre_cnt;
		sndr_id=rcv_stamp_lc.rec.node;
		sndr_txpower=rcv_stamp_lc.rec.tx_pwr;
	} else { // pre-burst,
		subtotal=sub_total_rcv[index];
		rcv_ratio_val[index][0]=subtotal;
		rcv_ratio_val[index][1]=LOSS_TEST_BURST_COUNT*rcv_stamp_lc.rec.flow;
		sndr_id=rcv_stamp_lc.rec.node;
		sndr_txpower=rcv_stamp_lc.rec.tx_pwr;
	}
	
	if(0==rcv_stamp_lc.rec.flow || 201<rcv_stamp_lc.rec.flow) {;}
	else if(0==memcmp(&rcv_stamp[index],&rcv_stamp_lc,offsetof(RCV_RECORD,flow))) {
		if(rcv_stamp[index].rec.flow==rcv_stamp_lc.rec.flow) {
			rcv_stamp_lc=rcv_stamp[index];
			rcv_stamp_lc.rec.subtotal=subtotal;
			rssi_avg_procedure(&rcv_stamp_lc,info_rssi);

			if(INT16_MAX==form_p->pre_cnt && !rcv_stamp_lc.rec.complete) { // sndr side completed
				rcv_stamp_lc.rec.complete=1;
				rec_sets[index]=rcv_stamp_lc.rec;
			} else if(0==form_p->pre_cnt) { // sndr side burst completed
				precnt_rcv[index] = 0;
				remote_resp_form[index]=*form_p;
				if(!rcv_stamp_lc.rec.dump_rcvinfo) 
					rcv_stamp_lc.rec.dump_rcvinfo=1, rcvinfo_output_req=true;
				rec_sets[index]=rcv_stamp_lc.rec;
			} else if(0>form_p->pre_cnt) {
				precnt_rcv[index] = form_p->pre_cnt;
			}
			rcv_stamp[index]=rcv_stamp_lc;
			rcv_rssi_val[index][0]=rcv_stamp_lc.rec.rssi;
			rcv_rssi_val[index][1]=(1>=rcv_stamp_lc.rssi_idx)?rcv_stamp_lc.rec.rssi:rcv_stamp_lc.rec.rssi_lower;
			rcv_rssi_val[index][2]=(1>=rcv_stamp_lc.rssi_idx)?rcv_stamp_lc.rec.rssi:rcv_stamp_lc.rec.rssi_upper;
			memcpy(peek_rcv_rssi[index],rcv_rssi_val[index],sizeof(peek_rcv_rssi[index]));
		} else {
			rcv_rssi_val[index][0]=rcv_stamp[index].rec.rssi =rcv_stamp[index].rssi_acc/rcv_stamp[index].rssi_idx;
			rcv_rssi_val[index][1]=(1>=rcv_stamp[index].rssi_idx)?rcv_stamp[index].rec.rssi:rcv_stamp[index].rec.rssi_lower;
			rcv_rssi_val[index][2]=(1>=rcv_stamp[index].rssi_idx)?rcv_stamp[index].rec.rssi:rcv_stamp[index].rec.rssi_upper;
			memcpy(peek_rcv_rssi[index],rcv_rssi_val[index],sizeof(peek_rcv_rssi[index]));
			remote_tx_pwr[index] = rcv_stamp[index].rec.tx_pwr;

			if(!rcv_stamp[index].rec.dump_rcvinfo) { 
				rcv_stamp[index].rec.dump_rcvinfo=1;
				rcvinfo_output_req=true;
				rec_sets[index]=rcv_stamp[index].rec;
			}

			rssi_idx_init(&rcv_stamp_lc,info_rssi);
			rcv_stamp[index]=rcv_stamp_lc;
		}
	} else {
		rssi_idx_init(&rcv_stamp_lc,info_rssi);
		rcv_stamp[index]=rcv_stamp_lc;
		char *dst_p=('\0'==*rcv_msg_str[0])?rcv_msg_str[0]:(('\0'==*rcv_msg_str[1])?rcv_msg_str[1]:rcv_msg_str[2]);
		sprintf(dst_p,log_sender_form
							,(uint8_t)rcv_stamp_lc.rec.node
							,pri_phy_typ[rcv_stamp_lc.rec.pri_phy]
							,sec_phy_typ[rcv_stamp_lc.rec.sec_phy]
							,subtotal,rcv_stamp_lc.rec.flow*LOSS_TEST_BURST_COUNT
							,rssi_toa(rcv_stamp_lc.rec.rssi,rssi_str[0])
							,rssi_toa(rcv_stamp_lc.rec.rssi_lower,rssi_str[1])
							,rssi_toa(rcv_stamp_lc.rec.rssi_upper,rssi_str[2])
							,txpwr_toa(rcv_stamp_lc.rec.tx_pwr,tx_pwr_str));
		//printf("%s\n",rcv_msg_str);
		//printf("%s\n",dst_p); *dst_p='\0';
	}

	if(rcvinfo_output_req) {
		char *dst_p=('\0'==*rcv_msg_str[0])?rcv_msg_str[0]:(('\0'==*rcv_msg_str[1])?rcv_msg_str[1]:rcv_msg_str[2]);
		sprintf(dst_p,log_rcvpkt_form
							,(uint8_t)rec_sets[index].node
							,pri_phy_typ[rec_sets[index].pri_phy]
							,sec_phy_typ[rec_sets[index].sec_phy]
							,sub_total_rcv[index],rec_sets[index].flow*LOSS_TEST_BURST_COUNT
							,rssi_toa(peek_rcv_rssi[index][0],rssi_str[0])
							,rssi_toa(peek_rcv_rssi[index][1],rssi_str[1])
							,rssi_toa(peek_rcv_rssi[index][2],rssi_str[2])
							,txpwr_toa(rec_sets[index].tx_pwr,tx_pwr_str));
		//printf("%s\n",rcv_msg_str);
		//printf("%s\n",dst_p); *dst_p='\0';
	}

	if(sndinfo_output_req) {
		char *dst_p=('\0'==*rcv_msg_str[0])?rcv_msg_str[0]:(('\0'==*rcv_msg_str[1])?rcv_msg_str[1]:rcv_msg_str[2]);
		sprintf(dst_p,log_sender_form
							,(uint8_t)rec_sets[index].node
							,pri_phy_typ[rec_sets[index].pri_phy]
							,sec_phy_typ[rec_sets[index].sec_phy]
							,subtotal,rec_sets[index].flow*LOSS_TEST_BURST_COUNT
							,rssi_toa(rec_sets[index].rssi,rssi_str[0])
							,rssi_toa(rec_sets[index].rssi_lower,rssi_str[1])
							,rssi_toa(rec_sets[index].rssi_upper,rssi_str[2])
							,txpwr_toa(rec_sets[index].tx_pwr,tx_pwr_str));
		//printf("%s\n",rcv_msg_str);
		//printf("%s\n",dst_p); *dst_p='\0';
	}
}


static bool test_form_parser(struct bt_data *data, void *user_data)
{
	struct DEV_FOUND_PARM_ST * dev_chr_p=((struct DEV_FOUND_PARM_ST *)user_data);
	struct bt_hci_evt_le_ext_advertising_info * adv_info_p=dev_chr_p->adv_info_p;
	if(BT_DATA_FLAGS==data->type) {
		if(0==dev_chr_p->step_raw) dev_chr_p->step_flag++;
		else dev_chr_p->step_fail=1;
	} else if(1==dev_chr_p->step_flag && BT_DATA_MANUFACTURER_DATA==data->type) {
		DEVICE_INFO_ST *rcv_data_p=(DEVICE_INFO_ST *)data->data;
		if(MANUFACTURER_ID==rcv_data_p->man_id && LOSS_TEST_FORM_ID==rcv_data_p->form_id) {
			tst_form_packet_rcv(adv_info_p,rcv_data_p);
			dev_chr_p->step_success=1;
		}
		else dev_chr_p->step_fail=1;
	} else dev_chr_p->step_fail=1;
	return (dev_chr_p->step_completed)?false:true;
}


static bool numcast_parser(struct bt_data *data, void *user_data)
{
	struct DEV_FOUND_PARM_ST * dev_chr_p=((struct DEV_FOUND_PARM_ST *)user_data);
	struct bt_hci_evt_le_ext_advertising_info * adv_info_p=dev_chr_p->adv_info_p;
	if(BT_DATA_FLAGS==data->type) {
		if(0==dev_chr_p->step_raw) dev_chr_p->step_flag++;
		else dev_chr_p->step_fail=1;
	} else if(1==dev_chr_p->step_flag && BT_DATA_MANUFACTURER_DATA==data->type) {
		dev_chr_p->step_special_stream++;
		int8_t arg_rssi=adv_info_p->rssi;
		arg_rssi=(20<arg_rssi)?-128:arg_rssi;
		int8_t idx;
		if(1==adv_info_p->prim_phy && 2==adv_info_p->sec_phy) idx=0;
		else if(1==adv_info_p->prim_phy && 1==adv_info_p->sec_phy) idx=1;
		else if(3==adv_info_p->prim_phy && 3==adv_info_p->sec_phy) idx=2;
		else if(1==adv_info_p->prim_phy && 0==adv_info_p->sec_phy) idx=3;
		else {
			dev_chr_p->step_fail=1;
			return false;
		}
		
		if(3==idx) {
			DEVICE_INFO_BTv4_ST *rcv_data_p=(DEVICE_INFO_BTv4_ST *)data->data;
			if(MANUFACTURER_ID==rcv_data_p->device_info.man_id && LOSS_TEST_FORM_ID ==rcv_data_p->device_info.form_id && UINT16_MAX ==*((uint16_t *)rcv_data_p->tail)) {
				numcast_packet_evt(idx,rcv_data_p,2+rcv_data_p->tail,arg_rssi);
				dev_chr_p->step_success=1;
			}
			else dev_chr_p->step_fail=1;
		} else {
			if(2==dev_chr_p->step_special_stream) {
				NUMCAST_INFO_ST *rcv_data_p=(NUMCAST_INFO_ST *)data->data;
				if(NULL!=(dev_chr_p->temp_ptr) &&0xFF==data->type && sizeof(NUMCAST_INFO_ST)==data->data_len) {
					numcast_packet_evt(idx,(DEVICE_INFO_ST *)(dev_chr_p->temp_ptr),rcv_data_p->number_cast_form,arg_rssi);
					dev_chr_p->step_success=1;
				}
				else dev_chr_p->step_fail=1;
			} else if(1==dev_chr_p->step_special_stream) {
				if(data->data_len==sizeof(DEVICE_INFO_ST)) dev_chr_p->temp_ptr=data->data;
				else dev_chr_p->step_fail=1;
			} else dev_chr_p->step_fail=1;
		}
	} else dev_chr_p->step_fail=1;

	return (dev_chr_p->step_completed)?false:true;
}


bool rc_rush_msg_outgoing(void * data_p, size_t sz)
{
	remote_ctrl_data[1].type=BT_DATA_MANUFACTURER_DATA;
	remote_ctrl_data[1].data_len=sz;
	remote_ctrl_data[1].data=data_p;
	
	struct bt_le_adv_param param={.id=0,
		.sid=0, .secondary_max_skip=0, .peer=NULL,
		.options=BT_LE_ADV_OPT_USE_IDENTITY 
				| BT_LE_ADV_OPT_USE_TX_POWER 
				| BT_LE_ADV_OPT_NO_2M 
				| BT_LE_ADV_OPT_EXT_ADV,
		.interval_min=PARAM_ADV_INT_MIN_0,
		.interval_max=PARAM_ADV_INT_MAX_0 };
	//blocking_adv(4);
	int err;
	int err_stamp=0;
	int64_t start_tm=k_uptime_get();
	do {
		//printf("%s LN%u, adv 4, data :%02x %2d %p, %02x %2d %p,\n",__FUNCTION__,__LINE__,remote_ctrl_data[0].type,remote_ctrl_data[0].data_len,remote_ctrl_data[0].data,remote_ctrl_data[1].type,remote_ctrl_data[1].data_len,remote_ctrl_data[1].data);
		err=update_adv(4, &param, remote_ctrl_data, BT_LE_EXT_ADV_START_PARAM(60,0));
		if(err) {
			err_stamp=err;
			//printf("%s LN%u, adv 4 try again\n",__FUNCTION__,__LINE__);
			if(k_can_yield()) k_yield();
		}
	} while(err);
	//if(err_stamp) printf("%s LN%u, adv 4 sent in %lld msec\n",__FUNCTION__,__LINE__,k_uptime_delta(&start_tm));
	
	return false;

}


bool rm_rush_msg_outgoing(void * data_p, size_t sz)
{
	remote_ctrl_data[1].type=BT_DATA_MANUFACTURER_DATA;
	remote_ctrl_data[1].data_len=sz;
	remote_ctrl_data[1].data=data_p;
	
	struct bt_le_adv_param param={.id=0,
		.sid=0, .secondary_max_skip=0, .peer=NULL,
		.options=BT_LE_ADV_OPT_USE_IDENTITY 
				| BT_LE_ADV_OPT_USE_TX_POWER 
				| BT_LE_ADV_OPT_NO_2M 
				| BT_LE_ADV_OPT_EXT_ADV,
		.interval_min=PARAM_ADV_INT_MIN_0,
		.interval_max=PARAM_ADV_INT_MAX_0 };
	//blocking_adv(4);
	int err;
	int err_stamp=0;
	int64_t start_tm=k_uptime_get();
	do {
		//printf("%s LN%u, adv 4, data :%02x %2d %p, %02x %2d %p,\n",__FUNCTION__,__LINE__,remote_ctrl_data[0].type,remote_ctrl_data[0].data_len,remote_ctrl_data[0].data,remote_ctrl_data[1].type,remote_ctrl_data[1].data_len,remote_ctrl_data[1].data);
		err=update_adv(4, &param, remote_ctrl_data, BT_LE_EXT_ADV_START_PARAM(30,0));
		if(err) {
			err_stamp=err;
			//printf("%s LN%u, adv 4 try again\n",__FUNCTION__,__LINE__);
			if(k_can_yield()) k_yield();
		}
	} while(err);
	//if(err_stamp) printf("%s LN%u, adv 4 sent in %lld msec\n",__FUNCTION__,__LINE__,k_uptime_delta(&start_tm));
	
	return false;

}


bool rm_msg_outgoing(void * data_p, size_t sz)
{
	remote_ctrl_data[1].type=BT_DATA_MANUFACTURER_DATA;
	remote_ctrl_data[1].data_len=sz;
	remote_ctrl_data[1].data=data_p;
	
	struct bt_le_adv_param param={.id=0,
		.sid=0, .secondary_max_skip=0, .peer=NULL,
		.options=BT_LE_ADV_OPT_USE_IDENTITY 
				| BT_LE_ADV_OPT_USE_TX_POWER 
				| BT_LE_ADV_OPT_NO_2M 
				| BT_LE_ADV_OPT_EXT_ADV,
		.interval_min=PARAM_ADV_INT_MIN_3,
		.interval_max=PARAM_ADV_INT_MAX_3 };
	//blocking_adv(4);
	int err;
	int err_stamp=0;
	int64_t start_tm=k_uptime_get();
	do {
		//printf("%s LN%u, adv 4, data :%02x %2d %p, %02x %2d %p,\n",__FUNCTION__,__LINE__,remote_ctrl_data[0].type,remote_ctrl_data[0].data_len,remote_ctrl_data[0].data,remote_ctrl_data[1].type,remote_ctrl_data[1].data_len,remote_ctrl_data[1].data);
		err=update_adv(4, &param, remote_ctrl_data, BT_LE_EXT_ADV_START_PARAM(100,0));
		if(err) {
			err_stamp=err;
			//printf("%s LN%u, adv 4 try again\n",__FUNCTION__,__LINE__);
			if(k_can_yield()) k_yield();
		}
	} while(err);
	//if(err_stamp) printf("%s LN%u, adv 4 sent in %lld msec\n",__FUNCTION__,__LINE__,k_uptime_delta(&start_tm));
	
	return false;

}


bool rc_msg_outgoing(void * data_p, size_t sz)
{
	remote_ctrl_data[1].type=BT_DATA_MANUFACTURER_DATA;
	remote_ctrl_data[1].data_len=sz;
	remote_ctrl_data[1].data=data_p;
	
	struct bt_le_adv_param param={.id=0,
		.sid=0, .secondary_max_skip=0, .peer=NULL,
		.options=BT_LE_ADV_OPT_USE_IDENTITY 
				| BT_LE_ADV_OPT_USE_TX_POWER 
				| BT_LE_ADV_OPT_NO_2M 
				| BT_LE_ADV_OPT_EXT_ADV,
		.interval_min=PARAM_ADV_INT_MIN_3,
		.interval_max=PARAM_ADV_INT_MAX_3 };
	//blocking_adv(4);
	int err;
	int err_stamp=0;
	int64_t start_tm=k_uptime_get();
	do {
		//printf("%s LN%u, adv 4, data :%02x %2d %p, %02x %2d %p,\n",__FUNCTION__,__LINE__,remote_ctrl_data[0].type,remote_ctrl_data[0].data_len,remote_ctrl_data[0].data,remote_ctrl_data[1].type,remote_ctrl_data[1].data_len,remote_ctrl_data[1].data);
		err=update_adv(4, &param, remote_ctrl_data, BT_LE_EXT_ADV_START_PARAM(200,0));
		if(err) {
			err_stamp=err;
			//printf("%s LN%u, adv 4 try again\n",__FUNCTION__,__LINE__);
			if(k_can_yield()) k_yield();
		}
	} while(err);
	//if(err_stamp) printf("%s LN%u, adv 4 sent in %lld msec\n",__FUNCTION__,__LINE__,k_uptime_delta(&start_tm));
	
	return false;
}


__attribute__((weak)) bool rc_msg_incomming(void * data_p, size_t sz, bt_addr_le_t *addr_p)
{
//	printf("%s, LN%u, rc_msg_incomming()\n",__FILE_NAME__,__LINE__);
	return false;
}

__attribute__((weak)) bool rm_msg_incomming(void * data_p, size_t sz, bt_addr_le_t *addr_p)
{
//	printf("%s, LN%u, rc_msg_incomming()\n",__FILE_NAME__,__LINE__);
	return false;
}

bool set_rc_party(bt_addr_le_t * addr_p)
{
	bool empty_party=bt_addr_le_eq(BT_ADDR_LE_ANY,&remote_ctrl_party)|bt_addr_le_eq(BT_ADDR_LE_NONE,&remote_ctrl_party);
	bool chk_addr;

	if(NULL==addr_p) chk_addr=false;
	else chk_addr=!(bt_addr_le_eq(BT_ADDR_LE_ANY,addr_p)|bt_addr_le_eq(BT_ADDR_LE_NONE,addr_p));

	if(empty_party && chk_addr) remote_ctrl_party=*addr_p, rc_party=true;
	else chk_addr=false;

	return chk_addr;
}

bool chk_rc_party(bt_addr_le_t * addr_p)
{
	bool empty_party=bt_addr_le_eq(BT_ADDR_LE_ANY,&remote_ctrl_party)|bt_addr_le_eq(BT_ADDR_LE_NONE,&remote_ctrl_party);
	if(NULL!=addr_p) *addr_p=remote_ctrl_party;
	return !empty_party;
}

bool clr_rc_party(void)
{
	remote_ctrl_party=*BT_ADDR_LE_ANY;
	rc_party=false;
	blocking_adv(4);
	//printf("%s LN%u, update_adv\n",__func__,__LINE__);
	update_adv(4,NULL,NULL,NULL);
	return false;
}

static bool remote_ctrl_parser(struct bt_data *data, void *user_data)
{
	struct DEV_FOUND_PARM_ST * dev_chr_p=((struct DEV_FOUND_PARM_ST *)user_data);
	struct bt_hci_evt_le_ext_advertising_info * adv_info_p=dev_chr_p->adv_info_p;
	if(BT_DATA_FLAGS==data->type) {
		if(0==dev_chr_p->step_raw) dev_chr_p->step_flag++;
		else dev_chr_p->step_fail;
	} else if(1==dev_chr_p->step_flag && BT_DATA_MANUFACTURER_DATA==data->type) {
		DEV_RESOURCE_MSG_FRAME_ST *msg_head_p=((DEV_RESOURCE_MSG_FRAME_ST *)(data->data));
		if(MANUFACTURER_ID!=msg_head_p->man_id || *(uint32_t *)(NRF_FICR->DEVICEADDR)!=msg_head_p->node_id) {}
		else if( LOSS_TEST_REMOTE_MONITOR_ID==msg_head_p->form_id ) {
			if(rm_msg_incomming(data->data,data->data_len,&adv_info_p->addr)) dev_chr_p->step_success=1;
		}
		//else if(rc_party && !bt_addr_le_eq(&adv_info_p->addr,&remote_ctrl_party)) { }
		else if( LOSS_TEST_REMOTE_CTRL_TERM_ID==msg_head_p->form_id 
				|| LOSS_TEST_REMOTE_CTRL_KEYPAD_ID==msg_head_p->form_id 
				|| LOSS_TEST_REMOTE_MONITOR_ID==msg_head_p->form_id ) {
			if(rc_msg_incomming(data->data,data->data_len,&adv_info_p->addr)) dev_chr_p->step_success=1;
			else dev_chr_p->step_fail=1;
		}
	}
	return (dev_chr_p->step_completed)?false:true;
}

// BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT;
// struct bt_hci_evt_le_ext_advertising_info;
// struct bt_hci_evt_le_ext_advertising_report

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	DEVICE_INFO_ST *src_devinfo;
	int8_t idx=-1;

	// LE Extended Advertising Report event
	struct bt_hci_evt_le_ext_advertising_info *adv_info=NULL;
	adv_info=(struct bt_hci_evt_le_ext_advertising_info *)((ptrdiff_t)ad->data - sizeof(struct bt_hci_evt_le_ext_advertising_info));


	if(1==adv_info->prim_phy && 2==adv_info->sec_phy) idx=0;
	else if(1==adv_info->prim_phy && 1==adv_info->sec_phy) idx=1;
	else if(3==adv_info->prim_phy && 3==adv_info->sec_phy) idx=2;
	else if(1==adv_info->prim_phy && 0==adv_info->sec_phy) idx=3;
	else return;

	if(0!=scanner_task_tgr(0))  { if(9999999ul<++rcv_stats[idx]) rcv_stats[idx]=9999999ul; }
	if(0!=envmon_task_tgr(0)) {
		REC_RSSI_STAMP loc_rec={.expired_tm=k_uptime_get()+60000,.rssi=MIN(20,rssi)};
		env_rssi_rec[idx][(ARRAY_SIZE(env_rssi_rec[idx])-1)&env_rssi_idx[idx]++]=loc_rec;
		if(9999999ul<++env_stats[idx])
			env_stats[idx]=9999999ul;
	}
	
	struct net_buf_simple_state buf_state;
	dev_chr.flw_cnt++;
	dev_chr.step_raw=0;
	dev_chr.adv_info_p=((ptrdiff_t)ad->data - sizeof(struct bt_hci_evt_le_ext_advertising_info));
	if(0!=numcst_task_tgr(0)) {
		net_buf_simple_save(ad,&buf_state);
		bt_data_parse(ad,numcast_parser,&dev_chr);
		if(dev_chr.step_success) return;
		net_buf_simple_restore(ad,&buf_state);
	}
	
	if(0!=sender_task_tgr(0) || 0!=scanner_task_tgr(0)) {
		dev_chr.step_raw=0;
		net_buf_simple_save(ad,&buf_state);
		bt_data_parse(ad,test_form_parser,&dev_chr);
		if(dev_chr.step_success) return;
		net_buf_simple_restore(ad,&buf_state);
	}

	if(1==idx) {
		dev_chr.step_raw=0;
		bt_data_parse(ad,remote_ctrl_parser,&dev_chr);
		//if(dev_chr.step_success) {
		//	char addr_str[32];
		//	bt_addr_le_to_str(addr,addr_str,31);
		//	printf("src addr :%s\n",addr_str);
		//}
	}


//bt_data_parse()
//bt_data_get_len()
//bt_data_serialize()
}

const static struct bt_le_scan_param passive_scan_phy_1m=
{
	.type=BT_LE_SCAN_TYPE_PASSIVE
	, .options=BT_LE_SCAN_OPT_NONE
	, .interval=BT_GAP_SCAN_FAST_INTERVAL
	, .window=BT_GAP_SCAN_FAST_INTERVAL //BT_GAP_SCAN_FAST_WINDOW
};

const static struct bt_le_scan_param passive_scan_phy_coded=
{
	.type=BT_LE_SCAN_TYPE_PASSIVE
	, .options=BT_LE_SCAN_OPT_CODED|BT_LE_SCAN_OPT_NO_1M //BT_LE_SCAN_OPT_NONE//
	, .interval=BT_GAP_SCAN_FAST_INTERVAL
	, .window=BT_GAP_SCAN_FAST_INTERVAL //BT_GAP_SCAN_FAST_WINDOW
};

const static struct bt_le_scan_param passive_scan_phy_all=
{
	.type=BT_LE_SCAN_TYPE_PASSIVE
	, .options=BT_LE_SCAN_OPT_CODED
	, .interval=BT_GAP_SCAN_FAST_INTERVAL
	, .window=BT_GAP_SCAN_FAST_WINDOW
};

const static struct bt_le_scan_param numcast_scan_phy_all=
{
	.type=BT_LE_SCAN_TYPE_PASSIVE
	, .options=BT_LE_SCAN_OPT_CODED
	, .interval=BT_GAP_SCAN_FAST_INTERVAL_CODED
	, .window=BT_GAP_SCAN_FAST_INTERVAL_CODED/2
};

const static const struct bt_le_scan_param *passive_scan_parms[]={&passive_scan_phy_all,&passive_scan_phy_1m,&passive_scan_phy_coded,&numcast_scan_phy_all};

static void passive_scan_method(int8_t method)
{
    if(!svc_init_success) return;
	static int8_t scan_method=-1;
	int err=0;
	if(0>method) method=-1, bt_le_scan_stop();
	else if(method!=scan_method) {
		bt_le_scan_stop();
		method=MAX(MIN(ARRAY_SIZE(passive_scan_parms)-1,method),0);
		err = bt_le_scan_start( passive_scan_parms[method], device_found);
	}

	if (err) {
		printk("Scanning failed (err %d, %s)\n", err, strerror(-err));
		return;
	}
	scan_method=method;
}

#undef CONFIG_BT_EXT_ADV_MAX_ADV_SET
#undef CONFIG_BT_DEVICE_NAME_MAX
#undef CONFIG_BT_DEVICE_NAME

GET_SYM_IMPL(CONFIG_BT_EXT_ADV_MAX_ADV_SET)
GET_SYM_IMPL(CONFIG_BT_DEVICE_NAME_MAX)
GET_SYM_IMPL(CONFIG_BT_DEVICE_NAME)
