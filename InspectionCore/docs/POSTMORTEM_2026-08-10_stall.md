# Postmortem 2026-08-10 — 每幀漏一棵 JSON 樹,看起來像排程問題

**症狀**:五小時 soak 每 7–42 分鐘停機一次,間隔逐漸縮短,`error_hist [1]`
(`INSP_RESULT_MATCHES_NO_OBJECT`)。
**根因**:`image_pipe_info_gc` 回收 pipe 插槽時沒有釋放 `datViewInfo.report_json`,
每幀漏掉一整棵 cJSON 樹,40 分鐘累積 3.46 GB,把 16 GB 主機推進壓縮與交換。
**修正**:`635865bc`。
**代價**:約一天,六個錯誤假設,其中一個我還宣告成「這不是我們的 bug」。

這份文件的重點**不是根因**——那三行就講完了。重點是**為什麼一個記憶體洩漏
可以偽裝成排程問題整整一天**,以及每一個錯誤假設是被什麼量測殺掉的。
下次再看到「執行緒沒被排到」,先把這裡的量測順序跑一遍。

---

## 1. 事實:預算與症狀

- CAM→SWITCH 預算 = `(29900−9315)/(2×plate_freq)`。freq 13000 時 **792ms**。
- 報告晚於預算 → 該顆被掃成 UNANSWERED → 遲到的報告配不到物件 → **error 1**。
- 所以「停機」等價於「**某一份報告晚了超過 792ms**」。整條調查就是在找那個延遲。

觀測到的延遲來源(`perif WAIT SPIKE`,報告在 `perifSendQueue` 裡等發送執行緒):

```
最大 1372.6ms   idle_before 1391.3ms   depth_at_pop 48   write 0.24ms
```

**佇列裡躺著 48 筆,執行緒閒置 1.4 秒,真正寫出去只要 0.24ms。**

---

## 2. 六個錯誤假設,以及殺死它們的量測

| # | 假設 | 為什麼看起來像 | 被什麼殺掉 |
|---|---|---|---|
| 1 | 序列線路停滯 | 已知的 macOS USB-serial 問題,run 4 量到 686ms | 98 筆 `perif tx stall`,**max 374ms,無一超過 792ms 預算** |
| 2 | TSQueue 遺失喚醒 | 「消費者閒置而佇列有東西」 | `pop_blocking` 用 predicate 在 mutex 下等、`push` 在 mutex 下 notify,教科書正確;且 core 自己量到 `push_max 0.380ms`(生產者從未阻塞) |
| 3 | 執行緒優先權不足 | 「沒被排到」 | **它早就是 `QOS_CLASS_USER_INTERACTIVE`**(上一輪為了量到的 216ms 間隙加的),仍被餓 1252ms。後來更決定性:**兩條執行緒一起凍**,提高其中一條不可能修好 |
| 4 | 預覽編碼搶 CPU | 尖峰當下 `dview 10/10` 滿載 | A/B:`--no-stream` 完全不訂閱影像串流,**40.5 分鐘照樣停機** |
| 5 | `g_log_mutex` 競爭 | 那是兩條「解耦」執行緒唯一共用的東西 | 唯一註冊的 sink 是 memcpy 到 shm;stderr sink 是關的;註解提到的 WebUI sink 在此 build 不存在。再加 A/B:`INSP_LOG=warn`(流量少 100 倍)**19 分鐘照樣停機** |
| 6 | ring 的 page fault | ring 是 16MB mmap,約 5 分鐘繞一圈重刷 | 同一個 `INSP_LOG=warn` A/B 把繞圈週期拉到約 500 分鐘,**仍然停機** |

### 6.5 最糟的一個:「這不是我們的 bug」

`vm_stat` 顯示停機窗口內 pagein 衝到 **58,000/s**(基線 25)、出現 swapin。
我接著跑 `ps -o rss` 讀到 core 只有 **23.2 MB**,於是宣告:

> 吃記憶體的不是受測程式……這不是環境以外的問題,目標平台是 Windows 專機。

**錯。** 使用者用活動監視器截圖打臉:`visSele` **3.46 GB**,全機第一。

> **`ps -o rss` 在 macOS 上不計入已壓縮的頁面。**
> 一個會被壓縮掉的洩漏,對最順手的那個工具是**隱形的**。
> 活動監視器的「記憶體」欄是 `phys_footprint`,才是真的。
> 指令列對應:`/usr/bin/footprint -p <pid> | grep phys_footprint`

