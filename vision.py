import serial
import base64
import numpy as np
import cv2
import time

COM_PORT = 'COM3' 
BAUD_RATE = 460800

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=2)
    print(f"已成功連接至 {COM_PORT}，等待 AI 影像串流中...")
except Exception as e:
    print(f"無法開啟 {COM_PORT}。請確認已關閉 Serial Monitor。")
    exit()

prev_time = time.time()

while True:
    try:
        line = ser.readline().decode('utf-8').strip()
        
        if line == "[[FRAME_START]]":
            prob_line = ser.readline().decode('utf-8').strip()
            # 接住 ESP32 傳過來的硬體 FPS
            esp32_fps_line = ser.readline().decode('utf-8').strip() 
            b64_line = ser.readline().decode('utf-8').strip()
            end_line = ser.readline().decode('utf-8').strip()

            if end_line == "[[FRAME_END]]":
                prob = int(prob_line)
                esp32_fps = float(esp32_fps_line) # 將字串轉為浮點數
                
                # 解碼與重組
                img_data = base64.b64decode(b64_line)
                img_np = np.frombuffer(img_data, dtype=np.uint8).reshape((96, 96))
                
                # 放大與轉彩
                img_display = cv2.resize(img_np, (480, 480), interpolation=cv2.INTER_LINEAR)
                img_color = cv2.cvtColor(img_display, cv2.COLOR_GRAY2BGR)
                
                # 判斷是否有人
                if prob > 150:
                    text = f"Person Detected! ({prob}/255)"
                    color = (0, 0, 255) 
                    cv2.rectangle(img_color, (0, 0), (480, 480), color, 4)
                else:
                    text = f"No Person ({prob}/255)"
                    color = (0, 255, 0) 

                cv2.putText(img_color, text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)
                
                # 計算 Python 端的系統 FPS
                current_time = time.time()
                time_diff = current_time - prev_time
                if time_diff > 0:
                    sys_fps = 1.0 / time_diff
                else:
                    sys_fps = 0.0
                prev_time = current_time
                
                # --- 將兩種 FPS 顯示在畫面上進行對比 ---
                
                # 1. 顯示 ESP32 端的純運算 FPS (橘色)
                #cv2.putText(img_color, f"ESP32 FPS: {esp32_fps:.1f}", (300, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 165, 255), 2)
                
                # 2. 顯示 Python 端的系統 FPS (青色)
                #cv2.putText(img_color, f"Python FPS: {sys_fps:.1f}", (300, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
                
                cv2.imshow("ESP32-CAM AI Vision", img_color)
                
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
                    
    except Exception as e:
        pass

ser.close()
cv2.destroyAllWindows()