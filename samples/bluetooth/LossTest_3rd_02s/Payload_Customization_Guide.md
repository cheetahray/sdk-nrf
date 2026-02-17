# Loss Test Payload Customization Guide

**Target Platform:** Silicon Labs BLE Stack  
**Source Reference:** Nordic nRF52833 Loss Test Project  
**Document Purpose:** 提供完整的客製化 Payload 實作規格供 AI 參考移植

---

## 1. 識別碼定義 (Identifier Definitions)

### 1.1 Manufacturer ID

```c
#ifndef MANUFACTURER_ID
  #define MANUFACTURER_ID   ((uint16_t)0xFFFF)
#endif
```

**說明：**
- 使用 `0xFFFF` 表示測試/開發用途（非官方註冊 ID）
- Nordic 官方 ID 是 `0x0059`，但此專案用於內部測試，故使用 `0xFFFF`
- Silicon Labs 實作時可保持 `0xFFFF` 或根據需求修改

### 1.2 Form ID (封包格式識別碼)

```c
#ifndef LOSS_TEST_FORM_ID
  #define LOSS_TEST_FORM_ID ((uint16_t)0xBAAB)
#endif
```

**說明：**
- `0xBAAB` 用於識別這是 Loss Test 專用封包格式
- 接收端會驗證此 ID 以過濾其他 BLE 裝置
- 必須在發送端和接收端保持一致

---

## 2. Payload 結構定義 (Payload Structures)

### 2.1 DEVICE_INFO_ST (標準 Extended Advertising 封包)

**用途：** 用於 BLE 5.x Extended Advertising (2M PHY, Coded S8 PHY)

```c
typedef struct __attribute__((__packed__)) {
    uint16_t man_id;         // Manufacturer ID = 0xFFFF
    uint16_t form_id;        // Form ID = 0xBAAB
    int16_t  pre_cnt;        // Previous count (封包計數器前值)
    uint16_t flw_cnt;        // Follow count (封包計數器當前值)
    struct __attribute__((scalar_storage_order("big-endian"))) {
        uint64_t eui_64;     // EUI-64 設備唯一識別碼 (大端序)
    };
} DEVICE_INFO_ST;
```

**關鍵特性：**
- **尺寸：** 16 bytes
- **Endianness：** EUI-64 欄位使用 **Big-Endian** (網路位元組序)
- **Packing：** 使用 `__attribute__((__packed__))` 確保無填充字節
- **適用 PHY：** 2M PHY, Coded S8 PHY

### 2.2 DEVICE_INFO_BTv4_ST (BLE 4.x Legacy 相容封包)

**用途：** 用於 BLE 4.x Legacy Advertising (1M PHY, 無 Extended Advertising)

```c
typedef struct __attribute__((__packed__)) {
    DEVICE_INFO_ST device_info;  // 包含標準 16 bytes 結構
    uint8_t tail[10];             // 填充 10 bytes (確保符合 31 byte 限制)
} DEVICE_INFO_BTv4_ST;
```

**關鍵特性：**
- **尺寸：** 26 bytes (16 + 10)
- **用途：** tail[10] 用於填充，確保 Legacy Advertising 封包達到最佳長度
- **適用 PHY：** 1M PHY (Legacy)

### 2.3 NUMCAST_INFO_ST (連號測試封包)

**用途：** 快速連續封包測試，檢測遺失與錯誤率

```c
typedef struct __attribute__((__packed__)) {
    uint16_t man_id;             // Manufacturer ID = 0xFFFF
    uint16_t form_id;            // Form ID = 0xBAAB
    uint16_t number_cast_form[4]; // 連號資訊 (4 個 uint16 計數器)
} NUMCAST_INFO_ST;
```

**關鍵特性：**
- **尺寸：** 10 bytes
- **用途：** number_cast_form[4] 存放連續計數器，用於快速封包順序驗證
- **適用 PHY：** 2M PHY, Coded S8 PHY

---

## 3. Payload 初始化與填充 (Initialization)

### 3.1 從硬體讀取 EUI-64

**Nordic nRF52 實作：**

```c
// 從 FICR (Factory Information Configuration Registers) 讀取
*((uint64_t *)&device_info_form[index].eui_64) = *((uint64_t *)NRF_FICR->DEVICEADDR);
```

**Silicon Labs 對應實作建議：**

Silicon Labs 平台應從對應的硬體暫存器讀取：
- **EFR32 系列：** 使用 `DEVINFO->UNIQUEH` 和 `DEVINFO->UNIQUEL`
- **建議實作：**

