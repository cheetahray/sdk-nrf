
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

// for SEND BREAK
#define STD_GPIO_UART_BRK      1
#define HAL_GPIO_UART_BRK      2
#define HAZARD_UART_BRK        3
#ifndef UART_SEND_BREAK_METHOD
  #define UART_SEND_BREAK_METHOD HAZARD_UART_BRK
#endif

// for TRNASCIVER CONTROL
#define STD_GPIO_UART_XMT_RCV  1
#define HAL_GPIO_UART_XMT_RCV  2
#define HAZARD_UART_XMT_RCV    3
#ifndef UART_XMT_RCV_METHOD
  #define UART_XMT_RCV_METHOD    HAZARD_UART_XMT_RCV
#endif

#define UART_CTRL_FULL_DUPLEX 1
#define UART_CTRL_HALF_DUPLEX 2

//  // transmitter enable
//  int spec_uart_xmt_enable(void);
//  
//  // transmitter disable
//  int spec_uart_xmt_disable(void);
//  
//  // receiver enable
//  int spec_uart_rcv_enable(void);
//  
//  // reciever disable
//  int spec_uart_rcv_disable(void);
//  int spec_uart_tx_break(void);
//  int spec_uart_tx_make(void);

void spec_uart_rcv_begin(void);
void spec_uart_rcv_end(void);

// arg 1 : PTR uart device
// arg 2 : PTR uart config 
// arg 3 : gpio_de_spec[] {PTR gpio txen/xmt, PTR rxen/rcv, PTR tx} , for auto xmt/rcv control
// arg 4 : sizeof arg 3
// arg 5 : UART_CTRL_FULL_DUPLEX / UART_CTRL_HALF_DUPLEX
void spec_uart_init(const struct device *spec_uart, struct uart_config *uart_cfg_p, const struct gpio_dt_spec * gpio_list_p, int gpio_list_sz, int aux_flags);

int spec_uart_write(void * buf_p, size_t len);
int spec_uart_putc(int ch);
int spec_uart_read(void * buf_p, size_t len);
int spec_uart_getc(void);

void spec_uart_sendbreak(int val);
bool spec_uart_throttle(uint32_t tm_us);
bool spec_uart_xmt_complete(void);
void spec_uart_xmt_drain(void);
void spec_uart_xmt_flush(void);
void spec_uart_rcv_flush(void);
// void gpiodt_2_pin_st(struct gpio_dt_spec * gpiodt_p,struct io_pin_st * pin_p);
// void spec_uart_cb(const struct device *dev, void *user_data);

