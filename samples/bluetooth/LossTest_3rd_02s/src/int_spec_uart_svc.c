#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include "int_spec_uart_svc.h"

static int spec_uart_rcv_enable(void);
static int spec_uart_rcv_disable(void);
static int spec_uart_xmt_enable(void);
static int spec_uart_xmt_disable(void);
static int spec_uart_tx_break(void);
static int spec_uart_tx_make(void);
static void spec_uart_cb(const struct device *dev, void *user_data);

struct io_pin_st {
	#if((HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)||(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD))
	  pinctrl_soc_pin_t   pin_sel; // pin,port,pull,drv,inv
	  uint32_t            mask;
	  
	  #if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	  NRF_GPIO_Type       *gpio_p;
	  
	  #elif(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	  uint32_t            *set_val_p;
	  uint32_t            *clr_val_p;
	  
	  #endif
	#endif
  
	#if(HAZARD_UART_BRK != UART_SEND_BREAK_METHOD)
	  uint32_t            pin_num; // pin,port
	#endif
	
  
	#if(0)
	  uint32_t            *set_input_p;
	  uint32_t            *set_output_p;
	#endif
  
	#if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	  const struct gpio_dt_spec *gpio_dt_p;
	#endif
};

typedef struct {
	NRF_UARTE_Type  *uart_p;
	struct io_pin_st tx; // TXD
	struct io_pin_st rx; // RXD
  #if(0)
	struct io_pin_st rts;
	struct io_pin_st cts;
  #endif
	struct io_pin_st xmt; // EN/nEN transmitter
	struct io_pin_st rcv; // EN/nEN receiver
	volatile union __attribute((packed)) {
		uint16_t     xmt_flags;
		struct {
			uint8_t  brk_cnt;
			unsigned brk_mab:1;
			unsigned xmt_abort:1;
			unsigned xmt_dly:1;
			unsigned xmt_run:1;
		};
	};
	unsigned         rcv_run:1;
	unsigned         half_duplex:1;
	unsigned         rcv_ovrflw:1;
	uint32_t         char_tm; // microsecond
    uint32_t         rcv_tm_stamp; // ticks
    uint32_t         xmt_tm_stamp; // ticks
} uart_ctrl_info;


#define UART_CTRL_FULL_DUPLEX 1
#define UART_CTRL_HALF_DUPLEX 2
#ifndef INT_UART_XMTBUF_SZ
  #define INT_UART_XMTBUF_SZ 512
#endif
#define INT_UART_XMTBUF_MSK (INT_UART_XMTBUF_SZ-1)
#define INT_UART_XMTBUF_CHK (INT_UART_XMTBUF_SZ&INT_UART_XMTBUF_MSK)
#ifndef INT_UART_RCVBUF_SZ
  #define INT_UART_RCVBUF_SZ 512
#endif
#define INT_UART_RCVBUF_MSK (INT_UART_RCVBUF_SZ-1)
#define INT_UART_RCVBUF_CHK (INT_UART_RCVBUF_SZ&INT_UART_RCVBUF_MSK)
#if(0!=INT_UART_XMTBUF_CHK || 0!=INT_UART_RCVBUF_CHK)
#error unsuitable buffer size
#endif

static struct device *spec_uart_p;

uint8_t int_uart_xmtbuf[INT_UART_XMTBUF_SZ];
uint8_t int_uart_rcvbuf[INT_UART_RCVBUF_SZ];
uint32_t xmt_put_idx,rcv_get_idx;
bool int_uart_rcv_ovrflw;
volatile uint32_t xmt_get_idx,rcv_put_idx;
volatile uart_ctrl_info spec_uart_ctrl_info;
uint64_t rcv_tick_stamp,rcv_tick_mark;


void spec_uart_rcv_begin(void)
{
	spec_uart_ctrl_info.rcv_run=1;
	uart_irq_rx_enable(spec_uart_p);
    spec_uart_rcv_enable();
}


void spec_uart_rcv_end(void)
{
	spec_uart_ctrl_info.rcv_run=0;
    spec_uart_rcv_disable();
}


bool spec_uart_xmt_complete(void)
{
	return (spec_uart_ctrl_info.xmt_flags)?false:true;
}


void spec_uart_xmt_drain(void)
{
    while(spec_uart_ctrl_info.xmt_flags) { if(k_can_yield()) k_yield(); }
}