```c
// EFR32 範例 (Silicon Labs)
uint64_t eui64;
eui64 = ((uint64_t)DEVINFO->UNIQUEH << 32) | DEVINFO->UNIQUEL;
device_info.eui_64 = eui64;  // 注意 Big-Endian 轉換
```

**重要：EUI-64 必須使用 Big-Endian 格式**

### 3.2 初始化 Payload 結構

**Nordic 實作範例：**

```c
static volatile DEVICE_INFO_ST device_info_form[4] = {
    {MANUFACTURER_ID, LOSS_TEST_FORM_ID, INT16_MIN, 255},
    {MANUFACTURER_ID, LOSS_TEST_FORM_ID, INT16_MIN, 255},
    {MANUFACTURER_ID, LOSS_TEST_FORM_ID, INT16_MIN, 255},
    {MANUFACTURER_ID, LOSS_TEST_FORM_ID, INT16_MIN, 255}
};

static volatile DEVICE_INFO_BTv4_ST device_info_bt4_form = {
    .device_info = {MANUFACTURER_ID, LOSS_TEST_FORM_ID, INT16_MIN, 255}
};

static NUMCAST_INFO_ST numcast_info_form = {
    MANUFACTURER_ID, 
    LOSS_TEST_FORM_ID
};
```

**Silicon Labs 實作建議：**
- 為每個 PHY 類型維護獨立的 payload 實例
- 初始值：`pre_cnt = INT16_MIN`, `flw_cnt = 255`
- 在首次使用前填入硬體 EUI-64

---

## 4. Advertising Data 封裝 (Advertising Data Encapsulation)

### 4.1 Extended Advertising 封包結構 (BLE 5.x)

**使用場景：** 2M PHY, Coded S8 PHY

```c
static struct bt_data ratio_test_data_set[] = {
    // AD Type 1: Flags
    {
        .type = BT_DATA_FLAGS,
        .data_len = 1,
        .data = (uint8_t[]){0x06}  // General Discoverable + BR/EDR Not Supported
    },
    // AD Type 2: Manufacturer Specific Data
    {
        .type = BT_DATA_MANUFACTURER_DATA,
        .data_len = sizeof(DEVICE_INFO_ST),  // 16 bytes
        .data = (const uint8_t *)&device_info_form[index]
    },
    // AD Type 3: Complete Local Name (Optional)
    {
        .type = BT_DATA_NAME_COMPLETE,
        .data = "LossTst(XXX)",
        .data_len = 12
    }
};
```

**Silicon Labs 對應 API：**
```c
// 使用 sl_bt_advertiser_set_data 設定資料
// 或 sl_bt_extended_advertiser_set_data (Extended Advertising)
```

### 4.2 Legacy Advertising 封包結構 (BLE 4.x)

**使用場景：** 1M PHY (Legacy)

```c
static struct bt_data legacy_test_data[] = {
    // AD Type 1: Flags
    {
        .type = BT_DATA_FLAGS,
        .data_len = 1,
        .data = (uint8_t[]){0x06}
    },
    // AD Type 2: Manufacturer Specific Data
    {
        .type = BT_DATA_MANUFACTURER_DATA,
        .data_len = sizeof(DEVICE_INFO_BTv4_ST),  // 26 bytes
        .data = (const uint8_t *)&device_info_bt4_form
    }
};
```

**重要：** Legacy Advertising 最大長度為 31 bytes (含 AD headers)

### 4.3 Number Cast 封包結構

**使用場景：** 連號測試模式

```c
static struct bt_data number_cast_data_set[] = {
    // AD Type 1: Flags
    {
        .type = BT_DATA_FLAGS,
        .data_len = 1,
        .data = (uint8_t[]){0x06}
    },
    // AD Type 2: DEVICE_INFO_ST (基本資訊)
    {
        .type = BT_DATA_MANUFACTURER_DATA,
        .data_len = sizeof(DEVICE_INFO_ST),
        .data = (const uint8_t *)&device_info_form[index]
    },
    // AD Type 3: NUMCAST_INFO_ST (連號資訊)
    {
        .type = BT_DATA_MANUFACTURER_DATA,  // 也是 0xFF
        .data_len = sizeof(NUMCAST_INFO_ST),  // 10 bytes
        .data = (uint8_t *)&numcast_info_form
    }
};
```

**特殊說明：** Number Cast 模式使用兩個 Manufacturer Data AD Types

---

## 5. 接收端解析邏輯 (Receiver Parser Logic)

### 5.1 Parser 選擇流程

接收端根據 PHY 類型選擇對應的 parser：

