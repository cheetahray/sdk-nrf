# BLE 封包遺失測試報告 — Silicon Labs Sender × Nordic Scanner（1M/1M PHY）


**日期：** 2026-02-28**測試目的：** 端對端驗證 Silicon Labs EFR32（Extended Advertising, 1M primary / 1M secondary）發送，Nordic nRF52833 正確接收


## 測試環境

| 角色 | 裝置 | 韌體 / SDK |
|----|----|----|
| **Sender** | Silicon Labs EFR32 (node 173) | LossTest SL port / Simplicity SDK |
| **Scanner** | Nordic nRF52833 (node 158) | LossTest_3rd_02s / NCS v2.8.0 |


## 測試配置

### Silicon Labs Sender

| 參數 | 值 |
|----|----|
| PHY | 1M primary / 1M secondary (Extended ADV) |
| Packet struct | `DEVICE_INFO_ST` (16 bytes Manufacturer Specific) |
| `man_id` / `form_id` | `0xFFFF` / `0xBAAB` |
| Burst count | 250 events/burst × 2 rounds = **500 total** |
| ADV interval | 60 ms |
| TX Power | 0.0 dBm (actual) |
| `pre_cnt` scheme | per-event decrement（250 → 1 per burst） |
| Preamble | −3 → −2 → −1（各 1 s） |
| Burst-end marker | `pre_cnt = 0` |
| Complete marker | `pre_cnt = 32767`（5 s 後停止） |

### Nordic Scanner

| 參數 | 值 |
|----|----|
| PHY 監聽 | 1M/1M (idx=1) |
| Dedup 機制 | 已加入（防護層，本次未觸發） |
| LossTest firmware | LossTest_3rd_02s rev. 2026-02-28 |


## 與 1M/2M 測試的差異

| 項目 | 1M/2M（TEST_REPORT_SL_SENDER.md） | 1M/1M（本報告） |
|----|----|----|
| SL sender idx | 0 | 1 |
| Secondary PHY | 2M | **1M** |
| Nordic scanner idx | 0 | **1** |
| Nordic `remote_ctrl_parser` | 不執行 | 執行（idx=1 特有） |
| `[RC_HIT]` 事件 | 不適用 | 未出現 ✅ |

> \
> Nordic scanner 在 `idx=1` 收包後，同一封包也會過 `remote_ctrl_parser` 檢查。本次測試中 `[RC_HIT]` **未出現**，確認 SL sender 封包均被 `test_form_parser` 正確吃走，未誤入 remote_ctrl 路徑。


## 接收結果（Nordic Scanner 端）

### Flow 結果

| Flow | PHY | 收到 | 總數 | 遺失 | 接收率 | RSSI (avg/min/max) | TX Power (PDU) |
|----|----|----|----|----|----|----|----|
| 1 | 1M/1M | 246 | 250 | 4 | **98.4%** | -19 / -22 / -18 dBm | T:8 |
| 2 | 1M/1M | 242 | 250 | 8 | **96.8%** | -19 / -21 / -18 dBm | T:8 |
| **累計** |    | **488** | **500** | **12** | **97.6%** |    |    |

