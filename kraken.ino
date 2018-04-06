#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <BME280I2C.h>
#include <MQ135.h>

#include <Wire.h>

#include <math.h>

#include "kraken.config.h"

#define LED D4
#define SENSORPIN A0
#define ANALOGPIN A0

char ssid[] = wlan_ssid;
const char* password = wlan_pass;


ESP8266WebServer server(80);

const int led = 13;

int sensorValue;
float airPres, airTemperature, airHumidity, ppm, ppmbalanced, rzero;
MQ135 gasSensor = MQ135(SENSORPIN);
BME280I2C bme;

// dew point, etc
// see https://www.wetterochs.de/wetter/feuchte.html
float saturation_vapor_pressure_hPa, vapor_pressure_hPa, dew_point_celsius, absolute_humidity_g_per_m3;
const float a = 7.5;
const float b = 237.3;
const float R = 8314.3;
const float mw = 18.016;
float v;

void handleRoot() {
//  digitalWrite(led, 1);
//  server.send(200, "text/plain", "hello from esp8266!");
//  digitalWrite(led, 0);
  server.send(200, "text/html", "<a href=\"/metrics\">metrics</a>");

}

void handleNotFound(){
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

String promResponse() {
  String promPage ="kraken_temperature_celsius{sensor_type=\"BME280\"} " + String(airTemperature) + "\n" ;
  promPage += "kraken_airpressure_hectopascal{sensor_type=\"BME280\"} " + String(airPres) + "\n";
  promPage += "kraken_relative_humidity_percent{sensor_type=\"BME280\"} " + String(airHumidity) + "\n";
  promPage += "kraken_dew_point_celsius " + String(dew_point_celsius) + "\n";
  promPage += "kraken_absolute_humidity_g_per_m3 " + String(absolute_humidity_g_per_m3) + "\n";
  promPage += "kraken_rzero{sensor_type=\"MQ135\"} " + String(rzero) + "\n";
  promPage += "kraken_raw{sensor_type=\"MQ135\"} " + String(sensorValue) + "\n";
  promPage += "kraken_voc_ppm{sensor_type=\"MQ135\"} " + String(ppm) + "\n";
  promPage += "kraken_voc_ppm_balanced{sensor_type=\"MQ135\"} " + String(ppmbalanced) + "\n";
  return promPage;
}

void setup(void){
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  // Wait for BME280
  Wire.begin(4, 5);
  while (!bme.begin()) {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }
  Serial.println("Found BME280 sensor!");

  server.on("/", handleRoot);

  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/metrics", [](){
    String client_ip = server.client().remoteIP().toString();
    Serial.println("Client connected from: " + client_ip);

    bme.read(airPres, airTemperature, airHumidity);
    sensorValue = analogRead(SENSORPIN);
    rzero = gasSensor.getRZero();
    ppm = gasSensor.getPPM();
    ppmbalanced = gasSensor.getCorrectedPPM(airTemperature, airHumidity);

//float a, b, R, mw, saturation_vapor_pressure_hPa, vapor_pressure_hPa, dew_point_celsius, absolute_humidity_g_per_m3;

    // dew point, as per https://www.wetterochs.de/wetter/feuchte.html
    saturation_vapor_pressure_hPa = 6.1078 * powf(10, ((a * airTemperature) / (b + airTemperature)));
    vapor_pressure_hPa = (airHumidity / 100) * saturation_vapor_pressure_hPa;
    v = log10(vapor_pressure_hPa/6.1078);
    dew_point_celsius = b * v / (a - v);
    //Serial.println("dew point: " + String(dew_point_celsius));

    // absolute humidity
    absolute_humidity_g_per_m3 = 100000 * mw / R  * vapor_pressure_hPa / (airTemperature + 273.15);
    //Serial.println("abs humidity: " + String(absolute_humidity_g_per_m3));

    server.send(200, "text/plain", promResponse());
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
  server.handleClient();
}
