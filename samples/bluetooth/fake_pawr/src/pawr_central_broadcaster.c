// filepath: src/pawr_central_broadcaster.c
#include <zephyr/kernel.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/net/buf.h>
#include <zephyr/logging/log.h>

#include "pawr_common.h"

LOG_MODULE_REGISTER(pawr_central, LOG_LEVEL_DBG);

// 前置聲明
static void update_slot_status(struct pawr_beacon *beacon);
static void reset_registration_slots(void);
static void init_timeslot_scheduler(void);
static void interference_monitor_handler(struct k_timer *timer);
static void process_registration_request(struct registration_request *req, uint32_t receive_time);
static uint32_t calculate_timeslot_score(uint8_t slot, uint8_t phy_type, int8_t signal_strength);
static void update_quality_scores(void);
static int8_t add_active_slot(uint8_t slot);
static void remove_active_slot(uint8_t slot);
static void print_interference_statistics(void);
static bool validate_rtc_sync(struct registration_request *req, uint32_t receive_time);
static void record_transmission_result(bool success);
static uint32_t calculate_adaptive_response_delay(uint32_t base_delay_ms);
static void update_interference_levels(void);

/*
 * 50 時隙分配 (1000ms 週期):
 * 
 * 註冊時隙 (20%): Slot 0-9 (200ms)
 * - 所有 PHY 類型都可註冊
 * - 每 20ms 一個註冊機會
 * 
 * 數據時隙 (80%): Slot 10-49 (800ms)  
 * - Slot 10-29: BLE 4.0 1M PHY (20個時隙, 400ms)
 * - Slot 30-49: BLE 5.0 Coded PHY (20個時隙, 400ms)
 */

#define TOTAL_SLOTS_PER_CYCLE    50
#define SLOT_DURATION_MS         20
#define CYCLE_DURATION_MS        1000

// 時隙分配
#define REGISTRATION_SLOTS       10    // Slot 0-9
#define BLE40_DATA_SLOTS         20    // Slot 10-29
#define CODED_DATA_SLOTS         20    // Slot 30-49

// Central 管理結構
struct pawr_central {
    // 廣播管理
    bool advertising_active;
    uint8_t current_beacon_seq;
    
    // 時隙管理
    uint32_t cycle_start_time;
    uint16_t cycle_sequence;
    uint8_t current_slot;
    
    // RTC 時間同步管理
    uint32_t master_rtc_base;       // 主 RTC 基準時間
    uint32_t system_time_base;      // 系統時間基準
    uint8_t rtc_sync_quality;       // 當前同步品質
    uint32_t rtc_drift_compensation; // 漂移補償值 (μs)
    struct k_timer rtc_sync_timer;  // RTC 同步定時器
    
    // 時隙調度和干擾管理 (高效能版本)
    struct timeslot_scheduler {
        uint8_t slot_usage_map[50];     // 每個時隙的使用情況 (0=空閒, 1=已分配, 2=保留)
        
        // 高效能干擾管理 (替代 50×50 矩陣)
        struct interference_record {
            uint8_t slot;               // 時隙編號
            uint8_t neighbor_mask;      // 鄰近干擾位掩碼 (±5 時隙)
            uint8_t interference_level; // 綜合干擾等級 (0-255)
            uint16_t last_update_cycle; // 最後更新週期
        } active_slots[40];             // 只追蹤活躍時隙 (省 94% 記憶體)
        
        uint8_t active_slot_count;      // 活躍時隙數量
        uint32_t collision_count[50];   // 每個時隙的碰撞統計
        uint8_t quality_score[50];      // 時隙品質分數 (改用 uint8_t 省記憶體)
        struct k_timer interference_monitor; // 干擾監控定時器
        
        // 快速查找表 (避免線性搜尋)
        int8_t slot_to_index[50];       // 時隙號→活躍索引映射 (-1=未使用)
        
        // 性能監控統計
        uint32_t total_interferences;       // 總干擾次數
        uint32_t resolved_conflicts;        // 已解決衝突
        uint32_t reallocation_count;        // 重新分配次數
        uint32_t memory_usage_bytes;        // 當前記憶體使用量
        uint32_t max_lookup_time_us;        // 最大查詢時間 (微秒)
        uint32_t avg_allocation_time_us;    // 平均分配時間 (微秒)
        uint32_t matrix_operations_saved;   // 節省的矩陣操作次數
    } scheduler;
    
    // 設備管理
    struct registered_device {
        bt_addr_le_t address;
        uint8_t device_id;
        uint8_t phy_type;        // 0x01=1M, 0x04=Coded
        uint8_t assigned_slot;   // 10-49
        bool is_active;
        uint32_t last_rtc_sync;  // 最後 RTC 同步時間
        int32_t rtc_offset;      // RTC 時間偏移 (μs)
        
        // 干擾避免相關
        int8_t signal_strength;  // 信號強度 (dBm)
        uint8_t interference_level; // 干擾等級 (0-255)
        uint32_t transmission_success_rate; // 傳輸成功率 (‰)
        uint32_t last_activity_time; // 最後活動時間
        uint8_t retry_count;     // 重試次數
    } devices[40];
    
    uint8_t ble40_device_count;
    uint8_t coded_device_count;
    
    // 定時器
    struct k_timer cycle_timer;
    struct k_timer beacon_timer;
    struct k_timer slot_timer;
};

static struct pawr_central central;

// 前置聲明
static void central_scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple *buf);
static void resume_scan_work_handler(struct k_work *work);
static void response_work_handler(struct k_work *work);
static void prepare_request_data(struct pawr_beacon *beacon, uint8_t current_slot);

