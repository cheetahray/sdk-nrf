#ifndef PAWR_COMMON_H
#define PAWR_COMMON_H

#include <zephyr/types.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>

// 通用常數定義
#define TOTAL_SLOTS_PER_CYCLE    50
#define SLOT_DURATION_MS         20
#define CYCLE_DURATION_MS        1000
#define REGISTRATION_SLOTS       10    // Slot 0-9
#define BLE40_DATA_SLOTS         20    // Slot 10-29
#define CODED_DATA_SLOTS         20    // Slot 30-49

// PAwR 信標結構
struct pawr_beacon {
    uint8_t message_type;           // 0x10 = PAWR_BEACON
    uint32_t cycle_start_time;      // 週期開始時間
    uint16_t cycle_sequence;        // 週期序號
    uint8_t current_slot;           // 當前時隙 (0-49)
    uint16_t slot_remaining_ms;     // 當前時隙剩餘時間
    uint8_t beacon_sequence;        // 信標序號
    
    // RTC 時間同步
    uint32_t master_rtc_time;       // 主設備 RTC 時間戳
    uint8_t rtc_sync_quality;       // 同步品質 (0-255)
    
    // 時隙狀態 (壓縮格式)
    uint8_t registration_status;    // 註冊時隙可用性 (bitmap)
    uint8_t ble40_slot_status[2];   // BLE 4.0 時隙狀態 (16 bits, 壓縮)
    uint8_t coded_slot_status[2];   // Coded PHY 時隙狀態 (16 bits, 壓縮)
    
    // 當前請求數據 (如果在請求時隙)
    uint8_t request_data[4];        // 請求數據 (縮短)
    uint8_t request_target_phy;     // 目標 PHY 類型
} __packed;

// 註冊請求結構
struct registration_request {
    uint8_t message_type;           // 0x20 = REGISTRATION_REQUEST
    uint8_t device_id[6];           // 設備 ID
    uint8_t phy_type;               // 0x01=1M, 0x04=Coded
    uint8_t requested_slot;         // 請求的時隙 (10-49, 0=auto)
    uint32_t local_rtc_time;        // 本地 RTC 時間
    int8_t rssi_threshold;          // RSSI 閾值
    uint8_t tx_power_level;         // 發射功率等級
    uint32_t rtc_offset_us;         // RTC 偏移 (微秒)
    uint8_t sync_confidence;        // 同步信心度 (0-255)
} __packed;

// 數據回應結構
struct data_response {
    uint8_t message_type;           // 0x30 = DATA_RESPONSE
    uint8_t device_id;              // 設備 ID
    uint8_t response_slot;          // 回應時隙
    uint32_t synced_rtc_timestamp;  // 同步後的 RTC 時間戳
    uint8_t rtc_sync_confidence;    // RTC 同步信心度
    uint8_t phy_type;              // PHY 類型
    uint8_t sensor_data[10];        // 感測器數據
} __packed;

// Pure Observer 需要的函數聲明
struct pawr_beacon* find_pawr_beacon(struct net_buf_simple *buf);
// 其他未使用的函數已移除

// Peripheral 管理結構定義 (共享給所有文件)
struct pawr_peripheral {
    // 設備信息
    uint8_t my_device_id;
    uint8_t my_phy_type;         // 0x01=1M, 0x04=Coded
    uint8_t assigned_slot;       // 10-49
    bool is_registered;
  
    // 同步狀態
    uint32_t synced_cycle_start;
    uint16_t synced_cycle_seq;
    bool sync_established;
    uint32_t last_beacon_time;
  
    // RTC 時間同步
    uint32_t local_rtc_time;        // 本地 RTC 時間
    uint32_t master_rtc_ref;        // 主設備 RTC 參考時間
    int32_t rtc_offset_us;          // RTC 偏移 (微秒)
    uint8_t sync_confidence;        // 同步信心度 (0-255)
    uint32_t last_rtc_sync_time;    // 最後同步時間
    struct k_timer rtc_update_timer; // RTC 更新定時器
  
    // 干擾監測和適應性傳輸 (Observer 回應時需要)
    struct {
        uint32_t transmission_attempts; // 傳輸嘗試次數
        uint32_t successful_transmissions; // 成功傳輸次數
        uint32_t collision_count;       // 碰撞計數
        int8_t measured_rssi;           // 測量的 RSSI 值
        uint8_t channel_quality;        // 信道品質 (0-255)
        uint32_t last_collision_time;   // 最後碰撞時間
      
        // 適應性參數 (用於回應調整)
        uint8_t transmission_power;     // 動態調整的發射功率
        uint16_t response_delay_offset; // 回應延遲偏移 (μs)
        uint8_t retry_backoff_level;    // 退避等級
        bool adaptive_timing_enabled;   // 適應性時序啟用
    } interference;
  
    // 回應管理 (Observer 回應 Central 數據請求時需要)
    struct k_work_delayable response_work;
    uint8_t pending_response[20];
    size_t response_length;
  
    // 掃描參數
    struct bt_le_scan_param scan_param;
    bool scanning_active;
};

#endif /* PAWR_COMMON_H */