---

## 3. 根因,以及它如何偽裝

改用正確的工具之後,三步就到底:

```
$ /usr/bin/footprint -p <pid>
    phys_footprint: 3674 MB        phys_footprint_peak: 3674 MB
    (peak == current -> 單調成長,從未回收)

$ vmmap --summary <pid>
    MALLOC_SMALL   3.6G   dirty 153.7M   swapped 3.4G   909 regions
    (dirty 只有 153.7M,這就是 ps 看到 23MB 的原因)

$ heap <pid>
    88,974,894 nodes / 3,821,436,400 bytes / 平均 42.9 bytes
    尺寸只有兩個峰:
        64 bytes × 48,471,197 = 3.10 GB     <- cJSON 節點正好 64 bytes
        16 bytes × 40,040,648 = 0.64 GB     <- 它的小字串
```

40 分鐘 / 約 87,600 幀 → **每幀約 553 個 64B 節點,約 92 MB/分鐘。**
「每幀建一棵 JSON 樹然後沒有 delete」的指紋。

程式碼:

```c
bool image_pipe_info_gc(image_pipe_info &info, resourcePool<image_pipe_info> &pool)
{
  if(info.occupyFlag!=0) return false;
  pool.retResrc(&info);        // <-- report_json 沒被釋放
  return true;
}
```

而 `resourcePool::retResrc` **只是把 flag 翻回 0**——不呼叫解構子、不釋放任何東西:

```c
bool _retResrc (int idx) { if(pool[idx].flag==1){ pool[idx].flag=0; rest_size++; ...} }
```

插槽重新發出後,`ImgPipeProcessCenter_imp` 直接覆寫指標:

```c
imgPipe->datViewInfo.report_json = matchingEng.FeatureReport2Json(report);
```

舊樹就此失聯。`report_json` 唯一被刪除的地方是內聯的非 pass-down 分支,
**而量產路徑幾乎不走那條**。

### 為什麼它長得像排程問題

這是整件事最值得記住的一段:

```
core 佔 3.46 GB / 16 GB 主機
  → 系統進入壓縮與交換(已壓縮 3.31 GB,交換檔 2.43 GB)
  → core 自己的頁面被換出
  → 任何執行緒碰到那些頁面 → 等解壓縮 / swapin
  → 停滯數百毫秒到 1.4 秒
```

於是產生了三個把人帶往錯誤方向的表象:

1. **兩條解耦的執行緒同時凍住同樣長度** —— 因為它們等的是同一個 VM 子系統,
   不是互搶,也不是共用鎖。
2. **兩條都不燒 CPU** —— page fault 不燒 CPU,所以看起來像「沒被排到」。
3. **拉高優先權無效** —— 優先權不能加速缺頁。

而**停機間隔單調縮短**(19 → 12 → 3.6 → 1.3 分鐘)正是佔用單調成長的直接投影。
這個形狀本來就該早點指向「累積型資源問題」,而不是隨機的排程抖動。

---

## 4. 量測工具本身的錯誤(比假設更貴)

這一天真正的損失來自儀表,不是來自推理。逐條記下:

- **`ps -o rss` 看不到壓縮頁**。見上。用 `footprint -p`。
- **佇列滿 ≠ 消費者忙**。`dview 10/10` 被我讀成「預覽在吃 CPU」,
  它實際的意思是「**預覽的消費者落後了**」——而那個消費者當時根本沒在跑。
  這一個誤讀直接生出假設 #4,浪費一次完整的 A/B。
- **`self_cpu ≈ 0` 不能區分「被餓死」和「等鎖」**。兩者都不燒 CPU。
  它能排除的只有「它在跑但做別的事」。初稿寫成「已排除卡在別的東西上」,是過度宣稱。
- **累積平均對短事件是瞎的**。`insp split` 的 `inspect avg` 在 34 萬幀上是 6.03ms,
  三分鐘的異常推不動它;**只有 `max` 從 293ms 跳到 729ms**。
  事件分析看 max,不看 avg。
- **取樣到錯的物件會回報 `None` 而不是 0**。`trig_wait_*` 在 core 的
  `perif_pairing`(GS item),不在板子的 `get_running_stat` 裡,
  從序列 console 永遠讀不到。**一個永遠是 None 的欄位比沒有這個欄位更危險。**
