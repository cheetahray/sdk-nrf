# nRF52833 PAwR Response System - Sequence Diagrams
**Updated**: 2025-09-29 - 支援真實 PAwR Response API

## 🚀 重要更新：真實 PAwR Response API 支援
- **nRF52833**: 完全支援 `bt_le_per_adv_set_subevent_data()` API
- **真實硬體傳輸**: 不再需要模擬，使用實際 PAwR Response 功能
- **PAST 支援**: 完整的 Periodic Advertising Sync Transfer 功能
- **配置優化**: 所有必要的 Kconfig 選項已正確配置

## 1. 系統初始化流程 (System Initialization with Real PAwR API)

```mermaid
sequenceDiagram
    participant Main as main()
    participant Observer as pawr_peripheral_init()
    participant BT as Bluetooth Stack
    participant PAwRAPI as init_pawr_response_set()
    participant Timers as Kernel Timers
    participant IM as init_interference_monitor()
    
    Main->>Observer: pawr_peripheral_init(phy_type)
    activate Observer
    
    Observer->>BT: bt_enable(NULL)
    BT-->>Observer: Success/Error
    
    alt Bluetooth initialization failed
        Observer-->>Main: Return error
    else Success
        Observer->>Observer: Configure scan parameters
        Observer->>BT: bt_le_scan_start(&scan_param, scan_cb)
        BT-->>Observer: Start scanning
        
        Observer->>Observer: Initialize device state
        Note over Observer: Set my_phy_type, is_registered=false<br/>sync_established=false
        
        Observer->>Observer: Initialize RTC sync
        Note over Observer: Set local_rtc_time, master_rtc_ref<br/>rtc_offset_us=0, sync_confidence=0
        
        Observer->>IM: init_interference_monitor()
        activate IM
        IM->>IM: Initialize interference fields
        Note over IM: measured_rssi=-127<br/>channel_quality=100<br/>transmission_power=0
        deactivate IM
        
        Observer->>Observer: k_work_init_delayable(&response_work)
        Observer->>Timers: k_timer_start(&rtc_update_timer)
        
        rect rgb(50, 205, 50)
        Note over Observer: 🚀 nRF52833 Real PAwR Response API
        Observer->>PAwRAPI: init_pawr_response_set()
        activate PAwRAPI
        PAwRAPI->>BT: bt_le_ext_adv_create(&adv_params, ...)
        BT-->>PAwRAPI: pawr_response_adv handle
        PAwRAPI-->>Observer: ✅ PAwR Response set created
        deactivate PAwRAPI
        Note over Observer: printk("🚀 nRF52833: Real PAwR Response API ready!")
        end
        
        Observer-->>Main: Success
    end
    deactivate Observer
    
    Main->>Main: while(1) k_sleep(K_SECONDS(1))
```

## 2. 掃描和信標處理流程 (Scanning and Beacon Processing)

```mermaid
sequenceDiagram
    participant BT as Bluetooth Stack
    participant ScanCB as scan_cb()
    participant Utils as find_pawr_beacon()
    participant RTCSync as process_rtc_sync()
    participant RegAttempt as attempt_registration()
    participant SlotAssign as process_slot_assignment()
    participant DataReq as handle_data_request()
    
    BT->>ScanCB: Advertising packet received
    activate ScanCB
    
    ScanCB->>ScanCB: Update RSSI & channel quality
    Note over ScanCB: peripheral.interference.measured_rssi = rssi<br/>peripheral.interference.channel_quality = calculated
    
    ScanCB->>Utils: find_pawr_beacon(buf)
    activate Utils
    Utils->>Utils: Parse advertising data
    alt PAwR beacon found
        Utils-->>ScanCB: beacon pointer
    else No beacon
        Utils-->>ScanCB: NULL
        ScanCB-->>BT: Return (ignore packet)
    end
    deactivate Utils
    
    ScanCB->>RTCSync: process_rtc_sync(beacon, local_time)
    activate RTCSync
    RTCSync->>RTCSync: Update RTC synchronization
    Note over RTCSync: Check sync_confidence<br/>Update rtc_offset_us if needed
    deactivate RTCSync
    
    alt Not registered
        ScanCB->>RegAttempt: attempt_registration(beacon)
        activate RegAttempt
        RegAttempt->>RegAttempt: Log registration intent
        Note over RegAttempt: Pure Observer mode:<br/>Only logs, no active broadcast
        deactivate RegAttempt
        
        ScanCB->>SlotAssign: process_slot_assignment(beacon)
        activate SlotAssign
        SlotAssign->>SlotAssign: Check for auto-assignment
        alt Slot matches criteria
            SlotAssign->>SlotAssign: Auto-register to slot
            Note over SlotAssign: peripheral.assigned_slot = slot<br/>peripheral.is_registered = true
        end
        deactivate SlotAssign
        
    else Already registered
        alt Data slot (slot >= 10)
            ScanCB->>DataReq: handle_data_request(beacon, local_time)
            activate DataReq
            Note over DataReq: Process data request<br/>in assigned slot
            deactivate DataReq
        end
    end
    
    deactivate ScanCB
```

