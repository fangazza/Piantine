// RTClock
const int IO = 27;    // DAT
const int SCLK = 14;  // CLK
const int CE = 26;    // RST
ThreeWire myWire(IO, SCLK, CE);
RtcDS1302<ThreeWire> Rtc(myWire);

// Air sensor 
#define DHT_SENSOR_PIN  32 
#define DHT_SENSOR_TYPE DHT11
DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

// Soil Humidity sensor 
int sensorvalu[] = {analogRead(A6),0,0};

// Light Sensor - HW-486 signal pin → A5
const int lightSensorPin = A5;

// Water Pump 
int pump;
#define RELAY_PIN 12 

// Time
unsigned long previousMillis = 0;

// Water Pump Duration
const long pumpInterval = 5000;   // 5 seconds

// Flash drive (Preferences)
Preferences prefs;
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t setting1;
  uint8_t setting2;
} schedule_t;

String processor(const String& var){
  Serial.println(var);
  return String();
}

// Real-time Clock
String printDateTime(const RtcDateTime& dt) {
  char datestring[17];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u"),
             dt.Day(),
             dt.Month(),
             dt.Year(),
             dt.Hour(),
             dt.Minute());
  return String(datestring);
}