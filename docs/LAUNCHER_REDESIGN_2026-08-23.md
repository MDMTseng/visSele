# 啟動器重新設計

2026-08-23 · `UI/Launcher/`(`UI/Electron_XPLAT` 原封不動保留)

**設計目標:啟動器的第一版就是最後一版。** 任何屬於某個應用、某台機台或某個
協定的東西,都必須能由應用自己表達 —— 否則它總有一天會逼出一次啟動器改版,
而啟動器改版正是**唯一無法用它自己內含的更新機制送出去的更新**。

---

## 1. 約束(使用者指定)

1. **絕不搬動或複製工作目錄裡的任何東西。** 那裡面是機台的校正、配方與快照,
   屬於機台,不屬於任何一個軟體版本。啟動器只讀路徑。
2. 使用者可指定**工作目錄**。
3. 使用者可指定**應用位置**。每個版本是一個資料夾,裡面有 `scripts/boot.js`
   決定這個 app 怎麼被啟動。
4. 更新包來自**本機檔案 / USB**,不走網路。
5. 啟動器與應用是**兩個獨立的可更新單元**。
6. 核心意外結束時**顯示錯誤並由操作員手動重啟**,不自動拉起。

---

## 2. 職責切分

```
應用(scripts/boot.js)決定 │ 啟動器保留
──────────────────────────┼──────────────────────────────
有幾個行程、叫什麼         │ spawn
執行檔位置、參數、環境變數 │ 抽乾每一條 stdio pipe
工作目錄用哪個             │ log ring 與輪替
怎麼問它健不健康           │ 關機的「逾時」
怎麼請它停止               │ 逾時之後的強制終止
啟動前什麼必須存在         │ 崩潰畫面
UI 在哪(檔案或 loopback URL)│ 拒絕自動重啟
```

右欄不是品味問題。**每一項都是舊設計交給 payload、而 payload 接著做錯的東西**
—— 而且是在一個每個版本都會繼承的檔案裡錯一次:

- stderr 被 pipe 但沒有讀者。Windows 匿名 pipe 約 64 KB,填滿之後核心下一次寫
  stderr **永久阻塞** —— 不是崩潰,行程還在、socket 還在聽,機器就是停住。
- `kill('SIGINT')` 在 Windows 上是 `TerminateProcess`,不是訊號。核心那條寫得
  很完整的優雅拆除路徑**從來沒有被執行過**。
- 緊接著的 `taskkill /f`,才是實際發生的事。

所以:**hook 可以決定「怎麼問它停」,不能決定「後面有沒有逾時」。**

---

## 3. boot.js 契約

```js
module.exports = {
  apiVersion: 1,
  describe(ctx) {          // ctx: { appDir, workingDir, platform, log }
    return {
      name: '...',
      core: {              // 或 processes: [...] 多行程,其中一個 primary: true
        exe: 'Core/visSele.exe',     // 相對於版本資料夾,不可逃出
        cwd: 'Core',                 // 或 '@app' / '@working'
        args: [`chdir=${ctx.workingDir}`],
        env: { INSP_CONTROL_PORT: '4098' },
        control: { host: '127.0.0.1', port: 4098 },
        readyTimeoutMs: 40000,
      },
      ui: { indexPath: 'WebUI/index.html' },   // 或 url: 'http://127.0.0.1:PORT/'
      requires: [{ path: `${ctx.workingDir}/data`, kind: 'dir', why: '…' }],
    };
  },

  // 以下全部可選,不寫就用內建預設
  async checkRequirements(services) {},
  async isReady(services) {},
  async health(services) {},
  async requestShutdown(services) {},
};
```

`services` 提供 `lineJson()`、`exists()`、`run()`(限版本資料夾內)、
`processes()`、`log()`。它是**方便,不是沙箱** —— hook 跑在主行程,想
`require()` 什麼都可以。它的價值在於常用的事情有一種不會變的寫法。

### 不認得的鍵一律報錯

```
boot.js: the plan has an unknown key "somethingNew".
This launcher understands: name, core, processes, ui, requires.
A newer package needs a newer launcher -- it is refused rather than half-applied.
```

這是「第一版即最後版」能成立的關鍵。靜默忽略未知鍵,新版套件配舊啟動器就會
**半套生效** —— 懂的部分套用了,不懂的部分丟掉,機器落在一個沒有人寫過的組態。
大聲失敗會讓版本不匹配在裝機時、在工作台上出現,而不是在產線上變成怪現象。

### 為什麼現在可以執行 payload 裡的程式碼

舊設計讓殼 `require()` 一個**剛解壓、從未檢查過**的 `launcher.js`。現在順序
反過來:套件要每個檔案都對得上 manifest 裡的 SHA256 才會被安裝,而只有**已安裝
且驗證過**的版本才會被載入。先驗證再執行站得住;執行後祈禱站不住。

---

## 4. 目錄配置