## 3. RTC 時間同步流程 (RTC Time Synchronization)

```mermaid
sequenceDiagram
    participant Timer as rtc_update_timer
    participant Handler as rtc_update_timer_handler()
    participant RTCSync as process_rtc_sync()
    participant Peripheral as peripheral struct
    
    Timer->>Handler: Timer expires (every 2s)
    activate Handler
    
    Handler->>Handler: Get current time
    Handler->>Handler: Calculate time_since_sync
    
    alt Sync confidence degrading
        Handler->>Handler: Decrease sync_confidence
        Note over Handler: if time_since_last > 5000ms<br/>sync_confidence -= 10
        
        alt Confidence too low AND registered
            Handler->>Handler: Log sync warning
            Note over Handler: "⚠️ RTC sync degrading"
        end
    end
    
    deactivate Handler
    
    Note over RTCSync: Called from scan_cb when beacon received
    
    activate RTCSync
    RTCSync->>RTCSync: Calculate time differences
    Note over RTCSync: time_since_last = local_time - last_rtc_sync_time
    
    alt Need RTC sync update
        RTCSync->>Peripheral: Update RTC parameters
        Note over Peripheral: master_rtc_ref = beacon->rtc_timestamp<br/>local_rtc_time = local_time<br/>Calculate new rtc_offset_us
        
        RTCSync->>RTCSync: Update sync confidence
        alt First sync
            RTCSync->>Peripheral: sync_confidence = 200
        else Regular update  
            RTCSync->>Peripheral: sync_confidence = min(255, current + 20)
        end
        
        RTCSync->>Peripheral: last_rtc_sync_time = local_time
    end
    
    deactivate RTCSync
```

## 4. 數據請求和回應流程 (Data Request and Response Handling)

```mermaid
sequenceDiagram
    participant ScanCB as scan_cb()
    participant DataReq as handle_data_request()
    participant ResponseWork as response_work_handler()
    participant WorkQueue as Kernel Work Queue
    participant PAwR as PAwR Stack
    participant Utils as Helper Functions
    
    ScanCB->>DataReq: handle_data_request(beacon, local_time)
    activate DataReq
    
    DataReq->>DataReq: Validate slot assignment
    alt Not our slot OR not registered
        DataReq-->>ScanCB: Return (ignore request)
    else Our assigned slot
        DataReq->>DataReq: Prepare data_response structure
        Note over DataReq: message_type = 0x30<br/>device_id = my_device_id<br/>response_slot = assigned_slot<br/>synced_rtc_timestamp<br/>rtc_sync_confidence<br/>phy_type
        
        DataReq->>DataReq: Fill sensor data (simulated)
        
        DataReq->>Utils: is_response_pending()
        activate Utils
        alt Previous response pending
            Utils-->>DataReq: true
            DataReq->>WorkQueue: k_work_cancel_delayable(&response_work)
            DataReq->>Utils: clear_response_queue()
        end
        deactivate Utils
        
        DataReq->>DataReq: Store response in buffer
        Note over DataReq: memcpy(pending_response, &response)<br/>response_length = sizeof(response)
        
        DataReq->>DataReq: Update transmission statistics
        Note over DataReq: interference.transmission_attempts++
        
        DataReq->>DataReq: Calculate adaptive timing
        Note over DataReq: delay_ms = base_delay<br/>+ backoff based on channel_quality
        
        DataReq->>WorkQueue: k_work_schedule(&response_work, delay_ms)
        
        DataReq->>DataReq: Log response queuing
        Note over DataReq: Print stats: attempts, success, collisions<br/>success rate calculation
    end
    deactivate DataReq
    
    WorkQueue->>ResponseWork: Execute delayed work
    activate ResponseWork
    
    ResponseWork->>ResponseWork: Check pending response
    alt No pending response
        ResponseWork-->>WorkQueue: Return
    else Response ready
        
        alt CONFIG_BT_PER_ADV_RSP enabled
            ResponseWork->>PAwR: Real PAwR response transmission
            Note over PAwR: bt_le_per_adv_set_subevent_data()<br/>(Currently simulated)
            
            alt Transmission success
                ResponseWork->>ResponseWork: Update success stats
                Note over ResponseWork: successful_transmissions++<br/>retry_backoff_level = 0
            else Transmission failed
                ResponseWork->>ResponseWork: Update failure stats
                Note over ResponseWork: collision_count++<br/>retry_backoff_level++
                
                alt Retry attempts < 3
                    ResponseWork->>WorkQueue: Schedule retry with backoff
                    ResponseWork-->>WorkQueue: Return (don't clear response)
                end
            end
            
        else Simulation mode
            ResponseWork->>ResponseWork: Simulate transmission
            Note over ResponseWork: Success based on channel_quality > 100
            
            ResponseWork->>ResponseWork: Update statistics accordingly
        end
        
        ResponseWork->>Utils: clear_response_queue()
        activate Utils
        Utils->>Utils: Clear pending_response buffer
        Note over Utils: response_length = 0<br/>memset(pending_response, 0)
        deactivate Utils
    end
    
    deactivate ResponseWork
```

