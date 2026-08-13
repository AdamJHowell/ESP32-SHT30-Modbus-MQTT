#include "Adafruit_SHT31.h"      // Driver library for the SHT30. This library includes Wire.h.
#include "PubSubClient.h"        // MQTT client by Nick O'Leary: https://github.com/knolleary/pubsubclient
#include "privateInfo.h"         // Location of Wi-Fi and MQTT settings.
#include <ArduinoJson.h>         // ArduinoJson by Benoît Blanchon: https://arduinojson.org/
#include <ArduinoOTA.h>          // Arduino Over-The-Air updates.
#include <WiFiUdp.h>             // Arduino Over-The-Air updates.
#include <ModbusIP_ESP8266.h>    // Modbus TCP Library

#ifdef ESP8266
#include <ESP8266WiFi.h> 
#include <ESP8266mDNS.h>
const unsigned int LED_ON = 0;
const unsigned int LED_OFF = 1;
#else
#include "ESPmDNS.h" 
#include <WiFi.h>        
const unsigned int LED_ON = 1;
const unsigned int LED_OFF = 0;
#endif

// --- Modbus Definitions ---
ModbusIP mb;
#define REG_TEMP_INT  100 // Integer portion of temperature
#define REG_TEMP_FRAC 101 // Fractional portion of temperature (2 decimal places)
#define REG_HUMIDITY  102 // Integer humidity percentage
#define MAX_MODBUS_CLIENTS 4

char ipAddress[16];                                              
char macAddress[18];                                              
long rssi;                                                            
unsigned int printInterval = 7000;                            
unsigned int publishInterval = 20000;                         
unsigned int wifiConnectCount = 0;                            
unsigned int mqttConnectCount = 0;                            
unsigned int invalidValueCount = 0;                           
unsigned int publishNow = 0;                                      
unsigned int publishFailCount = 0;                            
unsigned long printCount = 0;                                     
unsigned long publishCount = 0;                               
unsigned long callbackCount = 0;                              
unsigned long lastPrintTime = 0;                              
unsigned long lastPublishTime = 0;                            
unsigned long lastWifiConnectTime = 0;                        
unsigned long lastMqttConnectionTime = 0;                     
unsigned long wifiCoolDownInterval = 10000;               
unsigned long mqttCoolDownInterval = 10000;               
unsigned long wifiConnectionTimeout = 15000;              
unsigned long ledBlinkInterval = 200;                         
unsigned long lastLedBlinkTime = 0;                           
const unsigned int ONBOARD_LED = 2;                           
const unsigned int JSON_DOC_SIZE = 512;                   
const char *commandTopic = "AdamsEspArmada/commands";     
const char *topicPrefix = "AdamsEspArmada/";                  
const char *tempCTopic = "sht30/tempC";                   
const char *tempFTopic = "sht30/tempF";
const char *humidityTopic = "sht30/humidity";
const char *hostname = "esp32-sht30-modbus";
const char *hostnameTopic = "hostname";
const char *rssiTopic = "rssi";
const char *macTopic = "mac";
const char *ipTopic = "ip";                                   
const char *wifiCountTopic = "wifiCount";                     
const char *wifiCoolDownTopic = "wifiCoolDownInterval"; 
const char *mqttCountTopic = "mqttCount";                     
const char *mqttCoolDownTopic = "mqttCoolDownInterval"; 
const char *publishCountTopic = "publishCount";
const char *mqttTopic = "espWeather";
float sht30TempCArray[] = { 21.12, 21.12, 21.12 };        
float sht30HumidityArray[] = { 21.12, 21.12, 21.12 };     

Adafruit_SHT31 sht30 = Adafruit_SHT31();
WiFiClient wifiClient;
PubSubClient mqttClient( wifiClient );

/**
 * @brief setupSht30() will initialize the SHT30 temperature and humidity sensor.
 */
