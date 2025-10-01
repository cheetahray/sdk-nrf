// filepath: src/rtc_sync_utils.c

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/net/buf.h>

#include "pawr_common.h"

// 前向聲明和外部變量聲明
struct pawr_peripheral;
extern struct pawr_peripheral peripheral;

// 支援的 PHY 類型檢測 (已移除，未使用)

// TX 功率等級獲取 (已移除，未使用)

// RTC 時間同步函數 (已移除，未使用)// 查找 PAwR 信標
struct pawr_beacon* find_pawr_beacon(struct net_buf_simple *buf)
{
    while (buf->len > 0) {
        uint8_t ad_len = net_buf_simple_pull_u8(buf);
        if (ad_len == 0 || ad_len > buf->len) {
            break;
        }
        
        uint8_t ad_type = net_buf_simple_pull_u8(buf);
        ad_len--; // 減去 type 長度
        
        if (ad_type == BT_DATA_MANUFACTURER_DATA && ad_len >= sizeof(struct pawr_beacon)) {
            struct pawr_beacon *beacon = (struct pawr_beacon *)buf->data;
            if (beacon->message_type == 0x10) {
                return beacon;
            }
        }
        
        net_buf_simple_pull(buf, ad_len);
    }
    
    return NULL;
}

// find_registration_request 函數已移除 (Pure Observer 不需要處理註冊請求)