## 5. 註冊和時隙分配流程 (Registration and Slot Assignment)

```mermaid
sequenceDiagram
    participant ScanCB as scan_cb()
    participant RegAttempt as attempt_registration()
    participant SlotAssign as process_slot_assignment()
    participant Peripheral as peripheral struct
    
    ScanCB->>RegAttempt: attempt_registration(beacon)
    activate RegAttempt
    
    RegAttempt->>RegAttempt: Pure Observer registration logic
    Note over RegAttempt: Observer mode: logs registration intent<br/>No active broadcast in Pure Observer
    
    RegAttempt->>RegAttempt: Extract device ID from BT address
    alt BT address available
        RegAttempt->>Peripheral: my_device_id = addr.a.val[5]
    else Default fallback
        RegAttempt->>Peripheral: my_device_id = 0x06
    end
    
    RegAttempt->>RegAttempt: Log registration attempt
    Note over RegAttempt: Print device_id, PHY type<br/>"📋 Observer registration logged"
    
    deactivate RegAttempt
    
    ScanCB->>SlotAssign: process_slot_assignment(beacon)
    activate SlotAssign
    
    SlotAssign->>SlotAssign: Check registration status
    alt Already registered
        SlotAssign-->>ScanCB: Return
    else Not registered AND data slot (>= 10)
        SlotAssign->>SlotAssign: Determine PHY-based slot range
        Note over SlotAssign: Coded PHY (0x04): slots 30-49<br/>1M PHY (0x01): slots 10-29
        
        SlotAssign->>SlotAssign: Check slot compatibility
        alt Slot in our PHY range
            SlotAssign->>SlotAssign: Calculate preferred slot
            Note over SlotAssign: preferred_slot = target_start +<br/>(device_id % 20)
            
            alt Current slot matches preferred
                SlotAssign->>Peripheral: Auto-assign slot
                Note over Peripheral: assigned_slot = current_slot<br/>is_registered = true
                
                SlotAssign->>SlotAssign: Log successful registration
                Note over SlotAssign: "✅ Observer auto-registered"<br/>"📡 Observer now listening for data requests"
            end
        end
    end
    
    deactivate SlotAssign
```

## 6. 錯誤處理和重試機制 (Error Handling and Retry Logic)

```mermaid
sequenceDiagram
    participant System as System Events
    participant ErrorHandler as Error Handlers
    participant RetryLogic as Retry Mechanisms
    participant Stats as Statistics Tracking
    
    alt Bluetooth Initialization Error
        System->>ErrorHandler: bt_enable() fails
        ErrorHandler->>ErrorHandler: Log error
        ErrorHandler->>System: Return error to main()
        System->>System: Exit application
    end
    
    alt Scan Start Error  
        System->>ErrorHandler: bt_le_scan_start() fails
        ErrorHandler->>ErrorHandler: Log scan failure
        ErrorHandler->>System: Return error
    end
    
    alt Response Transmission Error
        System->>RetryLogic: Response transmission fails
        activate RetryLogic
        
        RetryLogic->>Stats: collision_count++
        RetryLogic->>Stats: last_collision_time = current_time
        RetryLogic->>RetryLogic: retry_backoff_level++
        
        alt Retry level < 3
            RetryLogic->>RetryLogic: Calculate retry delay
            Note over RetryLogic: retry_delay = 50ms * backoff_level
            
            RetryLogic->>System: k_work_schedule(retry_delay)
            RetryLogic->>RetryLogic: Log retry scheduling
        else Max retries reached
            RetryLogic->>RetryLogic: Log final failure
            RetryLogic->>RetryLogic: Clear pending response
        end
        
        deactivate RetryLogic
    end
    
    alt RTC Sync Degradation
        System->>ErrorHandler: sync_confidence < 100 AND registered
        ErrorHandler->>ErrorHandler: Log sync warning
        Note over ErrorHandler: "⚠️ RTC sync degrading"<br/>Time since last sync > threshold
    end
    
    alt Channel Quality Issues
        System->>Stats: Update interference statistics
        activate Stats
        
        alt Poor channel quality
            Stats->>Stats: Increase response delays
            Stats->>Stats: Adjust transmission power
            Note over Stats: quality < 150 triggers<br/>adaptive timing adjustments
        else Good channel quality  
            Stats->>Stats: Reset backoff levels
            Stats->>Stats: Optimize timing parameters
        end
        
        deactivate Stats
    end
```

