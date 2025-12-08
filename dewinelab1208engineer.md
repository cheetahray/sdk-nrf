raycheetah: 跟 dewinelab 開會第一次是業務，第二次才是工程師，幫我把切成兩份會議概要

GitHub Copilot: 好的，我幫你準備兩份會議概要：

---

# 🔧 Meeting 2: Technical Deep-Dive with DEWINE Labs Engineers

**Attendees:** Engineering Team (both sides)
**Duration:** 60-90 minutes
**Objective:** Validate technical feasibility and implementation details

## 🎯 Meeting Agenda

### 1. Technical Requirements Review (10 min)

**Present our system architecture:**

- Show **Central + Peripheral block diagrams** (you have them in draw.io!)
- Explain **PAwR-like periodic communication** requirements
- ~~Share **sequence diagrams** from our current implementation~~

**Our Requirements Summary:**

- Central: 1 unit broadcasting to multiple peripherals
- Peripheral: Dozens of sensor/actuator nodes
- Cycle: 1000ms with 50 timeslots (20ms each)
- PHY: Dual-mode (1M PHY + Coded PHY)
- Range: >100m in harsh environments
- ~~Reliability: >99.9% packet delivery in industrial settings~~
- **Mobile App**: iOS/Android app to monitor all peripheral sensor data via Central

### 2. Core Technology Discussion (25 min)

#### **Protocol & Architecture**

- ❓ Does your solution **natively support BLE 5.4 PAwR** (Periodic Advertising with Responses)?
- ❓ If not, what technology do you use to achieve similar functionality?
  - Custom TDMA over advertising?
  - Modified periodic advertising?
  - Connection-based approach?
- ❓ How does your **timeslot allocation** mechanism work?
- ❓ How do you handle **synchronization** between central and peripherals?
  - RTC sync mechanism?
  - Clock drift compensation?
  - Resync triggers and intervals?

#### **Hardware Compatibility**

- ❓ Which **Nordic chips** are supported?
  - nRF52833 (BLE 5.3, 128KB RAM)?
  - nRF52840 (BLE 5.3, 256KB RAM)?
  - nRF5340 (BLE 5.4, dual-core)?
- ❓ BLE version requirements: 5.3 vs 5.4?
- ❓ **Memory footprint**: RAM/Flash requirements?
- ❓ Can it run on **nRF52833** with limited resources?

#### **SDK Integration**

- ❓ Compatible with **nRF Connect SDK v2.7+**?
- ❓ **Zephyr RTOS** integration?
- ❓ Build system: CMake, Kconfig?
- ❓ Any dependencies on external libraries?

### 3. Timing & Performance (20 min)

#### **Periodic Communication**

- ❓ Can you support **1000ms broadcast cycle** with **50 timeslots** (20ms each)?
- ❓ Can we customize timeslot duration and count?
- ❓ **Synchronization accuracy**: what's the typical sync error (±ppm)?
- ❓ How do you handle **clock drift** over long operation (hours/days)?

#### **Response Mechanism**

- ❓ How does peripheral **response** work in your system?
- ❓ **Response latency**: typical and worst-case delay?
- ❓ **Adaptive retry mechanism**: what factors are considered?
  - RSSI?
  - Interference level?
  - Historical success rate?
  - Exponential backoff?
- ❓ How do you ensure response fits within timeslot budget?

#### **Scalability**

- ❓ Maximum number of **peripherals** per central?
- ❓ How is **slot assignment** managed?
  - Static allocation?
  - Dynamic allocation?
  - Collision avoidance algorithm?
- ❓ What happens when a new device joins?
- ❓ How do you handle device **disconnection/re-registration**?

### 4. Reliability in Harsh Environments (15 min)

#### **Interference Handling**

- ❓ How does your **adaptive algorithm** work?
  - What parameters are monitored (RSSI, PER(Packet Error Rate), CCA(Clear Channel Assessment))?
  - How do you detect interference?
  - What adjustments are made (power, timing, channel)?
- ❓ Performance in **2.4GHz congested** environments (WiFi, other BLE)?
- ❓ Can you provide **test data** on packet loss rates?

#### **Long Range & Penetration**

- ❓ **Coded PHY support** for extended range?
- ❓ Measured range in different environments:
  - Open space?
  - Industrial factory (metal, concrete)?
  - Outdoor?
- ❓ **Transmit power** settings: adjustable? Max power?
- ❓ How does the system perform through **obstacles** (walls, water, metal)?

#### **Deterministic Behavior**

- ❓ Can you guarantee **time-critical messages** arrive within specified timeslot?
- ❓ What's the **jitter** in message delivery?
- ❓ How do you prioritize **safety-critical** messages?
- ❓ Any **real-time OS** or scheduling guarantees?

### ~~5. Security & Safety (10 min)~~

#### **~~Encryption & Authentication~~**

- ~~❓ Supported encryption protocols?~~
  - ~~AES-128? AES-256?~~
  - ~~BLE Secure Connections?~~
- ~~❓ **Device pairing** mechanism?~~
- ~~❓ **Key management** strategy?~~
- ~~❓ Protection against replay attacks, eavesdropping?~~

#### **~~Safety Certifications~~**

