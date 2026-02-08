import cv2
import os
import time

# --- CẤU HÌNH ---
# Thay IP của ESP32-CAM (Con quay phim) vào đây
ESP32_CAM_IP = "172.20.10.14" 
#URL_STREAM = f"http://172.20.10.14/"
URL_STREAM = f"http://172.20.10.14:81/stream"

# Tên thư mục lưu ảnh
DATASET_DIR = "dataset"
if not os.path.exists(DATASET_DIR):
    os.makedirs(DATASET_DIR)

print(f"--- TOOL ĐĂNG KÝ KHUÔN MẶT ---")
print(f"1. Nhập tên người muốn đăng ký (Không dấu).")
print(f"2. Nhìn vào Camera -> Nhấn 'S' để lưu.")
print(f"3. Nhấn 'Q' để thoát.")

user_name = input(">> Nhập tên người dùng: ")
file_name = f"{user_name}.jpg"
save_path = os.path.join(DATASET_DIR, file_name)

cap = cv2.VideoCapture(URL_STREAM)

while True:
    ret, frame = cap.read()
    if not ret:
        print("Mất kết nối Camera...")
        break

    # Hiển thị hướng dẫn
    display = frame.copy()
    cv2.putText(display, f"Save: 'S' - Name: {user_name}", (10, 30), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
    
    cv2.imshow("Enrollment Tool", display)
    key = cv2.waitKey(1) & 0xFF

    if key == ord('s'):
        # Lưu ảnh
        cv2.imwrite(save_path, frame)
        print(f"[OK] Đã lưu ảnh: {save_path}")
        print("Đang thoát...")
        time.sleep(1)
        break
    elif key == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()