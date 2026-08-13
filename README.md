This sketch uses the Arduino IDE to program an ESP32 to collect temperature and humidity data from a Sensiron SHT30.
It then publishes the data to a MQTT broker and serves the data using the Modbus TCP protocol.
Several values are retrieved from privateInfo.h.
Wi-Fi:
   SSID
   password
MQTT broker:
   address
   port
   username
   password

Once this is running on the network, you can harvest its data using this input:

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
