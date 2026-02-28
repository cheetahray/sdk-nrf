# BLE 封包遺失測試報告 — 1M/2M PHY（Nordic Sender × Silicon Labs Scanner）


**日期：** 2026-02-26 / 2026-02-27**測試目的：** 端對端驗證 Nordic nRF52833 以 Extended Advertising (1M primary / 2M secondary) 發送，Silicon Labs EFR32MG27 正確接收


---

## 測試環境

| 角色 | 裝置 | 韌體 / SDK |
|----|----|----|
| **Sender** | Nordic nRF52833 (node 158) | LossTest_3rd_02s / NCS v2.8.0 |
| **Scanner** | EFR32MG27C140F768IM40 (node 173) | LossTest_3rd_02s / Simplicity SDK 2025.12.1 |


---

## 測試配置

### Nordic Sender

| 參數 | 值 |
|----|----|
| PHY 設定 | 2M: ✅   1M: ✗   S8: ✗   BLE4: ✗ |
| non_ANONYMOUS | `0`（匿名廣播） |
| mask_clr / mask_set | `0x00000000` / `0x00000000` |
| 封包總數 | 500 |
| TX Power | +8 dBm |
| ADV options | `0x00006400` — EXT_ADV + ANONYMOUS |

### Silicon Labs Scanner

| 參數 | 值 |
|----|----|
| PHY | 1M primary / 2M secondary |
| Burst Count | 500 packets per flow |
| Target Address | `C3:D7:89:36:DA:9E` |
| TX Power | 0.0 dBm |


---

## ADV 參數驗證（Nordic 端）

### `opt = 0x00006400` 解碼

| Flag | 值 | 意義 |
|----|----|----|
| `BT_LE_ADV_OPT_EXT_ADV` | **1** | Extended Advertising |
| `BT_LE_ADV_OPT_ANONYMOUS` | **1** | 匿名廣播（無 Advertiser Address） |
| `BT_LE_ADV_OPT_USE_IDENTITY` | 0 | — |
| `BT_LE_ADV_OPT_NO_2M` | 0 | Secondary PHY 允許 2M |
| `BT_LE_ADV_OPT_CODED` | 0 | 非 Coded PHY |

> **Secondary PHY = 2M**（`no2m=0` → controller 選 2M 作為 secondary channel PHY）

### ADVDBG 各階段摘要

| 階段 | tag | sec_phy | pdu_hint | ADVWARN |
|----|----|----|----|----|
| Sender 初始化 | `sender_setup` | — | — | 無 |
| 倒數廣播（-3→0 s） | `pre_burst` | **2M** | **extended** | 無 |
| 送進 BLE stack | `update_param` | **2M** | **extended** | 無 |
| 正式 Burst 發送 | `run_burst` | **2M** | **extended** | 無 |

所有階段 `opt` 值均固定為 `0x00006400`，**全程無** `[ADVWARN]`。


---

## 接收結果（Silicon Labs 端）

### 雙 ADV Set 現象說明

Sender 同時廣播兩組 Extended ADV（來自不同 adv set）：

| `phy` in `[SCAN_EVT][EXT]` | `len` | 內容 |
|----|----|----|
| `1/1`（1M primary + 1M secondary） | 151 | 非測試封包（form_id `0x4E53` ≠ `0xBAAB`）→ `[PARSE][DROP]` |
| `1/2`（1M primary + 2M secondary） | 35 | Loss test 封包 → `[PARSE] ✓` |

> 這導致 `ext_scan_evt_count` 約達 600 筆對應 500 個測試封包（約 100 筆額外 `phy=1/1` 事件）。為正常行為。

### Flow 結果

| Flow | PHY Config | 收到 | 總數 | 遺失 | RSSI（avg/min/max） | TX Power |
|----|----|----|----|----|----|----|
| 1 | 1M / 2M | 247 | 250 | 3（1.2%） | -31 / -37 / -30 dBm | +8 dBm |
| 2 | 1M / 2M | 494 | 500 | 6（1.2%） | -31 / -35 / -30 dBm | +8 dBm |

### 特殊事件說明

| 現象 | 說明 | 影響 |
|----|----|----|
| `[SCAN][MISS] idx=1` at cnt=1 | 測試開始前第一筆 `phy=1/1, len=151` 到達，尚無 valid test packet | 無影響，正常 |
| `pre_cnt` 跳過 7→1 | Burst 提前完成，sender 直接設 `pre_cnt=0` | 預期行為 |
| `Scanner: All flows complete` | 兩輪 flow 全部完成 | ✅ |


---

## 綜合結論