> `T:8` 為 SL sender PDU 內 TX power 欄位讀值；SL sender actual TX power = 0.0 dBm（不同資料來源）。原始 log 見\[附錄 B\](#附錄-b nordic-scanner-原始-log)。


---

## 對比：三種 PHY 接收率（SL Sender × Nordic Scanner）

| PHY | Flow 1 | Flow 2 | 累計 | RSSI avg |
|----|----|----|----|----|
| 1M/2M | 248/250 (99.2%) | 245/250 (98.0%) | 493/500 (**98.6%**) | -27 dBm |
| **1M/1M** | **246/250 (98.4%)** | **242/250 (96.8%)** | **488/500 (97.6%)** | **-19 dBm** |

> 1M/1M 接收率略低約 1%，在正常統計誤差範圍內。RSSI 較高（-19 vs -27 dBm），兩者距離條件不同。


## 驗證結果

| 驗證項目 | 結果 |
|----|----|
| SL sender PDU 類型 | ✅ Extended Advertising（1M/1M） |
| SL sender `pre_cnt` per-event decrement | ✅ 250 → 1，每事件遞減一次 |
| Nordic receiver 正確辨識 idx=1（1M/1M） | ✅ |
| `[1M1M]` first packet log 印出 | ✅ |
| `[RC_HIT]` 事件未出現 | ✅（封包路徑乾淨） |
| `[REORDER]` 事件未出現 | ✅ |
| `[DUP]` dedup 未觸發 | ✅ |
| Flow 1 接收率 ≥ 95% | ✅ 98.4% |
| Flow 2 接收率 ≥ 95% | ✅ 96.8% |
| 累計接收率 ≥ 95% | ✅ 97.6% |


## 關鍵問題記錄：`adv_handle` vs logical index 錯誤

本次 1M/1M 測試時，SL sender 出現 `pre_cnt=-32768`（無 burst chaining），調查後發現 `losstst_adv_sent_handler` 直接以 `adv_handle`（hw handle）當作 `index`，而 1M PHY slot 的 `ext_adv[1]` 在硬體層被分配到 hw handle `0`，導致檢查的是 `burst_active[0]`（false）而非 `burst_active[1]`。

| 問題 | 症狀 | 根本原因 | 修正方式 |
|----|----|----|----|
| handle vs index | `pre_cnt=-32768`，無 chaining | handler 誤用 `adv_handle` 作為 `index` | 改用 `get_adv_index_by_handle(adv_handle)` |
| log throttle 同問題 | `[ADV N]` 每個 event 都印 | `burst_active[handle]` 用了 hw handle | 改用 `burst_active[adv_index]` |


---

## Sender PHY 驗證

| 驗證點 | 預期 | 觀測 |
|----|----|----|
| Sender 初始化 | `pri=1(1M) sec=2(2M)`（預設 slot） | `[ADV 0] Setting PHY: pri=1(1M) sec=2(2M)` ✓ |
| Sender update（1M PHY slot） | `pri=1(1M) sec=1(1M)` | `[ADV 0] Updating PHY: pri=1(1M) sec=1(1M)` ✓ |
| Scanner 收到 PHY | `phy=1/1` | `[SCAN][EXT] ... phy=1/1` ✓ |


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
| Finit | 5 s re-advertise with `INT16_MAX` | ✓ |

### Transmission Counters

| Round | Before | After | Delta |
|----|----|----|----|
| Round 1 | 1M: 0 | 1M: 250 | +250 |
| Round 2 | 1M: 250 | 1M: 500 | +250 |
| **Total** |    | **500 / 500** | ✅ |


---

## 備註

* SL sender 已採用 per-event decrement，詳見 [PRE_CNT_PER_EVENT_FIX.md](PRE_CNT_PER_EVENT_FIX.md)。
* Nordic scanner dedup 防護層在本次測試中**未被觸發**，保留為防禦性程式碼。
* `[RC_HIT]` 未出現，確認 idx=1 封包路徑乾淨；`adv_sent idx=1` 確認 `get_adv_index_by_handle()` 修正後 hw handle `0` 正確映射至 logical index `1`。
* Round 1 後出現 `[ADV 2] Setting PHY: pri=1(1M) sec=2(2M)` 為 S8 slot lazy init，S8 未選用，不影響測試結果。


---

## 附錄 A：Silicon Labs Sender 原始 Log（condensed）

### Round 1

```
=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 15000 ms
PHY[1]: Starting burst with 250 events (1-event chain)
[ADV 0] Updating PHY: pri=1(1M) sec=1(1M)
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=249
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=200
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=150
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=100
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] Burst running: ~15s left | stop: 2M=- 1M=0 S8=- BLE4=-
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=50
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] PHY[1] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[1] 1M: 0 -> 250 (+250)
SND:173 P:1M/1M R:250/500 T:0
```

### Round 2

```
=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 15000 ms
PHY[1]: Starting burst with 250 events (1-event chain)
[ADV 0] Updating PHY: pri=1(1M) sec=1(1M)
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=249
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=200
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=150
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=100
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] Burst running: ~15s left | stop: 2M=- 1M=0 S8=- BLE4=-
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=50
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms (0x0006 * 10ms)
[SND] PHY[1] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[1] 1M: 250 -> 500 (+250)
SND:173 P:1M/1M R:500/500 T:0
SND:173 P:1M/1M Complete
```

### Finit

```
[SND] adv_sent idx=1 sndr_abort=1 pre_cnt=32767   ← abort handler, re-advertise 5 s
[SND] adv_sent idx=1 sndr_abort=0 pre_cnt=32767   ← 5 s finit ad expired, done
```


---

## 附錄 B：Nordic Scanner 原始 Log

```
Packet Loss Test (node 158) **** RCV SIDE ****
SENDER:173 P:1M/1M R:0/250 S:-19(..) T:8
RCV:173 P:1M/1M R:246/250 S:-19(-22..-18) T:8
RCV:173 P:1M/1M R:488/500 S:-19(-21..-18) T:8
```

* Nordic `idx=1` 特有的 `remote_ctrl_parser` 路徑經 `[RC_HIT]` log 監控，確認無干擾。


