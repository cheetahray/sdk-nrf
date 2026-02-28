# BLE 封包遺失測試報告 — Silicon Labs Sender × Nordic Scanner（S8/S8 PHY）


**日期：** 2026-02-28**測試目的：** 端對端驗證 Silicon Labs EFR32（Extended Advertising, Coded primary / Coded secondary S=8）發送，Nordic nRF52833 正確接收


---

## 測試環境

| 角色 | 裝置 | 韌體 / SDK |
|----|----|----|
| **Sender** | Silicon Labs EFR32 (node 173) | LossTest SL port / Simplicity SDK |
| **Scanner** | Nordic nRF52833 (node 158) | LossTest_3rd_02s / NCS v2.8.0 |


---

## 測試配置

### Silicon Labs Sender

| 參數 | 值 |
|----|----|
| PHY | Coded primary / Coded secondary S=8 (Extended ADV) |
| Packet struct | `DEVICE_INFO_ST` (16 bytes Manufacturer Specific) |
| `man_id` / `form_id` | `0xFFFF` / `0xBAAB` |
| Burst count | 250 events/burst × 2 rounds = **500 total** |
| ADV interval | **450 ms**（S8 Coded PHY 佔時長，需加大 interval） |
| TX Power | 0.0 dBm (actual) |
| `pre_cnt` scheme | per-event decrement（250 → 1 per burst） |
| Preamble | −3 → −2 → −1（各 1 s） |
| Burst-end marker | `pre_cnt = 0` |
| Complete marker | `pre_cnt = 32767`（5 s 後停止） |

### Nordic Scanner

| 參數 | 值 |
|----|----|
| PHY 監聽 | S8/S8 Coded (idx=2) |
| scan method | 2（`passive_scan_phy_coded`） |
| Dedup 機制 | 已加入（防護層，本次未觸發） |
| LossTest firmware | LossTest_3rd_02s rev. 2026-02-28 |


---

## 與其他 PHY 測試的差異

| 項目 | 1M/2M | 1M/1M | **S8/S8（本報告）** |
|----|----|----|----|
| SL sender idx | 0 | 1 | **2** |
| Primary / Secondary PHY | 1M / 2M | 1M / 1M | **Coded / Coded** |
| Nordic scanner idx | 0 | 1 | **2** |
| Nordic `remote_ctrl_parser` | 不執行 | 執行（idx=1 特有） | 不執行 |
| ADV event 實際佔時 | \~1× | \~1× | **\~8×（S=8 symbol spreading）** |

> \
> S8 Coded PHY 每個 advertising event 實際佔用時間約為 1M 的 8 倍（symbol rate 1/8）。SL sender 已將 S8 ADV interval 調高至 **450 ms**（1M/1M 為 60 ms）以容納 S8 event 佔時，每輪 burst 預期耗時約 **112 s**。


---

## 接收結果（Nordic Scanner 端）

### Flow 結果

| Flow | PHY | 收到 | 總數 | 遺失 | 接收率 | RSSI (avg/min/max) | TX Power (PDU) |
|----|----|----|----|----|----|----|----|
| 1 | S8/S8 | 241 | 250 | 9 | **96.4%** | -30 / -35 / -28 dBm | T:0 |
| 2 | S8/S8 | 231 | 250 | 19 | **92.8%** | -29 / -32 / -28 dBm | T:0 |
| **累計** |    | **472** | **500** | **28** | **94.4%** |    |    |

