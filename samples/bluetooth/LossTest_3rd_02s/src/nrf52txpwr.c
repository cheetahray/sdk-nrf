
#include "nrf52txpwr.h"

mpsl_tx_power_envelope_t txpwr_setting[6];
int8_t txpwr_envelope[6]={-1,-1,-1,-1,-1,-1};
//CONFIG_BT_CTLR_TX_PWR_ANTENNA
//CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL
//CONFIG_BT_CTLR_PHY_2M
//CONFIG_BT_CTLR_PHY_CODED

#define GET_SYM_DECL(CFG_SYM) uint32_t get_GSYM_ ##CFG_SYM(void) __attribute__((weak));
#define GET_SYM_VAL(CFG_SYM) get_GSYM_ ##CFG_SYM()
#define GET_SYM_IMPL(CFG_SYM) const uint32_t CFG_SYM __attribute__((weak)); uint32_t get_GSYM_ ##CFG_SYM (void) { return (uint32_t)&CFG_SYM ; }

GET_SYM_DECL(CONFIG_BT_CTLR_TX_PWR_ANTENNA)
GET_SYM_DECL(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
GET_SYM_DECL(CONFIG_BT_CTLR_PHY_2M)
GET_SYM_DECL(CONFIG_BT_CTLR_PHY_CODED)

int32_t mpsl_tx_power_channel_map_set(const mpsl_tx_power_envelope_t *const p_envelope) __attribute__((weak));
int32_t mpsl_tx_power_channel_map_set(const mpsl_tx_power_envelope_t *const p_envelope) { return 0; }

int32_t txpwr_set(mpsl_tx_power_t pwr)
{
	int32_t err;
	mpsl_tx_power_t setval;
	int8_t phy_select=MPSL_PHY_BLE_1M;
	int8_t envelope_next;
	int8_t envelope_prev;
	int8_t sweep;
	if(0==GET_SYM_VAL(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)) return -1;
	setval=MIN((CONFIG_BT_CTLR_TX_PWR_ANTENNA),pwr);
	//printk("CHK array size %u,%u\n",ARRAY_SIZE(txpwr_envelope),ARRAY_SIZE(txpwr_setting));
	do {
		switch(phy_select) {
			case MPSL_PHY_BLE_1M:
				break;
			case MPSL_PHY_BLE_2M:
			  #if(1)
				if(0==GET_SYM_VAL(CONFIG_BT_CTLR_PHY_2M)) continue;
			  #else
				#if !defined(CONFIG_BT_CTLR_PHY_2M)
				continue;
				#endif
			  #endif
				break;
			case MPSL_PHY_BLE_LR125Kbit:
			  #if(1)
				if(0==GET_SYM_VAL(CONFIG_BT_CTLR_PHY_2M)) continue;
			  #else
				#if !defined(CONFIG_BT_CTLR_PHY_CODED)
				continue;
				#endif
			  #endif
				break;
			case MPSL_PHY_BLE_LR500Kbit:
			  #if(1)
				if(0==GET_SYM_VAL(CONFIG_BT_CTLR_PHY_2M)) continue;
			  #else
				#if !defined(CONFIG_BT_CTLR_PHY_CODED)
				continue;
				#endif
			  #endif
				break;
			case MPSL_PHY_Ieee802154_250Kbit:
				break;
			default:
				continue;
		}
		envelope_prev=-1;
		for(sweep=0 ; ARRAY_SIZE(txpwr_envelope)>sweep ; sweep++) {
			if(phy_select==txpwr_envelope[sweep]) {
				envelope_prev=sweep;
				break;
			}
		}
		for(sweep=0 ; ARRAY_SIZE(txpwr_envelope)>sweep ; sweep++) {
			if(-1==txpwr_envelope[sweep]) {
				envelope_next=sweep;
				break;
			}
		}
		txpwr_setting[envelope_next].phy=phy_select;
		txpwr_setting[envelope_next].envelope.tx_power_ble[0]=setval;
		memset(&txpwr_setting[envelope_next].envelope.tx_power_ble[0],setval,sizeof(txpwr_setting[0].envelope));
		err=mpsl_tx_power_channel_map_set(&txpwr_setting[envelope_next]);
		//printk("phy_%u_prev(%d), phy_%u_next(%d), setval(%ddBm), %s\n",phy_select,envelope_prev,phy_select,envelope_next,setval,strerror(-err));
		if(err) break;
		txpwr_envelope[envelope_next]=phy_select;
		if(-1!=envelope_prev) txpwr_envelope[envelope_prev]=-1;
	} while(MPSL_PHY_Ieee802154_250Kbit >= ++phy_select);
	return err;
}


#undef CONFIG_BT_CTLR_TX_PWR_ANTENNA
#undef CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL
#undef CONFIG_BT_CTLR_PHY_2M
#undef CONFIG_BT_CTLR_PHY_CODED

GET_SYM_IMPL(CONFIG_BT_CTLR_TX_PWR_ANTENNA)
GET_SYM_IMPL(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
GET_SYM_IMPL(CONFIG_BT_CTLR_PHY_2M)
GET_SYM_IMPL(CONFIG_BT_CTLR_PHY_CODED)
