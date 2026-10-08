#include <dummy.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Preferences.h>

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

// WiFi
const char* ssid = "Rick&Morty";
const char* password = "PippiCalzelunghe";
const char* hostname;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Piantine</title>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  :root {
    --bg-color: rgb(40,40,40);
    --bg-color02: rgb(60,60,60);
    --topnav-color: rgb(33,33,33);
    --card-color: rgb(100,100,100);
    --card-head-color: rgb(60,60,60);
    --waterbtn: #3a3;
    --stripe-color0: #88d;
    --stripe-color1: #44a;
    --stripe-color2: #66c;
  }
  html {
    width: 100%%;
    height: 100%%;
  }
  body {
    font-family: sans-serif;
    margin: 0;
    line-height: 1;
    width: 100%%;
    height: 100%%;
    background-color: #2a4a28;
    background: radial-gradient(circle,var(--bg-color02) 0%%, var(--bg-color) 100%%);
  }  
  .errorTxt {
    color: red;
    font-style: italic;
  }
  .topnav {
    overflow: hidden;
    background-color: var(--topnav-color);
    border-bottom: 1px solid rgba(0,0,0,0.5);
  }
  h1 {
    font-size: 18pt;
    color: white;
    margin:0;
    padding: 0.5em 2.5em;
    line-height: 1;
    font-weight: bold;
    font-family: courier;
    letter-spacing: .1em;
    background-position: 0.5em 50%%;
    background-size: 1.5em auto;
    background-repeat: no-repeat;
    background-image: url("data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz4NCjwhRE9DVFlQRSBzdmcgUFVCTElDICItLy9XM0MvL0RURCBTVkcgMS4xLy9FTiIgImh0dHA6Ly93d3cudzMub3JnL0dyYXBoaWNzL1NWRy8xLjEvRFREL3N2ZzExLmR0ZCI+DQo8c3ZnIHZlcnNpb249IjEuMSIgaWQ9Il94MzJfIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHhtbG5zOnhsaW5rPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hsaW5rIiANCgkgd2lkdGg9IjgwMHB4IiBoZWlnaHQ9IjgwMHB4IiB2aWV3Qm94PSIwIDAgNTEyIDUxMiIgIHhtbDpzcGFjZT0icHJlc2VydmUiPg0KPHN0eWxlIHR5cGU9InRleHQvY3NzIj4NCjwhW0NEQVRBWw0KCS5zdDB7ZmlsbDojRkZGRkZGO30NCl1dPg0KPC9zdHlsZT4NCjxnPg0KCTxwYXRoIGNsYXNzPSJzdDAiIGQ9Ik0yMTQuMiwzMTkuOTkxTDIxNC4yLDMxOS45OTFMMjE0LjIsMzE5Ljk5MUMxOTcuMzQsMzAxLjU4NSwyMzIuMjkzLDE4MC4zMTksMjcuMDc1LDk3LjU2OQ0KCQljLTMzLjc1LDYuMTI1LTcxLjY4OCwxNzYuNjcyLDEyNy43MTksMjI4Ljg0NGM1OS42MDksMjkuNzk3LDUwLjIxOSw3OS40NjksNTAuMjE5LDEwNy4wNzhoMzguMzQ0DQoJCWMwLDAtMTUuNjg4LTg3LjU3OCw1NC4zNzUtMTE5LjY3MmM0Ny42NzItMjEuODI4LDE3OS4zMjgtMjguODI4LDIxNC4yNjUtMjI1LjU0N0M0NDUuMzg3LDQ5Ljc1NywyMDguODI1LDEyNC44MzUsMjE0LjIsMzE5Ljk5MXoNCgkJIE0xNjUuODQsMjg2LjgwNGMtNC4wNjMsMi44MTMtOS42NDEsMS43OTctMTIuNDUzLTIuMjY2Yy0xOS41MTYtMjguMjY2LTczLjQ4NC05My40ODQtOTcuMzQ0LTExNy4yODENCgkJYy0zLjUtMy40ODQtMy41LTkuMTU2LDAtMTIuNjQxYzMuNDg0LTMuNSw5LjE1Ni0zLjUsMTIuNjQxLDBjMjUuMzQ0LDI1LjM5MSw3OC42ODgsODkuODU5LDk5LjQyMiwxMTkuNzM0DQoJCUMxNzAuOTE4LDI3OC40MTMsMTY5LjkwMywyODMuOTkxLDE2NS44NCwyODYuODA0eiBNNDE1Ljk2NSwxNDEuNjYzYy02NS4yNSw0MS4yMzQtMTE4LjU3OCwxMDkuODQ0LTE0Mi42MjUsMTQ3LjEwOQ0KCQljLTIuNjcyLDQuMTQxLTguMjE5LDUuMzI4LTEyLjM1OSwyLjY1NmMtNC4xNDEtMi42ODgtNS4zMjgtOC4yMTktMi42NDEtMTIuMzc1YzI1LjE0MS0zOC43NjYsNzkuMTQxLTEwOC43NjYsMTQ4LjA0Ny0xNTIuNQ0KCQljNC4xNzItMi42NTYsOS42ODgtMS40MDYsMTIuMzQ0LDIuNzY2QzQyMS4zNzIsMTMzLjQ5MSw0MjAuMTM3LDEzOS4wMDcsNDE1Ljk2NSwxNDEuNjYzeiIvPg0KPC9nPg0KPC9zdmc+");
  }
  .error {
    color: red;
    font-weight: bold;
    font-style: italic;
  }
  .content {
    padding: 0;
    margin: 1em auto;    
    width: 100vw;
    height: auto;
    display: flex;
    flex-direction: row;
    flex-flow: wrap;
    justify-content: space-around;
    align-content: space-around;
  }
  .card {
    background-color: var(--card-color);
    box-shadow: 0 0 0 1px rgba(0,0,0,0.2);
    padding: 0;
    margin: 0;
    text-align: center;
    border-radius: 0.6em;
    flex-grow: 1;
    align-self: auto;
    align-content: space-between;
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    min-width: 20vw;
    max-width: 24vw;
    height: max-content;
    min-height: 15em;
    max-height: max-content;    
  }  
  .card.sys {
    font-size: 11pt;
    line-height: 1.5;
  }
  h2 {
    font-size: 15pt;
    line-height: 1;
    margin: 0;
    background-color: var(--card-head-color);
    border-radius: 0.5em 0.5em 0 0;
    padding: 0.5em 1em 0.5em 2em;
    color: #ddd;
    text-align: left;
    font-weight: normal;
  }
  .state {
    font-size: 1.5em;
    color: #fff;
    margin: 0;
    padding: 0 0 0 1.75em;
    text-align: left; 
    width: auto;
    display: inline-block;
    align-self: anchor-center;
  }
  .water{
    background-image: url("data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz48IS0tIFVwbG9hZGVkIHRvOiBTVkcgUmVwbywgd3d3LnN2Z3JlcG8uY29tLCBHZW5lcmF0b3I6IFNWRyBSZXBvIE1peGVyIFRvb2xzIC0tPgo8c3ZnIGZpbGw9IiNGRkZGRkYiIHdpZHRoPSI4MDBweCIgaGVpZ2h0PSI4MDBweCIgdmlld0JveD0iMCAwIDE2IDE2IiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPg0KPHBhdGggZD0iTTgsMTEuMDlhMS40MSwxLjQxLDAsMCwwLS4yLS43MSwxLjc0LDEuNzQsMCwwLDEtLjE2LS4yM2wtLjI2LS4zOGMtLjA4LS4xMi0uMTYtLjI3LS4yNi0uNDVhNCw0LDAsMCwxLS4yMS0uNTIuMi4yLDAsMCwwLS4yMS0uMTYuMi4yLDAsMCwwLS4yMi4xNiw0LDQsMCwwLDEtLjIxLjUyQTUsNSwwLDAsMSw2LDkuNzdsLS4yNi4zOGMtLjA5LjE1LS4xNS4yMy0uMTUuMjNhMS4zMywxLjMzLDAsMCwwLS4yMS43MSwxLjI4LDEuMjgsMCwwLDAsLjM4LjkyLDEuMywxLjMsMCwwLDAsLjkzLjM4QTEuMywxLjMsMCwwLDAsOCwxMS4wOVptNS4yMi0xLjMxYTUsNSwwLDAsMS0xLjUzLDMuNjlBNSw1LDAsMCwxLDgsMTVhNSw1LDAsMCwxLTMuNjktMS41M0E1LDUsMCwwLDEsMi43OCw5Ljc4LDUuMjIsNS4yMiwwLDAsMSwzLjYxLDdsLjYzLS45MkM0LjYzLDUuNSw1LDUsNS4yNyw0LjUyYTIwLjI3LDIwLjI3LDAsMCwwLDEtMS44MkExMi43LDEyLjcsMCwwLDAsNy4xMy42NS44OS44OSwwLDAsMSw3LjQ4LjE3YS44OC44OCwwLDAsMSwxLDAsLjg0Ljg0LDAsMCwxLC4zNS40OCwxMy4xNiwxMy4xNiwwLDAsMCwuODQsMi4wNiwxOC45NCwxOC45NCwwLDAsMCwxLDEuODFjLjMuNDcuNjQsMSwxLDEuNTRsLjYzLjkyYTUuMTIsNS4xMiwwLDAsMSwuODMsMi44WiIvPg0KPC9zdmc+");
    background-size: auto 1em;
    background-repeat: no-repeat;
    background-position: 0.5em 50%%;
  }
  .air{
    background-image: url('data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz48IS0tIFVwbG9hZGVkIHRvOiBTVkcgUmVwbywgd3d3LnN2Z3JlcG8uY29tLCBHZW5lcmF0b3I6IFNWRyBSZXBvIE1peGVyIFRvb2xzIC0tPgo8c3ZnIGZpbGw9IiNGRkZGRkYiIHdpZHRoPSI4MDBweCIgaGVpZ2h0PSI4MDBweCIgdmlld0JveD0iMCAwIDIwIDIwIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPjxwYXRoIGQ9Ik0yLjY0MyA2LjM1N2MxLjc0Ny0xLjUgMy4xMjctMi42ODYgNi44NzItLjU3IDEuNzk5IDEuMDE2IDMuMjUgMS40IDQuNDU3IDEuMzk4IDIuMTE1IDAgMy40ODYtMS4xNzYgNC42NzEtMi4xOTNhMS4wMzcgMS4wMzcgMCAwIDAgLjEyMi0xLjQzOS45ODcuOTg3IDAgMCAwLTEuNDEtLjEyNWMtMS43NDYgMS41MDItMy4xMjcgMi42ODgtNi44NzIuNTctNC45NDgtMi43OTMtNy4yNjYtLjgwMy05LjEyOC43OTdhMS4wMzcgMS4wMzcgMCAwIDAtLjEyMSAxLjQzOS45ODYuOTg2IDAgMCAwIDEuNDA5LjEyM3ptMTQuNzEyIDIuMTc4Yy0xLjc0NiAxLjUtMy4xMjcgMi42ODgtNi44NzIuNTctNC45NDgtMi43OTUtNy4yNjYtLjgwNC05LjEyOC43OTVhMS4wMzcgMS4wMzcgMCAwIDAtLjEyMSAxLjQzOS45ODYuOTg2IDAgMCAwIDEuNDA5LjEyNWMxLjc0Ny0xLjUwMSAzLjEyNy0yLjY4NyA2Ljg3Mi0uNTcyIDEuNzk5IDEuMDE4IDMuMjUgMS40IDQuNDU3IDEuNCAyLjExNSAwIDMuNDg2LTEuMTc2IDQuNjcxLTIuMTk1YTEuMDM1IDEuMDM1IDAgMCAwIC4xMjItMS40MzguOTg2Ljk4NiAwIDAgMC0xLjQxLS4xMjR6bTAgNS4xMDZjLTEuNzQ2IDEuNTAyLTMuMTI3IDIuNjg4LTYuODcyLjU3Mi00Ljk0OC0yLjc5NS03LjI2Ni0uODA1LTkuMTI4Ljc5NWExLjAzNyAxLjAzNyAwIDAgMC0uMTIxIDEuNDM5Ljk4NS45ODUgMCAwIDAgMS40MDkuMTIzYzEuNzQ3LTEuNSAzLjEyNy0yLjY4NSA2Ljg3Mi0uNTcgMS43OTkgMS4wMTYgMy4yNSAxLjQgNC40NTcgMS40IDIuMTE1IDAgMy40ODYtMS4xNzggNC42NzEtMi4xOTVhMS4wMzcgMS4wMzcgMCAwIDAgLjEyMi0xLjQzOS45ODguOTg4IDAgMCAwLTEuNDEtLjEyNXoiLz48L3N2Zz4=');
    background-size: auto 1em;
    background-repeat: no-repeat;
    background-position: 0.5em 50%%;    
  }
  .vase{
    background-image: url('data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz4NCjwhRE9DVFlQRSBzdmcgUFVCTElDICItLy9XM0MvL0RURCBTVkcgMS4xLy9FTiIgImh0dHA6Ly93d3cudzMub3JnL0dyYXBoaWNzL1NWRy8xLjEvRFREL3N2ZzExLmR0ZCI+DQo8c3ZnIHZlcnNpb249IjEuMSIgaWQ9Il94MzJfIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHhtbG5zOnhsaW5rPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hsaW5rIiBmaWxsPSIjRkZGRkZGIiB2aWV3Qm94PSIwIDAgNTEyIDUxMiIgIHhtbDpzcGFjZT0icHJlc2VydmUiPg0KPGc+DQoJPHBhdGggY2xhc3M9InN0MCIgZD0iTTM3My4wNDEsMjEwLjE2N2MtMjkuOTUyLTI5Ljk1Mi03MC45MzUtMzkuNjIxLTcwLjkzNS0xMDYuMjI2QzMwMi4xMDUsNTcuNzQ0LDMyNS4yODksMCwzMjUuMjg5LDBIMTg2LjY5Ng0KCQljMCwwLDIzLjE4Myw1Ny43NDQsMjMuMTgzLDEwMy45NDJjMCw2Ni42MDUtNDAuOTgyLDc2LjI3My03MC45MzQsMTA2LjIyNmMtMjkuOTUzLDI5Ljk1My00OC40NzUsNzEuMzQ0LTQ4LjQ3NSwxMTcuMDQ5DQoJCWMwLDI5LjAwNiw3LjQ2MSw1Ni4yNjcsMjAuNTY4LDc5Ljk3M2MxMy4xMjIsMjMuNjk5LDMxLjg5MSw0My44MzYsNTQuNDc0LDU4LjYyMUgyNTZoOTAuNDczDQoJCWMyMi41OTktMTQuNzg0LDQxLjM2Ny0zNC45MjIsNTQuNDc0LTU4LjYyMWMxMy4xMjItMjMuNzA2LDIwLjU4NC01MC45NjcsMjAuNTg0LTc5Ljk3Mw0KCQlDNDIxLjUzMSwyODEuNTExLDQwMy4wMDksMjQwLjEyLDM3My4wNDEsMjEwLjE2N3oiLz4NCgk8cmVjdCB4PSIxNjMuNjA0IiB5PSI0ODEuMjAxIiBjbGFzcz0ic3QwIiB3aWR0aD0iMTg0Ljc3NiIgaGVpZ2h0PSIzMC43OTkiLz4NCjwvZz4NCjwvc3ZnPg==');
    background-size: auto 1em;
    background-repeat: no-repeat;
    background-position: 0.5em 50%%;    
  }
  .temp{
    background-image: url('data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz4KDTwhLS0gVXBsb2FkZWQgdG86IFNWRyBSZXBvLCB3d3cuc3ZncmVwby5jb20sIEdlbmVyYXRvcjogU1ZHIFJlcG8gTWl4ZXIgVG9vbHMgLS0+Cjxzdmcgd2lkdGg9IjgwMHB4IiBoZWlnaHQ9IjgwMHB4IiB2aWV3Qm94PSIwIDAgMjQgMjQiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+CiAgICA8Zz4KICAgICAgICA8cGF0aCBmaWxsPSJub25lIiBkPSJNMCAwaDI0djI0SDB6Ii8+CiAgICAgICAgPHBhdGggZmlsbC1ydWxlPSJub256ZXJvIiBmaWxsPSIjRkZGRkZGIiBkPSJNOCA1YTQgNCAwIDEgMSA4IDB2NS4yNTVhNyA3IDAgMSAxLTggMFY1em0xLjE0NCA2Ljg5NWE1IDUgMCAxIDAgNS43MTIgMEwxNCAxMS4yOThWNWEyIDIgMCAxIDAtNCAwdjYuMjk4bC0uODU2LjU5N3pNOCAxNmg4YTQgNCAwIDEgMS04IDB6Ii8+CiAgICA8L2c+Cjwvc3ZnPg==');
    background-size: auto 1em;
    background-repeat: no-repeat;
    background-position: 0.5em 50%%;    
  }
  .humi{
    background-image: url('data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz4NCjxzdmcgZmlsbD0iI0ZGRkZGRiIgd2lkdGg9IjgwMHB4IiBoZWlnaHQ9IjgwMHB4IiB2aWV3Qm94PSIwIDAgMjQgMjQiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTEyLDIyYzIuNTc5LDAsNC0xLjM1LDQtMy44LDAtMy4yNDMtMy4yMzctNS44NzEtMy4zNzUtNS45ODFhMSwxLDAsMCwwLTEuMjUsMEMxMS4yMzcsMTIuMzI5LDgsMTQuOTU3LDgsMTguMiw4LDIwLjY1LDkuNDIxLDIyLDEyLDIyWm0wLTcuNjM5QTYuMTUzLDYuMTUzLDAsMCwxLDE0LDE4LjJjMCwxLjExMi0uMzM1LDEuOC0yLDEuOHMtMi0uNjg4LTItMS44QTYuMTUzLDYuMTUzLDAsMCwxLDEyLDE0LjM2MVpNNi42MjUsMi4yMTlhMSwxLDAsMCwwLTEuMjUsMEM1LjIzNywyLjMyOSwyLDQuOTU3LDIsOC4yLDIsMTAuNjUsMy40MjEsMTIsNiwxMnM0LTEuMzUsNC0zLjhDMTAsNC45NTcsNi43NjMsMi4zMjksNi42MjUsMi4yMTlaTTYsMTBjLTEuNjY1LDAtMi0uNjg4LTItMS44QTYuMTUzLDYuMTUzLDAsMCwxLDYsNC4zNjEsNi4xNTMsNi4xNTMsMCwwLDEsOCw4LjJDOCw5LjMxMiw3LjY2NSwxMCw2LDEwWk0xOC42MjUsMi4yMTlhMSwxLDAsMCwwLTEuMjUsMEMxNy4yMzcsMi4zMjksMTQsNC45NTcsMTQsOC4yYzAsMi40NSwxLjQyMSwzLjgsNCwzLjhzNC0xLjM1LDQtMy44QzIyLDQuOTU3LDE4Ljc2MywyLjMyOSwxOC42MjUsMi4yMTlaTTE4LDEwYy0xLjY2NSwwLTItLjY4OC0yLTEuOGE2LjE1Myw2LjE1MywwLDAsMSwyLTMuODM5QTYuMTUzLDYuMTUzLDAsMCwxLDIwLDguMkMyMCw5LjMxMiwxOS42NjUsMTAsMTgsMTBaIi8+PC9zdmc+');
    background-size: auto 1em;
    background-repeat: no-repeat;
    background-position: 0.5em 50%%;    
  }
  .info{
    background-image: url('data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iODAwcHgiIGhlaWdodD0iODAwcHgiIHZpZXdCb3g9IjAgMCAxMDI0IDEwMjQiIGZpbGw9IiNGRkZGRkYiICB2ZXJzaW9uPSIxLjEiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTUxMS45IDE4M2MtMTgxLjggMC0zMjkuMSAxNDcuNC0zMjkuMSAzMjkuMXMxNDcuNCAzMjkuMSAzMjkuMSAzMjkuMVM4NDEgNjkzLjkgODQxIDUxMi4yIDY5My42IDE4MyA1MTEuOSAxODN6IG0wIDU4NS4yYy0xNDEuMiAwLTI1Ni0xMTQuOC0yNTYtMjU2czExNC44LTI1NiAyNTYtMjU2IDI1NiAxMTQuOCAyNTYgMjU2LTExNC45IDI1Ni0yNTYgMjU2eiIgLz48cGF0aCBkPSJNNDc1LjQgMzY1LjdoNzMuMXYxODIuOWgtNzMuMXpNNDc1LjQgNTg1LjFoNzMuMXY3My4xaC03My4xeiIgLz48L3N2Zz4=');
    background-size: 1.5em;
    background-repeat: no-repeat;
    background-position: 0.5em 50%%;
  }
  .time {
    padding: 0;
  }
  .state.light {
    padding: 0;
    font-weight: bold;
  }
  .battery {
    font-size: 20pt;
    font-weight: bold;
    padding: 0.5em 1em;
  }
  .battery.warning {
      background-color: red;
      border-radius: 1em;
  }
  .waterPump.active .pump {
    animation: spin 2s linear infinite;
  }  
  @keyframes spin {
    0%% { transform: rotate(0deg); }
    100%% { transform: rotate(360deg); }
  }
  .waterPump.active .msg {
    height: 100%%;
    background-color: var(--stripe-color0);   
    background-image: repeating-linear-gradient(-45deg, var(--stripe-color1) 0, var(--stripe-color1) 20px, var(--stripe-color2) 21px, var(--stripe-color2) 40px); 
    animation: pan 2s linear infinite;
  }  
  @keyframes pan {
    from {
      background-position-x: 0;
    }
    to {
      background-position-x: 10rem;
    }
  }
  #toggleWater {
    max-width: 15em;
    font-size: 15pt;
    font-weight: bold;
    text-align: center;
    margin: 0;
    padding: 0.5em 1em;
    border: 0;
    border-radius: 1em;
    font-family: sans-serif;
    background-color: var(--waterbtn);
    color: #fff;
    text-shadow: 0 1px 1px rgba(0,0,0,0.65);
  }
  .waterPump.active #toggleWater {
    background-color: #66d;
  }
  .pump{
    background-image: url('data:image/svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0idXRmLTgiPz4NCjxzdmcgd2lkdGg9IjgwMHB4IiBoZWlnaHQ9IjgwMHB4IiB2aWV3Qm94PSIwIDAgMjQgMjQiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+DQo8cGF0aCBkPSJNMTgsMTJhNiw2LDAsMCwwLTUuNzQtNmwzLTNMMTMuODMsMS42MSw3LjcyLDcuNzJsMCwwYTYsNiwwLDAsMCw0LDEwLjIzbC0zLDMsMS40MiwxLjQxLDYuMTEtNi4xMSwwLDBBNiw2LDAsMCwwLDE4LDEyWm0tNiw0YTQsNCwwLDEsMSw0LTRBNCw0LDAsMCwxLDEyLDE2WiIgZmlsbD0iI0ZGRkZGRiIvPjwvc3ZnPg==');
    background-size: 4em;
    background-repeat: no-repeat;
    background-position: 50%% 50%%;
    width: 4em;
    height: 4em;
    display: block;
    padding: 0;
    margin: 0;   
    border: 3px solid #fff;
    border-radius: 50%%;
    animation: none;
  }
  .pumpBtn{
    display: flex;
    justify-content: space-evenly;
    align-content: center;
    align-items: center;
  }

  #val {
    font-weight: bold;
  }  
  #state {
    position: fixed;
    display: block;
    width: 100%%;
    margin: 0;
    padding: 1.5em 0;
    bottom: 0;
    left: 0;
    right: 0;
    background-color: rgba(0,0,0,0.5);
    color: #999;
    font-size: 14pt;
    font-family: courier;
    font-weight: normal;
    text-align: center;
  }
  #state::after {
    content: 'DATA';
    position: absolute;
    top: 0;
    left: 0;
    padding: 0.5em;
    background-color: #000;
  }
  .gauge-container {
    width: 94%%;
    height: 2em;
    margin: 0 auto;
    line-height: 2em;
    background: linear-gradient(90deg,rgba(230, 0, 0, 1) 0%%, rgba(230, 224, 71, 1) 50%%, rgba(3, 179, 0, 1) 100%%);
    border-radius: 1em;
    position: relative;
  }
  .gauge {
    --valu: attr(data-percentage);
    display: block;
    border-radius: 1em;
    height:1.5em;
    width:1.5em;
    background-color: #fff;
    position: absolute;
    top:0;
    border: 0.25em solid #fff;
    line-height: 1.5em;
    color: #000;
    font-weight: bold;
  }
  .msg {
    padding: 0.5em 0;
    background-color: rgba(0,0,0,0.1);
    display: inline-block;
    border-radius: 0 0 .5em .5em;
    font-size: 14pt;
    font-weight: bold;
    color: #000;
    margin: 0;
    width: 100%%;
    min-height: 1em;
  }
  .msg.red {
    background-color: rgba(255,0,0,0.5);
  }
  .msg.yellow {    
    background-color: rgba(255,255,0,0.5);
  }
  .msg.green {
    background-color: rgba(0,0,255,0.5);
  }
  .msg:after {
    content:'';
    color: #fff;
    font-family: courier;
    font-weight: 600;
  }
  .waterPump .msg {
    color: #fff;
  }

  @media (max-width: 1279px) {
    .content {
      padding: 0;
      margin: 0;
    }
    .card {
      max-width: 47vw;
      min-width: 47vw;
      width: auto;
      margin: 0.5em 0;
    }
  }