| 驗證項目 | 結果 |
|----|----|
| Nordic sender PDU 類型 | ✅ **Extended Advertising**（全程，無 legacy） |
| Nordic sender secondary PHY | ✅ **2M** |
| Silicon Labs 收到 Extended ADV event | ✅ `[SCAN_EVT][EXT]` |
| Silicon Labs 解析測試封包成功 | ✅ `[PARSE] ✓` |
| Flow 1 封包接收率 | ✅ 98.8%（247/250） |
| Flow 2 封包接收率 | ✅ 98.8%（494/500） |
| 無異常 timing error（`0x0021`） | ✅ Pass |
| 舊疑問：Nordic 是否送 legacy？ | ✅ **確認否** |

> \
> **先前疑慮（Silicon Labs 收到** `sl_bt_evt_scanner_legacy_advertisement_report_id`）已排除：Nordic 端全程 Extended ADV，問題來源為測試時 Silicon Labs scanner 尚未正確分辨 extended / legacy event 路由，已不復現。


---

## 附錄 A：Nordic Sender 原始 Log

```
[ADVDBG] sender_setup phy={2m:1,1m:0,s8:0,ble4:0} non_anon=0 mask_clr=0x00000000 mask_set=0x00000000
Packet Loss Test (node 158) **** SND SIDE ****
[ADVDBG] pre_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] pre_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] pre_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] run_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
SND:158 P:1M/2M R:250/500 T:8
[ADVDBG] pre_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] pre_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] pre_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] run_burst    idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
[ADVDBG] update_param idx=0 phy=1M/2M opt=0x00006400 ext=1 anon=1 id=0 no2m=0 coded=0 ad_items=3 pdu_hint=extended
SND:158 P:1M/2M R:500/500 T:8
SND:158 P:1M/2M Complete
```


---

## 附錄 B：Silicon Labs Scanner 原始 Log

```
Packet Loss Test (node 173) **** RCV SIDE ****
TX Power set: requested=0.0dBm, actual=0.0dBm for 4 sets
[SCAN_EVT][EXT] evt=0x020500A0 cnt=1 len=151 rssi=-26 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[SCAN][MISS] idx=1 rssi=-26 len=151 trig(snd=0 scn=2 num=0 env=0)
[SCAN_EVT][EXT] evt=0x020500A0 cnt=2 len=35 rssi=-35 txpwr=8 phy=1/2 addr=C3:D7:89:36:DA:9E
[SCAN_EVT][EXT] evt=0x020500A0 cnt=3 len=35 rssi=-31 txpwr=8 phy=1/2 addr=C3:D7:89:36:DA:9E
[SCAN_EVT][EXT] evt=0x020500A0 cnt=4 len=35 rssi=-31 txpwr=8 phy=1/2 addr=C3:D7:89:36:DA:9E
[SCAN_EVT][EXT] evt=0x020500A0 cnt=5 len=151 rssi=-30 txpwr=8 phy=1/1 addr=C3:D7:89:36:DA:9E
[PARSE][DROP] ID mismatch got=0xFFFF/0x4E53 expect=0xFFFF/0xBAAB
[PARSE] ✓ Valid test packet! pre_cnt=-3
SENDER:158 P:1M/2M R:0/0 S:-35(..) T:8
RCV:158 P:1M/2M R:0/0 S:-33(-33..-33) T:8
[PARSE] ✓ Valid test packet! pre_cnt=-2
[PARSE] ✓ Valid test packet! pre_cnt=-1
[PARSE] ✓ Valid test packet! pre_cnt=16
[SCAN_EVT][EXT] evt=0x020500A0 cnt=100 len=35 rssi=-32 txpwr=8 phy=1/2 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! pre_cnt=15
[PARSE] ✓ Valid test packet! pre_cnt=14
[PARSE] ✓ Valid test packet! pre_cnt=13
[SCAN_EVT][EXT] evt=0x020500A0 cnt=200 len=35 rssi=-31 txpwr=8 phy=1/2 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! pre_cnt=8
[PARSE] ✓ Valid test packet! pre_cnt=0
RCV:158 P:1M/2M R:247/250 S:-31(-37..-30) T:8
Scanner: PHY[0] flow=1 complete=0
[PARSE] ✓ Valid test packet! pre_cnt=-3
[PARSE] ✓ Valid test packet! pre_cnt=-2
[PARSE] ✓ Valid test packet! pre_cnt=-1
[PARSE] ✓ Valid test packet! pre_cnt=16
[SCAN_EVT][EXT] evt=0x020500A0 cnt=400 len=35 rssi=-33 txpwr=8 phy=1/2 addr=C3:D7:89:36:DA:9E
[PARSE] ✓ Valid test packet! pre_cnt=8
[PARSE] ✓ Valid test packet! pre_cnt=0
RCV:158 P:1M/2M R:494/500 S:-31(-35..-30) T:8
[PARSE] ✓ Valid test packet! pre_cnt=32767
Scanner: PHY[0] COMPLETE flag set (pre_cnt=32767)
Scanner: PHY[0] flow=2 complete=1
Scanner: All flows complete
```