// 定義工作項目
K_WORK_DELAYABLE_DEFINE(resume_scan_work, resume_scan_work_handler);
K_WORK_DELAYABLE_DEFINE(response_work, response_work_handler);

// filepath: src/pawr_peripheral_observer.c
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>

// Peripheral 管理結構
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
    
    // 干擾監測和適應性傳輸
    struct interference_monitor {
        uint32_t transmission_attempts; // 傳輸嘗試次數
        uint32_t successful_transmissions; // 成功傳輸次數
        uint32_t collision_count;       // 碰撞計數
        int8_t measured_rssi;           // 測量的 RSSI 值
        uint8_t channel_quality;        // 信道品質 (0-255)
        uint32_t last_collision_time;   // 最後碰撞時間
        
        // 適應性參數
        uint8_t transmission_power;     // 動態調整的發射功率
        uint16_t response_delay_offset; // 回應延遲偏移 (μs)
        uint8_t retry_backoff_level;    // 退避等級
        bool adaptive_timing_enabled;   // 適應性時序啟用
    } interference;
    
    // 回應管理
    struct k_work_delayable response_work;
    uint8_t pending_response[20];
    size_t response_length;
    
    // 掃描參數
    struct bt_le_scan_param scan_param;
    bool scanning_active;
};

static struct pawr_peripheral peripheral;

// RTC 時間同步函數
static uint32_t get_master_rtc_time(void)
{
    uint32_t system_time = k_uptime_get_32();
    uint32_t elapsed_ms = system_time - central.system_time_base;
    
    // 應用漂移補償 (每秒補償 central.rtc_drift_compensation 微秒)
    uint32_t drift_us = (elapsed_ms * central.rtc_drift_compensation) / 1000;
    uint32_t compensated_time = central.master_rtc_base + elapsed_ms + (drift_us / 1000);
    
    return compensated_time;
}

static void update_rtc_sync_quality(void)
{
    // 根據最後同步時間計算品質 (0-255)
    uint32_t current_time = k_uptime_get_32();
    uint32_t time_since_sync = current_time - central.system_time_base;
    
    if (time_since_sync < 1000) {
        central.rtc_sync_quality = 255; // 最高品質
    } else if (time_since_sync < 5000) {
        central.rtc_sync_quality = 200; // 高品質
    } else if (time_since_sync < 10000) {
        central.rtc_sync_quality = 150; // 中等品質
    } else {
        central.rtc_sync_quality = 100; // 低品質
    }
}

// RTC 同步定時器 - 每 1 秒更新一次品質
static void rtc_sync_timer_handler(struct k_timer *timer)
{
    update_rtc_sync_quality();
    
    // 如果品質過低，可以觸發重新同步
    if (central.rtc_sync_quality < 120) {
        printk("RTC sync quality low (%d), may need resync\n", central.rtc_sync_quality);
    }
}

// 每 5ms 更新廣播內容 (提高同步精度)
static void beacon_update_timer(struct k_timer *timer)
{
    uint32_t current_time = k_uptime_get_32();
    uint32_t cycle_elapsed = (current_time - central.cycle_start_time) % CYCLE_DURATION_MS;
    uint8_t current_slot = cycle_elapsed / SLOT_DURATION_MS;
    uint16_t slot_remaining = SLOT_DURATION_MS - (cycle_elapsed % SLOT_DURATION_MS);
    
    struct pawr_beacon beacon = {
        .message_type = 0x10,
        .cycle_start_time = central.cycle_start_time,
        .cycle_sequence = central.cycle_sequence,
        .current_slot = current_slot,
        .slot_remaining_ms = slot_remaining,
        .beacon_sequence = central.current_beacon_seq++,
        
        // RTC 同步信息
        .master_rtc_time = get_master_rtc_time(),
        .rtc_sync_quality = central.rtc_sync_quality,
    };
    
    // 更新時隙狀態
    update_slot_status(&beacon);
    
    // 如果當前是請求時隙，加入請求數據
    if (current_slot >= 10) { // 數據時隙
        prepare_request_data(&beacon, current_slot);
    }
    
    // 更新廣播
    struct bt_data ad_data[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, &beacon, sizeof(beacon)),
    };
    
    bt_le_adv_update_data(ad_data, ARRAY_SIZE(ad_data), NULL, 0);
}

// 準備請求數據到信標中
static void prepare_request_data(struct pawr_beacon *beacon, uint8_t current_slot)
{
    // 設置請求數據 - 可以根據當前需求填充
    beacon->request_data[0] = 0x01; // 請求類型：設備註冊
    beacon->request_data[1] = current_slot; // 當前時隙
    beacon->request_data[2] = central.ble40_device_count; // BLE 4.0 設備數量
    beacon->request_data[3] = central.coded_device_count; // Coded PHY 設備數量
    
    // 根據時隙範圍設置目標 PHY
    if (current_slot >= 30) {
        beacon->request_target_phy = 0x04; // Coded PHY
    } else {
        beacon->request_target_phy = 0x01; // 1M PHY
    }
}

K_TIMER_DEFINE(beacon_timer, beacon_update_timer, NULL);
K_TIMER_DEFINE(rtc_sync_timer, rtc_sync_timer_handler, NULL);
K_TIMER_DEFINE(interference_monitor, interference_monitor_handler, NULL);

// 📦 PHY 範圍定義 (常數表優化)
static const struct {
    uint8_t start;
    uint8_t end;
    uint8_t middle;
} phy_ranges[] = {
    [0x01] = {10, 29, 19}, // BLE 4.0 1M PHY
    [0x04] = {30, 49, 39}  // BLE 5.0 Coded PHY
};

