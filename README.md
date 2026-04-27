# ESP32-CAM AI Person Counter with OLED

這個專案使用 ESP32-CAM 結合 TensorFlow Lite Micro，實作邊緣運算 (Edge AI) 的人體偵測功能，並透過 I2C OLED 螢幕即時顯示辨識狀態與累積人數。

## 功能特色
* **邊緣 AI 推論**：內建 TensorFlow Lite 模型，無需依賴網路即可進行人體辨識。
* **人數計算**：具備狀態鎖定機制，避免同一個人在畫面前造成重複計數。
* **即時狀態顯示**：整合 0.96 吋 SSD1306 OLED，顯示 AI 狀態與總人數。

## 硬體接線
* **核心板**：AI Thinker ESP32-CAM
* **顯示器**：0.96" I2C OLED (SSD1306)


## 軟體環境與依賴庫
本專案使用 **PlatformIO** 開發，請確保 `platformio.ini` 包含以下設定：
* 開啟 PSRAM：`-DBOARD_HAS_PSRAM`
* `Adafruit SSD1306`
* `Adafruit GFX Library`

## 快速啟動
1. 確保接線正確。
2. 透過 PlatformIO 編譯並燒錄至 ESP32-CAM。
3. 開啟 Serial Monitor (Baud Rate: 460800) 查看初始化狀態，或執行電腦端的 Python 接收程式查看即時影像。
