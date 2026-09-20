# visSele 圖解機台 —— 給新 session 讀的一疊圖

整個系統畫成 15 張 JPG。**直接 `Read` 圖檔**,不用先讀文字文件 ——
這一疊就是 `SYSTEM_MAP.md` + `SYSTEM_OVERVIEW.md` 的圖解版。

每張是完整一版(標題 + 圖 + 說明 + 陷阱),1180px 寬、2× 縮放、淺色,
單獨讀得懂,不需要前後文。

| 檔 | 畫的是 | 什麼時候該讀 |
|---|---|---|
| `00-cover.jpg` | 封面:來源與校準日期 | — |
| `01-machine.jpg` | 零件從料斗到料道的六個位置 | 完全沒碰過這台機器 |
| `02-five-layers.jpg` | launcher / WebUI / core / 板子 / 相機 | 不知道某個檔屬於哪一層 |
| `03-ports.jpg` | 4090 · 4091 · 4098 · 4099 · 序列線 | 要連上去問東西之前 |
| `04-boot-and-update.jpg` | app root 的樹、`current.json`、安裝四步 | 動 launcher 或出更新包 |
| `05-frame-journey.jpg` | driver callback → 量測 → 分岔成 preview 與判定 | 效能、掉格、左右不同步 |
| `06-bpg-contract.jpg` | 封包格式、TL 表、兩個門鈴 pgID | 動 WebUI ↔ core 任何一端 |
| `07-pairing.jpg` | CAM_SYNC(活)vs CAM_PCNT(已淘汰) | 懷疑「這張圖配到錯的件」 |
| `08-board-state.jpg` | 狀態碼轉移圖 + 錯誤碼表 | 機器停著不動 |
| `09-threads-and-locks.jpg` | 八個執行緒 + 鎖序鏈 | 動 core 的併發 |
| `10-config-ownership.jpg` | 六個設定檔誰讀哪幾個 key | 「我改了但沒有用」 |
| `11-production-safety.jpg` | 跑產中 安全/可恢復/禁止 三格 + 十個陷阱 | **在活機上動手之前,必讀** |
| `12-code-webui.jpg` | WebUI 檔案樹(含行數)+ 狀態機 + def 一生 | 要改前端,找不到檔在哪 |
| `13-code-core.jpg` | core 九個模組 + `wiringPanel.cpp` 15,232 行的地層 | 要改 core,找不到那段在哪 |
| `14-code-firmware.jpg` | 韌體樹 + `LegacyFirmware.cpp` 12,017 行的地層 | 要改板子,找不到 ISR 或命令在哪 |

`INDEX.tsv` 是機器可讀版:`檔名 \t 標題 \t 一句話`。

## 重畫

**JPG 沒有進 git**(見 `.gitignore`)—— 它們是產生物,一個字動了整包就變。
進 git 的是 `plates.html`,那份是唯一的來源。剛 clone 的話先跑一次下面的指令把圖生出來:

```sh
cd UI/WebUI/tools/webctl
node plates_export.mjs ../../../../docs/plates/plates.html ../../../../docs/plates
```

約 10 秒。`plates.html` 直接用瀏覽器開也可以,有深色模式與索引列;
JPG 一律導成淺色,因為圖檔沒有主題,而深底在被工具縮圖時字會糊掉。

## 準確度

內容出自 `docs/SYSTEM_MAP.md`(2026-08-18)與 `docs/SYSTEM_OVERVIEW.md`(2026-08-27);
兩份衝突時以 OVERVIEW 為準(它才有 launcher 與 4098)。
原文件的行號與檔案大小已經過時,**plate 12–14 的行數與地層是 2026-09-18 當天量的**
(`wiringPanel.cpp` 15,232 行、`LegacyFirmware.cpp` 12,017 行,文件寫的 12,945 / 10,043 已過時)。
檔案還在長,所以找路用區塊順序,不要背行號。

**這是現況文件。發現圖跟機器不符,改 `plates.html` 重導,不要在旁邊加註解。**
