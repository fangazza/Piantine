/*
  Websocket Setup
*/
const char* hostname;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void notifyClients() {
  int water = analogRead(A6);
  int battery = analogRead(A3);
  int humi  = dht_sensor.readHumidity();
  int temp = dht_sensor.readTemperature();
  int lightValue = analogRead(lightSensorPin);  // 0 (bright) 4095 (dark)
  
  unsigned long allSeconds=millis()/1000;
  int runHours= allSeconds/3600;
  int secsRemaining=allSeconds%3600;
  int runMinutes=secsRemaining/60;
  int runSeconds=secsRemaining%60;
  String totTime = String(runHours) + ':' + String(runMinutes) + ':' + String(runSeconds);

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  String DTnow = printDateTime(compiled);

  // Define data array
  JSONVar myArray;
  myArray[0]=String(water);
  myArray[1]=String(humi);
  myArray[2]=String(temp);
  myArray[3]=String(pump);
  myArray[4]=String(totTime);
  myArray[5]=String(battery);  
  myArray[6]=String(lightValue);
  myArray[7]=String(DTnow);
  myArray[8]=String(hostname);  
  myArray[9]=String(prefs.getInt("counter"));
  String jsonString = JSON.stringify(myArray);  
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;

    // Water Pump
    if (strcmp((char*)data, "toggle") == 0) {      
      if (pump==0) {
        digitalWrite(RELAY_PIN, HIGH);  // Pump ON
        Serial.println("Water Pump ON");
        pump = 1;        
      } else{
        digitalWrite(RELAY_PIN, LOW);  // Pump OFF  
        Serial.println("Water Pump OFF");
        pump = 0;
      }
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}