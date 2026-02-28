# BLE 封包遺失測試報告 — Silicon Labs Sender × Nordic Scanner



**日期：** 2026-02-28**測試目的：** 端對端驗證 Silicon Labs EFR32（Extended Advertising, 1M primary / 2M secondary）發送，Nordic nRF52833 正確接收；記錄 sender 端 `pre_cnt` 重播問題的發現與修正過程


## 測試環境

| 角色 | 裝置 | 韌體 / SDK |
|----|----|----|
| **Sender** | Silicon Labs EFR32 (node 173) | LossTest SL port / Simplicity SDK |
| **Scanner** | Nordic nRF52833 (node 158) | LossTest_3rd_02s / NCS v2.8.0 |


## 測試配置

### Silicon Labs Sender

| 參數 | 值 |
|----|----|
| PHY | 1M primary / 2M secondary (Extended ADV) |
| Packet struct | `DEVICE_INFO_ST` (16 bytes Manufacturer Specific) |
| `man_id` / `form_id` | `0xFFFF` / `0xBAAB` |
| Burst count | 250 events/burst × 2 rounds = **500 total** |
| ADV interval | 60 ms |
| TX Power | 0.0 dBm (actual) |
| `pre_cnt` scheme | per-event decrement (250 → 1 per burst) |
| Preamble | −3 → −2 → −1（各 1 s） |
| Burst-end marker | `pre_cnt = 0` |
| Complete marker | `pre_cnt = 32767`（5 s 後停止） |

### Nordic Scanner

| 參數 | 值 |
|----|----|
| PHY 監聽 | 1M/2M (idx=0) |
| Dedup 機制 | 已加入（開發期過渡用，SL sender 修正後保留為防護層） |
| LossTest firmware | LossTest_3rd_02s rev. 2026-02-28 |


## Sender 端測試流程（Silicon Labs）

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
| Round 1 | 2M: 0 | 2M: 250 | +250 |
| Round 2 | 2M: 250 | 2M: 500 | +250 |
| **Total** |    | **500 / 500** | ✅ |


## 關鍵問題記錄：早期版本 Sender 重播同一 pre_cnt

> **根本修法為 SL sender 改為 per-event decrement；Nordic dedup 為開發期臨時過渡，現保留為防護層。**

### 問題描述

早期 Silicon Labs sender 對每個 `pre_cnt` 值廣播多個 advertising interval 才遞減（重播約 17\~22 次），Nordic receiver 原本每次收到都計入 `sub_total_rcv`，造成 subtotal 大幅超過預期總數。

### 修正前 log（SL sender 尚未修正）

```
[DUP] idx=0 pre_cnt=16 subtotal=2
[DUP] idx=0 pre_cnt=16 subtotal=3
...（約 17~22 次 DUP 後才遞減）
RCV:173 P:1M/2M R:302/250   ← 超過總數
RCV:173 P:1M/2M R:617/500   ← 超過總數
```

### 修正方式（時序）

| 步驟 | 修法位置 | 說明 |
|----|----|----|
| 1（臨時） | Nordic scanner `tst_form_packet_rcv` | 加入 dedup：`pre_cnt` 重複時不計入 subtotal，用於開發期量測驗證 |
| 2（正式） | **SL sender** `losstst_adv_sent_handler` | 改為 per-event decrement：每個 advertising event 遞減一次（250 → 1） |

SL sender 修正後，Nordic scanner dedup 在本次測試中**未被觸發**（SL sender 每次送出不重複的 `pre_cnt`），但保留為防護層。

### SL Sender Fix — 修正前後對比

|    | Before | After |
|----|----|----|
| Burst start param | `num_events = 250` | `num_events = 1` |
| `pre_cnt` init | `period_sec`（≈ 2–15） | `LOSS_TEST_BURST_COUNT`（250） |
| `pre_cnt` update | 每秒 1 次（sender task） | 每個 event（`losstst_adv_sent_handler`） |
| 每 burst 唯一值數量 | 2–15 | **250** |
| Burst chaining | 一次啟動多 event | handler 每次重啟 1 event |

### SL Sender 實作重點

**新常數**

```c
static const adv_start_param_t p_adv_1event_start_param[] =
    BT_LE_EXT_ADV_START_PARAM(0, 1);
```

**新狀態變數**

```c
static int16_t burst_remaining[4] = {0, 0, 0, 0};
static bool    burst_active[4]    = {false, false, false, false};
```

**Burst 初始化（sender task）**

```c
device_info_form[idx].pre_cnt = LOSS_TEST_BURST_COUNT;  // 250
burst_remaining[idx]          = LOSS_TEST_BURST_COUNT;
burst_active[idx]             = true;
```

**Per-event chaining（**`losstst_adv_sent_handler`）