void setupSht30()
{
    uint8_t address = 0x44;
    if( !sht30.begin( address ) )
    {
        Serial.printf( "Could not find SHT30 at address %X!\n", address );
        Serial.println( "  Please fix the problem and reboot the device." );
        Serial.println( "  This function is now in an infinite loop." );
        while( true )
            delay( 1000 );
    }

    Serial.print( "SHT30 heater state: " );
    if( sht30.isHeaterEnabled() )
        Serial.println( "ENABLED" );
    else
        Serial.println( "DISABLED" );
}

/**
 * @brief cToF() will convert Celsius to Fahrenheit.
 */
float cToF( float value )
{
    return ( value * 1.8 ) + 32;
}

/**
 * @brief addValue() will add the passed value to the passed array, after moving the existing array values to higher indexes.
 */
void addValue( float valueArray[], float value, float minValue, float maxValue )
{
    if( value < minValue || value > maxValue )
    {
        invalidValueCount++;
        return;
    }
    valueArray[2] = valueArray[1];
    valueArray[1] = valueArray[0];
    valueArray[0] = value;
}

/**
 * @brief averageArray() will return the average of values in the passed array.
 */
float averageArray( float valueArray[] )
{
    const unsigned int arraySize = 3;
    float tempValue = 0;
    for( int i = 0; i < arraySize; ++i )
    {
        tempValue += valueArray[i];
    }
    return tempValue / arraySize;
}

/**
 * @brief toggleLED() will change the state of the LED.
 */
void toggleLED()
{
    if( digitalRead( ONBOARD_LED ) != 1 )
        digitalWrite( ONBOARD_LED, LED_ON );
    else
        digitalWrite( ONBOARD_LED, LED_OFF );
}

/**
 * @brief deviceRestart() will restart the device.
 */
void deviceRestart()
{
    Serial.println( "Restarting in 5 seconds..." );
    delay( 5000 );
    Serial.println( "Restarting the device!" );
    ESP.restart();
}

/**
 * @brief lookupWifiCode() will return the string for an integer code.
 */
void lookupWifiCode( int code, char *buffer )
{
    switch( code )
    {
        case 0: snprintf( buffer, 26, "%s", "Idle" ); break;
        case 1: snprintf( buffer, 26, "%s", "No SSID" ); break;
        case 2: snprintf( buffer, 26, "%s", "Scan completed" ); break;
        case 3: snprintf( buffer, 26, "%s", "Connected" ); break;
        case 4: snprintf( buffer, 26, "%s", "Connection failed" ); break;
        case 5: snprintf( buffer, 26, "%s", "Connection lost" ); break;
        case 6: snprintf( buffer, 26, "%s", "Disconnected" ); break;
        default: snprintf( buffer, 26, "%s", "Unknown Wi-Fi status code" );
    }
}

/**
 * @brief lookupMQTTCode() will return the string for an integer state code.
 */
void lookupMQTTCode( int code, char *buffer )
{
    switch( code )
    {
        case -4: snprintf( buffer, 29, "%s", "Connection timeout" ); break;
        case -3: snprintf( buffer, 29, "%s", "Connection lost" ); break;
        case -2: snprintf( buffer, 29, "%s", "Connect failed" ); break;
        case -1: snprintf( buffer, 29, "%s", "Disconnected" ); break;
        case 0: snprintf( buffer, 29, "%s", "Connected" ); break;
        case 1: snprintf( buffer, 29, "%s", "Bad protocol" ); break;
        case 2: snprintf( buffer, 29, "%s", "Bad client ID" ); break;
        case 3: snprintf( buffer, 29, "%s", "Unavailable" ); break;
        case 4: snprintf( buffer, 29, "%s", "Bad credentials" ); break;
        case 5: snprintf( buffer, 29, "%s", "Unauthorized" ); break;
        default: snprintf( buffer, 29, "%s", "Unknown MQTT state code" );
    }
}

/**
 * @brief checkForSSID() scans for SSIDs.
 */