```
<appRoot>/                        使用者可指定,預設 <userData>/apps
  current.json                    {"version":"1.1.103"}   ← 唯一的切換點
  1.1.103/{info.json, scripts/boot.js, Core/, WebUI/, manifest.json}
  1.1.102/
  .staging/                       暫時性

<workingDir>/                     使用者指定,沒有預設
  data/                           機台的校正、配方、快照
```

**啟動器對 `<workingDir>` 只讀路徑。** 不建立、不複製、不 seed、不備份。
`selftest` 會對它做前後快照並比對,把這件事變成一個會失敗的測試,而不是一句
承諾。

`current.json` 順帶補上 v1 的一個缺口:v1 的更新會把新版解壓成同層的新資料夾,
但**從來沒有更新 `APPContentPath`**,所以更新完仍然跑舊版,要人工重新指一次。

---

## 5. 更新

```
選檔(zip)
  → 解到 .staging
  → 逐檔驗 SHA256(manifest 未列的檔案也拒絕 —— 沒被雜湊過的檔案不能上車,
     尤其現在其中一個會被執行)
  → 驗結構:info.json + scripts/boot.js,就這兩個
  → rename 進 <appRoot>/<version>      同磁碟,近乎原子
  → 【另一個明確動作】寫 current.json
```

**安裝不等於切換。** 一個複製完就生效的更新,不留任何一個讓操作員反悔的時刻。

保留最近 3 版,且只在成功啟動後才清理。回滾就是把 `current.json` 寫回去。

---

## 6. 核心端的配合

`wiringPanel.cpp` 新增 `ControlSocketThread` —— 只綁 `127.0.0.1`,常駐,
`INSP_CONTROL_PORT=0` 可關:

```
{"type":"ping"}      → {"type":"pong","pid":9820,"uptime_s":9.9,"version":"1.2",...}
{"type":"shutdown"}  → {"type":"shutdown","ack":true} 然後 g_shutdownRequested = 1
```

它解決兩件事:**Windows 上唯一能到達既有優雅拆除路徑的方法**(實測 0.8–1.1 秒
乾淨退出,log 出現 `graceful shutdown; dump on exit`),以及把「行程還在」和
「核心還會回應」分開。

### pong 裡的 pid,是測試逼出來的

第一次跑 selftest 時出現 `uptime_s: 4313.7` —— 那個回應來自我先前留在背景的
核心,不是測試剛 spawn 的子行程。健康檢查問到了陌生人,而 `stop()`
**把陌生人關掉了**,自己的子行程撐到 8.2 秒逾時被強殺。

**只用埠號辨識的控制通道,就只是用埠號辨識。** 殘留的核心、第二個啟動器,
在同一個 loopback 埠上看起來一模一樣。所以 pong 自報 pid,而預設的健康檢查與
關機請求都會先確認「回答的是不是我的行程」:

```
127.0.0.1:4098 answered by pid 4288, but core is pid 7312
  -- another instance is holding that port
```

不匹配時**拒絕送出 shutdown**,改走強制終止自己的子行程 —— 因為誤送 shutdown
的另一端,是一台正在運轉的機器。

回應裡沒有 pid 就跳過這個檢查,不猜:不回報 pid 的應用不是壞掉的應用,只是
拿不到這層保護。

### 而更根本的一層:Windows 的 SO_REUSEADDR 不是 POSIX 的那個

pid 檢查補的是症狀。查下去發現病因:控制 socket 設了 `SO_REUSEADDR`,而
**在 Windows 上這個選項允許第二個行程綁定一個已經有人在聽的埠**。POSIX 上它
只是允許重新綁定 TIME_WAIT 殘留的位址,完全是另一回事。

這不是推論,是 netstat 上看到的:

```
TCP    127.0.0.1:4098    LISTENING    8992
TCP    127.0.0.1:4098    LISTENING    9820
```

兩個核心同時 LISTENING,連線落到誰身上大致隨機 —— 所以監管者健康檢查了一個
行程,而 shutdown 會送給另一個。殘留的核心不是什麼異常狀態,它是星期二。

改成 Windows 用 `SO_EXCLUSIVEADDRUSE`(它才是 `SO_REUSEADDR` 在別的系統上
本來的意思):第二個 bind **失敗**,而不是靜默成功。監聽 socket 不會進
TIME_WAIT,所以重啟核心仍然可以立刻重新綁定。實測第二個核心啟動後,4098 上
只剩一個 listener。

bind 失敗時不致命(沒有控制通道的核心仍是一台能檢驗的機器),但訊息會直接
點名最可能的原因 —— 因為那個原因本身比缺少控制通道嚴重得多:

```
[control] bind/listen on 127.0.0.1:4098 failed -- graceful shutdown and
liveness checks are unavailable this run. Is another core already running?
Check for a leftover visSele process.
```

pid 檢查仍然留著。它防的是另一個情境:埠是自由的,但回答的是別人。

### 順手修的兩個相鄰缺陷