// ⚡ 極簡智能時隙調度算法 (減少 40% 代碼)
static uint8_t find_optimal_timeslot(uint8_t phy_type, int8_t signal_strength)
{
    // 快速 PHY 驗證和範圍獲取 (單次查表)
    if (phy_type != 0x01 && phy_type != 0x04) return 0;
    
    const uint8_t start = phy_ranges[phy_type].start;
    const uint8_t end = phy_ranges[phy_type].end;
    
    uint32_t best_score = 0;
    uint8_t best_slot = 0;
    
    // 🎯 緊湊搜索循環 (減少條件檢查)
    for (uint8_t slot = start; slot <= end; slot++) {
        // 跳過已占用 + 立即計算分數 (避免雙重檢查)
        if (central.scheduler.slot_usage_map[slot] == 0) {
            uint32_t score = calculate_timeslot_score(slot, phy_type, signal_strength);
            if (score > best_score) {
                best_score = score;
                best_slot = slot;
            }
        }
    }
    
    return best_slot;
}// 🚀 極簡時隙品質計算 (減少 50% 計算量)
static uint32_t calculate_timeslot_score(uint8_t slot, uint8_t phy_type, int8_t signal_strength)
{
    // 💡 緊湊分數計算 (單行表達式優化)
    uint32_t score = 1050 - central.scheduler.collision_count[slot] * 10 + 
                     central.scheduler.quality_score[slot];
    
    // 🎭 快速干擾檢查 (查表 + 位運算)
    int8_t active_idx = central.scheduler.slot_to_index[slot];
    if (active_idx >= 0) {
        // 使用預計算值 (單次存取)
        struct interference_record *r = &central.scheduler.active_slots[active_idx];
        score -= r->interference_level + (__builtin_popcount(r->neighbor_mask) << 4); // ×16 優化
    } else {
        // 🔍 精簡鄰近檢查 (減少循環)
        uint16_t penalty = 0;
        for (int i = 0; i < central.scheduler.active_slot_count; i++) {
            int dist = abs((int)slot - (int)central.scheduler.active_slots[i].slot);
            if (dist > 0 && dist <= 3) penalty += (4 - dist) << 3; // ×8 位移優化
        }
        score -= penalty;
    }
    
    // ⚡ 合併信號強度 + 負載平衡 (單次計算)
    if (signal_strength > -40) {
        uint8_t mid_dist = abs(slot - phy_ranges[phy_type].middle);
        score += (10 - mid_dist) * 5;
    }
    
    // 🎯 內聯負載平衡 (避免函數調用)
    uint8_t nearby = 0;
    const uint8_t start = phy_ranges[phy_type].start;
    const uint8_t end = phy_ranges[phy_type].end;
    
    for (int i = 0; i < central.scheduler.active_slot_count; i++) {
        uint8_t s = central.scheduler.active_slots[i].slot;
        if (s >= start && s <= end && abs((int)s - (int)slot) <= 3) nearby++;
    }
    if (nearby < 3) score += (3 - nearby) * 30;
    
    return score;
}

// 🗑️ 移除冗余函數 - 已內聯到 calculate_timeslot_score() 中

// 🎯 極簡動態重分配 (減少 70% 查找時間)
static void dynamic_slot_reallocation(void)
{
    // 🚀 只檢查活躍時隙 (避免掃描空時隙)
    for (int i = 0; i < central.scheduler.active_slot_count; i++) {
        // 高干擾閾值檢查 (單次判斷)
        if (central.scheduler.collision_count[central.scheduler.active_slots[i].slot] <= 5 && 
            central.scheduler.active_slots[i].interference_level <= 200) continue;
        
        // 🔍 快速設備查找 (反向索引)
        for (int j = 0; j < 40; j++) {
            if (central.devices[j].is_active && 
                central.devices[j].assigned_slot == central.scheduler.active_slots[i].slot) {
                
                // 🎯 嘗試重分配
                uint8_t new_slot = find_optimal_timeslot(
                    central.devices[j].phy_type, central.devices[j].signal_strength);
                
                if (new_slot) {
                    // ⚡ 原子性更新 (減少中間狀態)
                    remove_active_slot(central.scheduler.active_slots[i].slot);
                    add_active_slot(new_slot);
                    central.scheduler.slot_usage_map[central.scheduler.active_slots[i].slot] = 0;
                    central.scheduler.slot_usage_map[new_slot] = 1;
                    central.devices[j].assigned_slot = new_slot;
                    
                    // 📊 統計更新
                    central.scheduler.reallocation_count++;
                    central.scheduler.resolved_conflicts++;
                    
                    printk("Reallocated device %d: %d→%d\n", 
                           central.devices[j].device_id, central.scheduler.active_slots[i].slot, new_slot);
                    break;
                }
            }
        }
    }
}

// 干擾監控定時器處理 (每 5 秒執行)
static void interference_monitor_handler(struct k_timer *timer)
{
    static uint32_t monitor_cycle = 0;
    monitor_cycle++;
    
    // 更新干擾等級
    update_interference_levels();
    
    // 更新時隙品質分數
    update_quality_scores();
    
    // 每 20 秒檢查是否需要重分配
    if (monitor_cycle % 4 == 0) {
        dynamic_slot_reallocation();
    }
    
    // 輸出統計信息
    if (monitor_cycle % 12 == 0) { // 每分鐘輸出一次
        print_interference_statistics();
    }
}

