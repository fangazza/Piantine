#include <dummy.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Preferences.h>

// Piantine Includes
#include <webpage.h>
#include <sensors.h>
#include <websocket.h>

// WiFi Preferences
const char* ssid = "Rick&Morty";
const char* password = "PippiCalzelunghe";

/*
  Setup
*/ 
void setup(){
  Serial.begin(115200);

  // Realtime Clock
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.println(printDateTime(compiled));

  if (!Rtc.IsDateTimeValid()) {
    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }
  if (Rtc.GetIsWriteProtected()) {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }
  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }  

  // Initialize water pump
  pinMode(RELAY_PIN, OUTPUT);

  // Initialize DHT sensor
  dht_sensor.begin();
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  
  // Set custom hostname
  randomSeed(analogRead(3));    // Random from unconnected pin
  const int randNumber = random(999);  
  char charArray[3];
  char hname [15];
  itoa(randNumber, charArray, 10);
  strcpy (hname, "piantine-");
  strcat (hname, charArray);
  WiFi.setHostname(hname);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }  
  
  hostname = WiFi.getHostname();
  Serial.print("Hostname: ");
  Serial.println(WiFi.getHostname());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  /*
    Preferences examples (Flash drive read/write)

    Example: how to use Preferences (nvs) to store a structure.
    Note that the maximum size of a putBytes is 496K or 97% of the nvs partition size.
    nvs has significant overhead, so should not be used for data that will change often.
  */
  // Preferences example 1: reboot count
  prefs.begin("my-app");
  int counter = prefs.getInt("counter", 1); // default to 1
  Serial.print("Reboot count: ");
  Serial.println(counter);
  counter++;
  prefs.putInt("counter", counter);
  
  // Preferences example 2: schedule
  prefs.begin("schedule");                              // use "schedule" namespace
  uint8_t content[] = {9, 30, 235, 255, 20, 15, 0, 1};  // two entries
  prefs.putBytes("schedule", content, sizeof(content));
  size_t schLen = prefs.getBytesLength("schedule");
  char buffer[schLen];  // prepare a buffer for the data
  prefs.getBytes("schedule", buffer, schLen);
  if (schLen % sizeof(schedule_t)) {  // simple check that data fits
    log_e("Data is not correct size!");
    return;
  }
  schedule_t *schedule = (schedule_t *)buffer;  // cast the bytes into a struct ptr
  Serial.printf("%02u:%02u %u/%u\n", schedule[1].hour, schedule[1].minute, schedule[1].setting1, schedule[1].setting2);
  schedule[2] = {8, 30, 20, 21};  // add a third entry (unsafely)
                                  // force the struct array into a byte array
  prefs.putBytes("schedule", schedule, 3 * sizeof(schedule_t));
  schLen = prefs.getBytesLength("schedule");
  char buffer2[schLen];
  prefs.getBytes("schedule", buffer2, schLen);
  for (int x = 0; x < schLen; x++) {
    Serial.printf("%02X ", buffer[x]);
  }

  // Route for root / web page
  initWebSocket();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html, processor);
  });

  // Start server
  server.begin();
}

/*
  Loop
*/
void loop() {
  // RTClock
  RtcDateTime now = Rtc.GetDateTime();

  // Light Sensor
  int lightValue = analogRead(lightSensorPin);  // 0 (bright) 4095 (dark)

  // Battery
  int battLvl = analogRead(A3);

  // Water Pump AUTO OFF after pumpInterval
  unsigned long currentMillis = millis();
  if (pump == 1 && currentMillis - previousMillis >= pumpInterval)
  {
      previousMillis = currentMillis;
      digitalWrite(RELAY_PIN, LOW);  // Pump OFF;
      pump=0;
  }

  // Soil Water
  sensorvalu[0] = analogRead(A6);

  // Air humidity & temp  
  float humi  = dht_sensor.readHumidity();
  float tempC = dht_sensor.readTemperature();

  notifyClients();
  delay(500);
  ws.cleanupClients();
}
