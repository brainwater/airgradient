/*
This is the code for the AirGradient DIY BASIC Air Quality Sensor with an ESP8266 Microcontroller.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

Build Instructions: https://www.airgradient.com/open-airgradient/instructions/diy/

Kits (including a pre-soldered version) are available: https://www.airgradient.com/open-airgradient/kits/

The codes needs the following libraries installed:
“WifiManager by tzapu, tablatronix” tested with version 2.0.11-beta
“U8g2” by oliver tested with version 2.32.15

Configuration:
Please set in the code below the configuration parameters.

If you have any questions please visit our forum at https://forum.airgradient.com/

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/

MIT License

*/


#include <AirGradient.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <U8g2lib.h>
#include <ArduinoHA.h>

AirGradient ag = AirGradient();

U8G2_SSD1306_64X48_ER_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); //for DIY BASIC


// CONFIGURATION START

//set to the endpoint you would like to use
String APIROOT = "http://hw.airgradient.com/";

// set to true to switch from Celcius to Fahrenheit
boolean inF = false;

// PM2.5 in US AQI (default ug/m3)
boolean inUSAQI = false;

// set to true if you want to connect to wifi. You have 60 seconds to connect. Then it will go into an offline mode.
boolean connectWIFI=true;

// CONFIGURATION END


unsigned long currentMillis = 0;
//unsigned long previousMillis = 0;

const int oledInterval = 5000;
unsigned long previousOled = 0;

const int sendToServerInterval = 10000;
unsigned long previoussendToServer = 0;

const int co2Interval = 5000;
unsigned long previousCo2 = 0;
int Co2 = 0;

const int pm25Interval = 5000;
unsigned long previousPm25 = 0;
int pm25 = 0;

const int pm1Interval = 5000;
unsigned long previousPm1 = 0;
int pm1 = 0;

const int tempHumInterval = 2500;
unsigned long previousTempHum = 0;
float temp = 0;
int hum = 0;
long val;

const int mqttInterval = 5000;
unsigned long previousMqtt = 0;

char mqtt_server[40] = "homeassistant.local";
int mqtt_port = 1883;
char mqtt_username[40] = "username";
char mqtt_password[40] = "password";

char unique[40] = "tempUnique";

bool garageClosed = false;
bool garageOpen = false;

const int garageClosedPin = 13; //Labeled D7
const int garageButtonPin = 15; //Labeled D8
const int garageOpenPin = 16; // Labeled D0

WiFiManager wifiManager;
WiFiClient client;
HADevice device(unique);
HAMqtt mqtt(client, device, 11);

HASensorNumber haco2("airGradientCO2");
HASensorNumber hapms("airGradientPMS");
HASensorNumber hatmp("airGradientTmp", HASensorNumber::PrecisionP1);
HASensorNumber hahum("airGradientHum");
HASensorNumber hapm25("airGradientPM25");
HASensorNumber hapm1("airGradientPM1");
HASensorNumber haaqi("airGradientAQI");
HASensorNumber harssi("airGradientRSSI");
HACover hagarage("airGradientGarageCover");
HAButton hagarage_button("airGradientGarageButton");


void setup()
{
  // Avoid having the remote trigger on boot
  digitalWrite(garageButtonPin, HIGH);
  pinMode(garageButtonPin, OUTPUT);
  digitalWrite(garageButtonPin, HIGH);

  Serial.begin(115200);
  u8g2.setBusClock(100000);
  u8g2.begin();
  pinMode(garageClosedPin, INPUT_PULLUP);
  pinMode(garageOpenPin, INPUT_PULLUP);
  updateOLED();
  if (connectWIFI) {
    connectToWifi();
  }
  connectToMqtt();
  updateOLED2("Warm Up", "Serial#", String(ESP.getChipId(), HEX));
  ag.CO2_Init();
  ag.PMS_Init();
  ag.TMP_RH_Init(0x44);
}


void loop()
{
  currentMillis = millis();
  updateGarage();
  updateOLED();
  updateCo2();
  updatePm25();
  updatePm1();
  updateTempHum();
  //sendToServer();
  mqtt.loop();
  sendToMqtt();
  delay(50);
}

void onGarageButtonCommand(HAButton* sender) {
  if (sender == &hagarage_button) {
    triggerGarageButton();
  }
}

void triggerGarageButton() {
  Serial.println("Pressing the garage button!");
  digitalWrite(garageButtonPin, LOW);
  delay(1000); // TODO: see how long is the minimum for trigering the garage
  digitalWrite(garageButtonPin, HIGH);
}