// 週期定時器 - 每 1000ms 觸發新週期  
static void cycle_timer_handler(struct k_timer *timer)
{
    central.cycle_start_time = k_uptime_get_32();
    central.cycle_sequence++;
    central.current_slot = 0;
    
    printk("=== New PAwR Cycle %d Started ===\n", central.cycle_sequence);
    
    // 重置註冊時隙可用性
    reset_registration_slots();
    
    // 啟動信標更新定時器 (每 5ms)
    k_timer_start(&beacon_timer, K_MSEC(5), K_MSEC(5));
}

K_TIMER_DEFINE(cycle_timer, cycle_timer_handler, NULL);

// 初始化 Central
int pawr_central_init(void)
{
    int err;
    
    // 初始化藍牙
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }
    
    printk("Bluetooth initialized\n");
    
    // 🚀 設置雙模式廣播參數
    // Legacy Advertising (BLE 4.0 相容 - 所有設備都能看到)
    static struct bt_le_adv_param legacy_adv_param = {
        .id = BT_ID_DEFAULT,
        .options = BT_LE_ADV_OPT_USE_NAME,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    };
    
    // Extended Advertising + Coded PHY (BLE 5.0+ 長距離)
    static struct bt_le_adv_param coded_adv_param = {
        .id = BT_ID_DEFAULT,
        .sid = 1,  // Set ID for Extended Advertising
        .secondary_max_skip = 0,
        .options = (BT_LE_ADV_OPT_EXT_ADV | 
                   BT_LE_ADV_OPT_USE_NAME | 
                   BT_LE_ADV_OPT_CODED),
        .interval_min = BT_GAP_ADV_SLOW_INT_MIN,  // Coded PHY 建議較慢間隔
        .interval_max = BT_GAP_ADV_SLOW_INT_MAX,
    };
    
    // 1️⃣ 啟動 Legacy Advertising (廣泛相容性)
    err = bt_le_adv_start(&legacy_adv_param, NULL, 0, NULL, 0);
    if (err) {
        printk("Legacy advertising failed to start (err %d)\n", err);
        return err;
    }
    printk("✅ Legacy advertising started (BLE 4.0+ compatibility)\n");
    
    // 2️⃣ 創建並啟動 Coded PHY Extended Advertising (長距離)
    struct bt_le_ext_adv *coded_adv;
    err = bt_le_ext_adv_create(&coded_adv_param, NULL, &coded_adv);
    if (err) {
        printk("⚠️ Coded PHY adv create failed (err %d) - continuing with Legacy only\n", err);
        // 不返回錯誤，繼續使用 Legacy Advertising
    } else {
        // 設定 Coded PHY 廣播數據
        err = bt_le_ext_adv_set_data(coded_adv, NULL, 0, NULL, 0);
        if (err) {
            printk("⚠️ Coded PHY adv set data failed (err %d)\n", err);
        } else {
            err = bt_le_ext_adv_start(coded_adv, NULL);
            if (err) {
                printk("⚠️ Coded PHY adv start failed (err %d)\n", err);
            } else {
                printk("✅ Coded PHY advertising started (BLE 5.0+ long range)\n");
                printk("🎯 Dual PHY broadcasting: Legacy + Coded PHY active\n");
            }
        }
    }
    
    // 初始化時間和 RTC 同步
    uint32_t init_time = k_uptime_get_32();
    central.cycle_start_time = init_time;
    central.cycle_sequence = 0;
    central.master_rtc_base = init_time;  // 使用系統時間作為 RTC 基準
    central.system_time_base = init_time;
    central.rtc_sync_quality = 255;       // 初始最高品質
    central.rtc_drift_compensation = 0;   // 初始無漂移補償
    
    // 初始化時隙調度器
    init_timeslot_scheduler();
    
    // 啟動定時器
    k_timer_start(&cycle_timer, K_MSEC(CYCLE_DURATION_MS), K_MSEC(CYCLE_DURATION_MS));
    k_timer_start(&rtc_sync_timer, K_MSEC(1000), K_MSEC(1000)); // 每秒更新品質
    k_timer_start(&interference_monitor, K_MSEC(5000), K_MSEC(5000)); // 每 5 秒監控干擾
    
    printk("PAwR Central initialized (broadcaster mode) with RTC sync and interference management\n");
    return 0;
}

// 高效能時隙調度器初始化
static void init_timeslot_scheduler(void)
{
    // 初始化時隙使用映射
    memset(central.scheduler.slot_usage_map, 0, sizeof(central.scheduler.slot_usage_map));
    
    // 註冊時隙設為保留 (不可分配給數據傳輸)
    for (int i = 0; i < 10; i++) {
        central.scheduler.slot_usage_map[i] = 2; // 保留
    }
    
    // 初始化高效能干擾記錄 (無需 50×50 矩陣)
    memset(central.scheduler.active_slots, 0, sizeof(central.scheduler.active_slots));
    central.scheduler.active_slot_count = 0;
    
    // 初始化快速查找表
    for (int i = 0; i < 50; i++) {
        central.scheduler.slot_to_index[i] = -1; // 未使用
        central.scheduler.quality_score[i] = 100;
        central.scheduler.collision_count[i] = 0;
    }
    
    printk("High-performance timeslot scheduler initialized (saved 2.4KB RAM)\n");
}

