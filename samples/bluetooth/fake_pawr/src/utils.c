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

// 外部變量聲明
extern struct {
    uint8_t sync_confidence;
    uint32_t local_rtc_time;
    uint32_t master_rtc_ref;
    int32_t rtc_offset_us;
} peripheral;

// 支援的 PHY 類型檢測
uint8_t get_supported_phy_mask(void)
{
    uint8_t phy_mask = 0;
    
    // 所有設備都支援 1M PHY
    phy_mask |= 0x01; // bit0 = 1M PHY
    
    // 檢查是否支援 Coded PHY (需要 BLE 5.0+)
#if defined(CONFIG_BT_CTLR_PHY_CODED)
    phy_mask |= 0x04; // bit2 = Coded PHY
#endif
    
    return phy_mask;
}

// 獲取當前發射功率等級
int8_t get_tx_power_level(void)
{
    // 讀取當前發射功率設定
    int8_t tx_power = 0;
    
#if defined(CONFIG_BT_CTLR_TX_PWR_0)
    tx_power = 0;
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_8)
    tx_power = 8;
#elif defined(CONFIG_BT_CTLR_TX_PWR_PLUS_4)
    tx_power = 4;
#else
    tx_power = -4; // 默認較低功率
#endif
    
    return tx_power;
}

// 讀取感測器數據 (模擬)
void read_sensor_data(uint8_t *buffer, size_t length)
{
    static uint16_t sequence = 0;
    uint32_t timestamp = get_synced_rtc_time();
    
    // 模擬感測器數據格式:
    // [0-1]: 序號
    // [2-5]: 同步 RTC 時間戳
    // [6-9]: 模擬數據 (溫度、濕度等)
    
    if (length >= 10) {
        buffer[0] = sequence & 0xFF;
        buffer[1] = (sequence >> 8) & 0xFF;
        
        buffer[2] = timestamp & 0xFF;
        buffer[3] = (timestamp >> 8) & 0xFF;
        buffer[4] = (timestamp >> 16) & 0xFF;
        buffer[5] = (timestamp >> 24) & 0xFF;
        
        // 模擬溫度數據 (22-28°C)
        int16_t temp = 2200 + (sequence % 600);
        buffer[6] = temp & 0xFF;
        buffer[7] = (temp >> 8) & 0xFF;
        
        // 模擬濕度數據 (40-80%)
        uint8_t humidity = 40 + (sequence % 40);
        buffer[8] = humidity;
        
        // RTC 同步信心度
        buffer[9] = peripheral.sync_confidence;
    }
    
    sequence++;
}

// 獲取同步的 RTC 時間
uint32_t get_synced_rtc_time(void)
{
    uint32_t current_time = k_uptime_get_32();
    
    // 如果有 RTC 偏移補償，應用它
    if (peripheral.local_rtc_time != 0) {
        return peripheral.local_rtc_time + (current_time - peripheral.master_rtc_ref) + 
               (peripheral.rtc_offset_us / 1000);
    }
    
    // 如果沒有同步資訊，返回系統時間
    return current_time;
}

// 查找 PAwR 信標
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

// 查找註冊請求
struct registration_request* find_registration_request(struct net_buf_simple *buf)
{
    while (buf->len > 0) {
        uint8_t ad_len = net_buf_simple_pull_u8(buf);
        if (ad_len == 0 || ad_len > buf->len) {
            break;
        }
        
        uint8_t ad_type = net_buf_simple_pull_u8(buf);
        ad_len--;
        
        if (ad_type == BT_DATA_MANUFACTURER_DATA && ad_len >= sizeof(struct registration_request)) {
            struct registration_request *req = (struct registration_request *)buf->data;
            if (req->message_type == 0x20) {
                return req;
            }
        }
        
        net_buf_simple_pull(buf, ad_len);
    }
    
    return NULL;
}
