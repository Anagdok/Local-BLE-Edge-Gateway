#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/i2s.h>

// NOWE, UNIKALNE UUID DLA TEJ PŁYTKI
#define AUDIO_SERVICE_UUID  "11111111-1fb5-459e-8fcc-c5c9c331914b"
#define NOISE_CHAR_UUID     "22222222-36e1-4688-b7f5-ea07361b26a8"
#define LED_CHAR_UUID       "33333333-344c-4be3-ab3f-189f80dd7518"

BLEServer* pServer = NULL;
BLECharacteristic* pNoiseChar = NULL;
BLECharacteristic* pLedChar = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Wbudowana dioda na ESP32-32D 
const int pinLED = 2; 

const int I2S_WS = 25;
const int I2S_SCK = 26;
const int I2S_SD = 27;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

class LedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue().c_str();
      if (value.length() > 0) {
        if (value == "ON") { digitalWrite(pinLED, HIGH); } 
        else if (value == "OFF") { digitalWrite(pinLED, LOW); }
      }
    }
};

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}


unsigned long lastBLEUpdateTime = 0; // Stoper dla Bluetooth
long maxRmsSinceLastUpdate = 0;      // Zapamiętuje najgłośniejszy strzał

void setup() {
  Serial.begin(115200);
  pinMode(pinLED, OUTPUT);
  digitalWrite(pinLED, LOW); // Domyślnie zgaszona
  setupI2S();

  BLEDevice::init("ESP32_Salon_Audio"); // ZMIENIONA NAZWA
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(AUDIO_SERVICE_UUID);
  
  pNoiseChar = pService->createCharacteristic(NOISE_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pNoiseChar->addDescriptor(new BLE2902());

  pLedChar = pService->createCharacteristic(LED_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  pLedChar->setCallbacks(new LedCallbacks());
                    
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(AUDIO_SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("Audio & LED gotowe!");
}


void loop() {
  if (deviceConnected) {
    size_t bytesIn = 0;
    int32_t sampleBuffer[256];
    
    // Odczyt z mikrofonu
    esp_err_t result = i2s_read(I2S_NUM_0, &sampleBuffer, sizeof(sampleBuffer), &bytesIn, portMAX_DELAY);

    if (result == ESP_OK && bytesIn > 0) {
      int samplesRead = bytesIn / sizeof(int32_t);
      long sumAbsolute = 0;

      for (int i = 0; i < samplesRead; i++) {
        // Przesuwamy bity o 14 miejsc w prawo. 
        // Ucinamy błędy i konwertujemy gigantyczne liczby na małe i bezpieczne.
        long val = sampleBuffer[i] >> 14; 
        
        // Dodajemy wartość bezwzględną (abs) zamiast potęgowania! Brak ryzyka przepełnienia (NaN).
        sumAbsolute += abs(val); 
      }
      
      long currentVolume = sumAbsolute / samplesRead;

      // Szukamy najgłośniejszego momentu
      if (currentVolume > maxRmsSinceLastUpdate) {
        maxRmsSinceLastUpdate = currentVolume;
      }
    }

    // Wysyłka przez Bluetooth co 2 sekundy
    if (millis() - lastBLEUpdateTime >= 2000) {
      
      // Teraz surowa głośność to bezpieczne liczby (np. od 0 do 500).
      // Zmieniliśmy 50000 na 500 jako górny limit!
      int noise = map(maxRmsSinceLastUpdate, 0, 5000, 0, 100);
      noise = constrain(noise, 0, 100);

      char buffer[10];
      snprintf(buffer, sizeof(buffer), "%d", noise);

      pNoiseChar->setValue(buffer);
      pNoiseChar->notify();
      
      //WAŻNE: Podgląd na żywo, abyśmy wiedzieli co się dzieje
      Serial.print("Surowy Dzwiek: ");
      Serial.print(maxRmsSinceLastUpdate);
      Serial.print("  >>>  Zmapowany na %: ");
      Serial.println(noise);

      // Reset zmiennych
      maxRmsSinceLastUpdate = 0;
      lastBLEUpdateTime = millis();
    }
  }

  // Obsługa połączenia BLE
  if (!deviceConnected && oldDeviceConnected) {
      delay(500);
      pServer->startAdvertising(); 
      oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
}