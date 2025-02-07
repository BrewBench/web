#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
// [HEADERS]

String HOSTNAME = "[HOSTNAME]";
const PROGMEM uint8_t FREQUENCY_SECONDS = 60;
uint8_t secondCounter = 0;
const char* ssid     = "[SSID]";
const char* password = "[SSID_PASS]";

ESP8266WebServer server(80);

HTTPClient http;

#ifndef ARDUINO_BOARD
#define ARDUINO_BOARD "ESP8266"
#endif

// DHT DHTesp dht;
// BMP180 Adafruit_BMP085 bmp;

// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5
// the value of the 'other' resistor
#define SERIESRESISTOR 147000

uint16_t samples[NUMSAMPLES];

float Thermistor(float average) {
  float V_NTC = average / 1024;
  float R_NTC = (SERIESRESISTOR * V_NTC) / (3.3 - V_NTC);
  R_NTC = log(R_NTC);
  float Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * R_NTC * R_NTC ))* R_NTC );
  Temp = Temp - 273.15;
  return Temp;
}

void setupRest() {

  server.on("/", [](){
    String data = "{\"BrewBench\": {\"board\": \""+String(ARDUINO_BOARD)+"\", \"version\": \"[VERSION]\"";
    data += ",\"RSSI\":"+String(WiFi.RSSI());
    data += ",\"IP\":\""+WiFi.localIP().toString()+"\"";
    data += "}}";
    sendHeaders();
    server.send(200, "application/json", data);
  });

  server.on("/arduino/info", [](){
    String data = "{\"BrewBench\": {\"board\": \""+String(ARDUINO_BOARD)+"\", \"version\": \"[VERSION]\", \"status\": \"restarting\"";
    data += ",\"RSSI\":"+String(WiFi.RSSI());
    data += ",\"IP\":\""+WiFi.localIP().toString()+"\"";
    data += "}}";
    sendHeaders();
    server.send(200, "application/json", data);
  });

  server.on("/arduino/reboot", [](){
    sendHeaders();
    server.send(200, "application/json", "{\"reboot\":true}");
    delay(500);
    ESP.restart();
  });

  server.on("/arduino/digital", [](){
      sendHeaders();
      processRest("digital");
  });
  server.on("/arduino/Thermistor", [](){
    sendHeaders();
    processRest("Thermistor");
  });
  server.on("/arduino/PT100", [](){
    sendHeaders();
    processRest("PT100");
  });
  server.on("/arduino/SoilMoisture", [](){
    sendHeaders();
    processRest("SoilMoisture");
  });
  // DS18B20 server.on("/arduino/DS18B20", [](){
  // DS18B20   sendHeaders();
  // DS18B20   processRest("DS18B20");
  // DS18B20 });
  // DHT server.on("/arduino/DHT11", [](){
  // DHT   sendHeaders();
  // DHT   processRest("DHT11");
  // DHT });
  // DHT server.on("/arduino/DHT22", [](){
  // DHT   sendHeaders();
  // DHT   processRest("DHT22");
  // DHT });
  // BMP180 server.on("/arduino/BMP180", [](){
  // BMP180   sendHeaders();
  // BMP180   processRest("BMP180");
  // BMP180 });
}

void sendHeaders(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  server.sendHeader("Access-Control-Expose-Headers", "X-Sketch-Version");
  server.sendHeader("X-Sketch-Version", "[VERSION]");
  server.sendHeader("Connection", "close");
}

