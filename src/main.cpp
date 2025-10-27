#include <Arduino.h>
#include "secrets.h" // Create and include this file with your WiFi and Telegram credentials
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h> 
#include <ArduinoOTA.h>
#include <time.h>

// ==== WiFi Credentials ====
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASSWORD;

// ==== Telegram ====
const char* telegramBotToken = SECRET_BOT_TOKEN; // Get from BotFather
const char* chatID = SECRET_CHAT_ID; // Get from @userinfobot, or here: https://api.telegram.org/bot<YourBOTToken>/getUpdates


// ==== Pins ====
#define RELAY_PIN 16   //controls the relay to unlock the door
#define LED_PIN 15 // Built-in LED, 2 on regular ESP32 board
#define KEY_PIN 12  // GPIO keeping the charging module on
#define KICK_INTERVAL 10000  // milliseconds

unsigned int SHUTOFF_TIME = 60000; // 1 minute
const unsigned int PULSE_DURATION = 100;
unsigned long lastKickTime = 0;

WiFiClientSecure wifiClient;
UniversalTelegramBot bot(telegramBotToken, wifiClient);

unsigned long lastHealthReport = 0;

void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Trying to connect...");
    Serial.println(WiFi.status());
    delay(500);
  }
}

void sendHealthReport() {
  String report = "Device Health Report:\n";
  report += "Free Heap: " + String(ESP.getFreeHeap()) + "\n";
  report += "Min Free Heap: " + String(ESP.getMinFreeHeap()) + "\n";
  report += "Max Alloc Heap: " + String(ESP.getMaxAllocHeap()) + "\n";
  bot.sendMessage(chatID, report, "");
  lastHealthReport = millis();
}

void checkTelegramMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String text = bot.messages[i].text;
      String chat_id = bot.messages[i].chat_id;
      if (chat_id == chatID) {
        if (text == "/unlock") {
          digitalWrite(RELAY_PIN, HIGH);
          bot.sendMessage(chatID, "Door is unlocked", "");
          delay(4000);
          digitalWrite(RELAY_PIN, LOW);
        } else if (text == "/charging") {
          SHUTOFF_TIME += 3600000; // extend by 1 hour, 3600000
          bot.sendMessage(chatID, "Extended shutoff time by 60 minutes", "");
        } else if (text == "/resume") {
          SHUTOFF_TIME = 60000; // 1 minute
          bot.sendMessage(chatID, "Shutting off in 1 minute", "");
        } else if (text == "/health") {
          sendHealthReport();
        } 
      }
      bot.last_message_received = bot.messages[i].update_id;
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

// Function to keep the charging module ON by pulsing the KEY_PIN
void keepOn() {
  // Pulse LOW for 100 ms
  digitalWrite(KEY_PIN, LOW);
  delay(PULSE_DURATION);
  digitalWrite(KEY_PIN, HIGH);
}

void turnOff() {
    digitalWrite(KEY_PIN, HIGH);
    delay(PULSE_DURATION);
    digitalWrite(KEY_PIN, LOW);
}

void setup() {

  Serial.begin(115200);

  pinMode(KEY_PIN, OUTPUT);
  digitalWrite(KEY_PIN, HIGH);  

  pinMode(RELAY_PIN, OUTPUT);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Turn on LED when awake

  connectWiFi();
  wifiClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  bot.sendMessage(chatID, "Door Buzzed!\n /unlock ?", "");

  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname("MINI-ESP32-SmartBuzzer");
  ArduinoOTA.onStart([]() { Serial.println("Start OTA update"); });
  ArduinoOTA.onEnd([]() { Serial.println("End OTA update"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("OTA Progress: %u%%\r", (p / (t / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

}

void loop() {
  ArduinoOTA.handle();

// === Reconnect WiFi if needed ===
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  checkTelegramMessages();

  unsigned long currentTime = millis();
  // Only kick watchdog if we're still within SHUTOFF_TIME
  // Check if it's time to kick the watchdog
  if (currentTime < SHUTOFF_TIME) {
    if (currentTime - lastKickTime >= KICK_INTERVAL ) {
      Serial.println("Kicking watchdog...");
      keepOn();
      lastKickTime = currentTime;
    }
} else {
    Serial.println("Turning off...");
    turnOff();
    delay(60000);
}

  delay(10); 

}