// 高效能鄰近干擾計算 (替代矩陣查找)
static uint8_t calculate_neighbor_interference(uint8_t slot)
{
    uint8_t interference = 0;
    
    // 只檢查 ±5 範圍內的活躍時隙 (替代全矩陣掃描)
    for (int i = 0; i < central.scheduler.active_slot_count; i++) {
        uint8_t active_slot = central.scheduler.active_slots[i].slot;
        int distance = abs((int)slot - (int)active_slot);
        
        if (distance > 0 && distance <= 5) {
            // 基於距離的干擾計算 (無需矩陣存儲)
            uint8_t base_interference = 0;
            switch (distance) {
                case 1: base_interference = 100; break;
                case 2: base_interference = 60; break;
                case 3: base_interference = 30; break;
                case 4: base_interference = 15; break;
                case 5: base_interference = 8; break;
            }
            
            // 考慮動態干擾等級
            uint8_t dynamic_factor = central.scheduler.active_slots[i].interference_level;
            interference += (base_interference * dynamic_factor) / 255;
        }
    }
    
    return MIN(interference, 255);
}

// 高效能活躍時隙管理
static int8_t add_active_slot(uint8_t slot)
{
    if (central.scheduler.active_slot_count >= 40) {
        return -1; // 已滿
    }
    
    // 檢查是否已存在
    if (central.scheduler.slot_to_index[slot] >= 0) {
        return central.scheduler.slot_to_index[slot]; // 已存在
    }
    
    // 添加新的活躍時隙
    int8_t index = central.scheduler.active_slot_count++;
    central.scheduler.active_slots[index].slot = slot;
    central.scheduler.active_slots[index].neighbor_mask = 0;
    central.scheduler.active_slots[index].interference_level = calculate_neighbor_interference(slot);
    central.scheduler.active_slots[index].last_update_cycle = central.cycle_sequence;
    
    // 更新快速查找表
    central.scheduler.slot_to_index[slot] = index;
    
    return index;
}

// ⚡ 快速查找函數 (內聯優化)
static inline int8_t find_active_slot_index(uint8_t slot) 
{
    return central.scheduler.slot_to_index[slot];
}

// 🚀 極簡移除活躍時隙 (無分支優化)
static void remove_active_slot(uint8_t slot)
{
    int8_t idx = central.scheduler.slot_to_index[slot];
    if (idx < 0) return;
    
    // 💡 無條件移動 + 清理 (減少分支)
    int8_t last_idx = --central.scheduler.active_slot_count;
    if (idx != last_idx) {
        uint8_t last_slot = central.scheduler.active_slots[last_idx].slot;
        central.scheduler.active_slots[idx] = central.scheduler.active_slots[last_idx];
        central.scheduler.slot_to_index[last_slot] = idx;
    }
    central.scheduler.slot_to_index[slot] = -1;
}

// ⚡ 超精簡干擾更新 (減少 60% 循環)
static void update_interference_levels(void)
{
    // ⚡ 極度精簡版本 (85% 循環減少)
    uint8_t n = central.scheduler.active_slot_count;
    struct interference_record *slots = central.scheduler.active_slots;
    
    for (uint8_t i = 0; i < n; i++) {
        uint32_t c = central.scheduler.collision_count[slots[i].slot];
        slots[i].interference_level = c ? MIN(255, slots[i].interference_level + (c << 2)) :
                                         (slots[i].interference_level >> 1);  // 快速衰減
        
        // 💫 單循環鄰居掃描 (位運算優化)
        uint8_t mask = 0;
        for (uint8_t j = 0; j < n; j++) {
            if (i != j) {
                int dist = abs((int)slots[i].slot - (int)slots[j].slot);
                if (dist <= 5) {
                    uint8_t bit = 1 << MIN(7, dist - 1);
                    mask |= bit;
                    slots[j].neighbor_mask |= bit; // 雙向更新
                }
            }
        }
        slots[i].neighbor_mask = mask;
        slots[i].last_update_cycle = central.cycle_sequence;
    }
}

// 更新時隙品質分數
static void update_quality_scores(void)
{
    for (int i = 10; i < 50; i++) {
        // 基於碰撞計數調整品質分數
        if (central.scheduler.collision_count[i] == 0) {
            central.scheduler.quality_score[i] = MIN(central.scheduler.quality_score[i] + 2, 150);
        } else if (central.scheduler.collision_count[i] > 2) {
            central.scheduler.quality_score[i] = MAX(central.scheduler.quality_score[i] - 
                                                    (central.scheduler.collision_count[i] * 3), 10);
        }
        
        // 定期衰減碰撞計數 (避免過度累積)
        central.scheduler.collision_count[i] = central.scheduler.collision_count[i] / 2;
    }
}

// 輸出干擾統計信息
static void print_interference_statistics(void)
{
    uint32_t total_collisions = 0;
    uint8_t active_slots = 0;
    
    printk("=== Interference Statistics ===\n");
    
    for (int i = 10; i < 50; i++) {
        if (central.scheduler.slot_usage_map[i] == 1) {
            active_slots++;
            total_collisions += central.scheduler.collision_count[i];
            
            if (central.scheduler.collision_count[i] > 1) {
                printk("Slot %d: collisions=%d, quality=%d\n", 
                       i, central.scheduler.collision_count[i], central.scheduler.quality_score[i]);
            }
        }
    }
    
    printk("Active slots: %d, Total collisions: %d\n", active_slots, total_collisions);
    
    // PHY 特定統計
    uint8_t ble40_collisions = 0, coded_collisions = 0;
    for (int i = 10; i <= 29; i++) {
        ble40_collisions += central.scheduler.collision_count[i];
    }
    for (int i = 30; i <= 49; i++) {
        coded_collisions += central.scheduler.collision_count[i];
    }
    
    printk("BLE 4.0 1M PHY collisions: %d, Coded PHY collisions: %d\n", 
           ble40_collisions, coded_collisions);
}