> \
> `T:0` 為 SL sender PDU 內 TX power 欄位讀值（0 dBm），與 SL sender actual 0.0 dBm 一致。Flow 2 接收率 92.8% 略低於 95% 參考門檻；RSSI 穩定（-28～-35 dBm），訊號強度不是主因。原始 log 見[附錄 B](#%E9%99%84%E9%8C%84-b-nordic-scanner-%E5%8E%9F%E5%A7%8B-log)。


---

## 對比：三種 PHY 接收率（SL Sender × Nordic Scanner）

| PHY | Flow 1 | Flow 2 | 累計 | RSSI avg |
|----|----|----|----|----|
| 1M/2M | 248/250 (99.2%) | 245/250 (98.0%) | 493/500 (**98.6%**) | -27 dBm |
| 1M/1M | 246/250 (98.4%) | 242/250 (96.8%) | 488/500 (**97.6%**) | -19 dBm |
| **S8/S8** | **241/250 (96.4%)** | **231/250 (92.8%)** | **472/500 (94.4%)** | **-30 dBm** |

> \
> 接收率隨 PHY 速率降低而遞減（1M/2M > 1M/1M > S8/S8），符合 Coded PHY 時序特性。各組 RSSI avg 不同，測試距離條件各異，無法直接比較訊號品質。


---

## Sender PHY 驗證

| 驗證點 | 預期 | 觀測 |
|----|----|----|
| Sender update（S8 slot） | `pri=4(S8) sec=4(S8)` | `[ADV 0] Updating PHY: pri=4(S8) sec=4(S8)` ✓ |
| Advertiser start flags | `flags=0x03` (ANONYMOUS \| TX_POWER) | `[ADV 0] ext_start: mode=0 flags=0x03` ✓ |
| Scanner 收到 PHY | `phy=4/4` | `[SCAN][EXT] ... phy=4/4` ✓ |


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
| Round 1 | S8: 0 | S8: 250 | +250 |
| Round 2 | S8: 250 | S8: 500 | +250 |
| **Total** |    | **500 / 500** | ✅ |


---

## 驗證結果

| 驗證項目 | 結果 |
|----|----|
| SL sender PDU 類型 | ✅ Extended Advertising（Coded S8） |
| SL sender `pre_cnt` per-event decrement | ✅ 250 → 1，每事件遞減一次 |
| Nordic receiver 正確辨識 idx=2（S8/S8） | ✅ `[S8] first pkt prim=3 sec=3` |
| `[REORDER]` 事件未出現 | ✅ |
| `[DUP]` dedup 未觸發 | ✅ |
| Flow 1 接收率 ≥ 95% | ✅ 96.4% |
| Flow 2 接收率 ≥ 95% | ⚠️ 92.8%（略低，S8 時序敏感） |
| 累計接收率 ≥ 95% | ⚠️ 94.4% |


---

## 備註

* SL sender 已採用 per-event decrement，詳見 [PRE_CNT_PER_EVENT_FIX.md](PRE_CNT_PER_EVENT_FIX.md)。
* Nordic scanner dedup 防護層在本次測試中**未被觸發**，保留為防禦性程式碼。
* `adv_sent idx=2` 確認 `get_adv_index_by_handle()` 正確將 hw handle 映射至 logical index 2（S8 slot）。
* S8 ADV interval 為 450 ms，每輪 burst 約耗時 112 s（250 × 450 ms）；時序裕度充足，Flow 2 較低原因待進一步觀察。
* Flow log 頂部出現 `[PARSE] ✓ pre_cnt=-32768` 為前次測試殘留封包（其他 node），非本 sender 發出，無影響。
* `[ADV 1] ext_start: mode=0 flags=0x00` 出現一次為 1M slot lazy init，該 slot 未使用，不影響測試結果。


---

## 附錄 A：Silicon Labs Sender 原始 Log（condensed）

### Round 1

```
=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 112500 ms
PHY[2]: Starting burst with 250 events (1-event chain)
[ADV 0] Updating PHY: pri=4(S8) sec=4(S8)
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=249
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=200
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=150
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=100
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=50
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] PHY[2] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[2] S8: 0 -> 250 (+250)
SND:173 P:S8/S8 R:250/500 T:0
```

### Round 2

```
=== Starting Burst Phase ===
Burst count per PHY: 250
Expected duration: 112500 ms
PHY[2]: Starting burst with 250 events (1-event chain)
[ADV 0] Updating PHY: pri=4(S8) sec=4(S8)
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=249
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=200
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=150
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=100
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] adv_sent idx=2 sndr_abort=0 pre_cnt=50
  [ADV 0] num_events=1, interval=450 ms, duration=450 ms (0x002D * 10ms)
  [ADV 0] ext_start: mode=0 flags=0x03
[SND] PHY[2] burst complete (250 events sent)
=== Burst Phase Complete ===
PHY[2] S8: 250 -> 500 (+250)
SND:173 P:S8/S8 R:500/500 T:0
SND:173 P:S8/S8 Complete
```

### Finit

```
[SND] adv_sent idx=2 sndr_abort=1 pre_cnt=32767   ← abort handler, re-advertise 5 s
```


---

## 附錄 B：Nordic Scanner 原始 Log

```
Packet Loss Test (node 158) **** RCV SIDE ****
SENDER:173 P:S8/S8 R:0/250 S:-34(..) T:0
RCV:173 P:S8/S8 R:241/250 S:-30(-35..-28) T:0
RCV:173 P:S8/S8 R:472/500 S:-29(-32..-28) T:0
```


