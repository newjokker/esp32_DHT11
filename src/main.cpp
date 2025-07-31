#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_sleep.h"

// 屏幕配置
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 40
#define OLED_RESET -1
#define I2C_ADDRESS 0x3C

// DHT11配置
#define DHTPIN 4
#define DHTTYPE DHT11

// WiFi配置
// const char* ssid = "Saturn-Guest-2.4g";
// const char* password = "Tuxingkeji-0918";
const char* ssid = "LDQ-AP";
const char* password = "747225581";

// 目标服务配置
const char* serverUrl = "http://8.153.160.138:12345/iot_data";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);

// #define SLEEP_DURATION 60*30   
#define SLEEP_DURATION 60 * 5 // 5分钟深度睡眠

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  
  Serial.println("尝试连接WiFi...");
  WiFi.disconnect(true);  // 断开之前的连接并释放内存
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 5000) { // 5秒超时
    delay(200);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi已连接");
    return true;
  }
  
  Serial.println("\nWiFi连接失败");
  return false;
}

bool sendToService(float temperature, float humidity) {
  if (!connectWiFi()) return false;

  WiFiClient client;
  HTTPClient http;
  
  // 配置HTTP客户端
  http.setReuse(true);
  http.setTimeout(8000); // 8秒超时
  
  if (!http.begin(client, serverUrl)) {
    Serial.println("HTTP初始化失败");
    return false;
  }
  
  http.addHeader("Content-Type", "application/json");
  
  // 创建JSON数据
  String jsonData = "{\"device_id\":\"ESP32S3_DHT11\"," 
                   "\"temperature\":" + String(temperature) + 
                   ",\"humidity\":" + String(humidity) + "}";
  
  int httpResponseCode = http.POST(jsonData);
  bool success = false;
  
  if (httpResponseCode > 0) {
    Serial.printf("HTTP响应代码: %d\n", httpResponseCode);
    if (httpResponseCode == HTTP_CODE_OK) {
      success = true;
    }
  } else {
    Serial.printf("HTTP请求失败，错误: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
  return success;
}

void displayInfo(float temp, float humidity, bool sending, bool sensorError) {
  display.clearDisplay();
  
  display.setCursor(0, 0);
  display.print("状态: ");
  if (sensorError) {
    display.println("传感器错误");
  } else {
    display.println(sending ? "发送中" : "就绪");
  }
  
  display.setCursor(0, 15);
  display.print("Temp: ");
  if (sensorError) {
    display.println("读取失败");
  } else {
    display.print(temp, 1);
    display.println(" C");
  }
  
  display.setCursor(0, 30);
  display.print("Hum:  ");
  if (sensorError) {
    display.println("读取失败");
  } else {
    display.print(humidity, 1);
    display.println(" %");
  }
    
  display.display();
}

void goToSleep() {
  Serial.println("准备进入深度睡眠...");
  
  // 关闭显示
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  
  // 断开WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  // 配置唤醒源
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000000); // 微秒
  
  // 进入深度睡眠
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(100); // 短暂稳定时间
  
  // 检查是否为从深度睡眠唤醒
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("从深度睡眠唤醒");
  }

  // 初始化I2C
  Wire.begin(38, 37); // SDA=38, SCL=37
  
  // 初始化OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
    Serial.println(F("SSD1306初始化失败"));
    for(;;);
  }
  
  // 屏幕初始设置
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
  
  // 初始化DHT传感器
  dht.begin();
  
  // 读取传感器数据
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  bool sensorError = isnan(h) || isnan(t);
  
  if (sensorError) {
    Serial.println("传感器读取失败! 将上传-1值");
    h = -1;
    t = -1;
  } else {
    Serial.printf("湿度: %.1f%%\t温度: %.1f°C\n", h, t);
  }
  
  displayInfo(t, h, false, sensorError);
  
  // 尝试发送数据
  bool sendSuccess = sendToService(t, h);
  displayInfo(t, h, true, sensorError);
  delay(1000); // 短暂显示发送状态
  
  // 进入深度睡眠
  goToSleep();
}

void loop() {
  // 由于使用了深度睡眠，loop()不会被调用
}