void onGarageCommand(HACover::CoverCommand cmd, HACover *sender) {
  if (sender == &hagarage) {
    if (cmd == HACover::CommandOpen && garageClosed && !garageOpen && hagarage.getCurrentState() == HACover::StateClosed) {
      hagarage.setState(HACover::StateOpening);
      triggerGarageButton();
    } else if (cmd == HACover::CommandClose && !garageClosed && garageOpen && hagarage.getCurrentState() == HACover::StateOpen) {
      hagarage.setState(HACover::StateClosing);
      triggerGarageButton();
    } else {
      // else we are in an unknown state, or the garage is in the state requested
      Serial.println("Garage command recieved but we are in an unknown state or are already in the state requested.");
    }
  }
}

void updateGarage() {
  garageClosed = digitalRead(garageClosedPin) == LOW;
  garageOpen = digitalRead(garageOpenPin) == LOW;
}

void updateCo2()
{
    if (currentMillis - previousCo2 >= co2Interval) {
      previousCo2 += co2Interval;
      Co2 = ag.getCO2_Raw();
      //Serial.println(String(Co2));
    }
}

void updatePm25()
{
    if (currentMillis - previousPm25 >= pm25Interval) {
      previousPm25 += pm25Interval;
      pm25 = ag.getPM2_Raw();
      //Serial.println(String(pm25));
    }
}

void updatePm1()
{
    if (currentMillis - previousPm1 >= pm1Interval) {
      previousPm1 += pm1Interval;
      pm1 = ag.getPM1_Raw();
      Serial.println(String(pm1));
    }
}

void updateTempHum()
{
    if (currentMillis - previousTempHum >= tempHumInterval) {
      previousTempHum += tempHumInterval;
      TMP_RH result = ag.periodicFetchData();
      temp = result.t;
      hum = result.rh;
      //Serial.println(String(temp));
    }
}

void updateOLED() {
   if (currentMillis - previousOled >= oledInterval) {
     previousOled += oledInterval;

    String ln1;
    String ln2;
    String ln3;


    if (inUSAQI){
       ln1 = "AQI:" + String(PM_TO_AQI_US(pm25)) ;
    } else {
       ln1 = "PM: " + String(pm25) +"ug" ;
    }

    ln2 = "CO2:" + String(Co2);

      if (inF) {
        ln3 = String((temp* 9 / 5) + 32).substring(0,4) + " " + String(hum)+"%";
        } else {
        ln3 = String(temp).substring(0,4) + " " + String(hum)+"%";
       }
     updateOLED2(ln1, ln2, ln3);
   }
}

void updateOLED2(String ln1, String ln2, String ln3) {
      char buf[9];
          u8g2.firstPage();
          u8g2.firstPage();
          do {
          u8g2.setFont(u8g2_font_t0_16_tf);
          u8g2.drawStr(1, 10, String(ln1).c_str());
          u8g2.drawStr(1, 28, String(ln2).c_str());
          u8g2.drawStr(1, 46, String(ln3).c_str());
            } while ( u8g2.nextPage() );
}

void sendToServer() {
   if (currentMillis - previoussendToServer >= sendToServerInterval) {
     previoussendToServer += sendToServerInterval;

      String payload = "{\"wifi\":" + String(WiFi.RSSI())
      + (Co2 < 0 ? "" : ", \"rco2\":" + String(Co2))
      + (pm25 < 0 ? "" : ", \"pm02\":" + String(pm25))
      + ", \"atmp\":" + String(temp)
      + (hum < 0 ? "" : ", \"rhum\":" + String(hum))
      + "}";

      if(WiFi.status()== WL_CONNECTED){
        Serial.println(payload);
        String POSTURL = APIROOT + "sensors/airgradient:" + String(ESP.getChipId(), HEX) + "/measures";
        Serial.println(POSTURL);
        //WiFiClient client;
        HTTPClient http;
        http.begin(client, POSTURL);
        http.addHeader("content-type", "application/json");
        int httpCode = http.POST(payload);
        String response = http.getString();
        Serial.println(httpCode);
        Serial.println(response);
        http.end();
      }
      else {
        Serial.println("WiFi Disconnected");
      }
   }
}

// Wifi Manager
 void connectToWifi() {
   //WiFi.disconnect(); //to delete previous saved hotspot
   String HOTSPOT = "AG-" + String(ESP.getChipId(), HEX);
   updateOLED2("Connect", "Wifi AG-", String(ESP.getChipId(), HEX));
   delay(2000);
   wifiManager.setTimeout(90);
   //wifiManager.startConfigPortal();
   if (!wifiManager.autoConnect((const char * ) HOTSPOT.c_str())) {
     updateOLED2("Booting", "offline", "mode");
     Serial.println("failed to connect and hit timeout");
     delay(6000);
   }
}

