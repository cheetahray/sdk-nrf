# BLE 封包遺失測試報告 — 1M/1M PHY

**日期：** 2026-02-27  
**Nordic 裝置：** nRF52833 (node 158) — LossTest_3rd_02s / NCS v2.8.0  
**Silicon Labs 裝置：** EFR32MG27C140F768IM40 (node 173) — Simplicity SDK 2025.12.1  
**測試目的：** 確認 1M PHY sender（primary 1M / secondary 1M）使用 Extended Advertising PDU，並由 Silicon Labs scanner 正確接收

---

## 測試環境

| Item | Nordic Sender | Silicon Labs Scanner |
|------|--------------|---------------------|
| 裝置 | nRF52833 (node 158) | EFR32MG27C140F768IM40 (node 173) |
| SDK | NCS v2.8.0 | Simplicity SDK 2025.12.1 |
| 角色 | Sender (TX) | Scanner (RCV) |
| TX Power | +8 dBm | — |
| Sender Address | `C3:D7:89:36:DA:9E` | — |
| RX TX Power 記錄 | — | 0.0 dBm (scanner side) |

---

## Nordic Sender 配置

| 參數 | 值 |
|------|----|
| PHY 設定 | 2M: ✗ &nbsp; 1M: ✅ &nbsp; S8: ✗ &nbsp; BLE4: ✗ |
| non_ANONYMOUS | `0`（匿名廣播） |
| 封包總數 | 500 |
| TX Power | +8 dBm |
| ADV options | `0x00006c00` — EXT_ADV + ANONYMOUS + NO_2M |

---

## Silicon Labs Scanner 配置

| Parameter | Value |
|-----------|-------|
| PHY | 1M primary / 1M secondary |
| Burst Count | 500 packets per flow |
| Scan Method | method=1 (1M only, `phy=0x01`) |
| Scan Interval | 96 × 0.625ms = 60ms |
| Scan Window | 96 × 0.625ms = 60ms (100% duty cycle) |
| SCAN_LOG_TARGET_FILTER | 1 (target: `C3:D7:89:36:DA:9E`) |

---

## Nordic ADV 參數驗證

### `opt = 0x00006c00` 解碼

| Flag | 值 | 意義 |
|------|----|------|
| `BT_LE_ADV_OPT_EXT_ADV` | **1** | Extended Advertising |
| `BT_LE_ADV_OPT_ANONYMOUS` | **1** | 匿名廣播（無 Advertiser Address） |
| `BT_LE_ADV_OPT_USE_IDENTITY` | 0 | — |
| `BT_LE_ADV_OPT_NO_2M` | **1** | Secondary PHY 限制為 1M（禁用 2M） |
| `BT_LE_ADV_OPT_CODED` | 0 | 非 Coded PHY |

> **Secondary PHY = 1M**（`no2m=1` → controller 使用 1M 作為 secondary channel PHY）  
> 對應 `ADV_OPT_IDX_1` 定義：`EXT_ADV | ANONYMOUS | NO_2M`

### 與 1M/2M sender 的 opt 差異

| | 1M/2M sender | 1M sender（本次） |
|--|--|--|
| `opt` | `0x00006400` | `0x00006c00` |
| 差異 | — | `+0x00000800` = `BT_LE_ADV_OPT_NO_2M` |
| `sec_phy` | `2M` | `1M` |

### ADVDBG 各階段摘要

| 階段 | tag | idx | sec_phy | pdu_hint | ADVWARN |
|------|-----|-----|---------|----------|---------|
| 倒數廣播（-3→0 s） | `pre_burst` | 1 | **1M** | **extended** | 無 |
| 送進 BLE stack | `update_param` | 1 | **1M** | **extended** | 無 |
| 正式 Burst 發送 | `run_burst` | 1 | **1M** | **extended** | 無 |

所有階段 `opt` 值均固定為 `0x00006c00`，**全程無 `[ADVWARN]`**。

---

## Silicon Labs Scanner 接收結果

### Flow Summary

| Flow | PHY Config | Received | Total | Loss | RSSI (avg/min/max) | TX Power |
|------|-----------|----------|-------|------|--------------------|----------|
| 1 | 1M / 1M | 246 | 250 | 4 (1.6%) | -30 / -36 / -28 dBm | +8 dBm |
| 2 | 1M / 1M | 488 | 500 | 12 (2.4%) | -31 / -36 / -29 dBm | +8 dBm |

---

## 觀察與說明

### 1. Nordic 確認使用 Extended ADV
`pdu_hint=extended` + 無 `[ADVWARN]` → Nordic 全程使用 Extended Advertising PDU。  
Silicon Labs 收到 `[SCAN_EVT][EXT]`（非 `[SCAN_EVT][LEGACY]`）與此一致。

### 2. Dual Extended ADV from Sender
Sender 同時廣播兩種 Extended ADV（1M/1M）：

| `len` in `[SCAN_EVT][EXT]` | Content |
|---------------------------|---------|
| 147 | Non-test ADV (form_id `0x4E53` ≠ `0xBAAB`) → `[PARSE][DROP]` |
| 35  | Contains loss test packet (manufacturer data `len=16`) → `[PARSE] ✓` |

兩種均使用 `phy=1/1`——與 2M 測試中以 `phy=1/2` vs `phy=1/1` 區分不同，此處以 `len=35 vs 147` 區分。

`ext_scan_evt_count` 達到 ~700（500 test packets）是因含約 100 個額外 `len=147` ADV 事件一起計數。