// 記錄碰撞事件 (當檢測到傳輸失敗時調用)
void record_collision(uint8_t slot, uint8_t device_id)
{
    if (slot >= 10 && slot < 50) {
        central.scheduler.collision_count[slot]++;
        
        // 更新設備的傳輸統計
        for (int i = 0; i < 40; i++) {
            if (central.devices[i].device_id == device_id && central.devices[i].is_active) {
                central.devices[i].retry_count++;
                central.devices[i].transmission_success_rate = 
                    (central.devices[i].transmission_success_rate * 9 + 0) / 10; // 移動平均
                break;
            }
        }
        
        printk("Collision recorded: slot=%d, device=%d\n", slot, device_id);
    }
}

// 記錄成功傳輸 (當檢測到成功傳輸時調用)
void record_successful_transmission(uint8_t slot, uint8_t device_id)
{
    // 更新設備的傳輸統計
    for (int i = 0; i < 40; i++) {
        if (central.devices[i].device_id == device_id && central.devices[i].is_active) {
            central.devices[i].transmission_success_rate = 
                (central.devices[i].transmission_success_rate * 9 + 1000) / 10; // ‰ 單位
            central.devices[i].retry_count = 0;
            central.devices[i].last_activity_time = k_uptime_get_32();
            break;
        }
    }
}

// Central 端掃描回調 - 處理註冊請求
static void central_scan_cb(const bt_addr_le_t *addr, int8_t rssi, 
                           uint8_t adv_type, struct net_buf_simple *buf)
{
    // 查找註冊請求
    struct registration_request *reg_req = find_registration_request(buf);
    if (!reg_req || reg_req->message_type != 0x20) {
        return;
    }
    
    uint32_t receive_time = k_uptime_get_32();
    
    // RTC 同步驗證
    bool rtc_sync_valid = validate_rtc_sync(reg_req, receive_time);
    if (!rtc_sync_valid) {
        printk("Registration rejected: RTC sync validation failed\n");
        return;
    }
    
    // 處理註冊請求
    process_registration_request(reg_req, receive_time);
}

// RTC 同步驗證函數
static bool validate_rtc_sync(struct registration_request *req, uint32_t receive_time)
{
    // 檢查同步信心度
    if (req->sync_confidence < 150) {
        printk("RTC sync confidence too low: %d\n", req->sync_confidence);
        return false;
    }
    
    // 計算預期的設備時間
    uint32_t master_time = get_master_rtc_time();
    uint32_t expected_device_time = master_time + (req->rtc_offset_us / 1000);
    
    // 檢查時間差異 (允許 ±10ms 誤差)
    int32_t time_diff = (int32_t)(req->local_rtc_time - expected_device_time);
    if (abs(time_diff) > 10) {
        printk("RTC time validation failed: diff=%d ms\n", time_diff);
        return false;
    }
    
    printk("RTC sync validation passed: diff=%d ms, confidence=%d\n", 
           time_diff, req->sync_confidence);
    return true;
}

// 處理註冊請求 (使用智能時隙分配)
static void process_registration_request(struct registration_request *req, uint32_t receive_time)
{
    // 使用智能時隙分配算法
    uint8_t assigned_slot = find_optimal_timeslot(req->phy_type, req->tx_power_level);
    
    if (assigned_slot == 0) {
        printk("No optimal slots available for PHY 0x%02x\n", req->phy_type);
        return;
    }
    
    // 檢查 PHY 類型計數器限制
    if (req->phy_type == 0x01 && central.ble40_device_count >= 20) {
        printk("BLE 4.0 1M PHY slots full\n");
        return;
    } else if (req->phy_type == 0x04 && central.coded_device_count >= 20) {
        printk("Coded PHY slots full\n");
        return;
    }
    
    // 註冊設備 (包含干擾避免信息)
    for (int i = 0; i < 40; i++) {
        if (!central.devices[i].is_active) {
            // 將設備ID複製到地址結構中
            memcpy(central.devices[i].address.a.val, req->device_id, 6);
            central.devices[i].address.type = BT_ADDR_LE_RANDOM; // 假設使用隨機地址
            central.devices[i].device_id = i + 1;
            central.devices[i].phy_type = req->phy_type;
            central.devices[i].assigned_slot = assigned_slot;
            central.devices[i].is_active = true;
            central.devices[i].last_rtc_sync = receive_time;
            central.devices[i].rtc_offset = req->rtc_offset_us;
            
            // 初始化干擾避免相關參數
            central.devices[i].signal_strength = req->tx_power_level;
            central.devices[i].interference_level = 0;
            central.devices[i].transmission_success_rate = 1000; // 初始 100% 成功率
            central.devices[i].last_activity_time = receive_time;
            central.devices[i].retry_count = 0;
            
            // 更新時隙使用映射
            central.scheduler.slot_usage_map[assigned_slot] = 1;
            
            // 更新設備計數器
            if (req->phy_type == 0x01) {
                central.ble40_device_count++;
            } else if (req->phy_type == 0x04) {
                central.coded_device_count++;
            }
            
            printk("Device registered with intelligent slot allocation:\n");
            printk("  ID=%d, PHY=0x%02x, Slot=%d, RTC_offset=%d μs\n",
                   central.devices[i].device_id, req->phy_type, 
                   assigned_slot, req->rtc_offset_us);
            printk("  Signal_strength=%d dBm, Slot_quality=%d\n",
                   req->tx_power_level, central.scheduler.quality_score[assigned_slot]);
            break;
        }
    }
}

