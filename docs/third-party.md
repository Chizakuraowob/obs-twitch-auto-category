# 第三方授權與來源

本專案原創程式與文件採 GPL-2.0-or-later。隨本版本發行的二進位組合採用 GPL v2 相容的授權選項；這不會變更第三方的授權，也不表示可以將 GPL-2.0-only 元件任意改為 GPL v3。完整 GPL v2 原文附於 LICENSE。根目錄 LICENSE 的 FSF 著作權是授權文件本身的著作權，不是本插件的作者署名。

## OBS Plugin Template

- 來源：https://github.com/obsproject/obs-plugintemplate
- 使用基準：3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc。
- 授權：GPL v2；沿用檔案如宣告 v2-or-later 則保留該宣告。原有作者／授權資訊保留於來源與 Git 歷史。
- 用途：專案與 CMake／建置腳本基礎。本專案新增 Windows MSI、Twitch API、UI 與辨識實作。

## OBS Studio／libobs／obs-frontend-api

- 目標版本：32.2.2；來源：https://github.com/obsproject/obs-studio/tree/32.2.2
- OBS Project contributors；GPL v2 系列，細節以對應來源檔案聲明為準。
- 用途：OBS 外掛及前端 API。使用使用者 OBS 的執行期，不在 MSI 重新散布整套 OBS。

## Qt 6.11.1

- Copyright (C) The Qt Company Ltd. and other contributors.
- 本外掛使用 OBS 提供的 Qt Core／Widgets／Network／Concurrent；MSI 不覆寫 OBS 的 Qt DLL。
- MSI 另附未修改的 `qschannelbackend.dll`，採套件 SBOM 提供的 GPL-2.0-only 選項。其依賴的 Qt 執行期由 OBS 提供。
- 套件 SBOM 記錄 revision：8683107a02300923abe4f293920f4b5edc8cb624；此值無法從上游鏡像取得，應以 obs-deps 建置腳本指定的官方來源包加 patch 重建。
- 官方來源：https://download.qt.io/archive/qt/6.11/6.11.1/submodules/qtbase-everywhere-src-6.11.1.zip
- 來源 SHA-256：3529cc37297a5a7aae4486843b9fd41c30df1d79a770f85e240b537dcc327ca5。obs-deps 2026-07-15 的 Windows patch 與建置腳本亦須提供。
- 元件位置：`src/plugins/tls/schannel`；預建套件：obsproject/obs-deps 2026-07-15 Windows x64 Qt bundle。
- GPL v2 全文另隨 TLS 元件附於 `data/qt-plugins/Qt-GPL-2.0.txt`。對應來源及重建資料的發布清單見 `docs/source-distribution.md`；單獨列出網址不代表已完成所有原始碼提供義務。

## 建置工具（不作為外掛執行期散布）

- WiX Toolset 4.0.6：MS-RL；https://github.com/wixtoolset 。用於產生 MSI；安裝包另含所使用的 WiX UI／Util 安裝動作，應保留其授權，見隨包 MS-RL 全文。
- CMake：https://cmake.org ，BSD-3-Clause；用於建置，不是本 MSI 的 Qt 或 OBS 執行期。
- Microsoft Visual Studio Build Tools／Windows SDK／.NET：依 Microsoft 各產品條款；建置需求見開發指南。本文件不授予 Microsoft 軟體的額外權利。

介面中的箭頭與勾選 SVG 是專案內的簡單圖形，依本專案授權提供。Twitch 分類封面從 Twitch CDN 取得，不屬本專案原創資產，也不以本專案 GPL 重新授權。