void connectToMqtt() {
  // TODO: Is there support for ssl or encryption for mqtt?
  
  Serial.println("Connecting to mqtt/ha");
  strcpy(unique, String(ESP.getChipId(), HEX).c_str());
  /*byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));*/
  device.setName("Air Gradient");
  device.enableSharedAvailability();
  device.enableLastWill();
  device.setManufacturer("Air Gradient");
  device.setModel("DIY Basic PCBv2");
  
  hatmp.setName("Air Gradient Temperature");
  hahum.setName("Air Gradient Humidity");
  haco2.setName("Air Gradient CO2");
  hapm25.setName("Air Gradient PM2.5");
  hapm1.setName("Air Gradient PM1.0");
  haaqi.setName("Air Gradient AQI");
  harssi.setName("Air Gradient WiFi RSSI");
  hagarage.setName("Garage Door");
  hatmp.setDeviceClass("temperature");
  hahum.setDeviceClass("humidity");
  haco2.setDeviceClass("carbon_dioxide");
  hapm25.setDeviceClass("pm25");
  hapm1.setDeviceClass("pm1");
  haaqi.setDeviceClass("aqi");
  harssi.setDeviceClass("signal_strength");
  hagarage.setDeviceClass("garage");
  hatmp.setUnitOfMeasurement("°C");
  hahum.setUnitOfMeasurement("%");
  haco2.setUnitOfMeasurement("ppm");
  //TODO: Are the pm measurement units correct? https://www.home-assistant.io/integrations/sensor/ Indicates the measurements are in µg/m³
  hapm25.setUnitOfMeasurement("ppm");
  hapm1.setUnitOfMeasurement("ppm");
  harssi.setUnitOfMeasurement("dB");

  hagarage_button.setIcon("mdi:garage-alert");
  hagarage_button.setName("Garage Door Open Button");
  hagarage_button.onCommand(onGarageButtonCommand);
  hagarage.onCommand(onGarageCommand);

  hagarage.setIcon("mdi:garage");
  mqtt.begin(mqtt_server, mqtt_port, mqtt_username, mqtt_password);
  // Important! I think this is required to make sure the config is sent, but now it is sending the config without this!?
  device.setAvailability(true);
  //TODO: What happens with the config when home assistant is restarted?
  Serial.println("Connected to mqtt/ha");
  
}

void sendToMqtt() {

  if (currentMillis - previousMqtt >= mqttInterval) {
    previousMqtt += mqttInterval;
    hatmp.setValue(temp);
    hahum.setValue(hum);
    haco2.setValue(Co2);
    hapm25.setValue(pm25);
    hapm1.setValue(pm1);
    haaqi.setValue(PM_TO_AQI_US(pm25));
    harssi.setValue(WiFi.RSSI());
    if (garageClosed && garageOpen) {
      // We should never get here
      Serial.println("We should never get here! Garage is detected as BOTH open and closed!");
      hagarage.setState(HACover::StateClosing);
      hagarage.setAvailability(false);
    } else if (garageClosed && !garageOpen) {
      Serial.println("Garage Closed");
      hagarage.setAvailability(true);
      hagarage.setState(HACover::StateClosed);
    } else if (!garageClosed && garageOpen) {
      Serial.println("Garage Open");
      hagarage.setAvailability(true);
      hagarage.setState(HACover::StateOpen);
    } else if (!garageClosed && !garageOpen) {
      // Garage may be closing or opening, or a sensor may be offline
      Serial.println("Garage is in an unknown state!");
      hagarage.setState(HACover::StateOpening);
      hagarage.setAvailability(false);
    }
  }
}

// Calculate PM2.5 US AQI
int PM_TO_AQI_US(int pm02) {
  if (pm02 <= 12.0) return ((50 - 0) / (12.0 - .0) * (pm02 - .0) + 0);
  else if (pm02 <= 35.4) return ((100 - 50) / (35.4 - 12.0) * (pm02 - 12.0) + 50);
  else if (pm02 <= 55.4) return ((150 - 100) / (55.4 - 35.4) * (pm02 - 35.4) + 100);
  else if (pm02 <= 150.4) return ((200 - 150) / (150.4 - 55.4) * (pm02 - 55.4) + 150);
  else if (pm02 <= 250.4) return ((300 - 200) / (250.4 - 150.4) * (pm02 - 150.4) + 200);
  else if (pm02 <= 350.4) return ((400 - 300) / (350.4 - 250.4) * (pm02 - 250.4) + 300);
  else if (pm02 <= 500.4) return ((500 - 400) / (500.4 - 350.4) * (pm02 - 350.4) + 400);
  else return 500;
};