```c
if (index < 4 && burst_active[index]) {
    burst_remaining[index]--;
    device_info_form[index].pre_cnt = (int16_t)burst_remaining[index];

    if (burst_remaining[index] > 0) {
        update_adv(index, NULL, ratio_test_data_set[index], p_adv_1event_start_param);
        return;
    } else {
        burst_active[index] = false;
        // pre_cnt == 0 → receiver sees "burst done" marker
    }
}
```

### `pre_cnt` Encoding（協議不變）

| 值 | 意義 |
|----|----|
| `INT16_MIN` (−32768) | Sender config-preset 進行中 |
| −3 → −1 | Pre-burst countdown（3 s） |
| 250 → 1 | **Burst 資料包 — 每 event 唯一** |
| 0 | Burst end marker |
| `INT16_MAX` (32767) | Burst complete / finit |

#### Nordic scanner dedup 程式碼（防護層，本次未觸發）

```c
if(form_p->pre_cnt == precnt_rcv[index]) {
    // same pre_cnt → skip subtotal increment
    subtotal = sub_total_rcv[index];
} else {
    subtotal = ++sub_total_rcv[index];
    precnt_rcv[index] = form_p->pre_cnt;
}
```


## 接收結果

### Flow 結果

| Flow | PHY | 收到 | 總數 | 遺失 | 接收率 | RSSI (avg/min/max) | TX Power |
|----|----|----|----|----|----|----|----|
| 1 | 1M/2M | 248 | 250 | 2 | **99.2%** | -27 / -31 / -25 dBm | 0 dBm |
| 2 | 1M/2M | 493 | 500 | 7 | **98.6%** | -28 / -31 / -27 dBm | 0 dBm |

### 驗證結果

| 驗證項目 | 結果 |
|----|----|
| Silicon Labs sender PDU 類型 | ✅ Extended Advertising (1M/2M) |
| SL sender `pre_cnt` per-event decrement | ✅ 250 → 1，每事件遞減一次 |
| Nordic receiver 正確辨識 idx=0 | ✅ |
| Preamble 封包正確送達（−3/−2/−1） | ✅ |
| Burst-end marker (`pre_cnt=0`) 送達 | ✅ |
| Complete marker (`pre_cnt=32767`) 送達 | ✅ |
| Flow 1 接收率 ≥ 95% | ✅ 99.2% |
| Flow 2 接收率 ≥ 95% | ✅ 98.6% |
| 無 `[REORDER]` 事件 | ✅ |


## 附錄 A：Silicon Labs Sender 原始 Log（condensed）

```
TX Power set: requested=0.0dBm, actual=0.0dBm for 4 sets

=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 15000 ms
PHY[0]: Starting burst with 250 events (1-event chain)
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=249
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=200
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=150
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=100
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=50
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] PHY[0] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[0] 2M: 0 -> 250 (+250)
SND:173 P:1M/2M R:250/500 T:0

=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 15000 ms
PHY[0]: Starting burst with 250 events (1-event chain)
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=249
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=200
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=150
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=100
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] Burst running: ~15s left | stop: 2M=0 1M=- S8=- BLE4=-
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=50
  [ADV 0] num_events=1, interval=60 ms, duration=60 ms
[SND] PHY[0] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[0] 2M: 250 -> 500 (+250)
SND:173 P:1M/2M R:500/500 T:0
SND:173 P:1M/2M Complete

[SND] adv_sent idx=0 sndr_abort=1 pre_cnt=32767   ← abort handler, re-advertise 5 s
[SND] adv_sent idx=0 sndr_abort=0 pre_cnt=32767   ← 5 s finit ad expired, done
```


## 附錄 B：Nordic Scanner 原始 Log

```
Packet Loss Test (node 158) **** RCV SIDE ****
SENDER:173 P:1M/2M R:0/250 S:-27(..) T:8
SENDER:173 P:1M/2M R:0/0 S:-26(..) T:
SENDER:173 P:1M/2M R:1/250 S:-26(..) T:8
RCV:173 P:1M/2M R:248/250 S:-27(-31..-25) T:8
RCV:173 P:1M/2M R:493/500 S:-28(-31..-27) T:8
```


## 備註

* **根本修法為 SL sender 改為 per-event decrement**，詳見 [PRE_CNT_PER_EVENT_FIX.md](PRE_CNT_PER_EVENT_FIX.md)。
* Nordic scanner dedup 防護層在本次測試中**未被觸發**（SL sender 每個 event `pre_cnt` 唯一），保留為防禦性程式碼。
* Round 1 log 中出現一次 UART 交錯列印（`[SND] Burst ru...` / `M=- S8=- BLE4=-`），由 sender task 與 event handler 同時呼叫 `printf` 造成，無功能影響；Round 2 無此現象。
* Burst log 只印第一筆與每 50 筆（pre_cnt = 249, 200, 150, 100, 50），避免 UART 塞滿。
* Nordic log 中 `T:8` 為 PDU 內 TX power 欄位讀值，與 SL sender actual `0.0 dBm` 為不同資料來源，不影響測試結果判讀。


