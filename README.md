# Hệ thống nhận diện khuôn mặt chấm công/điểm danh (ESP32 + Python)

Dự án triển khai mô hình chấm công/điểm danh bằng nhận diện khuôn mặt, kết hợp:

- **ESP32-CAM**: cung cấp luồng video (MJPEG stream).
- **ESP32 (Control)**: đọc **cảm biến PIR**, điều khiển **servo** (chốt cửa/thiết bị), hiển thị **OLED**.
- **PC/Server Python**: chạy **Streamlit** để nhận diện khuôn mặt và ghi lịch sử chấm công.

## 1) Kiến trúc tổng quan

Luồng hoạt động chính (đúng theo code hiện có):

1. ESP32 (Control) đọc PIR.
2. Ứng dụng Streamlit trên PC **poll** endpoint `/check_pir`.
3. Khi PIR = HIGH, Streamlit lấy khung hình từ **ESP32-CAM** (`http://<cam-ip>:81/stream`) và chạy nhận diện.
4. Nếu **nhận diện thành công** → Streamlit gọi `/open?name=...` để ESP32:
	 - Hiển thị tên + giờ lên OLED.
	 - Mở servo trong ~8 giây rồi đóng lại.
5. Nếu **thất bại** → Streamlit gọi `/fail` để ESP32 hiển thị cảnh báo.
6. Trên giao diện Streamlit có luồng **đăng ký khuôn mặt mới** (chụp 3 góc: thẳng/trái/phải) và lưu vào thư mục `dataset/`.

## 2) Thành phần & thư mục

- `RTOS2.ino`: firmware cho **ESP32 Control** (PIR + Servo + OLED + WebServer).
- `template/app.py`: Streamlit UI + nhận diện bằng thư viện `face_recognition`.
- `template/recognitionFace.py`: gần giống `app.py` (cùng logic nhận diện/đăng ký).
- `template/enroll.py`: tool đăng ký nhanh 1 ảnh (nhấn `S` để lưu).
- `template/InsightFace.py`: phiên bản nhận diện dùng **InsightFace** (cần cài thêm dependency, xem mục 6).
- `dataset/`: nơi lưu ảnh khuôn mặt đã đăng ký.
- `logs/chamcong.csv`: file log mẫu (hiện tại UI lưu log trong session; chưa có đoạn ghi CSV tự động).

## 3) Phần cứng (ESP32 Control)

Theo cấu hình trong `RTOS2.ino`:

- PIR: `PIR_PIN = 14`
- Servo: `SERVO_PIN = 13`
- OLED I2C: `SDA = 21`, `SCL = 22`, địa chỉ thường `0x3C` (code có thử `0x3D` nếu không thấy)

Thư viện Arduino cần cài (Arduino IDE/PlatformIO):

- `WiFi` (core ESP32)
- `WebServer` (core ESP32)
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `ESP32Servo`

## 4) API ESP32 (Control)

ESP32 Control mở WebServer cổng `80` với các endpoint:

- `GET /check_pir` → trả về `"1"` nếu PIR HIGH, ngược lại `"0"`.
- `GET /open?name=<text>` → hiển thị lời chào và mở servo.
- `GET /fail` → hiển thị cảnh báo không nhận diện được.
- `GET /scan` → hiển thị trạng thái “đang quét” (hiện UI Python chưa gọi endpoint này).

## 5) Cài đặt & chạy (Python/Streamlit)

### 5.1 Yêu cầu

- Windows (đang dùng trong repo này) + Python **3.10** (khuyến nghị, vì requirements đang có wheel `cp310`).
- 2 thiết bị cùng mạng LAN/Wi‑Fi:
	- **ESP32-CAM** (stream video)
	- **ESP32 Control** (PIR/servo/OLED)

### 5.2 Cài dependency

Nếu bạn dùng `pip`:

```bash
pip install -r requirements.txt
```

Nếu bạn dùng Conda (đang giống môi trường của bạn):

```bash
conda create -n face_id python=3.10 -y
conda activate face_id
pip install -r requirements.txt
```

### 5.3 Cấu hình IP thiết bị

Trong các file Streamlit, sửa 2 biến:

- `ESP32_CAM_IP` (IP của ESP32‑CAM)
- `ESP32_CONTROL_IP` (IP của ESP32 Control)

Bạn sẽ thấy ở đầu file:

- `template/app.py`
- `template/recognitionFace.py`
- `template/InsightFace.py`

Ví dụ mặc định trong code:

- `ESP32_CAM_IP = "172.20.10.14"`
- `ESP32_CONTROL_IP = "172.20.10.2"`

### 5.4 Chạy giao diện chấm công

Chạy bản dùng `face_recognition` (khớp với `requirements.txt`):

```bash
streamlit run template/app.py
```

Sau đó mở URL Streamlit được in ra (thường là `http://localhost:8501`).

## 6) Nạp firmware cho ESP32

### 6.1 ESP32 Control (PIR/Servo/OLED)

1. Mở `RTOS2.ino`.
2. Sửa Wi‑Fi:

```cpp
const char* ssid = "hung";
const char* password = "12345678";
```

3. Chọn đúng board ESP32 và COM port → Upload.
4. Mở Serial Monitor (115200) để xem IP được cấp (`WiFi.localIP()`).

### 6.2 ESP32‑CAM (stream video)

Repo hiện **chưa kèm** file `.ino` cho ESP32‑CAM. Bạn cần nạp firmware ESP32‑CAM có MJPEG stream tại:

- `http://<ESP32_CAM_IP>:81/stream`

Gợi ý: dùng ví dụ `CameraWebServer` trong Arduino IDE (ESP32 examples), cấu hình đúng model camera (AI Thinker, …) và Wi‑Fi.

## 7) Đăng ký khuôn mặt (Enrolling)

Có 2 cách:

### Cách A — Đăng ký trực tiếp trên Streamlit

- Khi quét thất bại, UI sẽ hỏi “Bạn có muốn đăng ký khuôn mặt mới không?”
- Chọn **Đăng ký ngay**, nhập tên (không dấu, viết liền).
- Hệ thống tự chụp 3 góc và lưu:

`dataset/<ten>_front.jpg`, `dataset/<ten>_left.jpg`, `dataset/<ten>_right.jpg`

### Cách B — Tool `enroll.py` (1 ảnh)

```bash
python template/enroll.py
```

Nhập tên → nhìn vào camera → nhấn `S` để lưu ảnh vào `dataset/`.

## 8) Lưu ý & xử lý lỗi thường gặp

- **Không mở được stream**: kiểm tra URL `http://<cam-ip>:81/stream`, đảm bảo PC và ESP32‑CAM cùng mạng.
- **PIR luôn báo 1/0**: kiểm tra wiring PIR và chân `PIR_PIN = 14`.
- **OLED không lên**: kiểm tra SDA/SCL (21/22), địa chỉ I2C (`0x3C`/`0x3D`) và nguồn.
- **Servo rung/không đủ lực**: cấp nguồn servo riêng (GND chung với ESP32), tránh lấy trực tiếp từ 3.3V.

## 9) (Tuỳ chọn) Chạy bản InsightFace

File `template/InsightFace.py` dùng InsightFace (nhẹ và thường ổn định hơn). Tuy nhiên `requirements.txt` hiện **chưa** khai báo `insightface`/runtime tương ứng.

Nếu muốn dùng bản này, bạn cần cài thêm dependency phù hợp với máy (CPU/GPU). Sau khi cài xong, chạy:

```bash
streamlit run template/InsightFace.py
```