int checkForSSID( const char *ssidName )
{
    int ssidCount = 0;
    byte networkCount = WiFi.scanNetworks();
    if( networkCount == 0 )
        Serial.println( "No WiFi SSIDs are in range!" );
    else
    {
        Serial.printf( "WiFi SSIDs in range: %d\n", networkCount );
        for( int i = 0; i < networkCount; ++i )
        {
            if( strcmp( ssidName, WiFi.SSID( i ).c_str() ) == 0 )
                ssidCount++;
        }
    }
    return ssidCount;
}

/**
 * @brief wifiConnect() will connect to a SSID.
 */
void wifiConnect()
{
    if( lastWifiConnectTime == 0 || millis() - lastWifiConnectTime >= wifiCoolDownInterval )
    {
        int ssidCount = checkForSSID( wifiSsid );
        if( ssidCount == 0 )
        {
            Serial.printf( "SSID '%s' is not in range!\n", wifiSsid );
            digitalWrite( ONBOARD_LED, LED_OFF );
        }
        else
        {
            wifiConnectCount++;
            digitalWrite( ONBOARD_LED, LED_OFF );

            Serial.printf( "Attempting to connect to Wi-Fi SSID '%s'", wifiSsid );
            WiFi.mode( WIFI_STA );
            
            // Hostnames should contain lowercase ASCII (a-z), numbers (0-9), and hyphens (-).
            WiFi.setHostname( hostname );

            // Initiate the connection FIRST to fully wake the Wi-Fi radio.
            WiFi.begin( wifiSsid, wifiPassword );
            
            // Now that the radio is active, pull the MAC address from the efuse and assign that to the macAddress variable.
            snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );
            
            unsigned long wifiConnectionStartTime = millis();

            while( WiFi.status() != WL_CONNECTED && ( millis() - wifiConnectionStartTime < wifiConnectionTimeout ) )
            {
                Serial.print( "." );
                delay( 1000 );
            }
            Serial.println( "" );

            if( WiFi.status() == WL_CONNECTED )
            {
                Serial.println( "\nWi-Fi connection established!" );
                snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
                digitalWrite( ONBOARD_LED, LED_ON );
                return;
            }
            else
                Serial.println( "Wi-Fi failed to connect in the timeout period.\n" );
        }
        lastWifiConnectTime = millis();
    }
}

/**
 * @brief configureOTA()
 */
void configureOTA()
{
    Serial.println( "Configuring OTA." );

#ifdef ESP8266
    ArduinoOTA.onStart( []() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println( "Start updating " + type );
    } );
#else
    ArduinoOTA.setHostname( hostname );
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    ArduinoOTA.onStart( []() {
        String type = (ArduinoOTA.getCommand() == U_SPIFFS) ? "filesystem" : "flash";
        Serial.print( "OTA is updating the " );
        Serial.println( type );
    } );
#endif
    ArduinoOTA.onEnd( []() { Serial.println( "\nTerminating OTA communication." ); } );
    ArduinoOTA.onProgress( []( unsigned int progress, unsigned int total ) { Serial.printf( "OTA progress: %u%%\r", ( progress / ( total / 100 ) ) ); } );
    ArduinoOTA.onError( []( ota_error_t error ) {
        Serial.printf( "Error[%u]: ", error );
        if( error == OTA_AUTH_ERROR ) Serial.println( "OTA authentication failed!" );
        else if( error == OTA_BEGIN_ERROR ) Serial.println( "OTA transmission failed to initiate properly!" );
        else if( error == OTA_CONNECT_ERROR ) Serial.println( "OTA connection failed!" );
        else if( error == OTA_RECEIVE_ERROR ) Serial.println( "OTA client was unable to properly receive data!" );
        else if( error == OTA_END_ERROR ) Serial.println( "OTA transmission failed to terminate properly!" ); } );

    ArduinoOTA.begin();
    Serial.println( "OTA is configured and ready." );
}

/**
 * @brief readTelemetry() will read the telemetry, save to variables, and update Modbus registers.
 */