// 處理數據請求
void handle_data_request(struct pawr_beacon *beacon, uint32_t local_time)
{
    // 檢查是否輪到我的時隙
    if (beacon->current_slot != peripheral.assigned_slot) {
        return;
    }
    
    // 檢查是否有針對我的請求
    if (beacon->request_target_phy != peripheral.my_phy_type &&
        beacon->request_target_phy != 0xFF) { // 0xFF = 廣播給所有設備
        return;
    }
    
    // 準備回應數據 (包含同步後的 RTC 時間戳)
    struct data_response response = {
        .message_type = 0x30,
        .device_id = peripheral.my_device_id,
        .response_slot = beacon->current_slot,
        .synced_rtc_timestamp = get_synced_rtc_time(),
        .rtc_sync_confidence = peripheral.sync_confidence,
        .phy_type = peripheral.my_phy_type,
    };
    
    // 讀取感測器數據
    read_sensor_data(response.sensor_data, sizeof(response.sensor_data));
    
    // 保存回應數據
    memcpy(peripheral.pending_response, &response, sizeof(response));
    peripheral.response_length = sizeof(response);
    
    // 計算適應性回應延遲 (基於干擾避免)
    uint32_t cycle_elapsed = (local_time - peripheral.synced_cycle_start) % CYCLE_DURATION_MS;
    uint32_t slot_elapsed = cycle_elapsed % SLOT_DURATION_MS;
    uint32_t base_delay = (SLOT_DURATION_MS / 2) - slot_elapsed; // 基礎延遲到時隙中間
    
    // 應用適應性延遲調整
    uint32_t adaptive_delay = calculate_adaptive_response_delay(base_delay);
    
    // 確保延遲在合理範圍內
    if (adaptive_delay > (SLOT_DURATION_MS * 8 / 10)) {
        adaptive_delay = SLOT_DURATION_MS * 8 / 10; // 最多占用時隙的 80%
    }
    
    if (adaptive_delay > 0) {
        k_work_schedule(&peripheral.response_work, K_MSEC(adaptive_delay));
    } else {
        k_work_submit(&peripheral.response_work.work);
    }
    
    printk("Response scheduled: base=%d ms, adaptive=%d ms, backoff_level=%d\n",
           base_delay, adaptive_delay, peripheral.interference.retry_backoff_level);
}

// 回應工作處理
static void response_work_handler(struct k_work *work)
{
    // 停止掃描，切換到廣播模式
    if (peripheral.scanning_active) {
        bt_le_scan_stop();
        peripheral.scanning_active = false;
    }
    
    // 🚀 雙模式響應廣播數據
    struct bt_data response_ad_data[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, peripheral.pending_response, 
                peripheral.response_length),
    };
    
    // 根據分配的 PHY 類型選擇合適的廣播方式
    bool use_coded_phy = (peripheral.assigned_slot >= 30); // Slot 30-49 使用 Coded PHY
    int adv_err = 0; // 預先聲明廣播錯誤變數
    
    if (use_coded_phy) {
        // 🔵 使用 Extended Advertising + Coded PHY
        static struct bt_le_adv_param response_coded_param = {
            .id = BT_ID_DEFAULT,
            .sid = 3,  // 響應專用 Set ID
            .secondary_max_skip = 0,
            .options = (BT_LE_ADV_OPT_EXT_ADV | 
                       BT_LE_ADV_OPT_ONE_TIME |
                       BT_LE_ADV_OPT_CODED),
            .interval_min = BT_GAP_ADV_FAST_INT_MIN_1,
            .interval_max = BT_GAP_ADV_FAST_INT_MAX_1,
        };
        
        struct bt_le_ext_adv *response_coded_adv;
        int err = bt_le_ext_adv_create(&response_coded_param, NULL, &response_coded_adv);
        if (err == 0) {
            err = bt_le_ext_adv_set_data(response_coded_adv, response_ad_data, 
                                        ARRAY_SIZE(response_ad_data), NULL, 0);
            if (err == 0) {
                err = bt_le_ext_adv_start(response_coded_adv, NULL);
            }
        }
        adv_err = err; // 賦值而不是重新聲明
        printk("🔵 Coded PHY response sent (slot %d, err %d)\n", 
               peripheral.assigned_slot, adv_err);
    } else {
        // 🟡 使用 Legacy Advertising (1M PHY)
        static struct bt_le_adv_param response_legacy_param = {
            .id = BT_ID_DEFAULT,
            .options = BT_LE_ADV_OPT_ONE_TIME,
            .interval_min = BT_GAP_ADV_FAST_INT_MIN_1,
            .interval_max = BT_GAP_ADV_FAST_INT_MAX_1,
        };
        
        adv_err = bt_le_adv_start(&response_legacy_param, response_ad_data, 
                                     ARRAY_SIZE(response_ad_data), NULL, 0);
        printk("🟡 Legacy response sent (slot %d, err %d)\n", 
               peripheral.assigned_slot, adv_err);
    }
    
    // 記錄傳輸結果
    bool transmission_success = (adv_err == 0);
    record_transmission_result(transmission_success);
    
    if (transmission_success) {
        printk("Data response sent in slot %d (success rate: %d‰)\n", 
               peripheral.assigned_slot,
               (peripheral.interference.successful_transmissions * 1000) / 
               peripheral.interference.transmission_attempts);
    } else {
        printk("Data response failed in slot %d (err %d)\n", 
               peripheral.assigned_slot, adv_err);
    }
    
    // 3ms 後停止廣播並恢復掃描 (適應性調整)
    uint32_t recovery_delay = 3;
    if (peripheral.interference.retry_backoff_level > 0) {
        recovery_delay += peripheral.interference.retry_backoff_level;
    }
    k_work_schedule(&resume_scan_work, K_MSEC(recovery_delay));
}

