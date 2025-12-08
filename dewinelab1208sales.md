raycheetah: 跟 dewinelab 開會第一次是業務，第二次才是工程師，幫我把切成兩份會議概要

GitHub Copilot: 好的，我幫你準備兩份會議概要：

---

# 📋 Meeting 1: Business Discussion with DEWINE Labs

**Attendees:** Sales/Business Development Team
**Duration:** 30-45 minutes
**Objective:** Understand solution fit, pricing model, and partnership potential

## 🎯 Meeting Agenda

### 1. Company & Solution Introduction (10 min)

**Our side:**

- Brief introduction of our company and project
- Application scenario: Industrial automation, remote control in harsh environments
- Current challenge: Seeking reliable BLE solution for dozens of devices

**DEWINE Labs:**

- Company overview and key customers
- Product portfolio (LinkBlu, LinkBlu FS, MonBlu)
- Target industries and use cases

### 2. Application Fit Assessment (15 min)

**Key Questions:**

#### **Application Scope**

- ~~❓ Is your solution designed for **industrial control applications** or primarily retail/ESL?~~
- ~~❓ Do you have experience with **harsh environments** (water, dust, temperature extremes, high interference)?~~
- ❓ What are typical use cases similar to our requirements?

#### **Technical Capabilities (High-level)**

- ❓ Do you support **long-range communication** (>100m with Coded PHY)?
- ❓ Can your solution handle **dozens of devices** communicating with one central unit?
- ❓ Do you support **point-to-point periodic communication** with deterministic timing?
- ❓ Nordic nRF52/nRF53/nRF54 chip compatibility?
- ❓ Can the central device support **simultaneous BLE connection** (e.g., from mobile app) while maintaining the periodic communication with peripherals?

#### **Reference Cases**

- ❓ Can you share case studies in **industrial automation, AGV, robotics**, or similar fields?
- ❓ What are the typical deployment scales (number of devices)?
- ❓ Performance metrics in real-world harsh conditions?

  1. **Packet Delivery Rate**
  2. **Latency**
  3. **Range**
  4. **Connection Reliability**
  5. **Interference Resilience**
  6. **Power Consumption**

### 3. Business Model & Pricing (15 min)

**Key Questions:**

#### **Licensing Model**

- ❓ What is your licensing model?
  - Per-device royalty?
  - Per-project flat fee?
  - Subscription-based?
  - Hybrid model?

#### **Pricing Structure**

- ❓ What is the typical price range for projects of our scale (dozens of devices)?
- ❓ Are there volume discounts or tiered pricing?
- ❓ Any minimum guarantee or royalty cap?
- ❓ Upfront development fees?

#### **Deliverables**

- ❓ Do you provide **source code** or **binary libraries**?
- ❓ Can we customize the solution for our specific needs?
- ❓ IP ownership: What rights do we have over our own code?

#### **Support & Maintenance**

- ❓ What level of technical support is included?
- ❓ Duration and scope of support?
- ❓ Cost for ongoing support and future updates?
- ❓ SDK version upgrade policy (e.g., nRF Connect SDK v2.8 → v3.0)?

### 4. Mobile App Integration (5 min)

**Our Additional Requirement:**

We need a **mobile app (iOS/Android)** to monitor sensor data from all peripheral devices.

**Key Questions:**

- ❓ Does your solution support **mobile app connectivity**?
- ❓ Architecture: Mobile ↔ Central (GATT) ↔ Peripherals (PAwR)?
- ❓ Can the mobile app view **real-time sensor data** from all peripherals?
- ❓ Do you provide:
  - Mobile SDK (iOS/Android)?
  - Reference app implementation?
  - API documentation for mobile integration?
- ❓ Any additional licensing cost for mobile connectivity feature?

### 5. Next Steps & Timeline (5 min)

**Discuss:**

- ❓ Can you provide a **detailed quotation** with different scenarios (NDA required)?
  - Include mobile app integration costs
  - Different volume scenarios (10K, 50K, 200K units)
- ❓ Is there a **Proof-of-Concept (PoC)** program available?
  - Duration (2-4 weeks)?
  - Cost?
  - Hardware requirements?
  - Can PoC include mobile app demo?
- ❓ Timeline for technical deep-dive meeting with engineers?
- ❓ What information do you need from us for accurate proposal?

## 📤 Follow-up Actions

- [ ] Request detailed quotation under NDA
- [ ] Receive technical documentation (if available)
- [ ] Schedule technical meeting with engineering team
- [ ] Prepare technical requirements document for next meeting

---
