#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

Adafruit_AHTX0 aht;

const int pinSDA = 8;
const int pinSCL = 9;
const int pinLight = 2;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

void setup() {
  Serial.begin(115200);
  Wire.begin(pinSDA, pinSCL);
  aht.begin();
  analogReadResolution(12);

  BLEDevice::init("ESP32_Salon_Klimat"); // ZMIENIONA NAZWA
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
                    
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("Klimat gotowy!");
}

void loop() {
  if (deviceConnected) {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    int rawLight = analogRead(pinLight);

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%.1f,%.1f,%d", temp.temperature, humidity.relative_humidity, rawLight);

    pCharacteristic->setValue(buffer);
    pCharacteristic->notify();
    delay(5000); 
  }

  if (!deviceConnected && oldDeviceConnected) {
      delay(500);
      pServer->startAdvertising(); 
      oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
}