void readTelemetry()
{
    rssi = WiFi.RSSI();
    // Add current readings into the appropriate arrays.
    addValue( sht30TempCArray, sht30.readTemperature(), -42, 212 );
    addValue( sht30HumidityArray, sht30.readHumidity(), 0, 100 );

    // Update Modbus Registers
    float avgTempC = averageArray( sht30TempCArray );
    float avgHumidity = averageArray( sht30HumidityArray );

    int16_t tempInt = (int16_t)avgTempC;
    // Calculate fractional part as integer (e.g. 23.45 -> 45)
    uint16_t tempFrac = (uint16_t)(abs(avgTempC - tempInt) * 100.0);
    uint16_t humInt = (uint16_t)avgHumidity;

    mb.Hreg(REG_TEMP_INT, (uint16_t)tempInt);
    mb.Hreg(REG_TEMP_FRAC, tempFrac);
    mb.Hreg(REG_HUMIDITY, humInt);
}

/**
 * @brief printTelemetry() will print the telemetry to the serial port.
 */
void printTelemetry()
{
    Serial.println();
    printCount++;
    Serial.println( __FILE__ );
    Serial.printf( "Print count %ld\n", printCount );
    Serial.println();

    Serial.println( "Network stats:" );
    Serial.printf( "  MAC address: %s\n", macAddress );
    Serial.printf( "  Hostname: %s\n", hostname );
    int wifiStatusCode = WiFi.status();
    char buffer[29];
    lookupWifiCode( wifiStatusCode, buffer );
    if( wifiStatusCode == 3 )
    {
        Serial.printf( "  IP address: %s\n", ipAddress );
        Serial.printf( "  RSSI: %ld\n", rssi );
        Serial.print( "~~IP address: " );
        Serial.println( WiFi.localIP() );
    }
    Serial.printf( "  wifiConnectCount: %u\n", wifiConnectCount );
    Serial.printf( "  wifiCoolDownInterval: %lu\n", wifiCoolDownInterval );
    Serial.printf( "  Wi-Fi status text: %s\n", buffer );
    Serial.printf( "  Wi-Fi status code: %d\n", wifiStatusCode );
    Serial.println();

    Serial.println( "MQTT stats:" );
    Serial.printf( "  mqttConnectCount: %u\n", mqttConnectCount );
    Serial.printf( "  mqttCoolDownInterval: %lu\n", mqttCoolDownInterval );
    Serial.printf( "  Broker: %s:%d\n", mqttBroker, mqttPort );
    lookupMQTTCode( mqttClient.state(), buffer );
    Serial.printf( "  MQTT state: %s\n", buffer );
    Serial.printf( "  Publish count: %lu\n", publishCount );
    Serial.printf( "  Callback count: %lu\n", callbackCount );
    Serial.println();

    Serial.println( "Environmental stats:" );
    Serial.printf( "  SHT30 tempC: %f\n", averageArray( sht30TempCArray ) );
    Serial.printf( "  SHT30 tempF: %f\n", cToF( averageArray( sht30TempCArray ) ) );
    Serial.printf( "  SHT30 humidity: %f\n", averageArray( sht30HumidityArray ) );
    Serial.printf( "  Invalid readings: %u\n", invalidValueCount );
}

void publishAndReport( const char *topic, const char *valueBuffer )
{
    char topicBuffer[256] = "";
    snprintf( topicBuffer, 256, "%s%s%s%s", topicPrefix, hostname, "/", topic );
    if( mqttClient.publish( topicBuffer, valueBuffer ) )
        Serial.printf( "Published '%s' to '%s'.\n", valueBuffer, topicBuffer );
    else
        Serial.printf( "!!! Failed to publish '%s' to '%s' !!!\n", valueBuffer, topicBuffer );
}

/**
 * @brief publishTelemetry() publishes JSON payload and individual topics.
 */