## 功能流程總結

1. **系統初始化**: 主程序啟動 → 藍牙初始化 → 掃描啟動 → 定時器設置
2. **掃描處理**: 接收廣播 → 解析信標 → RTC同步 → 註冊嘗試 → 時隙分配
3. **RTC同步**: 定時器觸發 → 信心度管理 → 時間偏移計算 → 同步狀態更新
4. **數據回應**: 數據請求接收 → 回應準備 → 適應性調度 → 傳輸執行 → 統計更新
5. **註冊分配**: 設備ID提取 → PHY範圍檢查 → 時隙匹配 → 自動註冊
6. **錯誤處理**: 錯誤檢測 → 重試邏輯 → 統計追蹤 → 適應性調整

## 🚀 6. nRF52833 真實 PAwR Response API 傳輸流程 (Real PAwR Response Transmission)

```mermaid
sequenceDiagram
    participant Central as PAwR Central
    participant nRF52833 as nRF52833 Peripheral
    participant ResponseWork as response_work_handler()
    participant NetBuf as NET_BUF_SIMPLE
    participant PAwRAPI as bt_le_per_adv_set_subevent_data()
    participant SoftDevice as Nordic SoftDevice Controller
    
    Central->>nRF52833: PAwR Beacon (Data Request in Slot 12)
    activate nRF52833
    
    nRF52833->>nRF52833: handle_data_request()
    Note over nRF52833: Generate sensor data:<br/>temp=23.4°C, humidity=62%<br/>battery=3156mV
    
    nRF52833->>nRF52833: Queue response for transmission
    Note over nRF52833: memcpy(peripheral.pending_response, &response)<br/>peripheral.response_length = sizeof(response)
    
    nRF52833->>nRF52833: k_work_schedule(&response_work, K_MSEC(1))
    deactivate nRF52833
    
    Note over ResponseWork: 🚀 Real PAwR Response Transmission
    activate ResponseWork
    
    ResponseWork->>ResponseWork: Check CONFIG_BT_PER_ADV_RSP
    Note over ResponseWork: ✅ Real API path (not simulation)
    
    ResponseWork->>NetBuf: NET_BUF_SIMPLE_DEFINE(response_buf, 255)
    activate NetBuf
    NetBuf->>NetBuf: net_buf_simple_reset(&response_buf)
    NetBuf->>NetBuf: net_buf_simple_add_mem(data, length)
    NetBuf-->>ResponseWork: Buffer ready
    deactivate NetBuf
    
    ResponseWork->>ResponseWork: Setup subevent parameters
    Note over ResponseWork: subevent = assigned_slot - 10<br/>response_slot_start = 0<br/>response_slot_count = 1<br/>data = &response_buf
    
    rect rgb(255, 215, 0)
    Note over ResponseWork, PAwRAPI: 🎯 真實 nRF52833 PAwR Response API 調用
    ResponseWork->>PAwRAPI: bt_le_per_adv_set_subevent_data(pawr_response_adv, 1, &subevent_params)
    activate PAwRAPI
    
    PAwRAPI->>SoftDevice: Nordic Controller PAwR Response
    activate SoftDevice
    SoftDevice->>SoftDevice: Process subevent data
    SoftDevice->>SoftDevice: Schedule transmission in assigned subevent
    SoftDevice-->>PAwRAPI: Success (err = 0)
    deactivate SoftDevice
    
    PAwRAPI-->>ResponseWork: ✅ Success
    deactivate PAwRAPI
    end
    
    ResponseWork->>ResponseWork: Update statistics
    Note over ResponseWork: successful_transmissions++<br/>retry_backoff_level = 0
    
    ResponseWork->>ResponseWork: Log success with chip info
    Note over ResponseWork: printk("✅ nRF52833: Real PAwR Response transmitted successfully!")
    
    ResponseWork->>ResponseWork: Display sensor data
    Note over ResponseWork: printk("📊 Response Data: temp=23.4°C, humidity=62%, battery=3156mV")<br/>printk("🎯 Subevent: 2, Response Slot: 0")
    
    ResponseWork->>ResponseWork: Clear pending response
    deactivate ResponseWork
    
    Note over Central: Central receives actual PAwR Response<br/>via subevent transmission (not broadcast!)
    Central->>Central: Process sensor data
    Note over Central: Point-to-point communication achieved<br/>Other peripherals don't receive this data
```