</style>
</head>
<body>
  <div class="topnav">
    <h1>Piantine</h1>
  </div>
  <div class="content">
    <div class="card">
      <h2 class="vase">Terra</h2>
      <div class="gauge-container">
        <div class="gauge" data-percentage></div>
      </div>      
      <p class="state water">Umidità Terreno: <span id="val"></span></p>
      <p class="msg"></p>
    </div>
    <div class="card">
      <h2 class="air">Aria</h2>  
      <p class="state temp">Temperatura: <span id="temp"></span></p>
      <p class="state humi">Umidità: </span><span id="hum"></span></p>
      <p class="msg"></p>
    </div>
    <div class="card waterPump">
      <h2 class="water">Acqua</h2>  
      <div class="pumpBtn">
        <div class="pump"></div>
        <button id="toggleWater">Attiva Pompa</button>
      </div>
      <p class="msg"></p>
    </div>    
    <div class="card sys">
      <h2 class="info">Info</h2>  
      <p class="state battery">Batteria: <span id="battVal"></span></p>
      <p class="state hostname">Hostname: <span id="hostname"></span></p>
      <p class="state light">Light: <span id="lightVal"></span></p>
      <p class="state time">Uptime: <span id="timeVal"></span></p>
      <p class="state date">Date & Time: <span id="dateVal"></span></p>
      <p class="state">Device riavviato <span id="reboot"></span> volte.</p>
      <p class="msg"></p>
    </div>        
  </div>
  <span id="state">%STATE%</span>
  <script>
    var valu;
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    var msgTxt;
    var dataPerc;
    var pumpStatus = 0;
    var pumpTxt;
    var myData;
    var battVal;

    window.addEventListener('load', onLoad);
    function map_range(value, low1, high1, low2, high2) {
      return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
    }

    function initWebSocket() {
      console.log('Trying to open a WebSocket connection...');
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen(event) {
      console.log('Connection opened');
    }

    function onClose(event) {
      console.log('Connection closed');
      setTimeout(initWebSocket, 2000);
    }

    function onMessage(event) {
      myData = JSON.parse( event.data );
      //console.log(myData);
      
      // Debug JSON Data
      document.getElementById('state').innerHTML = event.data;

      // Temp & Humi Data
      if (myData[2] < 999) {
        document.getElementById('temp').innerHTML = myData[2] + "°C";
        document.getElementById('temp').classList.remove('error');
      } else {
        document.getElementById('temp').innerHTML = "Error!";
        document.getElementById('temp').classList.add('error');
      }    

      if (myData[2] < 999) {
        document.getElementById('hum').innerHTML = myData[1] + "%%";
        document.getElementById('hum').classList.remove('error');
      } else {
        document.getElementById('hum').innerHTML = "Error!";
        document.getElementById('hum').classList.add('error');
      }    

      // Water Pump
      if(myData[3] == 0) {
        pumpTxt = "Attiva Pompa";
        document.querySelector('.waterPump').classList.remove('active');
        document.querySelector('.waterPump .msg').innerHTML = "Pompa Acqua Spenta";
      } else {
        pumpTxt = "Spegni Pompa";
        document.querySelector('.waterPump').classList.add('active');
        document.querySelector('.waterPump .msg').innerHTML = "Pompa Acqua Attiva";
             
      }
      document.getElementById('toggleWater').innerHTML = pumpTxt;

      // Soil 
      valu = Math.floor(map_range(myData[0], 4095, 900, 0, 100));
      document.getElementById('val').innerHTML = valu + "%%";
      dataPerc = document.querySelector('[data-percentage]');
      dataPerc.setAttribute('data-percentage', valu);
      dataPerc.innerHTML = valu;
      dataPerc.style.setProperty('left', valu + "%%");
      msgTxt = document.querySelector('.msg');
      msgTxt.classList.remove("red", "yellow", "green");    
              
      if(myData[0] >= 3000) {
        msgTxt.innerHTML = "Secco!";
        msgTxt.classList.add("red");
      }
      if(myData[0] < 3000 && myData[0] > 1500 ) {
        msgTxt.innerHTML = "Umido";
        msgTxt.classList.add("yellow");
      }
      if(myData[0] <= 1500) {
        msgTxt.innerHTML = "Bagnato";
        msgTxt.classList.add("green");
      }
      if (myData[0] > 4095 || myData[0] < 900) {
        document.getElementById('val').innerHTML = "ERROR!";
        document.getElementById('val').classList.add("errorTxt");
        valu = -1;
      }

      // Battery & Time
      battVal = Math.floor(map_range(myData[5], 0, 4096, 0, 100));
      document.getElementById('battVal').innerHTML = battVal + "%%";

      if (battVal < 15) {
        document.querySelector('.battery').classList.add('warning');
      } else {
        document.querySelector('.battery').classList.remove('warning');
      }

      document.getElementById('timeVal').innerHTML = myData[4] +"";
      document.getElementById('dateVal').innerHTML = myData[7] +"";

      // Light
      lightVal = Math.floor(map_range(myData[6], 4095, 0, 0, 100));
      document.getElementById('lightVal').innerHTML = lightVal + "%%";    

      // Hostname
      document.querySelector('#hostname').innerHTML = "" + myData[8];

      // Reboot counter
      document.querySelector('#reboot').innerHTML = "" + myData[9];
    }

    function onLoad(event) {
      initWebSocket();    
      initButton(); 
    }

    // Water pump button
    function initButton() {
        document.getElementById('toggleWater').addEventListener('click', onToggleWater);
    }

    // Water Pump
    function onToggleWater(event) {
        websocket.send('toggle');
    }
  </script>
</body>
</html>
)rawliteral";

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

  // Define JSON array
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
        //delay(1000);
  
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

