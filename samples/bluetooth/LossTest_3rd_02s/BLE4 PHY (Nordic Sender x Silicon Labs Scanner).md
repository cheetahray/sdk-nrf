# BLE 封包遺失測試報告 — BLEv4 PHY

**日期：** 2026-02-27  
**Nordic 裝置：** nRF52833 (node 158) — LossTest_3rd_02s / NCS v2.8.0  
**Silicon Labs 裝置：** EFR32MG27C140F768IM40 (node 173) — Simplicity SDK 2025.12.1  
**測試目的：** 確認 BLEv4 模式 sender 使用 Legacy Advertising PDU，並由 Silicon Labs scanner 以 Legacy 事件正確接收


---

## 測試環境

| Item | Nordic Sender | Silicon Labs Scanner |
|------|--------------|---------------------|
| 裝置 | nRF52833 (node 158) | EFR32MG27C140F768IM40 (node 173) |
| SDK | NCS v2.8.0 | Simplicity SDK 2025.12.1 |
| 角色 | Sender (TX) | Scanner (RCV) |
| TX Power | +8 dBm | — |
| Sender Address | `C3:D7:89:36:DA:9E` | — |
| RX TX Power 記錄 | — | N/A（Legacy ADV 不帶 TX power 欄位） |

---

## Nordic Sender 配置

| 參數 | 值 |
|------|-----|
| PHY 設定 | 2M: ✗   1M: ✗   S8: ✗   BLE4: ✅ |
| non_ANONYMOUS | `1`（非匿名，帶 Advertiser Address） |
| 封包總數 | 500 |
| TX Power | +8 dBm |
| ADV options | `0x00000004` — USE_IDENTITY only |

---

## Silicon Labs Scanner 配置

| Parameter | Value |
|-----------|-------|
| PHY | BLE4 Legacy (1M primary / no secondary, `prim_phy=1 sec_phy=0`) |
| Burst Count | 250 packets per flow (2 flows) |
| Scan Method | method=1 (1M only, `sl_bt_scanner_scan_phy_1m`) |
| Scan Interval | 96 × 0.625ms = 60ms |
| Scan Window | 96 × 0.625ms = 60ms (100% duty cycle) |
| TX Power in event | N/A — legacy ADV 不帶 TX power 欄位 |
| SCAN_LOG_TARGET_FILTER | 1 (target: `C3:D7:89:36:DA:9E`) |

---

## Nordic ADV 參數驗證

### `opt = 0x00000004` 解碼

| Flag | 值 | 意義 |
|----|----|----|
| `BT_LE_ADV_OPT_EXT_ADV` | **0** | **非** Extended Advertising |
| `BT_LE_ADV_OPT_ANONYMOUS` | **0** | 非匿名（帶 Advertiser Address） |
| `BT_LE_ADV_OPT_USE_IDENTITY` | **1** | 使用 Identity Address |
| `BT_LE_ADV_OPT_NO_2M` | 0 | — |
| `BT_LE_ADV_OPT_CODED` | 0 | — |

> **PDU 類型 = Legacy**（`ext=0` → controller 使用 Legacy Advertising PDU）  
> 對應 `ADV_OPT_IDX_3` 定義：`USE_IDENTITY` only（無 `EXT_ADV`）

### sender_setup mask 操作

| 欄位 | 值 | 說明 |
|----|----|----|
| `non_anon` | `1` | 強制非匿名模式 |
| `mask_clr` | `0x00002000` | 清除 `BT_LE_ADV_OPT_ANONYMOUS` bit |
| `mask_set` | `0x00000004` | 設定 `BT_LE_ADV_OPT_USE_IDENTITY` bit |

### 各 PHY sender opt 比較

| PHY | `opt` | PDU 類型 |
|----|----|----|
| 1M/2M (idx=0) | `0x00006400` | Extended |
| 1M (idx=1) | `0x00006c00` | Extended |
| S8 (idx=2) | `0x00007c00` | Extended |
| **BLE4 (idx=3)** | `0x00000004` | **Legacy** |

### ADVDBG 各階段摘要

| 階段 | tag | idx | ext | pdu_hint | ADVWARN |
|----|----|----|----|----|----|
| 倒數廣播（-3→0 s） | `pre_burst` | 3 | **0** | **legacy** | **有** |
| 送進 BLE stack | `update_param` | 3 | **0** | **legacy** | **有** |
| 正式 Burst 發送 | `run_burst` | 3 | **0** | **legacy** | **有** |