```c
// 步驟 1: 從 Advertising Report 提取 PHY 資訊
uint8_t prim_phy = adv_info->prim_phy;  // 1=1M, 3=Coded
uint8_t sec_phy = adv_info->sec_phy;    // 0=Legacy, 1=1M, 2=2M, 3=Coded

// 步驟 2: 判斷 PHY 組合
int8_t idx;
if (prim_phy == 1 && sec_phy == 2)      idx = 0;  // 2M PHY
else if (prim_phy == 1 && sec_phy == 1) idx = 1;  // 1M PHY (Ext)
else if (prim_phy == 3 && sec_phy == 3) idx = 2;  // Coded S8
else if (prim_phy == 1 && sec_phy == 0) idx = 3;  // 1M Legacy
else return -1;  // Unknown PHY

// 步驟 3: 呼叫對應 parser
bt_data_parse(adv_data, parser_function, user_data);
```

### 5.2 test_form_parser (標準封包解析器)

**用途：** 解析 DEVICE_INFO_ST 或 DEVICE_INFO_BTv4_ST

```c
static bool test_form_parser(struct bt_data *data, void *user_data)
{
    // 步驟 1: 檢查 Flags AD Type
    if (BT_DATA_FLAGS == data->type) {
        // 確認這是第一個 AD element
        if (0 == step_flag_counter) {
            step_flag_counter++;
        } else {
            return false;  // 格式錯誤
        }
    }
    // 步驟 2: 解析 Manufacturer Data
    else if (1 == step_flag_counter && BT_DATA_MANUFACTURER_DATA == data->type) {
        DEVICE_INFO_ST *rcv_data = (DEVICE_INFO_ST *)data->data;
        
        // 驗證 Manufacturer ID 和 Form ID
        if (MANUFACTURER_ID == rcv_data->man_id && 
            LOSS_TEST_FORM_ID == rcv_data->form_id) {
            
            // 處理封包 (統計、計數、RSSI 記錄)
            tst_form_packet_rcv(adv_info, rcv_data);
            return false;  // 解析完成，停止迭代
        } else {
            return false;  // ID 不符，拒絕封包
        }
    } else {
        return false;  // 其他 AD Type，忽略
    }
    
    return true;  // 繼續解析下一個 AD element
}
```

**關鍵驗證點：**
1. ✅ Manufacturer ID = 0xFFFF
2. ✅ Form ID = 0xBAAB
3. ✅ Flags AD Type 必須是第一個
4. ✅ Manufacturer Data 必須是第二個

### 5.3 numcast_parser (連號封包解析器)

**用途：** 解析包含 NUMCAST_INFO_ST 的封包

```c
static bool numcast_parser(struct bt_data *data, void *user_data)
{
    // 步驟 1: Flags 驗證 (同上)
    if (BT_DATA_FLAGS == data->type) {
        if (0 == step_flag_counter) step_flag_counter++;
        else return false;
    }
    // 步驟 2: 解析 Manufacturer Data
    else if (1 == step_flag_counter && BT_DATA_MANUFACTURER_DATA == data->type) {
        step_special_stream++;
        
        // Legacy 模式 (idx == 3)
        if (idx == 3) {
            DEVICE_INFO_BTv4_ST *rcv_data = (DEVICE_INFO_BTv4_ST *)data->data;
            
            // 驗證 ID 和 tail marker
            if (MANUFACTURER_ID == rcv_data->device_info.man_id && 
                LOSS_TEST_FORM_ID == rcv_data->device_info.form_id &&
                UINT16_MAX == *((uint16_t *)rcv_data->tail)) {
                
                numcast_packet_evt(idx, rcv_data, 2 + rcv_data->tail, rssi);
                return false;
            } else {
                return false;
            }
        }
        // Extended Advertising 模式
        else {
            // 第一個 Manufacturer Data: DEVICE_INFO_ST
            if (1 == step_special_stream) {
                if (data->data_len == sizeof(DEVICE_INFO_ST)) {
                    temp_ptr = data->data;  // 暫存
                } else {
                    return false;
                }
            }
            // 第二個 Manufacturer Data: NUMCAST_INFO_ST
            else if (2 == step_special_stream) {
                NUMCAST_INFO_ST *numcast_data = (NUMCAST_INFO_ST *)data->data;
                
                if (temp_ptr != NULL && 
                    0xFF == data->type && 
                    sizeof(NUMCAST_INFO_ST) == data->data_len) {
                    
                    numcast_packet_evt(idx, 
                                     (DEVICE_INFO_ST *)temp_ptr, 
                                     numcast_data->number_cast_form, 
                                     rssi);
                    return false;
                } else {
                    return false;
                }
            }
        }
    } else {
        return false;
    }
    
    return true;
}
```

