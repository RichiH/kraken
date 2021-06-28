#include "config.h"
#include "certificates.h"
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>
PromLokiTransport transport;
PromClient client(transport);
// Create a write request for 2 series.
WriteRequest req(10, 2048);
// Check out https://prometheus.io/docs/practices/naming/ for metric naming and label conventions.
// This library does not currently create different metric types like gauges, counters, and histograms
// however internally Prometheus does not differentiate between these types, rather they are both
// a naming convention and a usage convention so it's possible to create any of these yourself.
// See the README at https://github.com/grafana/prometheus-arduino for more info.
// Define a TimeSeries which can hold up to 5 samples, has a name of `uptime_milliseconds`
TimeSeries ts1(5, "uptime_milliseconds_total", "{job=\"esp32-test\",host=\"esp32\"}");
// Define a TimeSeries which can hold up to 5 samples, has a name of `heap_free_bytes`
TimeSeries ts2(5, "heap_free_bytes", "{job=\"esp32-test\",host=\"esp32\",foo=\"bar\"}");
TimeSeries ts3(5, "heap_free_bytes_10", "{job=\"esp32-test\",host=\"esp32\",foo=\"bar\"}");
TimeSeries ts4(5, "heap_free_bytes_100", "{job=\"esp32-test\",host=\"esp32\",foo=\"bar\"}");
TimeSeries ts5(5, "heap_free_bytes_1000", "{job=\"esp32-test\",host=\"esp32\",foo=\"bar\"}");
TimeSeries ts6(5, "heap_free_bytes_10000", "{job=\"esp32-test\",host=\"esp32\",foo=\"bar\"}");
// Note, metrics with the same name and different labels are actually different series and you would need to define them separately
//TimeSeries ts2(5, "heap_free_bytes", "job=\"esp32-test\",host=\"esp32\",foo=\"bar\"");
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
    req.addTimeSeries(ts5);
    req.addTimeSeries(ts6);
    req.setDebug(Serial);  // Remove this line to disable debug logging of the write request serialization and compression.
    Serial.print("Free Mem After Setup: ");
    Serial.println(freeMemory());
};
void loop() {
    int64_t time;
    time = transport.getTimeMillis();
    Serial.println(time);
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
        ts5.resetSamples();
        ts6.resetSamples();
    }
    else {
        if (!ts1.addSample(time, millis())) {
            Serial.println(ts1.errmsg);
        }
        if (!ts2.addSample(time, freeMemory())) {
            Serial.println(ts2.errmsg);
        }
        if (!ts3.addSample(time, freeMemory()*10)) {
            Serial.println(ts2.errmsg);
        }
        if (!ts4.addSample(time, freeMemory()*100)) {
            Serial.println(ts2.errmsg);
        }
        if (!ts5.addSample(time, freeMemory()*1000)) {
            Serial.println(ts2.errmsg);
        }
        if (!ts6.addSample(time, freeMemory()*10000)) {
            Serial.println(ts2.errmsg);
        }
        loopCounter++;
    }
    delay(500);
};