所有階段 `opt` 值均固定為 `0x00000004`，**全程出現 `[ADVWARN]`**，確認使用 Legacy ADV 路徑。

---

## Silicon Labs Scanner 接收結果

### Flow Summary

| Flow | PHY Display | Received / Denominator | True Loss (of 249) | Loss Rate | RSSI (avg/min/max) | TX Power |
|------|------------|----------------------|-------------------|-----------|--------------------|---------:|
| 1 | 1M / NA | 244 / 250 | 5 / 249 | 2.0% | -25 / -28 / -23 dBm | N/A ¹ |
| 2 | 1M / NA | 250 / 250 ² | 0 / 249 | 0% ² | -25 / -28 / -23 dBm | N/A ¹ |

> ¹ BLE4 legacy ADV 不帶 TX power 欄位，firmware 填 127（unknown），顯示為空白 `T:`。  
> ² Burst 2 = 累計 `R:494` − burst 1 `244` = **250**，超過預期最大 249。`pre_cnt=0` 在 burst 2 中被多計一次（known off-by-one edge case），不影響測試完成邏輯。

---

## 觀察與說明

### 1. Silicon Labs 收到 `[SCAN_EVT][LEGACY]`
Nordic BLEv4 使用 Legacy ADV → Silicon Labs scanner 正確觸發 `sl_bt_evt_scanner_legacy_advertisement_report_id`，顯示為 `[SCAN_EVT][LEGACY]`。  
這與 Extended ADV（`[SCAN_EVT][EXT]`）路徑完全不同，與 `[ADVWARN]` 的預期一致。✅

### 2. `Scanner: PHY[3]` — 正確 BLE4 index
`prim_phy=1, sec_phy=0` → `idx=3`（BLE4 legacy）✅  
`P:1M/NA` — secondary PHY 顯示為 `NA`（無 secondary channel）✅

### 3. `T:` TX Power 空白
Legacy ADV HCI event 不帶 TX power，firmware 填 `127`（unknown），顯示為空白。預期行為。

### 4. `[SCAN][MISS] idx=1` at Start
Nordic sender 同時廣播 `phy=1/1 len=147` extended peek ADV（idx=1），但 BLE4-only 模式下 `round_phy_sel[1]=false` → `[SCAN][MISS]`。無害，BLE4 test packets 由 legacy ADV 路徑獨立接收。

### 5. 背景 Legacy 流量
掃描期間可見多個隨機 legacy advertiser（RSSI -24 ~ -62 dBm），約 1000 個 legacy 事件。均因 `LOSS_TEST_FORM_ID` 不符而被過濾，不影響結果。

### 6. Log Buffer Overflow（非漏收）
每 burst 僅可見 `pre_cnt=16 → 8`，`249 → 17` 因 BLE log buffer overflow 未印出。`R:244` 確認 firmware 已計數，非 RF 漏收。

---

## Pass / Fail

| Check | Result |
|-------|--------|
| Nordic ADVDBG: `pdu_hint=legacy` | ✅ Pass |
| Nordic ADVDBG: `[ADVWARN]` 全程出現 | ✅ Pass |
| Silicon Labs 收到 `[SCAN_EVT][LEGACY]`（非 EXT） | ✅ Pass |
| `Scanner: All PHYs COMPLETE, finishing` printed | ✅ Pass |
| `Scanner: PHY[3]` (correct BLE4 index) | ✅ Pass |
| `P:1M/NA` PHY display correct | ✅ Pass |
| No timeout between bursts | ✅ Pass |
| Flow 1: 244/250 (97.6% display / 98.0% true) | ✅ Pass |
| Flow 2: 494/500 cumulative | ✅ Pass |

---

## 各 PHY 損失率比較

| PHY | Flow 1 True Loss | Flow 2 True Loss | Air Time/pkt | PDU 類型 |
|-----|-----------------|-----------------|-------------|----------|
| 1M / 2M | 1.2% | 1.2% | ~0.15 ms | Extended |
| 1M / 1M | 1.6% | 2.4% | ~0.3 ms | Extended |
| **BLE4 Legacy** | **2.0%** | **~0%** | **~0.3 ms** | **Legacy** |
| S8 Coded | 4.8% | 6.0% | ~4–5 ms | Extended |

