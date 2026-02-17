#ifndef __LOSSTST_SVC_H__
#define __LOSSTST_SVC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct TEST_PARM
{
	int8_t txpwr;
	int8_t interval_idx;
	int8_t count_idx;
	bool (*envmon_abort)(void);
	bool (*sender_abort)(void);
	bool (*scanner_abort)(void);
	bool (*numcast_abort)(void);
	//void (*txpower_setup)(int8_t);
	unsigned phy_2m:1;
	unsigned phy_1m:1;
	unsigned phy_s8:1;
	unsigned phy_ble4:1;
	unsigned inhibit_ch37:1;
	unsigned inhibit_ch38:1;
	unsigned inhibit_ch39:1;
	unsigned non_ANONYMOUS:1;
	unsigned ignore_rcv_resp:1;
};

extern void sender_setup(struct TEST_PARM * parm_p);
extern void scanner_setup(struct TEST_PARM * parm_p);
extern void numcast_setup(struct TEST_PARM * parm_p);
extern void envmon_setup(struct TEST_PARM * parm_p);

extern int losstst_init(void);
extern int losstst_sender(void);
extern int losstst_scanner(void);
extern int losstst_numcast(void);
extern int losstst_envmon(void);

extern int8_t numcst_task_tgr(int8_t set);
extern int8_t scanner_task_tgr(int8_t set);
extern int8_t sender_task_tgr(int8_t set);
extern int8_t envmon_task_tgr(int8_t set);
extern int8_t numcst_task_status(void) ;
extern int8_t scanner_task_status(void);
extern int8_t sender_task_status(void) ;
extern int8_t envmon_task_status(void) ;

// param   : 0 enquire idx val
//         :>0 increase idx val
//         :<0 decrease idx val
// return  : idx val
extern uint8_t enum_adv_interval_idx(int8_t);

// param   :0..10 idx val ( adv_interval_lower(enum_adv_interval_idx(0)) )
// return  : minimum interval_tm (millisecond)
extern uint16_t adv_interval_lower(uint8_t);

// param   :0..10 idx val ( adv_interval_upper(enum_adv_interval_idx(0)) )
// return  : maximum interval_tm (millisecond)
extern uint16_t adv_interval_upper(uint8_t);


// param   : process vale (dBm)
// return  : set value (dBm)
extern int8_t get_txpower_sv(int8_t process_value);

// param   : set vale (dBm)
// return  : effect tx power (dBm)
extern int8_t get_txpower_pv(int8_t set_value);

// param   : tx power level (dBm)
// return  : effect tx power (dBm)
extern int8_t get_txpower_effect(int8_t tx_power_level);

// param   : 0 enquire idx val
//         :>0 increase idx val
//         :<0 decrease idx val
// return  : tx power (dBm)
extern int8_t enum_txpower(int8_t);

// param   : 0 enquire idx val
//         :>0 increase idx val
//         :<0 decrease idx val
// return  : idx val
extern uint8_t enum_totalnum_idx(int8_t);

