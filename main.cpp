#include <Arduino.h>
#include <HardwareSerial.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// UART2 для MaixDuino
HardwareSerial maixSerial(2);
const int RX2_PIN   = 16;
const int TX2_PIN   = 17;
const int UART_BAUD = 115200;

// BLE UUID
#define SERVICE_UUID   "12345678-1234-1234-1234-1234567890ab"
#define CHAR_CMD_UUID  "12345678-1234-1234-1234-1234567890ac"
#define CHAR_IMG_UUID  "12345678-1234-1234-1234-1234567890ad"

BLECharacteristic *pCharCmd;
BLECharacteristic *pCharImg;
bool deviceConnected = false;
// Флаг для запроса снимка
volatile bool snapshotRequested = false;

void readImageAndNotify();  // прототип

// ====== BLE Server Callbacks ======
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("[BLE] Клиент подключился");
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("[BLE] Клиент отключился");
  }
};

// ====== BLE Cmd Callback ======
class CmdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String cmd = pCharacteristic->getValue().c_str();
    cmd.trim();
    Serial.printf("[BLE] Получена команда: '%s'\n", cmd.c_str());

    if (cmd.equals("SNAPSHOT")) {
      Serial.println("[ESP32] -> K210: SNAPSHOT");
      maixSerial.println("SNAPSHOT");
      snapshotRequested = true;   // вместо прямого вызова
    }
    else if (cmd.equals("BLUE")) {
      Serial.println("[ESP32] -> K210: BLUE");
      maixSerial.println("BLUE");
    }
    else {
      Serial.printf("[ESP32] Неизвестная команда: '%s'\n", cmd.c_str());
    }
  }
};

// ====== Чтение изображения и рассылка ======
void readImageAndNotify() {
  // --- читаем заголовок ---
  String header = maixSerial.readStringUntil('\n');
  header.trim();
  Serial.printf("[ESP32] Header: %s\n", header.c_str());

  {
    const char* hdr_cstr = header.c_str();
    size_t hdr_len = header.length();
    pCharImg->setValue((uint8_t*)hdr_cstr, hdr_len);
    pCharImg->notify();
    delay(10);
  }

  if (header.startsWith("SIZE:")) {
    // JPEG...
    size_t total = header.substring(5).toInt();
    size_t rem = total;
    const size_t CHUNK = 200;
    uint8_t buf[CHUNK];
    Serial.printf("[ESP32] JPEG, %u bytes\n", total);
    while (deviceConnected && rem > 0) {
      size_t toRead = min(rem, CHUNK);
      size_t len = maixSerial.readBytes(buf, toRead);
      if (!len) { delay(5); continue; }
      pCharImg->setValue(buf, len);
      pCharImg->notify();
      rem -= len;
      delay(5);
    }
    Serial.println("[ESP32] JPEG done");
  }
  else if (header.startsWith("RAW:")) {
    // RAW...
    int c = header.indexOf(',');
    int w = header.substring(4, c).toInt();
    int h = header.substring(c+1).toInt();
    size_t total = (size_t)w*h*2;
    size_t rem = total;
    const size_t CHUNK = 256;
    uint8_t buf[CHUNK];
    Serial.printf("[ESP32] RAW %dx%d, %u bytes\n", w, h, total);
    while (deviceConnected && rem > 0) {
      size_t toRead = min(rem, CHUNK);
      size_t len = maixSerial.readBytes(buf, toRead);
      if (!len) { delay(5); continue; }
      pCharImg->setValue(buf, len);
      pCharImg->notify();
      rem -= len;
      delay(5);
    }
    Serial.println("[ESP32] RAW done");
  }
  else {
    Serial.println("[ESP32] Unknown header");
  }
}

void setup() {
  Serial.begin(115200);
  maixSerial.begin(UART_BAUD, SERIAL_8N1, RX2_PIN, TX2_PIN);
  Serial.printf("UART2 RX=%d TX=%d @%d\n", RX2_PIN, TX2_PIN, UART_BAUD);

  // BLE init
  BLEDevice::init("ESP32_MicArray");
  BLEServer *srv = BLEDevice::createServer();
  srv->setCallbacks(new ServerCallbacks());
  BLEService *svc = srv->createService(SERVICE_UUID);

  pCharCmd = svc->createCharacteristic(CHAR_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCharCmd->setCallbacks(new CmdCallbacks());
  pCharImg = svc->createCharacteristic(CHAR_IMG_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pCharImg->addDescriptor(new BLE2902());

  svc->start();
  BLEAdvertising *adv = srv->getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->start();
  Serial.println("[BLE] Сервис запущен");
}

void loop() {
  if (deviceConnected && snapshotRequested) {
    snapshotRequested = false;
    readImageAndNotify();
  }
  delay(10);
}
