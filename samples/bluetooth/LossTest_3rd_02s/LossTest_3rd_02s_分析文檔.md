# LossTest_3rd_02s 專案分析文檔

> \
> 本文檔整理了 LossTest_3rd_02s 藍牙丟包測試系統的完整分析日期：2026-02-13


---

## 目錄

* [專案概述](#%E5%B0%88%E6%A1%88%E6%A6%82%E8%BF%B0)
* [常見問題解答](#%E5%B8%B8%E8%A6%8B%E5%95%8F%E9%A1%8C%E8%A7%A3%E7%AD%94)
* [功能特性](#%E5%8A%9F%E8%83%BD%E7%89%B9%E6%80%A7)
* [測試情境配置](#%E6%B8%AC%E8%A9%A6%E6%83%85%E5%A2%83%E9%85%8D%E7%BD%AE)
* [配置設置位置](#%E9%85%8D%E7%BD%AE%E8%A8%AD%E7%BD%AE%E4%BD%8D%E7%BD%AE)
* [廣告封包 Payload 結構](#%E5%BB%A3%E5%91%8A%E5%B0%81%E5%8C%85-payload-%E7%B5%90%E6%A7%8B)
* [建置方法](#%E5%BB%BA%E7%BD%AE%E6%96%B9%E6%B3%95)


---

## 專案概述

**LossTest_3rd_02s** 是一個基於 Nordic nRF52833 的藍牙 5.x 丟包測試系統，具備完整的雙向通訊測試能力，支援 Extended Advertising 特性。

### 主要用途

* 藍牙廣告封包丟失率測試
* RSSI（信號強度）測量與分析
* 多 PHY 層性能評估（1M, 2M, Coded S8）
* 環境干擾評估
* 長距離通訊測試

### 硬體支援

* nRF52833 Development Kit
* nRF21540 DK（帶 FEM 前端模組）
* 自定義板：RS485 採集板、I²C 感測器板、NuMicro M482 板


---

## 功能特性

### 1. Extended Advertising 支援 ✅

**Kconfig 配置（prj.conf）：**

```properties
CONFIG_BT_EXT_ADV=y
CONFIG_BT_EXT_ADV_MAX_ADV_SET=5
CONFIG_BT_CTLR_PHY_2M=y
CONFIG_BT_CTLR_PHY_CODED=y
CONFIG_BT_CTLR_ADV_DATA_LEN_MAX=240
```

**代碼實現特點：**

* 最多支援 5 個同時運行的廣告集
* 廣告數據長度最大 240 bytes（遠超傳統 31 bytes）
* 支援 Burst 模式快速發送大量封包
* 擴展廣告回調機制

**相關代碼：**

* `losstst_svc.c`: 擴展廣告參數與回調
* `main.c`: 任務觸發與狀態管理

### 2. 雙向通訊能力 ✅

**角色配置（prj.conf）：**

```properties
CONFIG_BT_PERIPHERAL=y      # 可連接廣告
CONFIG_BT_CENTRAL=y         # 掃描和連接
CONFIG_BT_BROADCASTER=y     # 廣播（發送）
CONFIG_BT_OBSERVER=y        # 觀察者（接收/掃描）
```

**通訊方向：**

| 模式 | 說明 | 配置 |
|----|----|----|
| **發送端** | 廣播封包 | `sender_peek_msg()` |
| **接收端** | 掃描封包 | `scanner_peek_msg()` |
| **單向模式** | 只發送，不期待回應 | `uni_cast_method=true` |
| **雙向模式** | 發送並接收對方回應 | `uni_cast_method=false` |

**統計數據：**

```c
// 發送統計
sub_total_snd_1m, sub_total_snd_2m, sub_total_snd_s8, sub_total_snd_ble4

// 接收統計
sub_total_rcv[4], precnt_rcv[4], rcv_stamp[4]

// RSSI 記錄
numcst_rssi_rec[32], env_rssi_rec[4][256]
```


---

## 測試情境配置

### 一、主要場景模式（6 個）

根據 `ext_scr_svc.c` 的 `scene_procedure[]` 陣列定義：

| 場景索引 | 場景名稱 | 功能說明 | 觸發函數 |
|----|----|----|----|
| 0 | **PARM_cfg** | 參數配置場景（主頁） | - |
| 1 | **PARM_2nd_cfg** | 參數配置場景（第二頁） | - |
| 2 | **ENV_task** | 環境監測任務（RSSI 背景值） | `envmon_task_tgr` |
| 3 | **SND_task** | 純發送測試 | `sender_task_tgr` |
| 4 | **RCV_task** | 純接收測試（掃描） | `scanner_task_tgr` |
| 5 | **NUM_task** | 編號封包任務（丟包統計） | `numcst_task_tgr` |

**場景切換方式：**

* 外部螢幕界面：按鈕 12 循環切換
* DIP 開關模式：通過硬體開關直接觸發

### 二、PHY（物理層）配置（4 種）

| PHY 模式 | 主要/次要 PHY | 特點 | 適用場景 |
|----|----|----|----|
| **1M/1M** | 1M / 1M | BLE 傳統模式 | 相容性測試 |
| **1M/2M** | 1M / 2M | 高速模式 | 短距離高吞吐量 |
| **S8/S8** | Coded S8 / S8 | 長距離模式（S=8） | 遠距離測試 |
| **BLEv4** | - | BLE 4.x 相容 | 向下相容測試 |

**PHY 層選擇配置：**

```c
cfg_phy_sel[4] = {true, false, false, false};  // 預設 1M/2M
```

### 三、廣告間隔（11 種）

基於藍牙規範的標準間隔定義：

| 索引 | 範圍 (ms) | 說明 |
|----|----|----|
| 0 | 30\~60 | TGAP(adv_fast_interval1) |
| 1 | 60\~90 | - |
| 2 | 90\~180 | TGAP(adv_fast_interval1_coded) |
| 3 | 100\~150 | TGAP(adv_fast_interval2) |
| 4 | 200\~300 | - |
| 5 | 300\~450 | TGAP(adv_fast_interval2_coded) |
| 6 | 500\~650 | - |
| 7 | 750\~950 | - |
| 8 | 1000\~1200 | TGAP(adv_slow_interval) |
| 9 | 2000\~2400 | - |
| 10 | 3000\~3600 | TGAP(adv_slow_interval_coded) |

**實際參數值：**

```c
// 值 * 0.625ms = 實際時間
// 例如：30ms = 48 * 0.625ms
param_interval[0] = {48, 96};  // 30~60ms
```

### 四、總封包數（7 種）

```c
enum_total_num[] = {500, 1000, 2000, 5000, 10000, 20000, 50000};
```

**用途：**

* 短期測試：500-1000 封包
* 標準測試：2000-5000 封包
* 長期穩定性測試：10000-50000 封包

### 五、TX Power（發射功率）

**範圍配置（prj.conf）：**

```properties
CONFIG_BT_CTLR_TX_PWR_ANTENNA=8        # 最大功率 +8 dBm
CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL=y # 動態功率控制
```

**功率範圍：**

* **理論範圍**：-40 dBm \~ +8 dBm
* **實際範圍**：依硬體和 FEM（前端模組）決定
* **調整步進**：通常 4 dBm（硬體依賴）

**動態調整：**

```c
int32_t txpwr_set(mpsl_tx_power_t pwr);  // nrf52txpwr.c
```

### 六、頻道選擇（3 個獨立開關 = 8 種組合）

**藍牙廣告頻道：**

* **CH37**：2402 MHz
* **CH38**：2426 MHz
* **CH39**：2480 MHz

**組合示例：**


1. ✅ 三頻全開（預設，最大覆蓋）
2. ✅ 37+38（避開 39 的 2.4G WiFi 干擾）
3. ✅ 37+39
4. ✅ 38+39
5. ✅ 僅 37（單頻測試）
6. ✅ 僅 38
7. ✅ 僅 39
8. ⚠️ 全關（不應使用）

### 七、其他配置選項

#### 1. SOC_DCDC（電源模式）

| 模式 | 說明 | 功耗 | 雜訊 |
|----|----|----|----|
| **DC-DC** | 開啟 DC-DC 轉換器 | 低 | 稍高 |
| **LDO** | 使用線性穩壓器 | 高 | 低 |

**配置：**

```properties
CONFIG_SOC_DCDC_NRF52X=y      # 啟用 DC-DC
# CONFIG_SOC_DCDC_NRF52X=y    # 註解掉使用 LDO
```

#### 2. UNI_DIR（傳輸方向）

| 模式 | 說明 | 應用 |
|----|----|----|
| **單向** | 只發送，不期待回應 | 廣播類應用 |
| **雙向** | 發送並接收回應 | 互動式測試 |

#### 3. ANONYMOUS（匿名廣告）

| 模式 | 說明 |
|----|----|
| **匿名** | 不包含裝置 MAC 地址 |
| **非匿名** | 包含裝置 MAC 地址 |


---

## 總計測試情境組合數

**理論組合數：**

6 (場景) × 4 (PHY) × 11 (間隔) × 7 (封包數) × 8 (頻道) × 2 (DCDC) × 2 (方向) × 2 (匿名) × N (功率)

= **>50,000 種配置組合**

**實際使用情境示例：**

### 情境 1：短距離高速測試

```
場景：SND_task + RCV_task
PHY：1M/2M
間隔：30~60 ms (索引 0)
功率：+8 dBm
封包：1000
頻道：37+38+39
```

### 情境 2：長距離低速測試

```
場景：NUM_task
PHY：S8/S8
間隔：1000~1200 ms (索引 8)
功率：+8 dBm
封包：5000
頻道：37+38+39
```

### 情境 3：環境干擾評估

```
場景：ENV_task
PHY：所有 PHY 掃描
間隔：N/A (接收模式)
記錄：RSSI 分佈與統計
```

### 情境 4：DIP Switch 快速切換

```
透過硬體 DIP 開關即時切換：
- 立即發送/接收模式
- 預設參數組合
- 快速現場測試
```


---

## 配置設置位置

配置系統採用**5 層架構**，從編譯時到運行時提供完整的靈活性。

### 層次 1：編譯時配置（Kconfig）

#### 主配置文件：`prj.conf`

**位置：** `/opt/nordic/ncs/v2.8.0/nrf/samples/bluetooth/LossTest_3rd_02s/prj.conf`

**關鍵設定：**

```properties
# 藍牙基本配置
CONFIG_BT=y
CONFIG_BT_EXT_ADV=y
CONFIG_BT_EXT_ADV_MAX_ADV_SET=5

# PHY 支援
CONFIG_BT_CTLR_PHY_2M=y
CONFIG_BT_CTLR_PHY_CODED=y

# 功率控制
CONFIG_BT_CTLR_TX_PWR_ANTENNA=8
CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL=y

# 角色配置
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_CENTRAL=y
CONFIG_BT_BROADCASTER=y
CONFIG_BT_OBSERVER=y

# 緩衝區與數據長度
CONFIG_BT_CTLR_ADV_DATA_LEN_MAX=240
CONFIG_BT_DEVICE_NAME_MAX=62

# UART 配置
CONFIG_UART_INTERRUPT_DRIVEN=y
CONFIG_UART_USE_RUNTIME_CONFIGURE=y

# 其他
CONFIG_DK_LIBRARY=y
CONFIG_GPIO=y
CONFIG_FLASH=y
```

#### 補充配置文件：`supplement_*.conf`（14 個）

根據不同硬體板型和使用情境的配置文件：

| 文件名模式 | 用途 | 關鍵差異 |
|----|----|----|
| `supplement_52833dk_woDIPSW_*.conf` | nRF52833 DK 無 DIP | UART 控制台 |
| `supplement_52833dk_NuM482_*.conf` | 搭配 NuMicro M482 | 特殊 GPIO 配置 |
| `supplement_FFFFar52833_IICsensorBrd_*.conf` | I²C 感測器板 | I²C 與 FEM 配置 |
| `supplement_FFFFar52833_RS485ACQ_*.conf` | RS485 採集板 | RS485 GPIO, RTT 控制台 |
| `supplement_21540dk_woDIPSW_*.conf` | nRF21540 DK | FEM 配置 |

**通用設定項：**

```properties
# 編譯器選項（定義預處理器宏）
CONFIG_COMPILER_OPT="-DDTM_ADAPT_BRD=0 -DSHORT_NM ..."

# 二進位檔名
CONFIG_KERNEL_BIN_NAME="LossTest_XXX"

# 電源模式
CONFIG_SOC_DCDC_NRF52X=y  # DC-DC 或註解使用 LDO

# 控制台類型
CONFIG_RTT_CONSOLE=y      # RTT 或
CONFIG_UART_CONSOLE=y     # UART

# 日誌級別
CONFIG_BT_LOG_LEVEL_OFF=y

# 特殊功能
# CONFIG_SPI=y            # DIP Switch 讀取
```

**範例：RS485 採集板配置**

```properties
# supplement_FFFFar52833_RS485ACQ_woFEM.conf
CONFIG_BT_DIS_MODEL="nRF52833_QDAA"
CONFIG_BT_CTLR_TX_PWR_ANTENNA=8
CONFIG_UART_CONSOLE=n
CONFIG_RTT_CONSOLE=y
CONFIG_COMPILER_OPT="-DDTM_ADAPT_BRD=0 -DSHORT_NM -DRS485ACQ_BRD=1 -DUART_XMT_RCV_METHOD=HAZARD_UART_XMT_RCV -DUART_SEND_BREAK_METHOD=HAZARD_UART_BRK"
CONFIG_KERNEL_BIN_NAME="LossTest_FFFFar52833_RS485ACQ_woFEM"
```

### 層次 2：硬體配置（Device Tree）

#### Overlay 文件：`*.overlay`（12 個）

**用途：** 定義 GPIO 腳位、UART、SPI、I²C 等硬體資源

**範例：RS485 採集板 Overlay**

```dts
// nrf52833dk_RS485ACQ_woFEM.overlay

// UART1 腳位重映射
&uart1_default {
    group1 {
        psels = <NRF_PSEL(UART_TX, 0, 30)>;
    };
    group2 {
        psels = <NRF_PSEL(UART_RX, 0, 31)>;
    };
};

&uart1 {
    status = "okay";
};

// RTT 控制台配置
/ {
    rtt-uart {
        compatible = "segger,rtt-uart";
    };

    chosen {
        zephyr,console = &{/rtt-uart/};
        zephyr,shell-uart = &{/rtt-uart/};
    };
    
    // 數位 I/O 定義
    DIO:zephyr,user {
        dio-gpios = <&gpio0 20 (GPIO_ACTIVE_HIGH | GPIO_PULL_UP)>,
                    <&gpio0 21 (GPIO_ACTIVE_HIGH | GPIO_PULL_UP)>,
                    <&gpio0 22 (GPIO_ACTIVE_HIGH | GPIO_PULL_UP)>,
                    <&gpio0 23 (GPIO_ACTIVE_HIGH | GPIO_PULL_UP)>;
        status = "okay";
    };
};
```

**常見 Overlay 配置項：**

* UART 腳位映射（TX, RX, RTS, CTS）
* SPI 腳位與從設備定義（DIP Switch 讀取）
* GPIO 腳位功能定義
* FEM 控制腳位（PA, LNA, MODE）
* 控制台選擇（UART 或 RTT）

### 層次 3：硬體動態配置（DIP Switch）

**啟用條件：** `DTM_ADAPT_BRD=1`

**實現位置：** `src/main.c` (Line 138-214)

#### DIP Switch 讀取機制

透過 **SPI 接口**讀取 16 位元硬體開關狀態：

```c
// DIP Switch 結構（透過 SPI 讀取）
typedef struct {
    unsigned m0:1;    // Bit 0
    unsigned m1:1;    // Bit 1
    unsigned m2:1;    // Bit 2
    ...
    unsigned m15:1;   // Bit 15
} ADAPT_DIP_SW_ST;

// 全局變數
uint16_t dip_sw_map;      // 當前開關狀態
uint16_t dip_sw_chgd;     // 變更標記

// 初始化 SPI 並讀取
void ext_sio_init(void);
void ext_sio_read(void);

// 每 250ms 輪詢一次
K_WORK_DELAYABLE_DEFINE(unidir_spi_work, unidir_spi_work_handle);
```

#### 配置映射

```c
// 映射到測試參數（需依實際硬體定義）
cfg_switch.ADV_PARAM    // 廣告間隔索引 (0-10)
cfg_switch.PHY_1M       // 啟用 1M PHY
cfg_switch.PHY_S8       // 啟用 Coded PHY
cfg_switch.TX_ATT       // 功率衰減級別
cfg_switch.NUM          // 封包數量索引 (0-6)
cfg_switch.immd_SND     // 立即發送模式
cfg_switch.immd_RCV     // 立即接收模式
cfg_switch.SCANNER      // 掃描模式
```

#### 參數計算函數

```c
// 根據 DIP Switch 計算參數
int8_t get_cfg_tx_pwr(void) {
    return (CONFIG_BT_CTLR_TX_PWR_ANTENNA - (4 * cfg_switch.TX_ATT));
}

uint16_t get_cfg_total_num_idx(void) {
    return cfg_switch.NUM;
}

int8_t get_cfg_interval_idx(void) {
    return MIN(cfg_switch.ADV_PARAM, 10);
}

bool poll_cfg_switch(void) {
    bool result = false;
    if(dip_sw_chgd) {
        cfg_switch.u16_val = dip_sw_map;
        result = true;
    }
    return result;
}
```

#### 配置加載

```c
// src/main.c: load_parm_dipswitch()
void load_parm_dipswitch(void) {
    printk("CFG_sw ADV_PARM(%u) PHY_1M(%u) PHY_S8(%u) SCANNER(%u)\n"
           "       TX_ATT(%u) NUM(%u) immd_RCV(%u) immd_SND(%u)\n",
           cfg_switch.ADV_PARAM, cfg_switch.PHY_1M, cfg_switch.PHY_S8,
           cfg_switch.SCANNER, cfg_switch.TX_ATT, cfg_switch.NUM,
           cfg_switch.immd_RCV, cfg_switch.immd_SND);
    
    round_test_parm.txpwr = get_cfg_tx_pwr();
    round_test_parm.count_idx = get_cfg_total_num_idx();
    round_test_parm.interval_idx = get_cfg_interval_idx();
    round_test_parm.phy_2m = (cfg_switch.PHY_1M) ? true : false;
    round_test_parm.phy_1m = (cfg_switch.PHY_1M) ? true : false;
    round_test_parm.phy_s8 = (cfg_switch.PHY_S8) ? true : false;
    round_test_parm.phy_ble4 = false;
    // ... 其他參數
}
```

### 層次 4：運行時動態配置（外部螢幕界面）

**啟用條件：** `DTM_ADAPT_BRD=0`

**實現位置：** `src/ext_scr_svc.c`

#### 界面系統

透過 **UART 連接外部終端機**，支援 ANSI/VT100 控制碼：

**特性：**

* 支援游標控制、顏色、清屏等終端機控制
* 按鈕映射到鍵盤或虛擬按鈕
* 實時參數調整與顯示

#### 場景系統

```c
// 場景處理結構
struct scene_handler_st {
    void (*scr_procedure)(void);          // 螢幕顯示函數
    void (*btn_procedure)(int8_t);        // 按鈕處理函數
    int8_t (*trigger)(int8_t);            // 任務觸發函數
    const char *ext_scene_ctx;            // 場景內容
    uint8_t *ext_scene_idx;               // 場景索引
};

// 場景定義陣列
const static struct scene_handler_st scene_procedure[] = {
    // 場景 0: PARM_cfg
    { .scr_procedure = ext_scr_scene_parm,
      .btn_procedure = ext_scr_btn_hd_scene_parm,
      .ext_scene_ctx = ext_scene_parm,
      .ext_scene_idx = ext_scene_idx_parm },
    
    // 場景 1: PARM_2nd_cfg
    { .scr_procedure = ext_scr_scene_parm_2nd,
      .btn_procedure = ext_scr_btn_hd_scene_parm_2nd,
      .ext_scene_ctx = ext_scene_parm_2nd,
      .ext_scene_idx = ext_scene_idx_parm_2nd },
    
    // 場景 2: ENV_task
    { .scr_procedure = ext_scr_scene_env,
      .btn_procedure = ext_scr_btn_hd_scene_env,
      .trigger = envmon_task_tgr,
      .ext_scene_ctx = ext_scene_env,
      .ext_scene_idx = ext_scene_idx_env },
    
    // 場景 3: SND_task
    { .scr_procedure = ext_scr_scene_snd,
      .btn_procedure = ext_scr_btn_hd_scene_snd,
      .trigger = sender_task_tgr,
      .ext_scene_ctx = ext_scene_snd,
      .ext_scene_idx = ext_scene_idx_snd },
    
    // 場景 4: RCV_task
    { .scr_procedure = ext_scr_scene_rcv,
      .btn_procedure = ext_scr_btn_hd_scene_rcv,
      .trigger = scanner_task_tgr,
      .ext_scene_ctx = ext_scene_rcv,
      .ext_scene_idx = ext_scene_idx_rcv },
    
    // 場景 5: NUM_task
    { .scr_procedure = ext_scr_scene_num,
      .btn_procedure = ext_scr_btn_hd_scene_num,
      .trigger = numcst_task_tgr,
      .ext_scene_ctx = ext_scene_num,
      .ext_scene_idx = ext_scene_idx_num }
};
```

#### 資源與按鈕映射

```c
// 資源定義（可見項目）
const static char itemPARM_cfg[] = {"PARM_cfg"};
const static char itemENV_task[] = {"ENV_task"};
const static char itemSND_task[] = {"SND_task"};
const static char itemRCV_task[] = {"RCV_task"};
const static char itemNUM_task[] = {"NUM_task"};

// PHY 選項
const static char infoPHY1M2M[] = {"1M/2M"};
const static char infoPHY1M1M[] = {"1M/1M"};
const static char infoPHYS8S8[] = {"S8/S8"};
const static char infoPHYBLEv4[] = {"BLEv4"};

// 按鈕映射
RLST_REDIR_CSTR_HDL(RESOURCE_xST, itemPHY1M2M, infoPHY1M2M, 
                    RES_BTN_2, scr_resource, evt_hdl_cfg_phy2m);
RLST_REDIR_CSTR_HDL(RESOURCE_xST, itemPHY1M1M, infoPHY1M1M, 
                    RES_BTN_3, scr_resource, evt_hdl_cfg_phy1m);
RLST_REDIR_CSTR_HDL(RESOURCE_xST, itemPHYS8S8, infoPHYS8S8, 
                    RES_BTN_4, scr_resource, evt_hdl_cfg_phy8s);

// 其他配置按鈕
const static char itemInterval[] = {"Interval"};  // 按鈕 11
const static char itemPower[] = {"Power"};        // 按鈕 10
const static char itemTotal[] = {"Total"};        // 按鈕 9
const static char itemSOC_DCDC[] = {"SOC_DCDC"};  // 按鈕 11
const static char itemUNI_DIR[] = {"UNI_DIR"};    // 按鈕 10
const static char itemANONYMOUS[] = {"ANONYMOUS"}; // 按鈕 9
const static char itemCH37[] = {"CH37"};           // 按鈕 2
const static char itemCH38[] = {"CH38"};           // 按鈕 3
const static char itemCH39[] = {"CH39"};           // 按鈕 4
```

#### 可設定項目

**PARM_cfg 場景（主配置頁）：**

* 按鈕 2：PHY 1M/2M
* 按鈕 3：PHY 1M/1M
* 按鈕 4：PHY S8/S8
* 按鈕 7：PHY BLEv4
* 按鈕 9：Total 封包數（循環 7 種）
* 按鈕 10：Power 功率（+8/-40 dBm）
* 按鈕 11：Interval 廣告間隔（循環 11 種）
* 按鈕 12：切換場景

**PARM_2nd_cfg 場景（次配置頁）：**

* 按鈕 2：CH37 開/關
* 按鈕 3：CH38 開/關
* 按鈕 4：CH39 開/關
* 按鈕 9：ANONYMOUS 開/關
* 按鈕 10：UNI_DIR 單向/雙向
* 按鈕 11：SOC_DCDC DC-DC/LDO
* 按鈕 12：切換場景

#### 配置加載

```c
// src/main.c: load_parm_cfg()
void load_parm_cfg(void) {
    // 從外部螢幕界面讀取的配置
    round_test_parm.txpwr = enum_txpower(0);
    round_test_parm.count_idx = enum_totalnum_idx(0);
    round_test_parm.interval_idx = enum_adv_interval_idx(0);
    
    // 設定中止回調
    round_test_parm.envmon_abort = tst_envmon_abort;
    round_test_parm.sender_abort = tst_sender_abort;
    round_test_parm.scanner_abort = tst_scanner_abort;
    round_test_parm.numcast_abort = tst_numcast_abort;
    
    // PHY 選擇
    round_test_parm.phy_2m = get_cfg_phy_sel(0);
    round_test_parm.phy_1m = get_cfg_phy_sel(1);
    round_test_parm.phy_s8 = get_cfg_phy_sel(2);
    round_test_parm.phy_ble4 = get_cfg_phy_sel(3);
    
    // 頻道選擇（反邏輯：inhibit）
    round_test_parm.inhibit_ch37 = !get_cfg_ch37();
    round_test_parm.inhibit_ch38 = !get_cfg_ch38();
    round_test_parm.inhibit_ch39 = !get_cfg_ch39();
    
    // 其他選項
    round_test_parm.non_ANONYMOUS = get_cfg_NON_ANONYMOUS();
    round_test_parm.ignore_rcv_resp = get_uni_cast_method();
}
```

### 層次 5：代碼內部配置函數

**實現位置：** `src/losstst_svc.c`, `src/losstst_svc.h`

#### 配置讀取 API

```c
// 廣告間隔
extern uint8_t enum_adv_interval_idx(int8_t dir);
extern uint16_t adv_interval_lower(uint8_t idx);
extern uint16_t adv_interval_upper(uint8_t idx);

// 封包總數
extern uint8_t enum_totalnum_idx(int8_t dir);
extern uint16_t enum_totalnum(uint8_t idx);

// 發射功率
extern int8_t enum_txpower(int8_t dir);

// PHY 選擇
extern bool get_cfg_phy_sel(uint8_t idx);
// idx: 0=2M, 1=1M, 2=S8, 3=BLEv4

// 頻道選擇
extern bool get_cfg_ch37(void);
extern bool get_cfg_ch38(void);
extern bool get_cfg_ch39(void);

// 其他配置
extern bool get_cfg_NON_ANONYMOUS(void);
extern bool get_uni_cast_method(void);
extern bool get_cfg_soc_dcdc(void);

// 任務觸發與狀態
extern int8_t envmon_task_tgr(int8_t set);
extern int8_t sender_task_tgr(int8_t set);
extern int8_t scanner_task_tgr(int8_t set);
extern int8_t numcst_task_tgr(int8_t set);

extern int8_t envmon_task_status(void);
extern int8_t sender_task_status(void);
extern int8_t scanner_task_status(void);
extern int8_t numcst_task_status(void);
```

#### 內部配置變數

```c
// 配置狀態變數（losstst_svc.c）
static bool cfg_phy_sel[4] = {true, false, false, false};  // 預設 2M
static bool cfg_inhibit_ch37, cfg_inhibit_ch38, cfg_inhibit_ch39;
static bool cfg_non_ANONYMOUS;
static int8_t cfg_interval_sel_idx;
static int8_t cfg_totalnum_sel_idx;
static bool uni_cast_method;

// 當前輪次配置
static bool round_phy_sel[4] = {false, false, false, false};
static bool inhibit_ch37, inhibit_ch38, inhibit_ch39;
static bool non_ANONYMOUS;
static bool ignore_rcv_resp;
static uint16_t round_total_num;
static int8_t round_tx_pwr;
static uint8_t round_adv_param_index;
```


---

## 配置優先級

配置系統採用層次化優先級：

```
低優先級
    ↓
┌─────────────────────────────┐
│ 1. 編譯時 Kconfig (prj.conf)│  ← 基礎功能啟用
└─────────────────────────────┘
    ↓
┌─────────────────────────────┐
│ 2. Device Tree Overlay       │  ← 硬體資源定義
└─────────────────────────────┘
    ↓
┌─────────────────────────────┐
│ 3. 代碼內部預設值            │  ← 軟體邏輯預設
└─────────────────────────────┘
    ↓
┌─────────────────────────────┐
│ 4. DIP Switch / 外部螢幕     │  ← 使用者預設配置
└─────────────────────────────┘
    ↓
┌─────────────────────────────┐
│ 5. 運行時動態調整            │  ← 即時參數修改
└─────────────────────────────┘
    ↓
高優先級
```

**說明：**

* 下層配置會覆蓋上層配置
* 編譯時配置決定功能是否可用
* 運行時配置在可用範圍內調整


---

## 廣告封包 Payload 結構

本章詳細分析 LossTest_3rd_02s 系統在藍牙傳輸時使用的 Advertising Payload 結構，涵蓋資料格式、封裝方式、解析流程與實際範例。


### 核心資料結構

系統定義了三種主要的 payload 結構，位於 `src/losstst_svc.c`：

#### 1. DEVICE_INFO_ST（標準測試封包）

**定義位置：** `losstst_svc.c` 第 309 行

```c
typedef struct __attribute__((packed)) {
    uint16_t man_id;         // 製造商 ID (0xFFFF)
    uint16_t form_id;        // 表單 ID (0xBAAB)
    uint64_t eui64;          // EUI-64 設備識別碼 (大端序)
    uint8_t test_mode;       // 測試模式標識
    uint16_t pkt_idx;        // 封包序號
    uint8_t loss_rate;       // 丟包率
} DEVICE_INFO_ST;
```

**尺寸：** 16 bytes  
**用途：** PARM_cfg, PARM_2nd_cfg, ENV_task, SND_task, RCV_task  
**特性：** 
- 使用 `__attribute__((packed))` 確保無填充字節
- EUI-64 採用 **大端序（Big-Endian）** 儲存
- 封包序號支援 0-65535

#### 2. DEVICE_INFO_BTv4_ST（BLE 4.x 相容封包）

**定義位置：** `losstst_svc.c` 第 329 行

```c
typedef struct __attribute__((packed)) {
    uint16_t man_id;         // 製造商 ID (0xFFFF)
    uint16_t form_id;        // 表單 ID (0xBAAB)
    uint64_t eui64;          // EUI-64 設備識別碼
    uint8_t test_mode;       // 測試模式
    uint16_t pkt_idx;        // 封包序號
    uint8_t loss_rate;       // 丟包率
    uint8_t padding[10];     // 填充位元組（BLE 4.x 最大 31 byte）
} DEVICE_INFO_BTv4_ST;
```

**尺寸：** 26 bytes  
**用途：** 當測試情境使用 BLE 4.x Legacy PHY (1M) 時  
**特性：**
- 額外 10 bytes padding 確保符合 BLE 4.x 的 31 byte Ad Data 限制
- 與 Extended Advertising 相容但預留空間
- 當 `prim_phy=BT_GAP_LE_PHY_1M` 且 `sec_phy=0` 時使用

#### 3. NUMCAST_INFO_ST（連號測試封包）

**定義位置：** `losstst_svc.c` 第 339 行

```c
typedef struct __attribute__((packed)) {
    uint16_t man_id;         // 0xFFFF
    uint16_t form_id;        // 0xBAAB
    uint16_t numcast_tx;     // 傳送計數器
    uint16_t numcast_rx;     // 接收計數器
    uint16_t numcast_err;    // 錯誤計數器
} NUMCAST_INFO_ST;
```

**尺寸：** 10 bytes  
**用途：** NUM_task（連號檢測場景）  
**特性：**
- 超輕量級結構，適合高頻傳輸
- 三個計數器追蹤 TX/RX/Error 狀態
- 不包含 EUI-64（減少 overhead）


---

### Advertising Data 封裝方式

系統使用 Zephyr Bluetooth Stack 的 `bt_data` 結構封裝 Payload，位於 `update_adv()` 函數（`losstst_svc.c` 第 1060 行）：

#### 標準 Extended Advertising 封包結構

```c
static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, &dev_info, sizeof(dev_info)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME)-1),
};
```

**AD Type 組成：**

| AD Type                     | 長度      | 內容描述                          |
|-----------------------------|-----------|-----------------------------------|
| `BT_DATA_FLAGS`             | 1 byte    | `0x06` (General + BR/EDR 不支援)  |
| `BT_DATA_MANUFACTURER_DATA` | 變動      | 16/26/10 bytes（依結構而定）      |
| `BT_DATA_NAME_COMPLETE`     | ~10 bytes | 設備名稱（如 "LossTest"）         |

#### 總封包尺寸計算

**Extended Advertising (2M/Coded S8):**
```
總長度 = 1 (Length) + 1 (Type) + 1 (Flags Value)
       + 1 (Length) + 1 (Type) + 16 (DEVICE_INFO_ST)
       + 1 (Length) + 1 (Type) + ~10 (Device Name)
       = 約 33 bytes
```

**Legacy Advertising (1M):**
```
總長度 = 3 (Flags) + 28 (BTv4 結構 + header) = 31 bytes (BLE 4.x 最大值)
```


---

### 測試情境與 Payload 對應

不同測試情境使用不同的 Payload 結構：

| 測試情境         | 使用結構                 | PHY 類型                | Payload 尺寸 |
|------------------|--------------------------|-------------------------|--------------|
| **PARM_cfg**     | `DEVICE_INFO_ST`         | 2M / Coded S8          | 16 bytes     |
| **PARM_2nd_cfg** | `DEVICE_INFO_ST`         | 2M / Coded S8          | 16 bytes     |
| **ENV_task**     | `DEVICE_INFO_ST`         | 2M / Coded S8          | 16 bytes     |
| **SND_task**     | `DEVICE_INFO_ST`         | 2M / Coded S8          | 16 bytes     |
| **RCV_task**     | `DEVICE_INFO_ST`         | 2M / Coded S8          | 16 bytes     |
| **SND_legacy**   | `DEVICE_INFO_BTv4_ST`    | 1M (Legacy)            | 26 bytes     |
| **NUM_task**     | `NUMCAST_INFO_ST`        | 2M / Coded S8          | 10 bytes     |

**PHY 判斷邏輯：** 位於 `update_adv()` 函數

```c
if(BT_GAP_LE_PHY_1M == parm->prim_phy && 0 == parm->sec_phy) {
    // 使用 DEVICE_INFO_BTv4_ST (26 bytes)
} else {
    // 使用 DEVICE_INFO_ST (16 bytes) 或 NUMCAST_INFO_ST (10 bytes)
}
```


---

### Parser 處理流程

接收端使用兩個主要 parser 函數處理不同類型的 Advertising Data：

#### 1. ratio_parser()（標準封包解析）

**位置：** `losstst_svc.c` 第 2215 行

```c
static bool ratio_parser(struct bt_data *data, void *user_data) {
    struct adv_info_t *adv_info_p = user_data;
    dev_chr_t *dev_chr_p = &adv_info_p->dev_chr;
    
    if(BT_DATA_FLAGS == data->type) {
        dev_chr_p->step_flag++;
    } 
    else if(BT_DATA_MANUFACTURER_DATA == data->type) {
        DEVICE_INFO_ST *rcv_data_p = (DEVICE_INFO_ST *)data->data;
        
        // 驗證製造商 ID 和表單 ID
        if(MANUFACTURER_ID == rcv_data_p->man_id && 
           LOSS_TEST_FORM_ID == rcv_data_p->form_id) {
            
            // 處理封包（記錄序號、計算丟包率）
            tst_form_packet_rcv(adv_info_p, rcv_data_p);
            dev_chr_p->step_success = 1;
        }
    }
    return true;
}
```

**處理步驟：**
1. 檢測 `BT_DATA_FLAGS` → 驗證 Ad Data 有效性
2. 檢測 `BT_DATA_MANUFACTURER_DATA` → 提取 payload
3. 驗證 `man_id=0xFFFF` 且 `form_id=0xBAAB`
4. 呼叫 `tst_form_packet_rcv()` 記錄封包序號與丟包統計

#### 2. numcast_parser()（連號封包解析）

**位置：** `losstst_svc.c` 第 2237 行

```c
static bool numcast_parser(struct bt_data *data, void *user_data) {
    struct adv_info_t *adv_info_p = user_data;
    dev_chr_t *dev_chr_p = &adv_info_p->dev_chr;
    
    if(BT_DATA_FLAGS == data->type) {
        dev_chr_p->step_flag++;
    } 
    else if(BT_DATA_MANUFACTURER_DATA == data->type) {
        NUMCAST_INFO_ST *rcv_data_p = (NUMCAST_INFO_ST *)data->data;
        
        if(MANUFACTURER_ID == rcv_data_p->man_id && 
           LOSS_TEST_FORM_ID == rcv_data_p->form_id) {
            
            // 處理連號計數器
            numcast_form_packet_rcv(adv_info_p, rcv_data_p);
            dev_chr_p->step_success = 1;
        }
    }
    return true;
}
```

#### PHY 類型檢測

Parser 從 `bt_le_scan_recv_info` 取得 PHY 資訊：

```c
int8_t phy_class_idx = PHY_class_trans(info->primary_phy, info->secondary_phy);
```

**映射表：**

| Primary PHY | Secondary PHY | `phy_class_idx` | 說明       |
|-------------|---------------|-----------------|------------|
| 1M          | 0 (無)        | 0               | BLE 4.x    |
| 2M          | 2M            | 1               | Extended   |
| 1M          | Coded S8      | 2               | Long Range |
| 1M          | 2M            | 1               | Extended   |


---

### Hex 範例與分析

#### 範例 1：標準 Extended Advertising (2M PHY)

**Raw Hex Dump:**
```
02 01 06 13 FF FF FF AB BA 01 23 45 67 89 AB CD EF 
05 00 10 00 0A 09 4C 6F 73 73 54 65 73 74
```

**解析：**

| Bytes             | 描述                                  | 數值                           |
|-------------------|---------------------------------------|--------------------------------|
| `02 01 06`        | AD Type: Flags (General, No BR/EDR)   | -                              |
| `13 FF`           | Length=19, AD Type=Manufacturer Data  | -                              |
| `FF FF`           | 製造商 ID                             | `0xFFFF`                       |
| `AB BA`           | 表單 ID                               | `0xBAAB` (小端序)              |
| `01...EF`         | EUI-64                                | `0x0123456789ABCDEF` (大端序)  |
| `05`              | 測試模式                              | `5` (PARM_cfg)                 |
| `00 10`           | 封包序號                              | `0x1000` = 4096                |
| `00`              | 丟包率                                | `0%`                           |
| `0A 09 ...`       | Device Name = "LossTest"              | -                              |

#### 範例 2：Legacy Advertising (1M PHY)

**Raw Hex Dump:**
```
02 01 06 1D FF FF FF AB BA 01 23 45 67 89 AB CD EF 
05 00 50 02 00 00 00 00 00 00 00 00 00 00
```

**解析差異：**

| 欄位               | Extended (16B)      | Legacy (26B)                    |
|--------------------|---------------------|---------------------------------|
| 核心資料           | 16 bytes            | 16 bytes (相同)                 |
| Padding            | 無                  | 10 bytes `0x00` 填充            |
| 總 Manufacturer Data | 19 bytes (含 header) | 29 bytes (含 header)            |
| 封包序號範例       | `0x0010` = 16       | `0x0250` = 592                  |

#### 範例 3：連號測試封包（NUM_task）

**Raw Hex Dump:**
```
02 01 06 0D FF FF FF AB BA 01 F4 00 C8 00 05
```

**解析：**

| Bytes       | 描述            | 數值                    |
|-------------|-----------------|-------------------------|
| `02 01 06`  | Flags           | -                       |
| `0D FF`     | Length=13, MFG  | -                       |
| `FF FF`     | 製造商 ID       | `0xFFFF`                |
| `AB BA`     | 表單 ID         | `0xBAAB`                |
| `01 F4`     | TX 計數         | `0xF401` = 500 (小端序) |
| `00 C8`     | RX 計數         | `0xC800` = 200 (小端序) |
| `00 05`     | Error 計數      | `0x0500` = 5 (小端序)   |


---

### 傳輸效能特性

不同 PHY 與 Payload 組合的空中傳輸時間（不含 IFS）：

| PHY 類型    | Payload 尺寸 | Preamble | Access Address | PDU Header | 傳輸時間估算 |
|-------------|--------------|----------|----------------|------------|--------------|
| **1M**      | 26 bytes     | 1 byte   | 4 bytes        | 2 bytes    | ~216 μs      |
| **2M**      | 16 bytes     | 2 bytes  | 4 bytes        | 2 bytes    | ~108 μs      |
| **Coded S8**| 16 bytes     | 10 bytes | 32 bytes       | 16 bytes   | ~864 μs      |

**關鍵觀察：**

1. **2M PHY 效能最佳：** 16 byte payload 僅需 108 μs，適合高頻測試
2. **Coded S8 距離優勢：** 雖然傳輸時間長 8 倍，但距離可達 1M PHY 的 4 倍
3. **Legacy 浪費頻寬：** 26 byte payload 中有 10 bytes 無效填充


---

### 狀態訊息格式

傳送端使用 `sender_peek_msg()` 生成狀態訊息（`losstst_svc.c` 第 1225 行）：

```c
sprintf(buf, "SND:%03u P:%s%s R:%u/%u T:%d",
        par_tx_cnt,                         // 已傳送封包數
        phy_str_ary[phy_idx],               // PHY 類型 ("1M"/"2M"/"S8")
        ch_cnt_str,                         // 頻道數量字串
        par_adv_int,                        // Advertising Interval
        par_pkt_cnt,                        // 總封包數
        par_tx_pwr                          // TX Power (dBm)
);
```

**範例輸出：**
```
SND:042 P:2M(37) R:100/500 T:+4
```

**解讀：**
- 已傳送 42 個封包
- 使用 2M PHY，37 個頻道
- Interval 100ms，總共 500 個封包
- 發射功率 +4 dBm


---

### 關鍵常數定義

```c
#define MANUFACTURER_ID       0xFFFF    // 測試用製造商 ID
#define LOSS_TEST_FORM_ID     0xBAAB    // 封包識別 Magic Number
#define MAX_PACKET_COUNT      50000     // 最大封包數
#define MIN_ADV_INTERVAL      30        // 最小 Advertising Interval (ms)
#define MAX_ADV_INTERVAL      3600      // 最大 Advertising Interval (ms)
```


---

### 應用場景建議

根據不同需求選擇適當的 Payload 結構：

| 應用場景           | 推薦結構              | PHY 選擇   | 理由                           |
|--------------------|-----------------------|------------|--------------------------------|
| 高速短距測試       | `DEVICE_INFO_ST`      | 2M         | 最低延遲，最高吞吐量           |
| 長距離測試         | `DEVICE_INFO_ST`      | Coded S8   | 編碼增益，距離 4 倍提升        |
| 相容性測試         | `DEVICE_INFO_BTv4_ST` | 1M         | 兼容舊裝置，標準 31 byte 限制  |
| 連號檢測           | `NUMCAST_INFO_ST`     | 2M         | 最小 overhead，快速錯誤檢測    |
| 環境干擾評估       | `DEVICE_INFO_ST`      | Coded S8   | 抗干擾能力最強                 |


---

## 建置方法

### 方法 1：VS Code nRF Connect 擴充套件

#### 步驟 1：建立建置配置


1. 開啟 VS Code
2. 點擊左側 nRF Connect 圖示
3. 選擇 "Create a new build configuration"
4. 選擇板型：`nrf52833dk_nrf52833`

#### 步驟 2：設定額外參數

在 "Extra CMake arguments" 欄位輸入：

**範例 1：無 DIP Switch 配置**

```
-DOVERLAY_CONFIG=supplement_52833dk_woDIPSW_ncs280.conf
```

**範例 2：RS485 採集板配置**

```
-DOVERLAY_CONFIG=supplement_FFFFar52833_RS485ACQ_woFEM.conf -DDTC_OVERLAY_FILE=nrf52833dk_RS485ACQ_woFEM.overlay
```

**範例 3：多個 Overlay 組合**

```
-DOVERLAY_CONFIG="supplement_FFFFar52833_IICsensorBrd_FEM.conf;nrf52x_soc_ldo.overlay" -DDTC_OVERLAY_FILE=FFFFar_nrf52833_FEMv300.overlay
```

#### 步驟 3：建置

點擊 "Build" 按鈕

### 方法 2：命令列建置

#### 基本語法

```bash
cd /opt/nordic/ncs/v2.8.0/nrf/samples/bluetooth/LossTest_3rd_02s

west build -b nrf52833dk_nrf52833 \
  -- \
  -DOVERLAY_CONFIG=<supplement_config_file> \
  -DDTC_OVERLAY_FILE=<overlay_file>
```

#### 範例命令

**範例 1：標準 DK 建置**

```bash
west build -b nrf52833dk_nrf52833 -p \
  -- \
  -DOVERLAY_CONFIG=supplement_52833dk_woDIPSW_ncs280.conf
```

**範例 2：RS485 板建置**

```bash
west build -b nrf52833dk_nrf52833 -p \
  -- \
  -DOVERLAY_CONFIG=supplement_FFFFar52833_RS485ACQ_woFEM.conf \
  -DDTC_OVERLAY_FILE=nrf52833dk_RS485ACQ_woFEM.overlay
```

**範例 3：帶 FEM 的 I²C 感測器板**

```bash
west build -b nrf52833dk_nrf52833 -p \
  -- \
  -DOVERLAY_CONFIG=supplement_FFFFar52833_IICsensorBrd_FEM.conf \
  -DDTC_OVERLAY_FILE=FFFFar_nrf52833_FEMv300.overlay
```

#### 參數說明

| 參數 | 說明 |
|----|----|
| `-b` | 指定板型（target board） |
| `-p` | Pristine build（清除舊建置） |
| `--` | 之後的參數傳給 CMake |
| `-DOVERLAY_CONFIG` | 補充 Kconfig 文件 |
| `-DDTC_OVERLAY_FILE` | Device Tree Overlay 文件 |

### 方法 3：燒錄至裝置

#### VS Code

點擊 "Flash" 按鈕

#### 命令列

```bash
west flash
```

#### 使用 J-Link Commander

```bash
nrfjprog --program build/zephyr/zephyr.hex --chiperase --verify --reset
```


---

## 常用配置組合

### 組合 1：開發測試（外部螢幕）

**用途：** 開發階段，需要靈活調整參數

**配置：**

```bash
-DOVERLAY_CONFIG=supplement_52833dk_woDIPSW_ncs310.conf
```

**特點：**

* `DTM_ADAPT_BRD=0`（無 DIP Switch）
* UART 控制台
* 外部終端機界面實時調整

### 組合 2：現場快速測試（DIP Switch）

**用途：** 現場測試，需快速切換配置

**配置：**

```bash
-DOVERLAY_CONFIG=supplement_52833dk_NuM482_FEM.conf \
-DDTC_OVERLAY_FILE=nrf52833dk_NuM482_FEM.overlay
```

**特點：**

* `DTM_ADAPT_BRD=1`（啟用 DIP Switch）
* SPI 讀取硬體開關
* 預設參數組合快速切換

### 組合 3：長距離測試（FEM）

**用途：** 大功率長距離測試

**配置：**

```bash
-DOVERLAY_CONFIG=supplement_FFFFar52833_IICsensorBrd_FEM.conf \
-DDTC_OVERLAY_FILE=FFFFar_nrf52833_FEMv300.overlay
```

**特點：**

* 啟用 FEM（前端模組）
* 增益可達 +20 dBm 以上
* 動態功率模型

### 組合 4：低功耗測試

**用途：** 電池供電應用測試

**配置：**

```bash
-DOVERLAY_CONFIG=supplement_FFFFar52833_RS485ACQ_woFEM.conf \
-DDTC_OVERLAY_FILE="nrf52833dk_RS485ACQ_woFEM.overlay;nrf52x_soc_ldo.overlay"
```

**特點：**

* 無 FEM
* RTT 控制台（低功耗）
* DC-DC 轉換器


---

## 常見問題解答

### Q1: 為什麼在 CMake Extra Arguments 加了 `-- -DDTM_ADAPT_BRD=0` 還是編譯錯誤？

**錯誤訊息：**

```
error: 'DTM_ADAPT_BRD' undeclared (first use in this function)
```

**原因：** 當您使用 `-- -DDTM_ADAPT_BRD=0` 時，這個參數是傳遞給**構建工具**（如 ninja），而不是定義 C 預處理器宏。

**正確解決方法：**

在 `prj.conf` 或補充配置文件中添加：

```properties
CONFIG_COMPILER_OPT="-DDTM_ADAPT_BRD=0"
```

或包含完整的編譯選項：

```properties
CONFIG_COMPILER_OPT="-DDTM_ADAPT_BRD=0 -DSHORT_NM -DUART_XMT_RCV_METHOD=HAZARD_UART_XMT_RCV -DUART_SEND_BREAK_METHOD=HAZARD_UART_BRK"
```

**為什麼這樣才有效：** 這是 Zephyr/Nordic SDK 的標準做法，`CONFIG_COMPILER_OPT` 會正確地將定義傳遞給 C 編譯器。

**參考文件：**

* 查看 `supplement_52833dk_woDIPSW_ncs280.conf` 等補充配置文件中的示例


---

## 附錄

### A. 文件結構總覽

```
LossTest_3rd_02s/
├── prj.conf                    # 主配置文件
├── CMakeLists.txt              # 建置配置
├── README.rst                  # 專案說明
├── sample.yaml                 # 範例配置
│
├── supplement_*.conf (14個)    # 補充配置
│   ├── supplement_52833dk_woDIPSW_ncs280.conf
│   ├── supplement_52833dk_NuM482_FEM.conf
│   ├── supplement_FFFFar52833_IICsensorBrd_FEM.conf
│   ├── supplement_FFFFar52833_RS485ACQ_woFEM.conf
│   └── ...
│
├── *.overlay (12個)            # Device Tree Overlay
│   ├── nrf52833dk_nrf52833.overlay
│   ├── nrf52833dk_RS485ACQ_woFEM.overlay
│   ├── FFFFar_nrf52833_FEMv300.overlay
│   └── ...
│
├── src/                        # 原始碼
│   ├── main.c                  # 主程式
│   ├── losstst_svc.c           # 測試服務
│   ├── losstst_svc.h
│   ├── ext_scr_svc.c           # 外部螢幕服務
│   ├── ext_scr_csi.h           # CSI 控制碼
│   ├── int_spec_uart_svc.c     # UART 服務
│   ├── int_spec_uart_svc.h
│   ├── nrf52txpwr.c            # 功率控制
│   ├── nrf52txpwr.h
│   └── ui_resource_code.h      # UI 資源
│
├── child_image/                # 子映像配置
│   └── hci_rpmsg.conf
│
└── build/                      # 建置輸出目錄
```

### B. 關鍵符號定義

#### DTM_ADAPT_BRD

```c
#if(DTM_ADAPT_BRD)
    // 啟用 DIP Switch 模式
    // 透過 SPI 讀取硬體開關
#else
    // 啟用外部螢幕模式
    // 透過 UART 終端機配置
#endif
```

#### UART 方法

```c
// UART_XMT_RCV_METHOD 定義
#define HAZARD_UART_XMT_RCV     1  // 危險模式（直接操作暫存器）
#define HAL_GPIO_UART_XMT_RCV   2  // HAL GPIO 模式
#define STD_GPIO_UART_XMT_RCV   3  // 標準 GPIO 模式

// UART_SEND_BREAK_METHOD 定義
#define HAZARD_UART_BRK         1  // 危險模式
```

#### 其他定義

```c
#define SHORT_NM                   // 短裝置名稱
#define RS485ACQ_BRD        1      // RS485 採集板
```

### C. 參考資源

**官方文檔：**

* [Nordic nRF Connect SDK 文檔](https://docs.nordicsemi.com/)
* [Zephyr RTOS 文檔](https://docs.zephyrproject.org/)
* [Bluetooth Core Specification 5.4](https://www.bluetooth.com/specifications/specs/)

**相關專案文件：**

* `README.rst` - 專案說明
* `sample.yaml` - 範例配置
* 各 `supplement_*.conf` - 配置範例

**開發工具：**

* VS Code with nRF Connect Extension
* nRF Command Line Tools
* J-Link Software


---

## 結論

LossTest_3rd_02s 是一個功能完整、高度可配置的藍牙丟包測試系統。其多層次的配置架構提供了從編譯時到運行時的完整靈活性，適用於：





✅ **研發測試** - 外部螢幕界面靈活調整✅ **現場測試** - DIP Switch 快速切換✅ **生產驗證** - 固定配置自動化測試✅ **長距離測試** - FEM 支援大功率輸出✅ **多場景評估** - 6 大場景 × 數萬種組合

透過本文檔，您應該能夠：


1. 理解專案的整體架構
2. 解決常見的編譯配置問題
3. 選擇適合的配置組合
4. 客製化測試情境
5. 成功建置並部署到目標硬體


---



**文檔版本：** 1.0**最後更新：** 2026-02-13**維護者：** GitHub Copilot (Claude Sonnet 4.5)