#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

const char* ssid = "Wokwi-GUEST";  
const char* password = "";         
const char* mqtt_server = "broker.emqx.io";

#define DHTPIN 15     
#define DHTTYPE DHT22
#define LED_PIN 26

const char* topic_temp = "esp32/test/temp";
const char* topic_humid = "esp32/test/humid";
const char* topic_led = "esp32/test/led";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastMsg = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Hàm nhận dữ liệu từ Broker (Điều khiển LED) 
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Kiểm tra topic và nội dung để bật tắt LED
  if (String(topic) == topic_led) {
    if (message == "on" || message == "1") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ON");
    } else if (message == "off" || message == "0") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
    }
  }
}

// Hàm kết nối lại MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Đăng ký nhận tin nhắn điều khiển LED
      client.subscribe(topic_led);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // Khởi động DHT và LCD
  dht.begin();
  lcd.init();
  lcd.backlight();
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  // 1. Kiểm tra kết nối MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 2. Gửi dữ liệu định kỳ
  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    
    // Đọc dữ liệu từ cảm biến
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Kiểm tra lỗi đọc cảm biến
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Hiển thị lên LCD 
    lcd.setCursor(0, 0);
    lcd.print("Temp: " + String(t, 2) + "C   ");
    lcd.setCursor(0, 1);
    lcd.print("Humid: " + String(h, 2) + "%   ");

    String jsonPayload = "{\"temp\": " + String(t, 2) + ", \"humid\": " + String(h, 2) + "}";
    
    client.publish("esp32/test/all", jsonPayload.c_str()); 
    
    Serial.println(jsonPayload);
  }
}