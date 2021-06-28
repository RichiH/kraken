#include "config.h"
#include "certificates.h"
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10*/

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C


PromLokiTransport transport;
PromClient client(transport);

// Create a write request for 2 series.
WriteRequest req(2, 1024);

// Check out https://prometheus.io/docs/practices/naming/ for metric naming and label conventions.
// This library does not currently create different metric types like gauges, counters, and histograms
// however internally Prometheus does not differentiate between these types, rather they are both
// a naming convention and a usage convention so it's possible to create any of these yourself.
// See the README at https://github.com/grafana/prometheus-arduino for more info.

// Define a TimeSeries which can hold up to 5 samples, has a name of `uptime_milliseconds`
TimeSeries ts1(5, (char*)"uptime_milliseconds_total", (char*)"{job=\"esp32-test\",host=\"richih1\"}");

// Define a TimeSeries which can hold up to 5 samples, has a name of `heap_free_bytes`
TimeSeries ts2(5, (char*)"heap_free_bytes", (char*)"{job=\"esp32-test\",host=\"richih1\",foo=\"bar\"}");

//TimeSeries ts3(5, (char*)"kraken_temperature_celsius", (char*)"{job=\"esp32-test\",host=\"esp_1\",location=\"office\"}");
//TimeSeries ts3(5, (char*)"foobar", (char*)"{job=\"esp32-test\",host=\"esp_1\",location=\"office\"}");
TimeSeries ts3(5, (char*)"foobar", (char*)"{job=\"esp32-test\",host=\"richih1\",foo=\"bar\"}");

TimeSeries ts4(5, (char*)"asdf", (char*)"{job=\"esp32-test\",host=\"richih1\",foo=\"bar\"}");


// Note, metrics with the same name and different labels are actually different series and you would need to define them separately
//TimeSeries ts2(5, "heap_free_bytes", "job=\"esp32-test\",host=\"richih1\",foo=\"bar\"");

int loopCounter = 0;

void setup() {
    Serial.begin(115200);

    // Wait 5s for serial connection or continue without it
    // some boards like the esp32 will run whether or not the 
    // serial port is connected, others like the MKR boards will wait
    // for ever if you don't break the loop.
    uint8_t serialTimeout;
    while (!Serial && serialTimeout < 50) {
        delay(100);
        serialTimeout++;
    }

    Serial.println("Starting");
    Serial.print("Free Mem Before Setup: ");
    Serial.println(freeMemory());

    // Configure and start the transport layer
    transport.setUseTls(true);
    transport.setCerts(grafanaCert, strlen(grafanaCert));
    transport.setWifiSsid(WIFI_SSID);
    transport.setWifiPass(WIFI_PASSWORD);
    transport.setDebug(Serial);  // Remove this line to disable debug logging of the client.
    if (!transport.begin()) {
        Serial.println(transport.errmsg);
        while (true) {};
    }

    // Configure the client
    client.setUrl(GC_URL);
    //client.setPath((char*)GC_PATH);
    client.setPath((char*)GC_PATH);
    client.setPort(GC_PORT);
    client.setUser(GC_USER);
    client.setPass(GC_PASS);
    client.setDebug(Serial);  // Remove this line to disable debug logging of the client.
    if (!client.begin()) {
        Serial.println(client.errmsg);
        while (true) {};
    }


    // Add our TimeSeries to the WriteRequest
    req.addTimeSeries(ts1);
    req.addTimeSeries(ts2);
    req.addTimeSeries(ts3);
    req.addTimeSeries(ts4);

    req.setDebug(Serial);  // Remove this line to disable debug logging of the write request serialization and compression.

    Serial.print("Free Mem After Setup: ");
    Serial.println(freeMemory());

    Serial.println(F("--- --- --- ---"));
    Serial.println(F("BME680 async test"));

    if (!bme.begin()) {
      Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
      while (1);
    }

    // Set up oversampling and filter initialization
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms

};



void loop() {

    // Tell BME680 to begin measurement.
    unsigned long endTime = bme.beginReading();
    if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
      return;
    }
    Serial.print(F("Reading started at "));
    Serial.print(millis());
    Serial.print(F(" and will finish at "));
    Serial.println(endTime);

  if (!bme.endReading()) {
    Serial.println(F("Failed to complete reading :("));
    return;
  }
  Serial.print(F("Reading completed at "));
  Serial.println(millis());

  Serial.print(F("Temperature = "));
  Serial.print(bme.temperature);
  Serial.println(F(" *C"));

  Serial.print(F("Pressure = "));
  Serial.print(bme.pressure / 100.0);
  Serial.println(F(" hPa"));

  Serial.print(F("Humidity = "));
  Serial.print(bme.humidity);
  Serial.println(F(" %"));

  Serial.print(F("Gas = "));
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(F(" KOhms"));

  Serial.print(F("Approx. Altitude = "));
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(F(" m"));

  Serial.println();
  
    int64_t time;
    time = transport.getTimeMillis();
    Serial.println(time);
//



    // Efficiency in requests can be gained by batching writes so we accumulate 5 samples before sending.
    // This is not necessary however, especially if your writes are infrequent, but it's recommended to build batches when you can.
    if (loopCounter >= 5) {
        //Send
        loopCounter = 0;
        PromClient::SendResult res = client.send(req);
        if (!res == PromClient::SendResult::SUCCESS) {
            Serial.println(client.errmsg);
            // Note: additional retries or error handling could be implemented here.
            // the result could also be:
            // PromClient::SendResult::FAILED_DONT_RETRY
            // PromClient::SendResult::FAILED_RETRYABLE
        }
        // Batches are not automatically reset so that additional retry logic could be implemented by the library user.
        // Reset batches after a succesful send.
        ts1.resetSamples();
        ts2.resetSamples();
        ts3.resetSamples();
        ts4.resetSamples();

    }
    else {
        if (!ts1.addSample(time, millis())) {
            Serial.println(ts1.errmsg);
        }
        //if (!ts2.addSample(time, freeMemory())) {
        if (!ts2.addSample(time, bme.temperature)) {
            Serial.println(ts2.errmsg);
        }
        if (!ts3.addSample(time, freeMemory())) {
      //  if (!ts3.addSample(time, (bme.temperature))) {
            Serial.println(ts3.errmsg);
        }
        if (!ts4.addSample(time, bme.temperature)) {
            Serial.println(ts4.errmsg);
        }
        loopCounter++;
    }

    delay(500);

  //delay(5000);
};
