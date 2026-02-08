#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// RTOS LIBRARY
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// --- CẤU HÌNH ---
const char* ssid = "hung";
const char* password = "12345678";

#define PIR_PIN 14
#define SERVO_PIN 13

#define SCREEN_TIMEOUT 5000
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// Reset pin để -1 nếu dùng chung nguồn reset với ESP32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WebServer server(80);
Servo myservo;

// RTOS Objects
QueueHandle_t servoQueue;
SemaphoreHandle_t oledMutex;

struct OpenCommand {
  char name[20];
};

// ============================================================
// HÀM HỖ TRỢ HIỂN THỊ AN TOÀN (GỌI ĐƯỢC TỪ MỌI NƠI)
// ============================================================
void showMessage(const char* line1, const char* line2, int size1 = 1, int size2 = 2) {
  // Cố gắng lấy chìa khóa OLED trong 100ms, nếu k được thì bỏ qua (tránh treo)
  if (xSemaphoreTake(oledMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    display.setTextSize(size1);
    display.setCursor(0, 0);
    display.println(line1);
    
    display.setTextSize(size2);
    display.setCursor(0, 20);
    display.println(line2);
    
    display.display();
    xSemaphoreGive(oledMutex); // Trả chìa khóa
  }
}

// ============================================================
// CÁC HANDLER CỦA SERVER
// ============================================================
void handleCheckPIR() {
  int val = digitalRead(PIR_PIN);
  server.send(200, "text/plain", val == HIGH ? "1" : "0");
}

void handleOpen() {
  String nameStr = "Khach";
  if (server.hasArg("name")) nameStr = server.arg("name");
  
  OpenCommand cmd;
  nameStr.toCharArray(cmd.name, 20);
  
  // Gửi lệnh vào hàng đợi
  xQueueSend(servoQueue, &cmd, 0);
  server.send(200, "text/plain", "CMD_SENT");
}

void handleScan() {
  showMessage("HE THONG:", "DANG QUET...");
  server.send(200, "text/plain", "OK");
}

void handleFail() {
  // Đóng gói lệnh báo lỗi gửi cho Task xử lý
  OpenCommand cmd;
  // Đặt một cái tên đặc biệt để nhận biết là lỗi
  strcpy(cmd.name, "!FAIL"); 
  
  // Gửi vào hàng đợi
  xQueueSend(servoQueue, &cmd, 0);
  
  // Phản hồi cho Python biết
  server.send(200, "text/plain", "FAIL_SENT");
}

// ============================================================
// TASK SERVO (CHẠY CORE 1)
// ============================================================
void TaskServoControl(void *parameter) {
  OpenCommand rcvCmd;
  for (;;) {
    // Chờ lệnh từ hàng đợi (Ngủ đông cho đến khi có lệnh)
    if (xQueueReceive(servoQueue, &rcvCmd, portMAX_DELAY) == pdTRUE) {
      
      // --- KIỂM TRA: ĐÂY LÀ LỆNH LỖI HAY LỆNH MỞ CỬA? ---
      
      if (strcmp(rcvCmd.name, "!FAIL") == 0) {
        // === TRƯỜNG HỢP 1: BÁO LỖI ===
        if (xSemaphoreTake(oledMutex, portMAX_DELAY)) {
           display.clearDisplay();
           display.setTextSize(2);
           display.setCursor(0, 0);
           display.println("CANH BAO:"); // Dòng 1
           display.println("KHONG NHAN DIEN DUOC"); // Dòng 2
           display.display();
           xSemaphoreGive(oledMutex);
        }
        
        // Giữ màn hình cảnh báo trong 5 giây (5000ms)
        vTaskDelay(5000 / portTICK_PERIOD_MS); 
        
      } else {
        // === TRƯỜNG HỢP 2: MỞ CỬA (NGƯỜI QUEN) ===
        
        // 1. Hiện tên
        if (xSemaphoreTake(oledMutex, portMAX_DELAY)) {
           display.clearDisplay();
           display.setTextSize(1);
           display.setCursor(0, 0);
           display.println("XIN CHAO:");
           display.setTextSize(2);
           display.setCursor(0, 20);
           display.println(rcvCmd.name); // Hiện tên người
           display.display();
           xSemaphoreGive(oledMutex);
        }

        // 2. Quay Servo mở chốt
        myservo.attach(SERVO_PIN);
        myservo.write(180); 
        
        // Giữ cửa mở trong 8 giây (8000ms)
        vTaskDelay(8000 / portTICK_PERIOD_MS);
        
        // 3. Đóng Servo lại
        myservo.write(0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        myservo.detach(); 
      }
      
      // === BƯỚC CUỐI CÙNG (CHUNG CHO CẢ 2): TẮT MÀN HÌNH ===
      // Sau khi xong việc (dù là lỗi hay mở cửa), ĐỀU PHẢI TẮT MÀN HÌNH
      if (xSemaphoreTake(oledMutex, portMAX_DELAY)) {
        display.clearDisplay(); // Xóa sạch bộ nhớ hiển thị
        display.display();      // Màn hình đen thui
        xSemaphoreGive(oledMutex);
      }
    }
  }
}

// ============================================================
// SETUP - PHẦN QUAN TRỌNG NHẤT
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100); // Đợi nguồn ổn định
  
  Serial.println("\n--- BAT DAU KHOI TAO ---");

  // 1. Tạo RTOS Objects
  servoQueue = xQueueCreate(5, sizeof(OpenCommand));
  oledMutex = xSemaphoreCreateMutex();

  // 2. KHOI TAO I2C THU CONG (QUAN TRỌNG)
  // SDA = 21, SCL = 22
  Wire.begin(21, 22); 
  
  // 3. KHOI TAO OLED
  // Thử khởi tạo, nếu lỗi thì in ra Serial
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("ERROR: Khong tim thay OLED! Kiem tra day noi."));
    // Nếu lỗi, thử lại lần nữa với địa chỉ 0x3D (một số màn hình dùng địa chỉ này)
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
       Serial.println(F("ERROR: Van khong duoc. Chet man hinh hoac sai day."));
    }
  } else {
    Serial.println(F("OLED Init OK!"));
  }

  // 4. TEST MAN HINH NGAY LAP TUC
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("Khoi tao...");
  display.setCursor(0, 30);
  display.println("Vui long doi");
  display.display();
  delay(1000);

  // 5. Khoi tao Servo & PIR
  pinMode(PIR_PIN, INPUT);
  myservo.attach(SERVO_PIN);
  myservo.write(0);
  delay(500);
  myservo.detach();

  // 6. Ket noi Wifi
  Serial.print("Connecting to Wifi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWifi Connected!");
  Serial.println(WiFi.localIP());

  // 7. Cấu hình Server
  server.on("/check_pir", handleCheckPIR);
  server.on("/scan", handleScan);
  server.on("/open", handleOpen);
  server.on("/fail", handleFail);
  server.begin();

  // 8. TẠO TASK (CHẠY CÙNG CORE 1 VỚI ARDUINO)
  xTaskCreatePinnedToCore(
    TaskServoControl,   
    "ServoTask",        
    8192,               // Tăng Stack lên 8KB 
    NULL,               
    1,                  
    NULL,               
    1 // Chạy Core 1
  );

  Serial.println("HE THONG DA SANG SANG!");
  showMessage("HE THONG:", "DA SAN    SANG");
}

void loop() {
  server.handleClient();
  delay(2); // Nhường CPU chút xíu cho các task nền
}