bool spec_uart_throttle(uint32_t tm_us)
{
	bool retval=false;
	k_timeout_t threadhold=K_USEC(tm_us);
	uint32_t threhold_r;
	uint32_t threhold_x;
	uint16_t xmt_flags;
	int key=irq_lock();
	threhold_r=spec_uart_ctrl_info.rcv_tm_stamp;
	threhold_x=spec_uart_ctrl_info.xmt_tm_stamp;
	xmt_flags=spec_uart_ctrl_info.xmt_flags;
	irq_unlock(key);
	if(0==xmt_flags && 0==threhold_r && 0==threhold_x) retval=true;
	else {
		uint32_t stamp=k_uptime_ticks();
		if(threhold_r) {
			threhold_r+=(uint32_t)threadhold.ticks;
			threhold_r-=stamp;
			if(0>=((int32_t)threhold_r))
				spec_uart_ctrl_info.rcv_tm_stamp=threhold_r=0;
		}
		if(0==xmt_flags && 0!=threhold_x) {
			threhold_x+=(uint32_t)threadhold.ticks;
			threhold_x-=stamp;
			if(0>=((int32_t)threhold_x))
				spec_uart_ctrl_info.xmt_tm_stamp=threhold_x=0;
		}
		if(0==xmt_flags && 0==threhold_r && 0==threhold_x) retval=true;
	}
	
	return retval;
}

void spec_uart_xmt_flush(void)
{
    int key=irq_lock();
    if(spec_uart_ctrl_info.xmt_flags) {
        spec_uart_ctrl_info.xmt_abort=1;
    }
    irq_unlock(key);
    spec_uart_xmt_drain();
}


void spec_uart_rcv_flush(void)
{
    int key=irq_lock();
    spec_uart_ctrl_info.rcv_ovrflw=0;
    spec_uart_ctrl_info.rcv_tm_stamp=0;
    rcv_get_idx=rcv_put_idx;
    irq_unlock(key);
}


int spec_uart_read(void * buf_p, size_t len)
{
	int retval=0;
	uint8_t *dst_p;
	uint32_t cpy_sz;
	uint32_t sz;
	uint32_t idx;
	uint32_t xdet;
	if(NULL==(dst_p=buf_p)) return -1;
	if(0==len /*|| rcv_put_idx==rcv_get_idx*/) return 0;

	do {
		idx = rcv_get_idx;
		xdet = sz = rcv_put_idx;
		xdet ^= idx;
		xdet &= ~INT_UART_RCVBUF_MSK;
		if(xdet)
			sz = INT_UART_RCVBUF_SZ - (idx & INT_UART_RCVBUF_MSK);
		else
			sz -= rcv_get_idx;
		
		// nonblocking method
		if(0==(cpy_sz=MIN(sz,len))) {
            int key=irq_lock();
            if(rcv_put_idx==rcv_get_idx)
                // spec_uart_ctrl_info.rcv_tm_stamp=0;
                spec_uart_ctrl_info.rcv_ovrflw=0;
            irq_unlock(key);
            break;
        }
		
		idx &= INT_UART_RCVBUF_MSK;
		memcpy(dst_p, int_uart_rcvbuf + idx, cpy_sz);
		len -= cpy_sz;
		dst_p += cpy_sz;
		retval += cpy_sz;
		rcv_get_idx += cpy_sz;
	} while(0!=len);

	return retval;
}


int spec_uart_getc(void)
{
	int retval=-1;
	if(rcv_put_idx!=rcv_get_idx)
		retval=int_uart_rcvbuf[((rcv_get_idx++) & INT_UART_RCVBUF_MSK)];
    else {
        int key=irq_lock();
        if(rcv_put_idx==rcv_get_idx)
            // spec_uart_ctrl_info.rcv_tm_stamp=0;
            spec_uart_ctrl_info.rcv_ovrflw=0;
        irq_unlock(key);
    }
	return retval;
}


int spec_uart_write(void * buf_p, size_t len)
{
	int retval = 0;
	uint8_t *src_p;
	uint32_t cpy_sz;
	uint32_t sz;
	uint32_t idx;
	uint32_t xdet;
	if(NULL == (src_p = buf_p)) return -1;
	if(0 == len) return 0;

	do {
		idx = xmt_put_idx;
		xdet = sz = xmt_get_idx;
		xdet ^= idx;
		xdet &= ~INT_UART_XMTBUF_MSK;

		if(xdet)
			sz = sz + INT_UART_XMTBUF_SZ - idx;
		else
			sz = INT_UART_XMTBUF_SZ - (idx & INT_UART_XMTBUF_MSK);
		
		idx &= INT_UART_XMTBUF_MSK;
		if(0==(cpy_sz=MIN(sz,len))) {
		  #if(1) // blocking method
			//TOGGLE_SIG2();
			if(k_can_yield())
				k_yield();
			continue;
		  #else // non-blocking method
			uart_irq_tx_enable(spec_uart_p);
			break;
		  #endif
		}

		memcpy(int_uart_xmtbuf + idx, src_p ,cpy_sz);
		len -= cpy_sz;
		src_p += cpy_sz;
		retval += cpy_sz;
		xmt_put_idx += cpy_sz;
		uart_irq_tx_enable(spec_uart_p);
	} while(0 != len);
	return retval;
}