// param   : 0 enquire idx val
//         :>0 increase idx val
//         :<0 decrease idx val
// return  : total number (for sender/xmt)
extern uint16_t enum_totalnum(int8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :subtotal number
extern uint16_t xmt_ratio_lower(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :total number
extern uint16_t xmt_ratio_upper(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :subtotal number
extern uint16_t rcv_ratio_lower(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :subtotal number
extern uint16_t rcv_ratio_upper(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :rssi value (average, 20..-127 dBm)
extern int8_t rcv_rssi_average(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :rssi value (minimum, 20..-127 dBm)
extern int8_t rcv_rssi_lower(uint8_t);// param   :0 PHY_1M_2M

//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :rssi value (maxmum, 20..-127 dBm)
extern int8_t rcv_rssi_upper(uint8_t);


extern int8_t envmon_rssi_average(uint8_t idx);
extern int8_t envmon_rssi_lower(uint8_t idx);
extern int8_t envmon_rssi_upper(uint8_t idx);

// return  :rssi value (average, 20..-127 dBm)
extern int8_t numcst_rssi_average(void);

// return  :rssi value (minimum, 20..-127 dBm)
extern int8_t numcst_rssi_lower(void);

// return  :rssi value (minimum, 20..-127 dBm)
extern int8_t numcst_rssi_upper(void);

// param   :field, 0..3
//         :digit, setval 0..999, increment >1000, no change <0
// return  :digit, 0..999
extern int16_t numcst_setval(uint8_t,int16_t);

// param   :field, 0..3
// return  :digit, 0..999
extern int16_t numcst_rxval(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :receiver progress (0:idle, 1:pre-run, 2:runing, 3: post-run)
extern int8_t rcv_state_mark(uint8_t);
extern int8_t rcv_state_progress(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :sender progress (0:idle, 1:pre-run, 2:runing, 3: post-run)
extern int8_t snd_state_mark(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :received packet count (for receive/scanner progress)
extern uint32_t rcv_stats_val(uint8_t);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :received packet count (for environment progress)
extern uint32_t env_stats_val(uint8_t);

// clear received packet counter
extern void env_stats_clr(void);

// enquire sender id
extern uint16_t sender_id(void);
extern uint8_t sender_id_upper(void);
extern uint8_t sender_id_lower(void);

// enquire sender tx power
extern int8_t sender_txpower(void);

// receiver/scanner side don't respond
extern bool get_uni_cast_method(void);
extern void chg_uni_cast_method(void);

// enquire/change soc powewr regulator feature
// return  : 0, linear method; >0, switching method; <0, unknown regulator type
extern int chg_soc_dcdc(void);
extern int get_soc_dcdc(void);

// enquire/change number cast feature
// return  : false, manual method; true, auto/continuous method;
extern bool get_number_cast_auto(void);
extern bool chg_number_cast_auto(void);

// remote side node-id, upper field
extern uint8_t numcst_src_id_upper(void);

// remote side node-id, lower field
extern uint8_t numcst_src_id_lower(void);

// param   :0 PHY_1M_2M
//         :1 PHY_1M_1M
//         :2 PHY_S8_S8 (coded)
//         :3 PHY_BLEv4
// return  :packet arrive
extern bool numcast_phy_mark(uint8_t);

extern bool get_cfg_ch37(void);
extern bool get_cfg_ch38(void);
extern bool get_cfg_ch39(void);
extern bool get_cfg_ANONYMOUS(void);
extern bool get_cfg_NON_ANONYMOUS(void);
extern bool get_cfg_phy2m(void);
extern bool get_cfg_phy1m(void);
extern bool get_cfg_phy8s(void);
extern bool get_cfg_phyBLEv4(void);
extern bool get_cfg_phy_sel(uint8_t idx);

extern bool chg_cfg_ch37(void);
extern bool chg_cfg_ch38(void);
extern bool chg_cfg_ch39(void);
extern bool chg_cfg_phy2m(void);
extern bool chg_cfg_phy1m(void);
extern bool chg_cfg_phy8s(void);
extern bool chg_cfg_phyBLEv4(void);
extern bool chg_cfg_ANONYMOUS(void);

// local node id, upper field
extern uint8_t node_id_upper(void);
// local node id, upper field
extern uint8_t node_id_lower(void);


extern bool rm_rush_msg_outgoing(void * data_p, size_t sz);
extern bool rc_rush_msg_outgoing(void * data_p, size_t sz);
extern bool rm_msg_outgoing(void * data_p, size_t sz);
extern bool rc_msg_outgoing(void * data_p, size_t sz);
extern bool chk_rc_party(bt_addr_le_t * addr_p);
extern bool set_rc_party(bt_addr_le_t * addr_p);
extern bool clr_rc_party(void);

#ifdef __cplusplus
}
#endif

#endif //  __LOSSTST_SVC_H__