This sketch uses the Arduino IDE to program an ESP32 to collect temperature and humidity data from a Sensiron SHT30.

It then publishes the data to a MQTT broker and serves the data using the Modbus TCP protocol.

Several values are retrieved from privateInfo.h (not included in this repo).

Wi-Fi:
```
const char *wifiSsid = "MyAP";
const char *wifiPassword = "nunya";
```
MQTT broker:
```
const char *mqttBroker = "AmazingBrokerName";
const int mqttPort = 1883;
const char *mqttUsername = "GuestHorse";
const char *mqttPassword = "CorrectHorseBatteryStaple";
```
Once this is running on the network, a FairCom Edge server can harvest its data using Modbus via this input:

```json
{
  "api": "hub",
  "action": "createInput",
  "params": {
    "inputName": "ESP32_SHT30_Modbus",
    "serviceName": "modbus",
    "tableName": "esp32_sht30_data",
    "dataCollectionIntervalMilliseconds": 15000,
    "settings": {
      "modbusProtocol": "TCP",
      "modbusServer": "esp32-sht30-modbus",
      "modbusServerPort": 502,
      "propertyMapList": [
        {
          "propertyPath": "temperature_int",
          "modbusDataAccess": "holdingregister",
          "modbusDataAddress": 100,
          "modbusDataType": "int16SignedAB"
        },
        {
          "propertyPath": "temperature_frac",
          "modbusDataAccess": "holdingregister",
          "modbusDataAddress": 101,
          "modbusDataType": "int16SignedAB"
        },
        {
          "propertyPath": "humidity_percent",
          "modbusDataAccess": "holdingregister",
          "modbusDataAddress": 102,
          "modbusDataType": "int16SignedAB"
        }
      ]
    }
  },
  "authToken": ""
}
```

A typical output on the serial port will look like this:
```
C:\Users\MyUserName\Documents\Code\Arduino\ESP32-SHT30-Modbus-MQTT\ESP32-SHT30-Modbus-MQTT.ino
Print count 9

Network stats:
  MAC address: 01:23:45:AB:CD:EF
  Hostname: esp32-sht30-modbus
  IP address: 10.0.0.2
  RSSI: -67
~~IP address: 10.0.0.2
  wifiConnectCount: 1
  wifiCoolDownInterval: 10000
  Wi-Fi status text: Connected
  Wi-Fi status code: 3

MQTT stats:
  mqttConnectCount: 1
  mqttCoolDownInterval: 10000
  Broker: AmazingBrokerName:1883
  MQTT state: Connected
  Publish count: 0
  Callback count: 0

Environmental stats:
  SHT30 tempC: 23.623335
  SHT30 tempF: 74.522003
  SHT30 humidity: 38.410000
  Invalid readings: 0
Next print in 7 seconds.

Published '23.623335' to 'AdamsEspArmada/esp32-sht30-modbus/sht30/tempC'.
Published '74.522003' to 'AdamsEspArmada/esp32-sht30-modbus/sht30/tempF'.
Published '38.410000' to 'AdamsEspArmada/esp32-sht30-modbus/sht30/humidity'.
Published '-67' to 'AdamsEspArmada/esp32-sht30-modbus/rssi'.
Published 'esp32-sht30-modbus' to 'AdamsEspArmada/esp32-sht30-modbus/hostname'.
Published '01:23:45:AB:CD:EF' to 'AdamsEspArmada/esp32-sht30-modbus/mac'.
Published '10.250.250.72' to 'AdamsEspArmada/esp32-sht30-modbus/ip'.
Published '1' to 'AdamsEspArmada/esp32-sht30-modbus/wifiCount'.
Published '10000' to 'AdamsEspArmada/esp32-sht30-modbus/wifiCoolDownInterval'.
Published '1' to 'AdamsEspArmada/esp32-sht30-modbus/mqttCount'.
Published '10000' to 'AdamsEspArmada/esp32-sht30-modbus/mqttCoolDownInterval'.
Published '0' to 'AdamsEspArmada/esp32-sht30-modbus/publishCount'.
Next publish in 20 seconds.
```
By running a terminal program on the COM port, I can easily see what code the connected device is running, the hostname it is using, the MAC address, and the IPv4 address.  This helps figure out what a years-old neglected device is doing.

The tildes are from a debug session where I was trying different ways to get the IPv4 address.

The "AdamsEspArmada" is because I use many ESP32 boards to assault my network with real-world data.