**Number Cast 特殊邏輯：**
1. Extended Advertising 模式有**兩個** Manufacturer Data AD Types
2. 第一個是 DEVICE_INFO_ST (16 bytes)
3. 第二個是 NUMCAST_INFO_ST (10 bytes)
4. Legacy 模式使用 DEVICE_INFO_BTv4_ST，tail 的前 2 bytes 標記為 `0xFFFF`

---

## 6. PHY 類型與 Payload 對應表

| PHY 組合                  | idx | Payload 類型            | 尺寸     | Parser           |
|---------------------------|-----|-------------------------|----------|------------------|
| Prim=1M, Sec=2M           | 0   | DEVICE_INFO_ST          | 16 bytes | test_form_parser |
| Prim=1M, Sec=1M           | 1   | DEVICE_INFO_ST          | 16 bytes | test_form_parser |
| Prim=Coded, Sec=Coded(S8) | 2   | DEVICE_INFO_ST          | 16 bytes | test_form_parser |
| Prim=1M, Sec=0 (Legacy)   | 3   | DEVICE_INFO_BTv4_ST     | 26 bytes | test_form_parser |
| Number Cast (2M)          | 0   | DEVICE_INFO_ST + NUMCAST| 26 bytes | numcast_parser   |
| Number Cast (Legacy)      | 3   | DEVICE_INFO_BTv4_ST*    | 26 bytes | numcast_parser   |

**注意：** Number Cast Legacy 模式的 tail[0:1] = 0xFFFF 作為標記

---

## 7. Silicon Labs 實作檢查清單

### 7.1 必須實作的項目

- [ ] 定義 `MANUFACTURER_ID = 0xFFFF`
- [ ] 定義 `LOSS_TEST_FORM_ID = 0xBAAB`
- [ ] 實作 `DEVICE_INFO_ST` 結構（16 bytes, packed, EUI-64 Big-Endian）
- [ ] 實作 `DEVICE_INFO_BTv4_ST` 結構（26 bytes）
- [ ] 實作 `NUMCAST_INFO_ST` 結構（10 bytes）
- [ ] 從 Silicon Labs 硬體讀取唯一 ID 填入 `eui_64` 欄位
- [ ] 正確設定 Advertising Data（含 Flags + Manufacturer Data）
- [ ] 實作接收端 parser 驗證 Manufacturer ID 和 Form ID
- [ ] 根據 PHY 類型選擇對應的 payload 結構

### 7.2 關鍵注意事項

1. **Endianness:** EUI-64 必須使用 Big-Endian（網路位元組序）
2. **Struct Packing:** 使用編譯器屬性確保無填充字節
3. **AD Type 順序:** Flags 必須在前，Manufacturer Data 在後
4. **Legacy Advertising:** 總長度限制 31 bytes
5. **Extended Advertising:** 可使用更大長度，但建議保持 payload 精簡

### 7.3 Silicon Labs API 對應建議

| Nordic API                      | Silicon Labs API                        |
|---------------------------------|-----------------------------------------|
| `bt_le_ext_adv_create()`        | `sl_bt_advertiser_create_set()`         |
| `bt_le_ext_adv_set_data()`      | `sl_bt_extended_advertiser_set_data()`  |
| `bt_le_ext_adv_start()`         | `sl_bt_advertiser_start()`              |
| `bt_le_scan_start()`            | `sl_bt_scanner_start()`                 |
| `bt_data_parse()`               | 需自行實作 AD data parser               |
| `NRF_FICR->DEVICEADDR`          | `DEVINFO->UNIQUEH/L`                    |

---

## 8. 測試驗證步驟

### 8.1 發送端驗證

1. 使用 nRF Connect for Mobile 掃描廣播
2. 檢查 Manufacturer Data 前 4 bytes:
   - Byte 0-1: `0xFF 0xFF` (MANUFACTURER_ID, Little-Endian)
   - Byte 2-3: `0xAB 0xBA` (LOSS_TEST_FORM_ID, Little-Endian)
3. 驗證 EUI-64 欄位是否為 Big-Endian
4. 確認封包長度符合預期（16/26/10 bytes + header）

### 8.2 接收端驗證

1. 掃描並過濾 Manufacturer ID = 0xFFFF 的封包
2. 驗證 Form ID = 0xBAAB
3. 提取 pre_cnt, flw_cnt, eui_64 並記錄
4. 計算封包遺失率（應與 Nordic 端對應）
5. 驗證 RSSI 讀取正確