String processor(const String& var){
  Serial.println(var);
  return String();
}

// void printDateTime(const RtcDateTime& dt) {
//   char datestring[20];

//   snprintf_P(datestring,
//              countof(datestring),
//              PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
//              dt.Day(),
//              dt.Month(),
//              dt.Year(),
//              dt.Hour(),
//              dt.Minute(),
//              dt.Second());
//   Serial.print(datestring);
// }

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


// --------------------------------------------------
void setup(){
  Serial.begin(115200);

  // Realtime Clock
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.println(printDateTime(compiled));

  if (!Rtc.IsDateTimeValid()) {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing
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
  
  // Custom hostname
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
    Example: how to use Preferences (nvs) to store a structure.
    Note that the maximum size of a putBytes is 496K
    or 97% of the nvs partition size.  nvs has significant overhead,
    so should not be used for data that will change often.
  */
  // Preferences
  prefs.begin("my-app");
  int counter = prefs.getInt("counter", 1); // default to 1
  Serial.print("Reboot count: ");
  Serial.println(counter);
  counter++;
  prefs.putInt("counter", counter);
  
  // Preferences
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

// --------------------------------------------------
void loop() {
  // RTClock
  RtcDateTime now = Rtc.GetDateTime();
  //printDateTime(now);
  //Serial.println();

  // Light Sensor
  int lightValue = analogRead(lightSensorPin);  // 0 (bright) 4095 (dark)
  //Serial.print("Light Sensor: ");
  //Serial.println(lightValue);

  // Battery
  int battLvl = analogRead(A3);
  //Serial.print("Battery: ");
  //Serial.println(battLvl);

  // Water Pump AUTO OFF after pumpInterval
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= pumpInterval && pump == 1)
  {
      previousMillis = currentMillis;
      digitalWrite(RELAY_PIN, LOW);  // Pump OFF;
      pump=0;
  }

  // Soil Water
  sensorvalu[0] = analogRead(A6);
  //Serial.print("Soil Humidity is: ");
  //Serial.println(sensorvalu);

  // Air humidity & temp  
  float humi  = dht_sensor.readHumidity();
  float tempC = dht_sensor.readTemperature();

  notifyClients();
  delay(500);
  ws.cleanupClients();
}