- ~~❓ Any **functional safety** certifications?~~
  - ~~ISO 26262 (automotive)?~~
  - ~~IEC 61508 (industrial)?~~
- ~~❓ Fault detection and handling?~~
- ~~❓ Safe state behavior on communication loss?~~

### 6. Power Consumption (5 min)

- ❓ **Peripheral average power consumption** in active mode?
- ❓ Sleep mode support and current draw?
- ❓ Battery life estimation for typical use case?
- ❓ Any power optimization features?

### 7. Development & Integration (10 min)

#### **SDK & Tools**

- ❓ Development workflow and toolchain?
- ❓ Debugging tools and diagnostics?
- ❓ APIs: well-documented? Examples provided?
- ❓ **Testing framework** or unit tests included?

#### **Customization**

- ❓ Can we modify **timeslot allocation** algorithms?
- ❓ Can we add custom **application-layer protocols**?
- ❓ Access to source code for debugging/optimization?
- ❓ Callbacks/hooks for custom interference monitoring?

#### **Migration Path**

- ❓ If we start with nRF52833, can we easily migrate to nRF5340 later?
- ❓ Code portability between Nordic chips?

### 8. Mobile App Integration (10 min)

**Architecture Discussion:**

We need mobile app (iOS/Android) to monitor sensor data from all peripherals.

#### **Proposed Architecture:**
```
Mobile App (iOS/Android)
   │
   │ BLE Connection (GATT Client)
   ↓
Central Device (GATT Server + PAwR Broadcaster)
   │
   │ PAwR (1000ms cycle, 50 slots)
   ↓
Peripheral 1, 2, 3... N (Sensors)
```

**Key Questions:**

#### **Central Device Dual Role**
- ❓ Can the Central device act as both:
  - **PAwR Broadcaster** (to peripherals)
  - **GATT Server** (to mobile app)
- ❓ simultaneously without interference?
- ❓ What's the impact on performance (latency, reliability, power)?

#### **Data Flow**
- ❓ How does Central **cache** sensor data from peripherals?
  - RAM-based cache?
  - Update frequency (every PAwR cycle vs. throttled)?
- ❓ How does mobile app **access** the cached data?
  - GATT Read?
  - GATT Notification/Indication?
  - Custom service/characteristic design?

#### **GATT Service Design**
- ❓ Do you provide a **predefined GATT service** for sensor monitoring?
- ❓ Suggested characteristic structure:
  - Peripheral list/status
  - Individual peripheral sensor data
  - System status (interference level, etc.)
- ❓ Can we **customize** the GATT service for our sensor types?

#### **Mobile SDK**
- ❓ Do you provide **mobile SDK** (iOS/Android)?
- ❓ What does it include?
  - BLE connection management
  - Data parsing
  - UI components/widgets
- ❓ Reference app available for demonstration?
- ❓ Is it **open source** or binary library?

#### **Connection Management**
- ❓ Can multiple mobile devices connect to the same Central?
- ❓ Connection priority: mobile vs. PAwR communication?
- ❓ What happens if mobile connection drops?

#### **Update Frequency**
- ❓ Recommended **notification interval** to mobile app?
  - Every PAwR cycle (1000ms)?
  - Throttled (e.g., 5000ms)?
- ❓ How to balance:
  - Real-time updates
  - Power consumption
  - BLE throughput

#### **Security**
- ❓ Mobile-Central connection security:
  - Pairing required?
  - Encryption level?
  - Authentication mechanism?

### 9. Proof of Concept (5 min)

**Discuss PoC Setup:**

- ❓ Recommended hardware setup (dev boards, number of devices)?
- ❓ What can be validated in 2-4 weeks?
  - Basic communication?
  - Multi-device operation?
  - Interference resilience?
  - **Mobile app monitoring?**
- ❓ Do you provide **reference implementations** or demo code?
  - Including mobile app demo?
- ❓ On-site support or remote support during PoC?

## 📊 Technical Information to Share

**Prepare to provide DEWINE Labs:**

- [ ] Current system architecture diagrams (Central & Peripheral)
- [ ] Sequence diagrams of communication flow
- [ ] Detailed requirements document:
  - Number of devices
  - Communication frequency and data volume
  - Operating environment conditions
  - Target latency and reliability metrics
- [ ] Hardware constraints (chip model, memory, power budget)
- [ ] Project timeline and milestones

## 📤 Follow-up Actions

- [ ] Request technical documentation (API reference, integration guide)
- [ ] Request performance test reports
- [ ] Define PoC scope and success criteria
- [ ] Set up PoC environment and timeline
- [ ] Establish regular technical sync meetings during evaluation

---

## 📝 Notes

**Between Meeting 1 & 2:**

- Use business meeting insights to refine technical questions
- Prepare detailed technical requirements based on pricing discussion
- Set up NDA if required for deeper technical disclosure
- Prepare our current prototype demo (optional, if it helps discussion)

**Critical Decision Points:**
After both meetings, evaluate:

- ✅ Technical feasibility for our requirements
- ✅ Cost vs. development time savings
- ✅ Risk of vendor lock-in vs. time-to-market
- ✅ Comparison with in-house development (nRF5340 + native PAwR)