### 3. `[PARSE] ✓` 顯示 `phy=1/1 len=16`
- `phy=1/1` 確認正確路由至 idx=1（PHY[1] = 1M）
- `len=16` 為 manufacturer data payload（`sizeof(device_info_t)`）

### 4. `Scanner: PHY[1]`（非 PHY[0]）
- PHY[0] = 2M，PHY[1] = 1M → 確認 PHY 索引映射正確 ✅

### 5. `pre_cnt` 跳變（8 → 0）
與 2M 測試相同：burst 完成後 timer 前，sender 立即設 `pre_cnt = 0`。為預期行為。

### 6. Scan Method 切換
```
[SCAN] Start scan method=0 phy=0x05   ← initial idle scan (1M + Coded)
[SCAN] Start scan method=1 phy=0x01   ← switched to 1M-only when pre-burst detected
```
`phy=0x05` = `sl_bt_scanner_scan_phy_1m_and_coded`，`phy=0x01` = `sl_bt_scanner_scan_phy_1m`。切換正確。

---

## Pass / Fail

| Check | Result |
|-------|--------|
| Nordic ADVDBG: `pdu_hint=extended` | ✅ Pass |
| Nordic ADVDBG: 無 `[ADVWARN]` | ✅ Pass |
| No `0x0021` advertiser timing errors | ✅ Pass |
| `Scanner: All flows complete` printed | ✅ Pass |
| `[PARSE] ✓` all show `phy=1/1` | ✅ Pass |
| `Scanner: PHY[1]` (correct 1M index) | ✅ Pass |
| Flow 1: 246/250 (98.4%) | ✅ Pass |
| Flow 2: 488/500 (97.6%) | ✅ Pass |
| Scan method switched to `phy=0x01` on receive | ✅ Pass |

---

## 附錄 A：Nordic Sender 原始 Log

```
[ADVDBG] pre_burst    idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] run_burst    idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
SND:158 P:1M/1M R:250/500 T:8
[ADVDBG] pre_burst    idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] pre_burst    idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] pre_burst    idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] run_burst    idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=1 phy=1M opt=0x00006c00 ext=1 anon=1 id=0 no2m=1 coded=0 sec_phy=1M ad_items=3 pdu_hint=extended
SND:158 P:1M/1M R:500/500 T:8
SND:158 P:1M/1M Complete
```

---

## 附錄 B：Silicon Labs Scanner 原始 Log

```
Packet Loss Test (node 173) **** RCV SIDE ****
TX Power set: requested=0.0dBm, actual=0.0dBm for 4 sets
[SCAN] Start scan method=0 phy=0x05 interval=96 window=96
is_re_sche: update=1 stamp=0 tgr_val=2 numcst=0
is_re_sche: Task activated! stamp=0->2, result=2, update=1
is_re_sche: update=0 stamp=2 tgr_val=2 numcst=0
[SCAN] Start scan method=1 phy=0x01 interval=96 window=96
[SCAN_EVT][EXT] evt=0x020500A0 cnt=1 len=147 rssi=-34 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=1 len=147 rssi=-34 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[SCAN_EVT][EXT] evt=0x020500A0 cnt=2 len=147 rssi=-29 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=2 len=147 rssi=-29 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[SCAN_EVT][EXT] evt=0x020500A0 cnt=3 len=35 rssi=-32 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=3 len=35 rssi=-32 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN_EVT][EXT] evt=0x020500A0 cnt=4 len=35 rssi=-32 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=4 len=35 rssi=-32 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN_EVT][EXT] evt=0x020500A0 cnt=5 len=35 rssi=-30 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=5 len=35 rssi=-30 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=-3
SENDER:158 P:1M/1M R:0/0 S:-32(..) T:8
RCV:158 P:1M/1M R:0/0 S:-29(-29..-29) T:8
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=-2
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=-1
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=16
[SCAN_EVT][EXT] evt=0x020500A0 cnt=100 len=35 rssi=-29 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=100 len=35 rssi=-29 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=15
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=14
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=13
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=12
[SCAN_EVT][EXT] evt=0x020500A0 cnt=200 len=147 rssi=-29 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=200 len=147 rssi=-29 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=11
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=10
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=9
[SCAN_EVT][EXT] evt=0x020500A0 cnt=300 len=35 rssi=-28 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=300 len=35 rssi=-28 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=8
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=0
RCV:158 P:1M/1M R:246/250 S:-30(-36..-28) T:8
Scanner: PHY[1] flow=1 complete=0
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=-3
[SCAN_EVT][EXT] evt=0x020500A0 cnt=400 len=35 rssi=-34 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=400 len=35 rssi=-34 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=-2
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=-1
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=16
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=15
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=14
[SCAN_EVT][EXT] evt=0x020500A0 cnt=500 len=35 rssi=-35 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=500 len=35 rssi=-35 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=13
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=12
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=11
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=10
[SCAN_EVT][EXT] evt=0x020500A0 cnt=600 len=35 rssi=-30 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=600 len=35 rssi=-30 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=9
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=8
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=0
RCV:158 P:1M/1M R:488/500 S:-31(-36..-29) T:8
Scanner: PHY[1] flow=2 complete=0
[SCAN_EVT][EXT] evt=0x020500A0 cnt=700 len=35 rssi=-36 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[SCAN][EXT] cnt=700 len=35 rssi=-36 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! phy=1/1 len=16 pre_cnt=32767
Scanner: PHY[1] COMPLETE flag set (pre_cnt=32767)
Scanner: PHY[1] flow=2 complete=1
Scanner: All flows complete
```