void publishTelemetry()
{
    char valueBuffer[25] = "";
    StaticJsonDocument<JSON_DOC_SIZE> publishTelemetryJsonDoc;
    
    publishTelemetryJsonDoc["sketch"] = __FILE__;
    publishTelemetryJsonDoc["mac"] = macAddress;
    publishTelemetryJsonDoc["hostname"] = hostname;
    publishTelemetryJsonDoc["ip"] = ipAddress;
    publishTelemetryJsonDoc["tempC"] = averageArray( sht30TempCArray );
    publishTelemetryJsonDoc["tempF"] = cToF( averageArray( sht30TempCArray ) );
    publishTelemetryJsonDoc["humidity"] = averageArray( sht30HumidityArray );
    publishTelemetryJsonDoc["rssi"] = rssi;
    publishTelemetryJsonDoc["publishCount"] = publishCount;
    publishTelemetryJsonDoc["invalidValueCount"] = invalidValueCount;
    
    char mqttString[JSON_DOC_SIZE];
    serializeJsonPretty( publishTelemetryJsonDoc, mqttString );
    
    bool success = mqttClient.publish( mqttTopic, mqttString, false );
    if( success )
    {
        Serial.printf( "Successfully published to '%s'\n", mqttTopic );
        publishCount++;
        publishFailCount = 0;
    }
    else
        publishFailCount++;

    if( publishFailCount > 10 )
        deviceRestart();

    snprintf( valueBuffer, 25, "%f", averageArray( sht30TempCArray ) );
    publishAndReport( tempCTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%f", cToF( averageArray( sht30TempCArray ) ) );
    publishAndReport( tempFTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%f", averageArray( sht30HumidityArray ) );
    publishAndReport( humidityTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%ld", rssi );
    publishAndReport( rssiTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%s", hostname );
    publishAndReport( hostnameTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%s", macAddress );
    publishAndReport( macTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%s", ipAddress );
    publishAndReport( ipTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%u", wifiConnectCount );
    publishAndReport( wifiCountTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%lu", wifiCoolDownInterval );
    publishAndReport( wifiCoolDownTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%u", mqttConnectCount );
    publishAndReport( mqttCountTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%lu", mqttCoolDownInterval );
    publishAndReport( mqttCoolDownTopic, valueBuffer );

    snprintf( valueBuffer, 25, "%lu", publishCount );
    publishAndReport( publishCountTopic, valueBuffer );
}

/**
 * @brief mqttCallback() handles incoming MQTT commands.
 */
void mqttCallback( char *topic, byte *payload, unsigned int length )
{
    callbackCount++;
    Serial.printf( "\nMessage arrived on Topic: '%s':\n", topic );

    StaticJsonDocument<JSON_DOC_SIZE> staticJsonDocument;
    deserializeJson( staticJsonDocument, payload, length );

    for( int i = 0; i < length; i++ )
        Serial.print( ( char ) payload[i] );
    Serial.println( "\n" );

    const char *command = staticJsonDocument["command"];
    Serial.printf( "Processing command '%s'.\n", command );
    if( strcmp( command, "publishTelemetry" ) == 0 )
    {
        publishNow = 1;
    }
    else if( strcmp( command, "changeTelemetryInterval" ) == 0 )
    {
        unsigned long tempValue = staticJsonDocument["value"];
        if( tempValue > 4000 )
            publishInterval = tempValue;
        Serial.printf( "MQTT publish interval has been updated to %u\n", publishInterval );
        lastPublishTime = 0;
    }
    else if( strcmp( command, "publishStatus" ) == 0 )
        Serial.println( "publishStatus is not yet implemented." );
    else if( strcmp( command, "restart" ) == 0 )
        deviceRestart();
    else
        Serial.printf( "Unknown command '%s'\n", command );
}

/**
 * @brief mqttConnect() will connect to the MQTT broker.
 */
void mqttConnect()
{
    if( lastMqttConnectionTime == 0 || millis() - lastMqttConnectionTime > mqttCoolDownInterval )
    {
        mqttConnectCount++;
        digitalWrite( ONBOARD_LED, LED_OFF );
        Serial.printf( "Connecting to broker at %s:%d...\n", mqttBroker, mqttPort );
        mqttClient.setServer( mqttBroker, mqttPort );
        mqttClient.setCallback( mqttCallback );

        if( mqttClient.connect( macAddress, mqttUsername, mqttPassword ) )
        {
            Serial.println( "Connected to MQTT Broker." );
            if( mqttClient.subscribe( commandTopic, 1 ) )
                Serial.print( "Subscribed" );
            else
                Serial.print( "Failed to subscribe" );
            Serial.printf( " to '%s'.\n", commandTopic );
            digitalWrite( ONBOARD_LED, LED_ON );
        }
        else
        {
            int mqttStateCode = mqttClient.state();
            char buffer[29];
            lookupMQTTCode( mqttStateCode, buffer );
            Serial.printf( "MQTT state: %s\n", buffer );
            Serial.printf( "MQTT state code: %d\n", mqttStateCode );

            if( mqttCoolDownInterval > 120000 )
                mqttCoolDownInterval = 0;
            mqttCoolDownInterval += 10000;
        }

        lastMqttConnectionTime = millis();
    }
}

/**
 * @brief setup() will configure the program.
 */
void setup()
{
    // This delay gives the serial port enough time to make a connection afte a reboot.
    // I am willing to endure a 3 second delay in order to get better setup logging.
    delay( 3000 );
    Serial.begin( 115200 );
    if( !Serial )
        delay( 1000 );
    Serial.println( "\n" );
    Serial.println( "Function setup() is beginning." );

    wifiConnect();
    // WiFi.mode( WIFI_STA ); 
    // snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );

    pinMode( ONBOARD_LED, OUTPUT );
    digitalWrite( ONBOARD_LED, LED_ON );

    setupSht30();

    // Initialize Modbus Server Registers
    // mb.server(502, MAX_MODBUS_CLIENTS);
    mb.server( 502 );
    mb.addHreg( REG_TEMP_INT, 0 );
    mb.addHreg( REG_TEMP_FRAC, 0 );
    mb.addHreg( REG_HUMIDITY, 0 );

    // Read from the sensors twice, to populate telemetry arrays and initial Modbus states.
    readTelemetry();
    readTelemetry();

    configureOTA();

    Serial.println( "Function setup() has completed." );
}

/**
 * @brief loop() repeats over and over.
 */
void loop()
{
    // Reconnect Wi-Fi if needed, reconnect MQTT as needed.
    if( WiFi.status() != WL_CONNECTED )
        wifiConnect();
    else if( !mqttClient.connected() )
        mqttConnect();
    else
    {
        mqttClient.loop();
        ArduinoOTA.handle();
    }
    
    // Process Modbus clients whenever possible 
    mb.task();

    if( lastPrintTime == 0 || millis() - lastPrintTime >= printInterval )
    {
        readTelemetry(); // This now also updates Modbus registers.
        printTelemetry();
        Serial.printf( "Next print in %u seconds.\n\n", printInterval / 1000 );
        lastPrintTime = millis();
    }

    if( mqttClient.connected() && ( publishNow == 1 || lastPublishTime == 0 || millis() - lastPublishTime >= publishInterval ) )
    {
        if( publishNow == 1 )
            readTelemetry();
        publishTelemetry();
        publishNow = 0;
        Serial.printf( "Next publish in %u seconds.\n\n", publishInterval / 1000 );
        lastPublishTime = millis();
    }

    if( lastLedBlinkTime == 0 || millis() - lastLedBlinkTime >= ledBlinkInterval )
    {
        if( WiFi.status() == WL_CONNECTED )
        {
            if( mqttClient.state() != 0 )
                toggleLED();                                 
            else
                digitalWrite( ONBOARD_LED, LED_ON ); 
        }
        else
            digitalWrite( ONBOARD_LED, LED_OFF ); 
        lastLedBlinkTime = millis();
    }
}