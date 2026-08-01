# 可靠性路線圖 —— 對照業界正典的差距分析與實施計畫

2026-08-01,基於七份平行研究(四份現況體檢 + 三份參考架構:商用分選機、
Klipper/LinuxCNC 開源機控、PLC 工法)。結論先講:**核心架構就是業界正典**,
三個獨立傳統都收斂到同一個模式,而本機全部具備 ——

| 正典元素 | 本機實現 |
|---|---|
| 位置主軸(encoder/pulse),絕不用 wall-clock | stepper pulse count(SYS_STEP_COUNT) |
| 偵測事件進位置時鐘的 FIFO/shift register | RBuf + gate_pulse |
| 非同步判定用 ID 婚配,不靠到達順序 | tid handshake |
| 致動器在固定位置 offset 執行 | stage_pulse_offset 排程 |
| Fail-closed:沒答案的料絕不放行 | 停線(error 2) |
| 追蹤超時是具名故障,不是靜默失誤 | error 2 + tid + late_pulses |

所以方向是**補齊正典的周邊防線**,不是重寫。差距分三層。

## 第一層:實體監督(需要機構配合,下次動硬體時加)

商用機「紅旗清單」—— 每台認真的機器都有、缺了就是紅旗的五件事:

1. **每轉一次的 index mark**(prox/光電)。開環 pulse 計數沒有獨立交叉驗證,
   業界不接受:gate 有料時能部分自我校正,空轉時累積滑步無人發現。
   韌體側:到 mark 時檢查計數容差窗,超窗 = 追蹤完整性故障,停線。
2. **剔除確認 sensor**(NG chute 一顆光電):驗證吹了真的有料掉出去。
   死掉的氣閥才不會把 NG 默默當良品放行。
3. **氣壓開關 / 閥回饋**;NG 槽滿料偵測。
4. **數量對帳(parity)**:第二顆光電在下游驗證「FIFO 說該有料」vs「實際有料」,
   抓料件中途消失/鬼件 —— 追蹤 vs 現實的漂移。
5. **Commissioning 子命令**:打測試件自動量測/記錄各站 offset+dwell
   (grill 的基礎設施可直接沿用),取代手調常數。

## 第二層:故障哲學細分(PLC 正典;韌體可先做)

現行「有疑慮一律停線」比正典嚴,傷稼動率。業界的分法:

| 故障 | 正典處置 |
|---|---|
| 單件判定遲到/無答案 | **fail-to-reject**:強制 NA 回收,繼續跑,計數;連續 N 件才停 |
| 視覺鏈路斷 | 全部打 NA + 告警;連續 N 件後停 |
| parity 不符 / index mark 超窗 / FIFO 鬼件 | **立即停線**(追蹤完整性喪失) |
| 剔除確認失敗 | 告警;連續 N 次後停 |

原則:**品質不確定 → 剔除;追蹤完整性不確定 → 停線。**
實作為 `unanswered_policy: stop | force_na`(+ `unanswered_stop_after`),
預設維持 stop(檢測機保守值),量產穩定後可切 force_na。

## 第三層:連結契約(Klipper 正典;純軟體)

1. **Framing**:brace-counting → NDJSON + CRC16 + 序號(`{...}*HHHH\n`),
   一壞字元不再無限失步;掉幀可偵測。保留 RESET 逃生門於其下。
2. **統一 fault→安全態狀態機**:每個輸出宣告 safe default;host 心跳
   `max_duration` 逾期即進安全態(視覺程式 hang 不需 host 配合也停線)。
3. **連線 config hash 核對**:host 啟動時比對設定雜湊,不符拒絕進 READY
   (防 NVS 與 host 認知漂移)。
4. **遲到判定升格為協定違規**(配合第二層:能趕上 NA 窗就強制 NA,
   連 NA 窗都過了才算違規)。
5. 狀態名借用 PackML 語意(輕量,不搞完整合規)。

## 韌體體質(現況體檢的存量項目)

- [ ] **task WDT 啟用 + wdt_test 演練**(main loop 卡死目前無人知曉 —— 最高性價比)
- [ ] **健康遙測進 get_running_stat**:min-free-heap、最大 free block、
      stack 高水位、ISR tick-gap 高水位、RBuf 深度峰值 —— soak 心跳免費變趨勢監測
- [x] ~~ISR 呼叫鏈全面 IRAM 化~~ **決議不做**(2026-08-02):flash 寫入
      已收斂到單一入口(NVS save)且被 standstill guard 硬性攔截;無 OTA、
      無 WiFi、無 flash 日誌,cache-disable 條件已封死。全鏈 IRAM 化的
      維護成本(巨集鏈、漏標即炸)高於殘餘風險;`isr_gap_max_us` 遙測
      持續實證。**翻案條件**:未來引入 OTA / WiFi / flash 日誌任一者。
- [ ] 清 comm 路徑 std::string(長月運轉頭號殺手 = 堆碎片)
- [ ] device 端故障注入鉤子(跳 trigger / 竄改 tid⋯)= pipeline 的 mutation testing
- [ ] 斷電注入 rig(USB relay 斬 5V)+ powerchaos 子命令
- [ ] NVS 磨耗計數;flash 溫度遙測
- [ ] v2 emulator 接 CI(chaos/grill 語意每 commit 跑)

## 傳輸硬體(獨立軌道)

- 短期:正牌 FTDI + ADuM 磁隔離 + uhubctl 可控 hub(soak 連重插都自動化)。
  已觀測的掉線 = 氣閥電磁暫態打死橋接晶片,隔離斷根。
- 平台級:完成 ESP32_Eth(W5500)。磁隔離天生免疫、TCP 斷線軟體可恢復、
  Wireshark 可除錯;JSON 層原封不動。
- 不做:ESP32-S3 原生 USB(重開機重列舉毀掉自動重連)、WiFi、EtherCAT、
  換 MCU(百萬件零故障,遷移只重開已驗證行為)。

## 實施順序

1. ✅(本輪)task WDT + wdt_test;unanswered_policy;健康遙測
2. framing CRC(雙端)+ config hash + safe-state 狀態機
3. 故障注入鉤子 + powerchaos;IRAM 化 + std::string 清理
4. 硬體:隔離/FTDI/hub;三顆 sensor(index mark、剔除確認、氣壓)+ parity
5. commissioning 子命令;emulator CI;ESP32_Eth
