#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "SHIWORI trial"

// SHIWORI service UUID
#define USER_SERVICE_UUID "5D97F2C7-C2A5-4937-87AF-6BA97A35803B"
// User service characteristics
#define USER_SERVICE_UUID "6087A279-23B6-4909-840D-E880987AA192"
#define WRITE_CHARACTERISTIC_UUID "23F77884-A41B-45B0-AB06-1FE1424E8760"
#define NOTIFY_CHARACTERISTIC_UUID "4711F5D3-556A-4A3B-9263-CE98424B3B80"

// PSDI Service UUID: Device only reader
#define PSDI_SERVICE_UUID "6DA2DE8E-C374-4447-BBCC-71C678B5191E"
#define PSDI_CHARACTERISTIC_UUID "90824656-5B19-4F06-A057-80824F122A8D"

#define BUTTON 0
#define LED1 5

BLEServer* shiworiServer;
BLESecurity *shiworiSecurity;
BLEService* userService;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;
BLECharacteristic* writeCharacteristic;
BLECharacteristic* notifyCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

volatile int btnAction = 0;

class serverCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
   deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class writeCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *bleWriteCharacteristic) {
    std::string value = bleWriteCharacteristic->getValue();
    if ((char)value[0] <= 1) {
      digitalWrite(LED1, (char)value[0]);
    }
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(LED1, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  attachInterrupt(BUTTON, buttonAction, CHANGE);

  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);

  // Security Settings
  //BLESecurity *shiworiSecurity = new BLESecurity();
  //shiworiSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  //shiworiSecurity->setCapability(ESP_IO_CAP_NONE);
  //shiworiSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");
}

void loop() {
  uint8_t btnValue;

  while (btnAction > 0 && deviceConnected) {
    btnValue = !digitalRead(BUTTON);
    btnAction = 0;
    notifyCharacteristic->setValue(&btnValue, 1);
    notifyCharacteristic->notify();
    delay(20);
  }
  // Disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Wait for BLE Stack to be ready
    shiworiServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
  }
  // Connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}

void setupServices(void) {
  // Create BLE Server
  shiworiServer = BLEDevice::createServer();
  shiworiServer->setCallbacks(new serverCallbacks());

  // Setup User Service
  userService = shiworiServer->createService(USER_SERVICE_UUID);
  // Create Characteristics for User Service
  writeCharacteristic = userService->createCharacteristic(WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  writeCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  writeCharacteristic->setCallbacks(new writeCallback());

  notifyCharacteristic = userService->createCharacteristic(NOTIFY_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  notifyCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  BLE2902* ble9202 = new BLE2902();
  ble9202->setNotifications(true);
  ble9202->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  notifyCharacteristic->addDescriptor(ble9202);

  // Setup PSDI Service
  psdiService = shiworiServer->createService(PSDI_SERVICE_UUID);
  psdiCharacteristic = psdiService->createCharacteristic(PSDI_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  psdiCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // Set PSDI (Product Specific Device ID) value
  uint64_t macAddress = ESP.getEfuseMac();
  psdiCharacteristic->setValue((uint8_t*) &macAddress, sizeof(macAddress));

  // Start BLE Services
  userService->start();
  psdiService->start();
}

void startAdvertising(void) {
  // Start Advertising
  BLEAdvertisementData scanResponseData = BLEAdvertisementData();
  scanResponseData.setFlags(0x06); // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  scanResponseData.setName(DEVICE_NAME);

  shiworiServer->getAdvertising()->addServiceUUID(userService->getUUID());
  shiworiServer->getAdvertising()->setScanResponseData(scanResponseData);
  shiworiServer->getAdvertising()->start();
}

void buttonAction() {
  btnAction++;
}