int spec_uart_putc(int ch)
{
	uint32_t diff_idx = xmt_put_idx - xmt_get_idx;
	if(diff_idx == INT_UART_XMTBUF_SZ) return -1;
	int_uart_xmtbuf[INT_UART_XMTBUF_MSK & (xmt_put_idx++)]=ch;
	if(0 == (diff_idx&(~0b11 & INT_UART_XMTBUF_MSK)))
		uart_irq_tx_enable(spec_uart_p);
	return ch;
}


static void gpiodt_2_pin_st(struct gpio_dt_spec * gpiodt_p,struct io_pin_st * pin_p)
{
	NRF_GPIO_Type * gpio_p;
	gpio_dt_flags_t dt_flags;
	uint32_t port_num;
	uint32_t pinmap;
	uint32_t pin_num;
	pinctrl_soc_pin_t pin_sel;
	uint32_t *set_val_p;
	uint32_t *clr_val_p;
	uint32_t *set_input_p;
	uint32_t *set_output_p;

	// /ncs/v2.7.0/zephyr/drivers/gpio/gpio_nrfx.c >> LN 23
	gpio_p = *(NRF_GPIO_Type **)(sizeof(struct gpio_driver_config)+((ptrdiff_t)gpiodt_p->port->config)),
	port_num = ((ptrdiff_t)gpio_p-(ptrdiff_t)NRF_P0) / ((ptrdiff_t)NRF_P1-(ptrdiff_t)NRF_P0);
	pin_sel = pin_num = NRF_PIN_PORT_TO_PIN_NUMBER(gpiodt_p->pin,port_num);
	dt_flags = gpiodt_p->dt_flags;
	pin_sel |= (((GPIO_PULL_UP&dt_flags)? NRF_PULL_UP : ((GPIO_PULL_DOWN&dt_flags)? NRF_PULL_DOWN : NRF_PULL_NONE)) << NRF_PULL_POS);
	pin_sel |= (((GPIO_ACTIVE_LOW&dt_flags)?1:0) << NRF_INVERT_POS);
	dt_flags &= (GPIO_OPEN_DRAIN|GPIO_OPEN_SOURCE);
	pin_sel |= (((GPIO_OPEN_DRAIN==dt_flags)?NRF_DRIVE_S0D1:((GPIO_OPEN_SOURCE==dt_flags)?NRF_DRIVE_D0S1:0)) << NRF_DRIVE_POS);
	pinmap = 1 << NRF_PIN_NUMBER_TO_PIN(pin_num);
	set_val_p = (void *)(offsetof(NRF_GPIO_Type,OUTSET) + (ptrdiff_t)gpio_p);
	clr_val_p = (void *)(offsetof(NRF_GPIO_Type,OUTCLR) + (ptrdiff_t)gpio_p);
	set_input_p = (void *)(offsetof(NRF_GPIO_Type,DIRCLR) + (ptrdiff_t)gpio_p);
	set_output_p = (void *)(offsetof(NRF_GPIO_Type,DIRSET) + (ptrdiff_t)gpio_p);
	*pin_p=(struct io_pin_st){
	  #if((HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)||(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD))
		.pin_sel = pin_sel,
		.mask = pinmap,
	  #endif
	  #if(HAZARD_UART_BRK != UART_SEND_BREAK_METHOD)
		.pin_num = pin_num,
	  #endif
	  #if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
		.gpio_p = gpio_p,
	  #endif
	  #if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
		.set_val_p = set_val_p,
		.clr_val_p = clr_val_p,
	  #endif
		// .set_input_p = set_input_p,
		// .set_output_p = set_output_p,
	  #if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
		.gpio_dt_p = gpiodt_p
	  #endif
		 };
}


