#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <BME280I2C.h>
#include <MQ135.h>

#include <Wire.h>


#define LED D4
#define SENSORPIN A0
#define ANALOGPIN A0

char ssid[] = "XXX";
const char* password = "XXX";

ESP8266WebServer server(80);

const int led = 13;

int sensorValue;
float airPres, airTemperature, airHumidity, ppm, ppmbalanced, rzero;
MQ135 gasSensor = MQ135(SENSORPIN);
BME280I2C bme;


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
  String promPage ="kraken_temperature_celsius{sensor=\"BME280\"} " + String(airTemperature) + "\n" ;
  promPage += "kraken_airpressure_hectopascals{sensor=\"BME280\"} " + String(airPres) + "\n";
  promPage += "kraken_relative_humidity_percent{sensor=\"BME280\"} " + String(airHumidity) + "\n";
  promPage += "kraken_rzero{sensor=\"MQ135\"} " + String(rzero) + "\n";
  promPage += "kraken_raw{sensor=\"MQ135\"} " + String(sensorValue) + "\n";
  promPage += "kraken_voc_ppm{sensor=\"MQ135\"} " + String(ppm) + "\n";
  promPage += "kraken_voc_ppm_balanced{sensor=\"MQ135\"} " + String(ppmbalanced) + "\n";
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

  server.on("/", handleRoot);

  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/metrics", [](){
    Serial.println("\n[Client connected]");

    bme.read(airPres, airTemperature, airHumidity);
    sensorValue = analogRead(SENSORPIN);
    rzero = gasSensor.getRZero();
    ppm = gasSensor.getPPM();
    ppmbalanced = gasSensor.getCorrectedPPM(airTemperature, airHumidity);

    server.send(200, "text/plain", promResponse());
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
  server.handleClient();
}