- **`dump: files: []` 是假的**。harness 只 sleep 8 秒,而 dump 在 10 秒後才落地。
  那是唯一記錄到崩塌的 ring,差點被當成「沒有 dump」丟掉。已改為輪詢並等大小穩定。
- **`WAIT SPIKE` 只在刷新最大值時才印**。所以「多跑一會兒多收幾筆」是錯的,
  後續較小的尖峰一行都不會出現。
- **監看工具把自己偽裝成受測對象**。footprint 取樣器的命令列裡含有
  `pgrep -f "mac-arm64/visSele"`,於是 soak 的「是否已有 core 在跑」檢查比對到它,
  拒絕啟動整輪(`verdict: UNSET`);而我事後用同一個 pgrep 檢查,又看到它,
  以為 core 活著。**白跑半小時。** 修法:`pgrep -f "mac-arm64/[v]isSele"`。
- **`virt_drop` 是停機的結果,不是原因**。停機樣本裡 `virt_drop 119` 與
  `rej.rej_blocked 119` **完全相等**——那是 `blockNewDetectedObject` 為真時的拒收,
  也就是機器判定停機後**自己關閘**的動作。把它讀成「虛擬物件產生端在掉」會把調查
  推向板端,但板端當下 `rbuf_peak 44`、`min_heap 190KB`、`isr_gap_max_us 14.4ms`,
  毫無壓力。**看 `rej` 的分項,不要只看 `virt_drop` 的總數。**
- **在受測主機上做分析會汙染資料**。我用來查證的 15MB dump grep / `ps -Ao -m`
  就落在其中一次停機的窗口裡,那筆相關性不能採計。

---

## 5. 這次修掉的另一個真缺陷(獨立於根因)

追根因的路上發現並修掉了一個**不同的**問題,值得分開記:

**`TRIG_WAIT_MAX_MS = 700` 是個不會自我恢復的陷阱。**
這個等待是為「單一遲到的通告」定價的(實測最遲 652ms),但當上行整個安靜下來,
它變成每一幀都收 700ms:吞吐掉到 1.41/s 對上相機的 36/s,inspQueue 灌爆,
**而等待本身丟掉了那些遲到通告本來要配對的幀** → 再配不到 → 再等 700ms。

2026-08-10 的 run 5 在 2.65h 崩塌 25 倍並持續三分鐘零回復。修正(`80c92836`):
連續 5 次滿額逾時後放棄等待,把靜默的 25 倍降速換成快速而大聲的停機。

**注意這和本文根因是兩件事**,只是症狀都落在同一條報告路徑上。

---

## 6. 復發時的檢查順序

按成本由低到高,而且**前三步就能排除這次追了一天的所有東西**:

1. `/usr/bin/footprint -p <core-pid> | grep phys_footprint`
   —— `peak == current` 且持續成長 = 洩漏。**先做這個。**
2. `vmmap --summary <pid>` 看哪一類記憶體在漲(`MALLOC_SMALL` = 大量小配置)。
3. `heap <pid>` 看尺寸分佈。64B 的峰 = cJSON 節點;數量 ÷ 幀數 = 每幀漏幾個。
4. 才輪到 `perif WAIT SPIKE` 那組儀表(`self_cpu_over_gap` / `dview_beat_age`)。
5. 停機間隔若**單調縮短**,幾乎必然是累積型資源問題,不要往排程找。

相關文件:`CORE0_1_CAVEATS.md`(逐條 caveat)、
`../../Peripheral/uInspESP32/docs/MACHINE_FLOW.md`(預算與時間戳參考點)。

---

## 7. 修正的驗證

`635865bc` 之後,同樣條件(freq 13000、36.5 obj/s、預覽 8fps)每分鐘取樣一次
`phys_footprint`:

```
修正前                                修正後(run 13)
  92 MB/分鐘,單調上升                  14:22  37 MB
  40 分鐘 → 3,674 MB                    14:23  35 MB
  peak == current(從不回收)             14:24  41 MB
                                        14:25  40 MB
                                        14:26  42 MB
                                        → 5 分鐘內 35–42 MB 震盪,無趨勢
```

**約 90 倍的差異,而且斜率從單調上升變成持平。**
啟動尖峰 94 MB 之後穩定在 40 MB 上下。

判準:**`phys_footprint_peak == phys_footprint` 且持續成長 = 還在漏。**
持平且 peak 停在啟動值 = 正常。