void spec_uart_init(const struct device *spec_uart, struct uart_config *uart_cfg_p, const struct gpio_dt_spec * gpio_list_p, int gpio_list_sz, int aux_flags)
{
	if(NULL!=spec_uart && NULL!=uart_cfg_p) {
		int err=uart_configure(spec_uart,uart_cfg_p);
		if(err) printf("LN%u, uart_configure %d (%s)\n",__LINE__,err,strerror(-err));
	}

	if(NULL!=spec_uart) {
		struct uart_config cfg_st;
		uart_config_get(spec_uart,&cfg_st);
		uint32_t char_tm=(1000000ul*(1+(cfg_st.data_bits+5)+((cfg_st.parity)?1:0)+((UART_CFG_STOP_BITS_1<cfg_st.stop_bits)?2:1)))/(cfg_st.baudrate);
		spec_uart_ctrl_info.char_tm=char_tm;
        spec_uart_p=spec_uart;
	}
	
	if(UART_CTRL_FULL_DUPLEX & aux_flags) spec_uart_ctrl_info.half_duplex=0;
	else
	if(UART_CTRL_HALF_DUPLEX & aux_flags) spec_uart_ctrl_info.half_duplex=1;

	if(NULL==spec_uart_ctrl_info.uart_p) {
		// ncs/v2.7.0/zephyr/drivers/serial/uart_nrfx_uarte.c >> LN 186, offset 0
		// ncs/v2.8.0/zephyr/drivers/serial/uart_nrfx_uarte.c >> LN 277, offset 0
		spec_uart_ctrl_info.uart_p = *((NRF_UARTE_Type**)(spec_uart->config));

		// ncs/v2.7.0/zephyr/drivers/serial/uart_nrfx_uarte.c >> LN 186, offset 12
		// ncs/v2.8.0/zephyr/drivers/serial/uart_nrfx_uarte.c >> LN 277, offset 12
		struct pinctrl_dev_config *pcfg_p = *(struct pinctrl_dev_config **)(3+(uint32_t *)(spec_uart->config));

		const struct pinctrl_state *state_p = pcfg_p->states;
		uint8_t state_cnt = pcfg_p->state_cnt;
		const pinctrl_soc_pin_t *pins_p = NULL;

		NRF_GPIO_Type * gpio_p;
		//gpio_dt_flags_t dt_flags;
		//uint32_t port_num;
		uint32_t pinmap;
		uint32_t pin_num,tx_pinmask=0;
		pinctrl_soc_pin_t pin_sel;
		uint32_t *set_val_p;
		uint32_t *clr_val_p, *tx_clr_val_p=NULL;
		uint32_t *set_input_p;
		uint32_t *set_output_p, *tx_set_output_p=NULL;
		
		for(uint8_t idx=0 ; idx < state_cnt ; idx++) {
			if(PINCTRL_STATE_DEFAULT==(idx+state_p)->id) {
				state_p+=idx;
				pins_p=state_p->pins;
				break;
			}
		}

		if(NULL != pins_p) {
			uint8_t pin_cnt = state_p->pin_cnt;
			for(uint8_t idx=0 ; idx < pin_cnt ; idx++) {
				pin_sel = *(idx+pins_p);
				pin_num = NRF_GET_PIN(pin_sel);
				pinmap = 1 << NRF_PIN_NUMBER_TO_PIN(pin_num);
				gpio_p = (NRF_GPIO_Type *)(NRF_PIN_NUMBER_TO_PORT(pin_num)*((ptrdiff_t)NRF_P1-(ptrdiff_t)NRF_P0) + (ptrdiff_t)NRF_P0);
				set_val_p = (void *)(offsetof(NRF_GPIO_Type,OUTSET) + (ptrdiff_t)gpio_p);
				clr_val_p = (void *)(offsetof(NRF_GPIO_Type,OUTCLR) + (ptrdiff_t)gpio_p);
				set_input_p = (void *)(offsetof(NRF_GPIO_Type,DIRCLR) + (ptrdiff_t)gpio_p);
				set_output_p = (void *)(offsetof(NRF_GPIO_Type,DIRSET) + (ptrdiff_t)gpio_p);
				struct io_pin_st pininfo ={
				  #if((HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)||(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD))
					.pin_sel = pin_sel,
					.mask = pinmap,
				  #endif
				  #if(HAZARD_UART_BRK != UART_SEND_BREAK_METHOD)
					.pin_num = pin_num,
				  #endif
				  #if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
					.gpio_p = gpio_p,
				  #endif
				  #if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
					.set_val_p = set_val_p,
					.clr_val_p = clr_val_p,
				  #endif
					// .set_input_p = set_input_p,
					// .set_output_p = set_output_p
					};

				//pinctrl_soc_pin_t pinfunc = NRF_GET_PIN(pin_sel);
				switch(NRF_GET_FUN(pin_sel)) {
				case NRF_FUN_UART_TX:
					spec_uart_ctrl_info.tx=pininfo;
					tx_set_output_p=set_output_p;
					tx_clr_val_p=clr_val_p;
					tx_pinmask=pinmap;
					break;
				case NRF_FUN_UART_RX:
					spec_uart_ctrl_info.rx=pininfo;
					break;
				  #if(0)
				case NRF_FUN_UART_RTS:
					spec_uart_ctrl_info.rts=pininfo;
					break;
				case NRF_FUN_UART_CTS:
					spec_uart_ctrl_info.cts=pininfo;
					break;
				  #endif
				}
			}
		}

		if(NULL!=gpio_list_p && 0!=gpio_list_sz && device_is_ready(gpio_list_p->port)) {
			// preset "TXEN/XMT" logic level
			gpio_pin_configure_dt(gpio_list_p,GPIO_OUTPUT_INACTIVE|gpio_list_p->dt_flags);
													// /ncs/2.x.x/zephyr/include/zephyr/dt-bindings/gpio/gpio.h
			//(gpio_dt_spec *)->dt_flags&GPIO_ACTIVE_LOW)   // bit_0:GPIO_ACTIVE_LOW,!GPIO_ACTIVE_HIGH
			//(gpio_dt_spec *)->dt_flags&GPIO_PUSH_PULL)    // bit_1:GPIO_SINGLE_ENDED,!GPIO_PUSH_PULL
			//(gpio_dt_spec *)->dt_flags&GPIO_SINGLE_ENDED) // bit_2:GPIO_LINE_OPEN_DRAIN,!GPIO_LINE_OPEN_SOURCE
			//(gpio_dt_spec *)->dt_flags&GPIO_PULL_UP)      // bit_4:GPIO_PULL_UP
			//(gpio_dt_spec *)->dt_flags&GPIO_PULL_DOWN)    // bit_5:GPIO_PULL_DOWN

													// /ncs/v2.x.x/zephyr/include/zephyr/drivers/gpio.h
															// bit_16:GPIO_INPUT
															// bit_17:GPIO_OUTPUT
															// bit_18:GPIO_OUTPUT_INIT_LOW
															// bit_19:GPIO_OUTPUT_INIT_HIGH
															// bit_20:GPIO_OUTPUT_INIT_LOGICAL
															// bit_21:GPIO_INT_DISABLE
															// bit_22:GPIO_INT_ENABLE
															// bit_23:GPIO_INT_LEVELS_LOGICAL
															// bit_24:GPIO_INT_EDGE
															// bit_25:GPIO_INT_LOW_0
															// bit_26:GPIO_INT_HIGH_1
			gpiodt_2_pin_st(gpio_list_p,&spec_uart_ctrl_info.xmt);
		}
		
		// spec_uart_ctrl[1]
		if(NULL!=gpio_list_p && 1<gpio_list_sz && device_is_ready((gpio_list_p+1)->port)) {
			// preset "RXEN/RCV" logic level
			gpio_pin_configure_dt((gpio_list_p+1),GPIO_OUTPUT_INACTIVE|((gpio_list_p+1))->dt_flags);
			
			gpiodt_2_pin_st((gpio_list_p+1),&spec_uart_ctrl_info.rcv);
		}
	  #if  (HAZARD_UART_BRK   == UART_SEND_BREAK_METHOD)
		// preset "TX break" logic level
		*tx_clr_val_p=tx_pinmask;
		*tx_set_output_p=tx_pinmask;

	  #elif(HAL_GPIO_UART_BRK == UART_SEND_BREAK_METHOD)
		// preset "TX break" logic level
		nrf_gpio_pin_clear(spec_uart_ctrl_info.tx.pin_num);
		nrf_gpio_cfg_output(spec_uart_ctrl_info.tx.pin_num);

	  #elif(STD_GPIO_UART_BRK == UART_SEND_BREAK_METHOD)
		if(NULL!=gpio_list_p && 2<gpio_list_sz && device_is_ready((gpio_list_p+2)->port)) {
			// preset "TX break" logic level
			gpio_pin_configure_dt((gpio_list_p+2),GPIO_OUTPUT_INACTIVE|(gpio_list_p+2)->dt_flags);
			
			gpiodt_2_pin_st((gpio_list_p+2),&spec_uart_ctrl_info.tx);
		}

	  #endif
        
	  uart_irq_callback_user_data_set(spec_uart,(uart_irq_callback_user_data_t)spec_uart_cb,NULL);
	}
}

