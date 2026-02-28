# BLE 封包遺失測試報告 — Silicon Labs Sender × Nordic Scanner（BLE4 Legacy PHY）


**日期：** 2026-02-28**測試目的：** 端對端驗證 Silicon Labs EFR32（Legacy Advertising, 1M primary / no secondary）發送，Nordic nRF52833 正確接收


## 測試環境

| 角色 | 裝置 | 韌體 / SDK |
|----|----|----|
| **Sender** | Silicon Labs EFR32 (node 173) | LossTest SL port / Simplicity SDK |
| **Scanner** | Nordic nRF52833 (node 158) | LossTest_3rd_02s / NCS v2.8.0 |


## 測試配置

### Silicon Labs Sender

| 參數 | 值 |
|----|----|
| PHY | BLE4 Legacy（1M primary / no secondary） |
| PDU 類型 | Legacy Advertising PDU |
| Packet struct | `DEVICE_INFO_ST` (16 bytes Manufacturer Specific) |
| `man_id` / `form_id` | `0xFFFF` / `0xBAAB` |
| Burst count | 250 events/burst × 2 rounds = **500 total** |
| TX Power | 0.0 dBm (actual) |
| TX Power in PDU | **N/A**（Legacy ADV 不帶 TX power 欄位） |
| ADV interval | 60 ms |
| `pre_cnt` scheme | per-event decrement（250 → 1 per burst） |
| Preamble | −3 → −2 → −1（各 1 s） |
| Burst-end marker | `pre_cnt = 0` |
| Complete marker | `pre_cnt = 32767` |

### Nordic Scanner

| 參數 | 值 |
|----|----|
| PHY 監聽 | BLE4 Legacy (idx=3)，prim_phy=1 sec_phy=0 |
| scan method | 1（`passive_scan_phy_1m`，Legacy ADV 使用 1M primary） |
| Dedup 機制 | 已加入（防護層，本次未觸發） |
| LossTest firmware | LossTest_3rd_02s rev. 2026-02-28 |


## 與其他 PHY 測試的差異

| 項目 | 1M/2M | 1M/1M | S8/S8 | **BLE4（本報告）** |
|----|----|----|----|----|
| SL sender idx | 0 | 1 | 2 | **3** |
| PDU 類型 | Extended | Extended | Extended | **Legacy** |
| Primary / Secondary | 1M / 2M | 1M / 1M | Coded / Coded | **1M / NA** |
| Nordic scanner idx | 0 | 1 | 2 | **3** |
| TX Power in PDU | T:0 | T:8 | T:0 | **T:（空白）** |
| `remote_ctrl_parser` | 不執行 | 執行 | 不執行 | 不執行 |

> Legacy ADV HCI event 不含 TX power 欄位，Nordic firmware 讀值為 127（unknown），顯示為空白 `T:`。為預期行為，非錯誤。


## 接收結果（Nordic Scanner 端）

### Flow 結果

| Flow | PHY | 收到 | 總數 | 遺失 | 接收率 | RSSI (avg/min/max) | TX Power (PDU) |
|----|----|----|----|----|----|----|----|
| 1 | 1M/NA | 244 | 250 | 6 | **97.6%** | -26 / -28 / -25 dBm | N/A |
| 2 | 1M/NA | 248 | 250 | 2 | **99.2%** | -26 / -28 / -25 dBm | N/A |
| **累計** |    | **492** | **500** | **8** | **98.4%** |    |    |

