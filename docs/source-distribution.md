# 原始碼與再散布

本專案原創部分採 GPL-2.0-or-later；目前二進位發行使用 GPL v2 相容選項。完整授權見根目錄 LICENSE。提供 MSI 時，必須讓收件者同時取得該版本的完整對應原始碼與建置／安裝腳本，並履行隨包元件的義務。

## 每次 Release 必備

1. 指向確切 commit 的專案來源壓縮檔，包含 CMake、scripts、installer、資料與授權文件；不要僅指向持續變動的 main。
2. QtBase 官方 `qtbase-everywhere-src-6.11.1.zip`，SHA-256 `3529cc37297a5a7aae4486843b9fd41c30df1d79a770f85e240b537dcc327ca5`。這與 obs-deps 2026-07-15 的來源校驗檔一致；保留完整來源與授權，不只提供單個 Schannel `.cpp`。
3. obs-deps `2026-07-15` 的來源／Qt 建置設定及任何實際套用的 patch，用來說明預建 Qt 元件如何重建；同時保留該二進位套件的 SBOM。
4. WiX 4.0.6 所使用安裝動作的對應來源及 MS-RL，確認散布義務。
5. 記錄本專案與第三方來源的雜湊、版本與下載方式，與二進位在同一 Release 提供方便取得的來源資產。不得把 Client Secret、Token、個人設定或 CMakeCache 放入來源包。

專案：https://github.com/Chizakuraowob/obs-twitch-auto-category

QtBase：https://download.qt.io/archive/qt/6.11/6.11.1/submodules/qtbase-everywhere-src-6.11.1.zip

obs-deps：https://github.com/obsproject/obs-deps/tree/2026-07-15

WiX：https://github.com/wixtoolset/wix/tree/v4.0.6

本機 dist/sources 已準備 QtBase（經上游 SHA-256 核對）、obs-deps、WiX 來源及 Qt SBOM。Qt SBOM 的 revision 不可作為唯一下載方式；重建使用上述官方來源加 obs-deps 的 deps.qt/patches/Qt6/win/0001-QTBUG-146665.patch 與 deps.qt/qt6.ps1。尚未宣稱二進位位元完全一致的重建已驗證。本機 MSI 是驗證產物，不代表公開 Release 的來源資產已上傳完成。正式對外提供二進位之前，維護者必須完成以上來源清單。此清單不以第三方網址取代自己應履行的原始碼提供義務。