static int spec_uart_rcv_enable(void)
{
	int retval=-ENOTSUP;
	//int retval=-EIO;
	struct io_pin_st *spec_pin_p=&spec_uart_ctrl_info.rcv;
  #if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	struct gpio_dt_spec *gpio_dt_p = spec_pin_p->gpio_dt_p;
  #endif

  #if((HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)||(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD))
	uint32_t            mask       = spec_pin_p->mask;
	uint32_t            invert     = (NRF_INVERT_MSK<<NRF_INVERT_POS)&spec_pin_p->pin_sel;
  #if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	NRF_GPIO_Type       *gpio_p    = spec_pin_p->gpio_p;
  #else // (HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	uint32_t            *set_val_p = spec_pin_p->set_val_p;
	uint32_t            *clr_val_p = spec_pin_p->clr_val_p;
	uint32_t            *val_dist_p= (invert)? clr_val_p : set_val_p;
  #endif
  #endif

  #if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hazard RXEN/RCV
		if(NULL!=val_dist_p) retval=1, *val_dist_p=mask; else retval=-ENOTSUP;
	}

  #elif(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hal_gpio RXEN/RCV
		if(NULL==gpio_p) retval=-ENOTSUP;
		else
		if(invert)
			retval=1, nrf_gpio_port_out_clear(gpio_p,mask);
		else
			retval=1, nrf_gpio_port_out_set(gpio_p,mask);
	}

  #elif(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	// std_gpio RXEN/RCV
		if(NULL==gpio_dt_p) retval=-ENOTSUP;
		else
		if(0==gpio_pin_set_dt(gpio_dt_p, 1)) retval=1;
	
  #endif
	
	return retval;
}


