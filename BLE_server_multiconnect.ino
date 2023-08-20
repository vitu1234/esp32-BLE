#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <string>

/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updated by chegewara

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
   And has a characteristic of: beb5483e-36e1-4688-b7f5-ea07361b26a8

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   A connect hander associated with the server starts a background task that performs notification
   every couple of seconds.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"



BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

// Timer variables
unsigned long previousMillis = 0;    // will store last time DHT was updated

// Updates DHT readings every 10 seconds
const long interval = 10000;  

float temp;
float tempF;
float hum;

#define DHTPIN 5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Temperature Characteristic and Descriptor
// #ifdef temperatureCelsius
  BLECharacteristic bmeTemperatureCelsiusCharacteristics("cba1d466-344c-4be3-ab3f-189f80dd7518", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor bmeTemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2901));
// #else
  BLECharacteristic bmeTemperatureFahrenheitCharacteristics("f78ebbff-c8b7-4107-93de-889a6a06d408", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor bmeTemperatureFahrenheitDescriptor(BLEUUID((uint16_t)0x2901));
// #endif

// Humidity Characteristic and Descriptor
BLECharacteristic bmeHumidityCharacteristics("171e3016-005a-4a55-b218-a39806dab82b", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeHumidityDescriptor(BLEUUID((uint16_t)0x2902));


//old code 0x0540
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};



void setup() {
  Serial.begin(115200);

  dht.begin();


  // Create the BLE Device
  BLEDevice::init("ESP32");
  
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  // pCharacteristic = pService->createCharacteristic(
  //                     CHARACTERISTIC_UUID,
  //                     BLECharacteristic::PROPERTY_READ   |
  //                     BLECharacteristic::PROPERTY_WRITE  |
  //                     BLECharacteristic::PROPERTY_NOTIFY |
  //                     BLECharacteristic::PROPERTY_INDICATE
  //                   );
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
//  pCharacteristic->addDescriptor(new BLE2902());

    // Humidity
  pService->addCharacteristic(&bmeHumidityCharacteristics);
  bmeHumidityDescriptor.setValue("Humidity");
  bmeHumidityCharacteristics.addDescriptor(&bmeHumidityDescriptor);

  // #ifdef temperatureCelsius
    pService->addCharacteristic(&bmeTemperatureCelsiusCharacteristics);
    bmeTemperatureCelsiusDescriptor.setValue("Temperature Celsius");
    bmeTemperatureCelsiusCharacteristics.addDescriptor(&bmeTemperatureCelsiusDescriptor);
  // #else
    pService->addCharacteristic(&bmeTemperatureFahrenheitCharacteristics);
    bmeTemperatureFahrenheitDescriptor.setValue("Temperature Fahrenheit");
    bmeTemperatureFahrenheitCharacteristics.addDescriptor(&bmeTemperatureFahrenheitDescriptor);
  // #endif  


  

 

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  

  // uint16_t testCompanyIdentifier = 0x1234; // Replace with your desired value
  // String manufacturerData = "DCN Lab";
  // String customData = String(testCompanyIdentifier, HEX) + manufacturerData;

  // std::string stdString = customData.c_str();
  // BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

  // oAdvertisementData.setManufacturerData(stdString);
  // pAdvertising->setAdvertisementData(oAdvertisementData);
  // oAdvertisementData.setName("ESP32");
  // oAdvertisementData.setShortName("ESP32");

  
  

  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    // notify changed value
    if (deviceConnected) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
          previousMillis = currentMillis;
          
          // Read humidity
          hum = dht.readHumidity();
          temp = dht.readTemperature();
          tempF = 1.8*temp +32;
          if (isnan(temp) || isnan(hum)) {
            Serial.println("ERROR: Failed to read from DHT sensor!");
            return;
          }

                    //Notify humidity reading from BME
          static char humidityTemp[6];
          dtostrf(hum, 6, 2, humidityTemp);
          //Set humidity Characteristic value and notify connected client
          bmeHumidityCharacteristics.setValue(humidityTemp);
          bmeHumidityCharacteristics.notify();   
          Serial.print(" - Humidity: ");
          Serial.print(hum);
          Serial.println(" %");

          // #ifdef temperatureCelsius
              static char temperatureCTemp[6];
              dtostrf(temp, 6, 2, temperatureCTemp);
              //Set temperature Characteristic value and notify connected client
              bmeTemperatureCelsiusCharacteristics.setValue(temperatureCTemp);
              bmeTemperatureCelsiusCharacteristics.notify();
              Serial.print("Temperature Celsius: ");
              Serial.print(temp);
              Serial.print(" ºC");
            // #else
              static char temperatureFTemp[6];
              dtostrf(tempF, 6, 2, temperatureFTemp);
              //Set temperature Characteristic value and notify connected client
              bmeTemperatureFahrenheitCharacteristics.setValue(temperatureFTemp);
              bmeTemperatureFahrenheitCharacteristics.notify();
              Serial.print("Temperature Fahrenheit: ");
              Serial.print(tempF);
              Serial.print(" ºF");
            // #endif
        }

        // pCharacteristic->setValue((uint8_t*)&value, 4);
        // pCharacteristic->notify();
        value++;
        delay(100); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}