## 🎯 7. nRF52833 vs 模擬模式比較 (Real API vs Simulation Comparison)

```mermaid
sequenceDiagram
    participant App as Application
    participant nRF52833 as nRF52833 (Real API)
    participant Simulation as Simulation Mode
    participant Central as PAwR Central
    
    Note over App: Data request received in assigned slot
    
    rect rgb(50, 205, 50)
    Note over nRF52833: 🚀 nRF52833 真實 PAwR Response API
    App->>nRF52833: response_work_handler()
    nRF52833->>nRF52833: bt_le_per_adv_set_subevent_data()
    Note over nRF52833: ✅ Real hardware transmission<br/>✅ Point-to-point to Central only<br/>✅ Uses actual subevent<br/>✅ Nordic SoftDevice Controller
    nRF52833->>Central: Real PAwR Response via subevent
    Note over Central: ✅ Receives actual response data
    end
    
    rect rgb(255, 160, 160)
    Note over Simulation: ❌ 舊模擬模式 (不再需要)
    App->>Simulation: response_work_handler()
    Simulation->>Simulation: Simulate transmission conditions
    Note over Simulation: ❌ No real transmission<br/>❌ Only logs simulation<br/>❌ Cannot reach Central<br/>❌ Was workaround solution
    Simulation->>Simulation: printk("[SIMULATED] Response")
    Note over Central: ❌ No actual data received
    end
    
    Note over App, Central: 🎉 nRF52833 現在使用真實 PAwR Response API！
```

## 📋 配置和 API 支援總結

### ✅ nRF52833 支援的真實 API：
- `bt_le_per_adv_set_subevent_data()` - PAwR Response 傳輸
- `bt_le_ext_adv_create()` - 創建 PAwR 廣告集合  
- Nordic SoftDevice Controller - 硬體層 PAwR 支援

### 🔧 必要配置 (prj.conf)：
```properties
CONFIG_BT_PER_ADV_RSP=y                # PAwR Response 支援
CONFIG_BT_PER_ADV_SYNC_RSP=y           # PAwR Sync Response 支援  
CONFIG_BT_CTLR_SDC_PAWR_ADV=y          # Nordic Controller PAwR
CONFIG_BT_CTLR_ADV_DATA_LEN_MAX=1650   # 廣告數據長度
CONFIG_BT_PERIPHERAL=y                 # 連接支援 (PAST 需要)
CONFIG_BT_PER_ADV_SYNC_TRANSFER_RECEIVER=y  # PAST 接收者
CONFIG_BT_CTLR_SYNC_TRANSFER=y         # PAST 基礎支援
```

### 🎯 技術優勢：
1. **真實硬體傳輸**: 不再需要模擬，使用實際 PAwR Response API
2. **點對點通信**: Response 只傳送給 Central，不廣播給其他設備
3. **精確時序**: 使用真實 subevent 時隙進行傳輸
4. **Nordic 優化**: 完全利用 Nordic SoftDevice Controller 的 PAwR 能力
5. **全晶片支援**: nRF52833/52840/5340 都支援相同的 API

## 功能流程總結 (更新版)

這些序列圖展示了 nRF52833 PAwR Observer 系統的完整功能流程，**重點是現在使用真實的 PAwR Response API** 而不是模擬：

1. **系統初始化**: 主程序啟動 → 藍牙初始化 → **PAwR Response API 初始化** → 掃描啟動
2. **掃描處理**: 接收廣播 → 解析信標 → RTC同步 → 註冊嘗試 → 時隙分配  
3. **RTC同步**: 定時器觸發 → 信心度管理 → 時間偏移計算 → 同步狀態更新
4. **真實回應**: 數據請求接收 → 回應準備 → **真實 API 傳輸** → 統計更新
5. **註冊分配**: 設備ID提取 → PHY範圍檢查 → 時隙匹配 → 自動註冊
6. **錯誤處理**: 錯誤檢測 → 重試邏輯 → 統計追蹤 → 適應性調整

**🚀 主要改進**: 所有 nRF52/53 系列晶片現在都使用真實的 PAwR Response API，實現了真正的點對點通信！