void processRest(const String command) {
  String apin = "";
  String dpin = "";
  int16_t value = -1;
  int16_t index = -1;
  for (uint8_t i = 0; i < server.args(); i++) {
    if( server.argName(i) == "dpin" )
      dpin = server.arg(i);
    else if( server.argName(i) == "apin" )
      apin = server.arg(i);
    else if( server.argName(i) == "value" )
      value = server.arg(i).toInt();
    else if( server.argName(i) == "index" )
      index = server.arg(i).toInt();
  }
  String data = "";

  if (command == "digital" || command == "analog" || command == "adc") {
    data = adCommand(dpin, apin, value, command);
  }
  else if (command == "Thermistor" || command == "DS18B20" || command == "PT100" ||
      command == "DHT11" || command == "DHT22" || command == "SoilMoisture" ||
      command == "BMP180") {
    data = sensorCommand(dpin, apin, index, command);
  }
  server.send(200, "application/json", data);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

String adCommand(const String dpin, const String apin, int16_t value, const String type) {
  uint8_t pin;
  if( dpin != "" )
    pin = dpin.substring(1).toInt();
  else
    pin = apin.substring(1).toInt();

  // write
  if ( value >= 0 ) {
    pinMode(pin, OUTPUT);
    if( type == "analog" ){
      analogWrite(pin, value);//0 - 255
    }
    else if( type == "digital" ){
      if(value == 1)
        digitalWrite(pin, LOW);//turn on relay
      else
        digitalWrite(pin, HIGH);//turn off relay
    }
  } else {
    // read
    pinMode(pin, INPUT);
    if( type == "analog" ){
      value = analogRead(pin);
    }
    else if( type == "digital" ){
      value = digitalRead(pin);
    }
  }

  // Send JSON response to client
  String data = "{\"hostname\":\""+String(HOSTNAME)+"\",\"pin\":\""+String(dpin)+"\",\"value\":\""+String(value)+"\",\"sensor\":\""+String(type)+"\"}";
  if( apin != "" )
    data = "{\"hostname\":\""+String(HOSTNAME)+"\",\"pin\":\""+String(apin)+"\",\"value\":\""+String(value)+"\",\"sensor\":\""+String(type)+"\"}";

  return data;
}

String sensorCommand(const String dpin, const String apin, const int16_t index, const String type) {
  uint8_t pin;
  if( dpin != "" )
    pin = dpin.substring(1).toInt();
  else
    pin = apin.substring(1).toInt();
  float temp = 0.00;
  float raw = 0.00;
  float percent = 0.00;
  float volts = 0.00;
  // ADC int16_t adc0 = 0;
  float resistance = 0.0;

  String data = "{\"hostname\":\""+String(HOSTNAME)+"\",\"sensor\":\""+String(type)+"\"";
  if( dpin != "" )
    data += ",\"pin\":\""+String(dpin)+"\"";
  else
    data += ",\"pin\":\""+String(apin)+"\"";

  if( dpin != "" ){
    raw = digitalRead(pin);
  } else {
    raw = analogRead(pin);
    volts = raw * 0.0049;
  }

  if(type == "Thermistor"){
    samples[0] = raw;
    uint8_t i;
    // take N samples in a row, with a slight delay
    for (i=1; i< NUMSAMPLES; i++) {
      samples[i] = analogRead(pin);
      delay(10);
    }
    // average all the samples out
    for (i=0; i< NUMSAMPLES; i++) {
       resistance += samples[i];
    }
    resistance /= NUMSAMPLES;
    raw = resistance;
    temp = Thermistor(resistance);
  }
  else if(type == "PT100"){
    if (raw>409){
      temp = (150*map(raw,410,1023,0,614))/614;
    }
  }
  else if(type == "SoilMoisture"){
    if( dpin != "" ){
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      delay(10);
    }
    raw = analogRead(apin.substring(1).toInt());
    if( dpin != "" ){
      digitalWrite(pin, LOW);
    }
    percent = map(raw, 0, 880, 0, 100);
    data += ",\"percent\":"+String(percent);
  }
  // DS18B20 else if(type.substring(0,7) == "DS18B20"){
  // DS18B20   OneWire oneWire(pin);
  // DS18B20   DallasTemperature sensors(&oneWire);
  // DS18B20   sensors.begin();
  // DS18B20   sensors.requestTemperatures();
  // DS18B20   if( index > 0 )
  // DS18B20     temp = sensors.getTempCByIndex(index);
  // DS18B20   else
  // DS18B20     temp = sensors.getTempCByIndex(0);
  // DS18B20 }
  // DHT else if(type == "DHT11" || type == "DHT12"){
  // DHT   if(type == "DHT11"){
  // DHT     dht.setup(pin, DHTesp::DHT11);
  // DHT     delay(dht.getMinimumSamplingPeriod());
  // DHT     temp = dht.getTemperature();
  // DHT     percent = dht.getHumidity();
  // DHT   } else if(type == "DHT22"){
  // DHT     dht.setup(pin, DHTesp::DHT22);
  // DHT     delay(dht.getMinimumSamplingPeriod());
  // DHT     temp = dht.getTemperature();
  // DHT     percent = dht.getHumidity();
  // DHT   }
  // DHT   if(isnan(temp)) temp = 0;
  // DHT   if(isnan(percent)) percent = 0;
  // DHT   data += ",\"percent\":"+String(percent);
  // DHT }
  // BMP180 else if(type == "BMP180"){
  // BMP180   if (bmp.begin()) {
  // BMP180     temp = bmp.readTemperature();
  // BMP180     data += ",\"altitude\":"+String(bmp.readAltitude());
  // BMP180     data += ",\"pressure\":"+String(bmp.readPressure());
  // BMP180   } else {
  // BMP180     data += ",\"altitude\":0";
  // BMP180     data += ",\"pressure\":0";
  // BMP180   }
  // BMP180 }

  data += ",\"temp\":"+String(temp);
  data += ",\"raw\":"+String(raw);
  data += ",\"volts\":"+String(volts);
  data += "}";
  return data;
}

float actionsCommand(const String source, const String spin, const String type, const float adjustTemp) {
  float temp = 0.00;
  float raw = 0.00;
  float volts = 0.00;
  uint8_t pin = spin.substring(1).toInt();

  float percent = 0.00;
  // BMP180 float pressure = 0.00;
  // ADC int16_t adc0 = 0;
  float resistance = 0.0;

  if( spin.substring(0,1) == "A" ){
    raw = analogRead(pin);
    volts = raw * 0.0049;
  }
  else if( spin.substring(0,1) == "D" ){
    raw = digitalRead(pin);
  }

  if(type == "Thermistor"){
    samples[0] = raw;
    uint8_t i;
    // take N samples in a row, with a slight delay
    for (i=1; i< NUMSAMPLES; i++) {
      samples[i] = analogRead(pin);
      delay(10);
    }
    // average all the samples out
    for (i=0; i< NUMSAMPLES; i++) {
       resistance += samples[i];
    }
    resistance /= NUMSAMPLES;
    raw = resistance;
    temp = Thermistor(resistance);
  }
  else if(type == "PT100"){
    if (raw>409){
      temp = (150*map(raw,410,1023,0,614))/614;
    }
  }
  else if(type.substring(0,12) == "SoilMoisture"){
    uint8_t dpin = -1;
    if(type.substring(0,13) == "SoilMoistureD"){
      dpin = type.substring(13).toInt();
      pinMode(dpin, OUTPUT);
      digitalWrite(dpin, HIGH);
      delay(10);
    }
    raw = analogRead(pin);
    if(dpin >= 0){
      digitalWrite(dpin, LOW);
    }
    percent = map(raw, 0, 880, 0, 100);
  }
  // DS18B20 else if(type.substring(0,7) == "DS18B20"){
  // DS18B20   // format DS18B20-index
  // DS18B20   int16_t index = -1;
  // DS18B20   if( type.length() > 7 )
  // DS18B20     index = type.substring(8).toInt();
  // DS18B20   OneWire oneWire(pin);
  // DS18B20   DallasTemperature sensors(&oneWire);
  // DS18B20   sensors.begin();
  // DS18B20   sensors.requestTemperatures();
  // DS18B20   if( index > 0 )
  // DS18B20     temp = sensors.getTempCByIndex(index);
  // DS18B20   else
  // DS18B20     temp = sensors.getTempCByIndex(0);
  // DS18B20 }
  // DHT else if(type == "DHT11" || type == "DHT12"){
  // DHT   if(type == "DHT11"){
  // DHT     dht.setup(pin, DHTesp::DHT11);
  // DHT     delay(dht.getMinimumSamplingPeriod());
  // DHT     temp = dht.getTemperature();
  // DHT     percent = dht.getHumidity();
  // DHT   } else if(type == "DHT22"){
  // DHT     dht.setup(pin, DHTesp::DHT22);
  // DHT     delay(dht.getMinimumSamplingPeriod());
  // DHT     temp = dht.getTemperature();
  // DHT     percent = dht.getHumidity();
  // DHT   }
  // DHT   if(isnan(temp)) temp = 0;
  // DHT   if(isnan(percent)) percent = 0;
  // DHT }
  // BMP180 else if(type == "BMP180"){
  // BMP180   if (bmp.begin()) {
  // BMP180     temp = bmp.readTemperature();
  // BMP180     pressure = bmp.readPressure();
  // BMP180   }
  // BMP180 }

  // adjust temp if we have it
  if(temp) temp = temp+adjustTemp;
  // Send JSON response to client
  String data = "temperature,sensor="+type+",pin="+spin+",source="+source+",host="+String(HOSTNAME)+" value="+String(temp);
  // SoilMoistureD only has percent so replace data
  if(type.substring(0,12) == "SoilMoisture") {
    data = "percent,sensor="+type+",pin="+spin+",source="+source+",host="+String(HOSTNAME)+" value="+String(percent);
    data += "\nbits,sensor="+type+",pin="+spin+",source="+source+",host="+String(HOSTNAME)+" value="+String(raw);
  } else if(type.substring(0,3) == "DHT"){
    data += "\npercent,sensor="+type+",pin="+spin+",source="+source+",host="+String(HOSTNAME)+" value="+String(percent);
  } else if(type.substring(0,3) == "BMP"){
    data += "\npressure,sensor="+type+",pin="+spin+",source="+source+",host="+String(HOSTNAME)+" value="+String(pressure);
  } else if(percent){
    data += "\npercent,sensor="+type+",pin="+spin+",source="+source+",host="+String(HOSTNAME)+" value="+String(percent);
  } else {
    data += "\nbits,sensor="+type+",pin="+spin+",source="+source+",host="+String(HOSTNAME)+" value="+String(raw);
  }

  postData(data);

  if(type.substring(0,12) == "SoilMoisture"){
    return percent;
  } else {
    return temp;
  }
}

void postData(const String data){

  if(http.begin("[INFLUXDB_CONNECTION]")){
    http.setAuthorization("[INFLUXDB_AUTH]");
    http.addHeader("X-API-KEY", "[API_KEY]");
    http.addHeader("User-Agent", "BrewBench/[VERSION]");
    int response = http.POST(data);
    Serial.print("InfluxDB POST Response: ");
    Serial.println(response);
    http.end();
  }

}

void connect(){
  WiFi.mode(WIFI_STA);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);
  }
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
   Serial.println("Connection Failed! Rebooting...");
   delay(5000);
   ESP.restart();
 }

  // Start the mDNS responder to set hostname bbesp.local
  if (MDNS.begin("[HOSTNAME]"))
    HOSTNAME = "[HOSTNAME]";
  else
    HOSTNAME = WiFi.hostname();

  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Host %s.local\n", HOSTNAME.c_str());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
}

void runActions(){
  // [ACTIONS]
}

void setup() {

  Serial.begin(115200);

  connect();

  setupRest();

  server.begin();

  server.onNotFound(handleNotFound);

  http.setReuse(true);

  ArduinoOTA.setPort(3232);

  ArduinoOTA.setHostname(HOSTNAME.c_str());

  ArduinoOTA.setPasswordHash("[ARDUINO_PASS]");

  ArduinoOTA.onStart([]() {
    String type;

    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  runActions();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  secondCounter+=1;
  if( secondCounter == FREQUENCY_SECONDS ){
    // reset the secondCounter
    secondCounter = 0;
    runActions();
  }

  delay(1000);
}