static int spec_uart_rcv_disable(void)
{
	int retval=-ENOTSUP;
	//int retval=-EIO;
	struct io_pin_st *spec_pin_p=&spec_uart_ctrl_info.rcv;
  #if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	struct gpio_dt_spec *gpio_dt_p = spec_pin_p->gpio_dt_p;
  #endif

  #if((HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)||(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD))
	uint32_t            mask       = spec_pin_p->mask;
	uint32_t            invert     = (NRF_INVERT_MSK<<NRF_INVERT_POS)&spec_pin_p->pin_sel;
  #if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	NRF_GPIO_Type       *gpio_p    = spec_pin_p->gpio_p;
  #else // (HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	uint32_t            *set_val_p = spec_pin_p->set_val_p;
	uint32_t            *clr_val_p = spec_pin_p->clr_val_p;
	uint32_t            *val_dist_p= (invert)? set_val_p : clr_val_p;
  #endif
  #endif

  #if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hazard !RXEN/RCV
		if(NULL!=val_dist_p) retval=1, *val_dist_p=mask; else retval=-ENOTSUP;
	}

  #elif(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hal_gpio !RXEN/RCV
		if(NULL==gpio_p) retval=-ENOTSUP;
		else
		if(invert)
			retval=0, nrf_gpio_port_out_set(gpio_p,mask);
		else
			retval=0, nrf_gpio_port_out_clear(gpio_p,mask);
	}

  #elif(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	// std_gpio !RXEN/RCV
		if(NULL==gpio_dt_p) retval=-ENOTSUP;
		else
		if(0==gpio_pin_set_dt(gpio_dt_p, 0)) retval=0;
	
  #endif

	return retval;
}


static int spec_uart_xmt_enable(void)
{
	int retval=-ENOTSUP;
	//int retval=-EIO;
	struct io_pin_st *spec_pin_p=&spec_uart_ctrl_info.xmt;
  #if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	struct gpio_dt_spec *gpio_dt_p = spec_pin_p->gpio_dt_p;
  #endif

  #if((HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)||(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD))
	uint32_t            mask       = spec_pin_p->mask;
	uint32_t            invert     = (NRF_INVERT_MSK<<NRF_INVERT_POS)&spec_pin_p->pin_sel;
  #if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	NRF_GPIO_Type       *gpio_p    = spec_pin_p->gpio_p;
  #else // (HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	uint32_t            *set_val_p = spec_pin_p->set_val_p;
	uint32_t            *clr_val_p = spec_pin_p->clr_val_p;
	uint32_t            *val_dist_p= (invert)? clr_val_p : set_val_p;
  #endif
  #endif


  #if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hazard TXEN/XMT
		if(NULL!=val_dist_p) retval=1, *val_dist_p=mask; else retval=-ENOTSUP;
	}

  #elif(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hal_gpio TXEN/XMT
		if(NULL==gpio_p) retval=-ENOTSUP;
		else
		if(invert)
			retval=1, nrf_gpio_port_out_clear(gpio_p,mask);
		else
			retval=1, nrf_gpio_port_out_set(gpio_p,mask);
	}

  #elif(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
		// std_gpio TXEN/XMT
		if(NULL==gpio_dt_p) retval=-ENOTSUP;
		else
		if(0==gpio_pin_set_dt(gpio_dt_p, 1)) retval=1;
	
  #endif
	
	return retval;
}