### 8.3 互通性測試

- Nordic 發送 → Silicon Labs 接收
- Silicon Labs 發送 → Nordic 接收
- 確認雙向統計數據一致

---

## 9. 範例程式碼片段

### 9.1 Silicon Labs 發送端範例

```c
#include "sl_bt_api.h"

#define MANUFACTURER_ID   0xFFFF
#define LOSS_TEST_FORM_ID 0xBAAB

typedef struct __attribute__((__packed__)) {
    uint16_t man_id;
    uint16_t form_id;
    int16_t  pre_cnt;
    uint16_t flw_cnt;
    uint64_t eui_64;  // Big-Endian
} DEVICE_INFO_ST;

DEVICE_INFO_ST payload;

void init_payload(void) {
    payload.man_id = MANUFACTURER_ID;
    payload.form_id = LOSS_TEST_FORM_ID;
    payload.pre_cnt = 0;
    payload.flw_cnt = 0;
    
    // 讀取硬體 ID (Big-Endian 轉換)
    uint64_t unique_id = ((uint64_t)DEVINFO->UNIQUEH << 32) | DEVINFO->UNIQUEL;
    payload.eui_64 = __builtin_bswap64(unique_id);  // 轉 Big-Endian
}

void start_advertising(void) {
    uint8_t adv_data[32];
    uint8_t idx = 0;
    
    // AD Type 1: Flags
    adv_data[idx++] = 2;     // Length
    adv_data[idx++] = 0x01;  // Type: Flags
    adv_data[idx++] = 0x06;  // Value
    
    // AD Type 2: Manufacturer Data
    adv_data[idx++] = 1 + sizeof(DEVICE_INFO_ST);  // Length
    adv_data[idx++] = 0xFF;  // Type: Manufacturer Data
    memcpy(&adv_data[idx], &payload, sizeof(DEVICE_INFO_ST));
    idx += sizeof(DEVICE_INFO_ST);
    
    sl_bt_extended_advertiser_set_data(advertising_set_handle, 
                                       idx, 
                                       adv_data);
    sl_bt_advertiser_start(advertising_set_handle, ...);
}
```

### 9.2 Silicon Labs 接收端範例

```c
void process_scan_response(sl_bt_evt_scanner_scan_report_t *scan_report) {
    uint8_t *data = scan_report->data.data;
    uint8_t len = scan_report->data.len;
    uint8_t idx = 0;
    
    while (idx < len) {
        uint8_t field_len = data[idx++];
        if (field_len == 0) break;
        
        uint8_t type = data[idx++];
        field_len--;  // 扣除 type byte
        
        if (type == 0xFF && field_len >= sizeof(DEVICE_INFO_ST)) {
            DEVICE_INFO_ST *payload = (DEVICE_INFO_ST *)&data[idx];
            
            // 驗證 ID
            if (payload->man_id == MANUFACTURER_ID && 
                payload->form_id == LOSS_TEST_FORM_ID) {
                
                // 處理有效封包
                process_loss_test_packet(payload, scan_report->rssi);
            }
        }
        
        idx += field_len;
    }
}
```

---

## 10. 附錄：Endianness 說明

### 為何 EUI-64 使用 Big-Endian？

1. **網路標準：** IEEE 802 標準規定 EUI-64 使用網路位元組序（Big-Endian）
2. **跨平台相容：** 確保不同架構（ARM, x86, RISC-V）間資料一致
3. **可讀性：** Wireshark 等工具預設顯示 Big-Endian

### Little-Endian vs Big-Endian 範例

假設 EUI-64 = `0x0123456789ABCDEF`

**Little-Endian (Nordic nRF 預設，需轉換):**
```
Memory: EF CD AB 89 67 45 23 01
```

**Big-Endian (網路位元組序，正確格式):**
```
Memory: 01 23 45 67 89 AB CD EF
```

**轉換方式：**
```c
// Nordic nRF
device_info.eui_64 = sys_cpu_to_be64(*((uint64_t *)NRF_FICR->DEVICEADDR));

// Silicon Labs
uint64_t unique = ((uint64_t)DEVINFO->UNIQUEH << 32) | DEVINFO->UNIQUEL;
device_info.eui_64 = __builtin_bswap64(unique);
```

---

## 文件版本

- **版本：** 1.0
- **最後更新：** 2026-02-15
- **作者：** Nordic nRF52833 專案移植文件
- **目標平台：** Silicon Labs EFR32 系列

**結束**