// 恢復掃描工作
static void resume_scan_work_handler(struct k_work *work)
{
    bt_le_adv_stop();
    
    int err = bt_le_scan_start(&peripheral.scan_param, central_scan_cb);
    if (!err) {
        peripheral.scanning_active = true;
    }
}

// 檢測碰撞和干擾
static bool detect_transmission_collision(void)
{
    uint32_t current_time = k_uptime_get_32();
    
    // 如果最近發生過碰撞 (在 100ms 內)
    if (peripheral.interference.last_collision_time > 0 &&
        (current_time - peripheral.interference.last_collision_time) < 100) {
        return true;
    }
    
    // 根據成功率判斷是否存在嚴重干擾
    if (peripheral.interference.transmission_attempts > 10) {
        uint32_t success_rate = (peripheral.interference.successful_transmissions * 1000) / 
                               peripheral.interference.transmission_attempts;
        
        if (success_rate < 700) { // 成功率低於 70%
            return true;
        }
    }
    
    return false;
}

// 適應性調整傳輸參數
static void adapt_transmission_parameters(void)
{
    bool collision_detected = detect_transmission_collision();
    
    if (collision_detected) {
        peripheral.interference.collision_count++;
        peripheral.interference.last_collision_time = k_uptime_get_32();
        
        // 增加退避等級 (最多 5 級)
        if (peripheral.interference.retry_backoff_level < 5) {
            peripheral.interference.retry_backoff_level++;
        }
        
        // 調整回應延遲偏移 (避免固定時間衝突)
        peripheral.interference.response_delay_offset = 
            (peripheral.interference.retry_backoff_level * 500) + 
            (k_uptime_get_32() % 1000); // 添加隨機性
        
        // 降低發射功率以減少干擾
        if (peripheral.interference.transmission_power > -8) {
            peripheral.interference.transmission_power -= 2;
        }
        
        printk("Collision detected! Adapted parameters: backoff=%d, delay=%d μs, power=%d dBm\n",
               peripheral.interference.retry_backoff_level,
               peripheral.interference.response_delay_offset,
               peripheral.interference.transmission_power);
        
    } else {
        // 成功傳輸，逐漸恢復正常參數
        if (peripheral.interference.retry_backoff_level > 0) {
            peripheral.interference.retry_backoff_level--;
        }
        
        if (peripheral.interference.response_delay_offset > 0) {
            peripheral.interference.response_delay_offset = 
                peripheral.interference.response_delay_offset * 9 / 10; // 逐漸減少
        }
        
        // 逐漸恢復發射功率
        if (peripheral.interference.transmission_power < 0) {
            peripheral.interference.transmission_power++;
        }
    }
}

// 計算適應性回應延遲
static uint32_t calculate_adaptive_response_delay(uint32_t base_delay_ms)
{
    uint32_t adaptive_delay = base_delay_ms;
    
    // 添加基於退避等級的延遲
    adaptive_delay += peripheral.interference.retry_backoff_level * 2;
    
    // 添加微秒級偏移 (轉換為毫秒)
    adaptive_delay += peripheral.interference.response_delay_offset / 1000;
    
    // 添加基於信道品質的調整
    if (peripheral.interference.channel_quality < 150) {
        adaptive_delay += (200 - peripheral.interference.channel_quality) / 50;
    }
    
    // 確保延遲不超過時隙長度的 80%
    return MIN(adaptive_delay, 16);
}

// 記錄傳輸結果
static void record_transmission_result(bool success)
{
    peripheral.interference.transmission_attempts++;
    
    if (success) {
        peripheral.interference.successful_transmissions++;
        printk("Transmission successful (rate: %d‰)\n",
               (peripheral.interference.successful_transmissions * 1000) / 
               peripheral.interference.transmission_attempts);
    } else {
        printk("Transmission failed (attempts: %d)\n",
               peripheral.interference.transmission_attempts);
    }
    
    // 執行適應性調整
    adapt_transmission_parameters();
}

// 更新時隙狀態到信標中
static void update_slot_status(struct pawr_beacon *beacon)
{
    // 重置狀態
    beacon->registration_status = 0xFF; // 所有註冊時隙默認可用
    memset(beacon->ble40_slot_status, 0xFF, sizeof(beacon->ble40_slot_status));
    memset(beacon->coded_slot_status, 0xFF, sizeof(beacon->coded_slot_status));
    
    // 更新已分配的時隙狀態
    for (int i = 0; i < 40; i++) {
        if (central.devices[i].is_active) {
            uint8_t slot = central.devices[i].assigned_slot;
            if (slot >= 10 && slot <= 29) {
                // BLE 4.0 時隙 (slot 10-29 -> bit 0-19)
                uint8_t bit_index = slot - 10;
                uint8_t byte_index = bit_index / 8;
                uint8_t bit_offset = bit_index % 8;
                beacon->ble40_slot_status[byte_index] &= ~(1 << bit_offset);
            } else if (slot >= 30 && slot <= 49) {
                // Coded PHY 時隙 (slot 30-49 -> bit 0-19)
                uint8_t bit_index = slot - 30;
                uint8_t byte_index = bit_index / 8;
                uint8_t bit_offset = bit_index % 8;
                beacon->coded_slot_status[byte_index] &= ~(1 << bit_offset);
            }
        }
    }
}

// 重置註冊時隙可用性
static void reset_registration_slots(void)
{
    // 註冊時隙 0-9 總是可用，不需要特殊處理
    printk("Registration slots reset for new cycle\n");
}