- `chdir()` 的回傳值原本被丟掉。目錄不存在時核心會用**啟動時的 cwd** 繼續跑,
  載入另一個 `data/` —— 拿別人的配方檢驗,而且完全不出聲。現在直接拒絕啟動。
- `CamInitStyle` 有兩個一模一樣的 `strcmp(str, "1")`,所以 `cam=2`
  (`// 2 for fake only`)從來不可達。

---

## 7. 安全設定

```js
contextIsolation: true,   nodeIntegration: false,
webSecurity: true,        sandbox: true
```

舊殼三個全反,而且用了 `electron.remote` —— 那正是它卡在 Electron 12 升不上去
的原因。新殼一處都沒有。

WebUI 那顆「檢測快照儲存位置」按鈕改走 `window.launcher.pickFolder`,
`platform_api` 那條 WS 保留當備援。整套 express + apollo + mongoose + 8085
就是為了這一個對話框而存在的,現在不需要了。

**一個測試斷言的更正**:`window.require` 在 WebUI 頁面裡是有的,但它是
`UI/WebUI/index.html` 裡刻意放的 no-op shim(`function () { return {}; }`),
讓 ESM 頁面不會因為 top-level `require('electron')` 而爆掉。斷言它不存在會回報
一個不存在的漏洞。改成檢查真正要緊的:`process` / `Buffer` / `child_process`
都拿不到,`require('fs')` 回傳空物件。

---

## 8. 測試

```
mingw32-make v2_app     建核心 + WebUI → 組合成 export_v2/<version>/     ← 開發迴圈
mingw32-make package_v2 → export_v2/update_<version>_win.zip(逐檔 SHA256)
mingw32-make v2         以上全部 + 打包啟動器本身
mingw32-make test_v2    selftest + shell_smoke
node tools/integration.mjs <zip> <workingDir>
```

`v2_app` 停在資料夾,是刻意的。組合出來的資料夾**本身就是一個合法的 app root
條目**(用版本號命名,不是 `payload`),所以開發迴圈是一個指令:build 完把
啟動器的 app root 指到 `export_v2/`,它就會找到這個版本 —— 不用 zip、不用對
244 MB 做一次 SHA256、也不用安裝。連版本都不用選,因為沒有 `current.json` 時
啟動器會退回最新的有效版本。

實測 66 秒(核心增量 + WebUI 完整重建)。`v2_app` 會先檢查工具鏈
(`g++` / `npm` / `python`)並在缺少時用一句話說明,而不是在 cmake 跑到一半時
才炸;`MINGW_BIN` 可覆寫,預設 `/c/msys64/mingw64/bin`。

三套的分工是刻意的:

| | 涵蓋 | 不涵蓋 |
|---|---|---|
| `selftest`(22 項) | 安裝、竄改偵測、指標、boot.js、監管、優雅關機、prune、**工作目錄未被寫入** | 視窗 |
| `shell_smoke`(11 項) | 殼的首次執行狀態、contextIsolation 真的生效、主行程拒絕越權呼叫 | 交棒給應用 UI |
| `integration`(12 項) | 真套件 + 真核心跑完整流程,**殼交棒給 WebUI 的那一刻** | — |

第三套存在的理由:preload 或導覽出錯,就躲在那個交棒點。

### 工具鏈上抓到的坑

- **`bsdtar` 兩個**:絕對路徑給 `-f` 會被當成 `host:path`
  (`Cannot connect to C: resolve failed`);bare `tar` 在這台機器解析到 MSYS2 的
  GNU tar,它讀不了 zip。都改成明確路徑 + basename,並補了 PowerShell
  `Expand-Archive` 退路。
- **`showShell()` 的未處理 promise rejection**:核心結束時若視窗正在拆除,
  `loadFile` 以 `ERR_FAILED (-2)` reject。每次乾淨結束都印一次堆疊,正是那種會
  訓練所有人忽略錯誤訊息的東西。

---

## 9. 打包差異

`export_v2/update_<version>_win.zip` 與舊的 `update_win.zip`:

- `info.json` 在**根目錄**
- 多一個 `manifest.json`,每檔一個 SHA256
- `scripts/` 只有**一個檔案** `boot.js`。舊的裝著 apollo_gql_server、它的
  node_modules 和 InspMonitor build —— 一個 GraphQL 伺服器和一個 MongoDB
  客戶端,而這台機器的設定沒有指向其中任何一個
- `.debug` **有**包(約 31 MB)。那是讓現場 crash dump 符號化成函式名的東西

實測:117 檔、244.5 MB → **88.6 MB**。

---

## 10. 還沒做

- **manifest 簽章。** `verifySignature()` 是一個具名的空 hook。逐檔雜湊證明
  套件完整,不證明它是誰做的 —— 能換掉檔案的人也能換掉 manifest。
  這件事現在比之前更重要,因為啟動器**會執行**已安裝版本裡的 `boot.js`,
  「套件完整」和「套件是我們的」已經不是同一個問題。
  要決定的是金鑰保管、輪替、以及已經在現場的舊套件怎麼辦。