---

## 測試結果摘要

| 項目 | 結果 |
|------|------|
| Packet Loss Test 完成 | ✅ SND:158 P:BLEv4 Complete |
| Flow 1 傳送 | 250 / 500 |
| Flow 2 傳送 | 500 / 500 |
| PDU 類型 | ✅ **Legacy Advertising PDU** |
| Legacy 路徑確認 | ✅ `[ADVWARN]` 全程出現 |
| Primary PHY | 1M (Legacy) |
| Secondary PHY | N/A |

---

## 附錄 A：Nordic Sender 原始 Log

```
[ADVDBG] sender_setup phy={2m:0,1m:0,s8:0,ble4:1} non_anon=1 mask_clr=0x00002000 mask_set=0x00000004
Packet Loss Test (node 158) **** SND SIDE ****
[ADVDBG] pre_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] pre_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] pre_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] pre_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] pre_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] pre_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] run_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] run_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
SND:158 P:BLEv4 R:250/500 T:8
[ADVDBG] pre_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] pre_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] pre_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] pre_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] pre_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] pre_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] run_burst    idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] run_burst idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
[ADVDBG] update_param idx=3 phy=BLE4 opt=0x00000004 ext=0 anon=0 id=1 no2m=0 coded=0 sec_phy=2M ad_items=2 pdu_hint=legacy
[ADVWARN] update_param idx=3 phy=BLE4 opt=0x00000004 pdu_hint=legacy (possible legacy path)
SND:158 P:BLEv4 R:500/500 T:8
SND:158 P:BLEv4 Complete
```

---

## 附錄 B：Silicon Labs Scanner 原始 Log

```
Packet Loss Test (node 173) **** RCV SIDE ****
TX Power set: requested=0.0dBm, actual=0.0dBm for 4 sets
[SCAN_EVT][LEGACY] total=1 rssi=-51 len=18 addr=48:32:F3:08:11:9C
[SCAN][LEGACY] cnt=1 len=18 rssi=-51 txpwr=N/A addr=48:32:F3:08:11:9C
is_re_sche: update=1 stamp=0 tgr_val=2 numcst=0
is_re_sche: Task activated! stamp=0->2, result=2, update=1
[SCAN_EVT][LEGACY] total=2 rssi=-58 len=19 addr=63:1D:D3:84:B2:76
[SCAN][LEGACY] cnt=2 len=19 rssi=-58 txpwr=N/A addr=63:1D:D3:84:B2:76
[SCAN_EVT][EXT] evt=0x020500A0 cnt=1 len=147 rssi=-27 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=1 len=147 rssi=-27 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[SCAN][MISS] idx=1 rssi=-27 len=147 trig(snd=0 scn=2 num=0 env=0)
... (phy=1/1 extended peek ADV packets, all DROP - expected)
[PARSE] ✓ Valid test packet! pre_cnt=-3
SENDER:158 P:1M/NA R:0/0 S:-28(..) T:
RCV:158 P:1M/NA R:0/0 S:-27(-27..-27) T:
[PARSE] ✓ Valid test packet! pre_cnt=-2
[PARSE] ✓ Valid test packet! pre_cnt=-1
[PARSE] ✓ Valid test packet! pre_cnt=16
... (pre_cnt 15 → 8 visible, 249 → 17 suppressed by log buffer overflow)
[PARSE] ✓ Valid test packet! pre_cnt=0
RCV:158 P:1M/NA R:244/250 S:-25(-28..-23) T:
Scanner: PHY[3] flow=1 complete=0
[PARSE] ✓ Valid test packet! pre_cnt=-3
[PARSE] ✓ Valid test packet! pre_cnt=-2
[PARSE] ✓ Valid test packet! pre_cnt=-1
[PARSE] ✓ Valid test packet! pre_cnt=16
... (burst 2)
[PARSE] ✓ Valid test packet! pre_cnt=0
RCV:158 P:1M/NA R:494/500 S:-25(-28..-23) T:
Scanner: PHY[3] flow=2 complete=0
[PARSE] ✓ Valid test packet! pre_cnt=32767
Scanner: PHY[3] COMPLETE flag set (pre_cnt=32767)
Scanner: All PHYs COMPLETE, wait 10s
Scanner: All PHYs COMPLETE, finishing
```