> RSSI 範圍 -28～-25 dBm，為四組測試中最窄，訊號極穩定。原始 log 見[附錄 B](#%E9%99%84%E9%8C%84-b-nordic-scanner-%E5%8E%9F%E5%A7%8B-log)。


---

## 對比：四種 PHY 接收率（SL Sender × Nordic Scanner）

| PHY | Flow 1 | Flow 2 | 累計 | RSSI avg |
|----|----|----|----|----|
| 1M/2M | 248/250 (99.2%) | 245/250 (98.0%) | 493/500 (**98.6%**) | -27 dBm |
| 1M/1M | 246/250 (98.4%) | 242/250 (96.8%) | 488/500 (**97.6%**) | -19 dBm |
| S8/S8 | 241/250 (96.4%) | 231/250 (92.8%) | 472/500 (**94.4%**) | -30 dBm |
| **BLE4** | **244/250 (97.6%)** | **248/250 (99.2%)** | **492/500 (98.4%)** | **-26 dBm** |

> \
> BLE4 接收率（98.4%）與 1M/2M（98.6%）相當，且是四組中 RSSI 最穩定的測試。各組測試距離條件不同，RSSI avg 無法直接比較。


## 驗證結果

| 驗證項目 | 結果 |
|----|----|
| SL sender PDU 類型 | ✅ Legacy Advertising PDU |
| SL sender `pre_cnt` per-event decrement | ✅ 250 → 1，每事件遞減一次 |
| Nordic receiver 正確辨識 idx=3（BLE4） | ✅ `[BLE4] first pkt prim=1 sec=0` |
| `P:1M/NA` PHY display 正確 | ✅ |
| `T:` TX power 空白（Legacy 不帶 txpwr） | ✅ 預期行為 |
| `[REORDER]` 事件未出現 | ✅ |
| `[DUP]` dedup 未觸發 | ✅ |
| Flow 1 接收率 ≥ 95% | ✅ 97.6% |
| Flow 2 接收率 ≥ 95% | ✅ 99.2% |
| 累計接收率 ≥ 95% | ✅ 98.4% |


## 關鍵問題記錄：Legacy ADV data API 路徑錯誤

| 問題 | 症狀 | 根本原因 | 修正方式 |
|----|----|----|----|
| Legacy data API | BLE4 slot 呼叫 `sl_bt_extended_advertiser_set_data` | `platform_set_adv_data` 對所有 slot 走同一條路徑 | 偵測 `stored_adv_params[adv_index].options` 是否含 `BT_LE_ADV_OPT_EXT_ADV`；legacy slot 改呼叫 `sl_bt_legacy_advertiser_set_data` |


---

## Sender PHY 驗證

| 驗證點 | 預期 | 觀測 |
|----|----|----|
| Adv data API | legacy path | `legacy_set_data: len=31` ✓ |
| Adv start API | legacy, non-connectable | `legacy_start: mode=0` ✓ |
| 無 PHY 設定呼叫 | legacy 不設 PHY | 無 `Setting PHY` / `Updating PHY` ✓ |
| 無 `ext_start` | legacy 不走 extended path | 無 `ext_start:` ✓ |

> \
> Legacy ADV payload = 31 bytes（BLE 4.x 最大值），確認 data path 正確。BLE4 slot 不呼叫 `sl_bt_extended_advertiser_set_phy`，`Setting PHY` / `Updating PHY` 缺席為預期行為。


---

## Sender 測試流程（Silicon Labs）

| 階段 | 說明 | 結果 |
|----|----|----|
| Init | TX power set for 4 adv sets | ✓ |
| Countdown | −3 → −2 → −1（3 × 1 s pre-burst） | ✓ |
| Burst Round 1 | 250 events, pre_cnt 250 → 1 | ✓ |
| Post-burst 1 | pre_cnt = 0，counters updated | ✓ |
| Burst Round 2 | 250 events, pre_cnt 250 → 1 | ✓ |
| Post-burst 2 | pre_cnt = 0，counters updated | ✓ |
| Complete | `Complete` flag set，task stopped | ✓ |
| Finit | re-advertise with `INT16_MAX` | ✓ |

### Transmission Counters

| Round | Before | After | Delta |
|----|----|----|----|
| Round 1 | BLE4: 0 | BLE4: 250 | +250 |
| Round 2 | BLE4: 250 | BLE4: 500 | +250 |
| **Total** |    | **500 / 500** | ✅ |


---

## 備註

* SL sender 已採用 per-event decrement，詳見 [PRE_CNT_PER_EVENT_FIX.md](PRE_CNT_PER_EVENT_FIX.md)。
* Nordic scanner dedup 防護層在本次測試中**未被觸發**，保留為防禦性程式碼。
* `adv_sent idx=3` 確認 `get_adv_index_by_handle()` 正確將 hw handle 映射至 logical index 3（BLE4 slot）。
* BLE4 Legacy ADV 僅使用 1M primary channel，無 secondary channel（`sec_phy=0`），Nordic 顯示 `P:1M/NA`。
* SL sender BLE4 slot 使用非匿名模式（帶 Advertiser Address）；Nordic scanner 以 `prim_phy=1 sec_phy=0` 辨識 idx=3，不依賴 address。
* Round 1 後出現 `[ADV 1] Setting PHY: pri=1(1M) sec=2(2M)` 為 1M slot lazy init，該 slot 未使用，不影響測試結果。


---

## 附錄 A：Silicon Labs Sender 原始 Log（condensed）

### Round 1

```
=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 15000 ms
PHY[3]: Starting burst with 250 events (1-event chain)
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=249
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=200
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=150
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=100
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=50
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] PHY[3] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[3] BLE4: 0 -> 250 (+250)
SND:173 P:BLEv4 R:250/500 T:0
```

### Round 2

```
=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 15000 ms
PHY[3]: Starting burst with 250 events (1-event chain)
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=249
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=200
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=150
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=100
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] adv_sent idx=3 sndr_abort=0 pre_cnt=50
  [ADV 0] legacy_set_data: len=31
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
  [ADV 0] legacy_start: mode=0
[SND] PHY[3] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[3] BLE4: 250 -> 500 (+250)
SND:173 P:BLEv4 R:500/500 T:0
SND:173 P:BLEv4 Complete
```

### Finit

```
[SND] adv_sent idx=3 sndr_abort=1 pre_cnt=32767   ← abort handler, re-advertise 5 s
```


---

## 附錄 B：Nordic Scanner 原始 Log

```
Packet Loss Test (node 158) **** RCV SIDE ****
SENDER:173 P:1M/NA R:0/250 S:-30(..) T:
RCV:173 P:1M/NA R:244/250 S:-26(-28..-25) T:
RCV:173 P:1M/NA R:492/500 S:-26(-28..-25) T:
```