static int spec_uart_xmt_disable(void)
{
	int retval=-ENOTSUP;
	struct io_pin_st *spec_pin_p=&spec_uart_ctrl_info.xmt;
  #if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	struct gpio_dt_spec *gpio_dt_p = spec_pin_p->gpio_dt_p;
  #endif

  #if((HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)||(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD))
	uint32_t            mask       = spec_pin_p->mask;
	uint32_t            invert     = (NRF_INVERT_MSK<<NRF_INVERT_POS)&spec_pin_p->pin_sel;
  #if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	NRF_GPIO_Type       *gpio_p    = spec_pin_p->gpio_p;
  #else // (HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	uint32_t            *set_val_p = spec_pin_p->set_val_p;
	uint32_t            *clr_val_p = spec_pin_p->clr_val_p;
	uint32_t            *val_dist_p= (invert)? set_val_p : clr_val_p;
  #endif
  #endif

  #if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAZARD_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hazard !TXEN/XMT
		if(NULL!=val_dist_p) retval=1, *val_dist_p=mask; else retval=-ENOTSUP;
	}

  #elif(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(HAL_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD) {
	// hal_gpio !TXEN/XMT
		if(NULL==gpio_p) retval=-ENOTSUP;
		else
		if(invert)
			retval=0, nrf_gpio_port_out_set(gpio_p,mask);
		else
			retval=0, nrf_gpio_port_out_clear(gpio_p,mask);
	}

  #elif(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
	if(STD_GPIO_UART_XMT_RCV==UART_XMT_RCV_METHOD)
		// std_gpio !TXEN/XMT
		if(NULL==gpio_dt_p) retval=-ENOTSUP;
		else
		if(0==gpio_pin_set_dt(gpio_dt_p, 0)) retval=0;
	
  #endif

	return retval;
}


static int spec_uart_tx_break(void)
{
	int retval=-ENOTSUP;
	//int retval=-EIO;
	NRF_UARTE_Type *uart_p = spec_uart_ctrl_info.uart_p;
  #if(HAZARD_UART_BRK!=UART_SEND_BREAK_METHOD)
	uint32_t rx_pin_num=spec_uart_ctrl_info.rx.pin_num;
	if((HAL_GPIO_UART_BRK==UART_SEND_BREAK_METHOD)||(STD_GPIO_UART_BRK==UART_SEND_BREAK_METHOD)) {
		// hal_gpio method, set break
		if(NULL!=uart_p)
			retval=0, nrf_uarte_txrx_pins_set(uart_p, NRF_UARTE_PSEL_DISCONNECTED, rx_pin_num);
	}

  #elif(HAZARD_UART_BRK==UART_SEND_BREAK_METHOD)
	if(HAZARD_UART_BRK==UART_SEND_BREAK_METHOD) {
		// hazard method, set break
		//(*((NRF_UARTE_Type**)(spec_uart->config)))->PSEL.TXD|=0x80000000;
		retval=0, uart_p->PSEL.TXD|=0x80000000;
	}
	
  #endif

	return retval;
}


static int spec_uart_tx_make(void)
{
	int retval=-ENOTSUP;
	//int retval=-EIO;
	NRF_UARTE_Type *uart_p = spec_uart_ctrl_info.uart_p;
  #if(HAZARD_UART_BRK!=UART_SEND_BREAK_METHOD)
	uint32_t tx_pin_num=spec_uart_ctrl_info.tx.pin_num;
	uint32_t rx_pin_num=spec_uart_ctrl_info.rx.pin_num;

	if((HAL_GPIO_UART_BRK==UART_SEND_BREAK_METHOD)||(STD_GPIO_UART_BRK==UART_SEND_BREAK_METHOD)) {
		// hal_gpio method, set make
		if(NULL!=uart_p)
			retval=1, nrf_uarte_txrx_pins_set(uart_p, tx_pin_num, rx_pin_num);
	}

  #elif(HAZARD_UART_BRK==UART_SEND_BREAK_METHOD)
	if(HAZARD_UART_BRK==UART_SEND_BREAK_METHOD) {
		// hazard method, set make
		//(*((NRF_UARTE_Type**)(spec_uart->config)))->PSEL.TXD&=~0x80000000;
		retval=1, uart_p->PSEL.TXD&=~0x80000000;
	}

  #endif

	return retval;
}


void spec_uart_sendbreak(int val)
{
	uint8_t brk_cnt;
	if(0==(brk_cnt=MIN(UINT8_MAX,MAX(val,0)))) brk_cnt=UINT8_MAX;
	spec_uart_ctrl_info.brk_cnt=brk_cnt;
	uart_irq_tx_enable(spec_uart_p);
}


static void spec_uart_xmt_isr(const struct device *dev)
{
	uint16_t fill_sz=0;
	bool brk_progress=false;
    
	if(spec_uart_ctrl_info.brk_cnt) {
		brk_progress=true;
		spec_uart_tx_break();
		fill_sz=spec_uart_ctrl_info.brk_cnt;
	}
	else if(spec_uart_ctrl_info.xmt_abort) {
        xmt_get_idx=xmt_put_idx;
    }
	else if(xmt_put_idx!=xmt_get_idx) {
		if((xmt_put_idx^xmt_get_idx) & ~INT_UART_XMTBUF_MSK) {
			fill_sz=INT_UART_XMTBUF_SZ-(INT_UART_XMTBUF_MSK&xmt_get_idx);
		}
		else {
			fill_sz=xmt_put_idx-xmt_get_idx;
		}
	}

	if(spec_uart_ctrl_info.brk_mab) {
		k_busy_wait(10);
		spec_uart_ctrl_info.brk_mab=0;
		spec_uart_tx_make();
	}

	if(0==fill_sz) {
		spec_uart_xmt_disable();
		if(spec_uart_ctrl_info.half_duplex)
			spec_uart_rcv_enable();
		
		uart_irq_tx_disable(dev);
        spec_uart_ctrl_info.xmt_flags=0;

		uint32_t stamp;
        if(0==(stamp=k_uptime_ticks())) stamp=1;
        spec_uart_ctrl_info.xmt_tm_stamp=stamp;
	}
	else {
        spec_uart_ctrl_info.xmt_tm_stamp=0;
		spec_uart_ctrl_info.xmt_run=1;
		if(spec_uart_ctrl_info.half_duplex)
			spec_uart_rcv_disable();
		spec_uart_xmt_enable();
		
		uint8_t len=uart_fifo_fill(dev,int_uart_xmtbuf+(INT_UART_XMTBUF_MSK&xmt_get_idx),fill_sz);
		if(brk_progress) {
			spec_uart_ctrl_info.brk_cnt-=len;
			if(fill_sz==len) spec_uart_ctrl_info.brk_mab=1;
		}
		else
			xmt_get_idx+=len;
	}

}

static void spec_uart_rcv_isr(const struct device *dev)
{
	uint32_t cpy_sz;
	uint32_t sz;
	uint32_t idx;
	uint32_t xdet;
	do {
		idx = rcv_put_idx;
		xdet = sz = rcv_get_idx;
		xdet ^= idx;
		xdet &= ~INT_UART_RCVBUF_MSK;
		if(xdet)
			sz = sz + INT_UART_RCVBUF_SZ - idx;
		else
			sz = INT_UART_RCVBUF_SZ - (idx & INT_UART_RCVBUF_MSK);
		
		idx &= INT_UART_XMTBUF_MSK;
		if(0==sz) {
			uint8_t ovr_buf[4];
			while(0!=uart_fifo_read(dev,&ovr_buf,4)) ;
			int_uart_rcv_ovrflw=true;
			break;
		}
		cpy_sz = uart_fifo_read(dev,int_uart_rcvbuf + idx, sz);
		rcv_put_idx += cpy_sz;
        uint32_t stamp;
        if(0==(stamp=k_uptime_ticks())) stamp=1;
        spec_uart_ctrl_info.rcv_tm_stamp=stamp;
	} while(0!=cpy_sz && sz==cpy_sz);
	
}

static void spec_uart_cb(const struct device *dev, void *user_data)
{
	uart_irq_update(dev);

	if(uart_irq_rx_ready(dev)) {
		spec_uart_rcv_isr(dev);
	}
	
	if(uart_irq_tx_complete(dev)) {
		;
	}

	if(uart_irq_tx_ready(dev)) {
		spec_uart_xmt_isr(dev);
	}

}
