#include "Arduino.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

#ifndef APSSID
#define APSSID "ESPap"
#define APPSK  "thereisnospoon"
#define rxPin D4
#define txPin D3
#endif

const char *ssid = APSSID;
const char *password = APPSK;

String phone = "+79000000000";
float threshold = 500;
float value = 0;
String mqStatus = "";
bool alarmTriggered = false;

const char indexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>
    <title>Gas leak detector</title>
    <style>
        body,
        html {
            height: 100%;
        }
        table {
            border: none;
            border-collapse: collapse;
            border-spacing: 0;
            width: 70%;
            height: 100%;
            margin-left: auto;
            margin-right: auto;
        }
        th,
        td {
            border-style: solid;
            border-width: 0px;
            font-family: Arial, sans-serif;
            text-align: center;
        }
        thead {
            font-size: 24px;
            height: 10%;
        }
        tfoot {
            height: 10%;
        }
        input {
            width: 70%;
            height: 30px;
        }
        .titles {
            height: 10%;
            background-color: beige;
            font-size: 20px;
        }
        .value {
            width: 50%;
            font-size: 100px;
            background-color: rgb(252, 204, 145);
        }
        .settings {
            width: 50%;
            background-color: rgb(197, 255, 236);
        }
        .save {
            height: 50px;
            width: 50%;
        }
    </style>
</head>

<body>
    <table>
        <thead>
            <tr>
                <th colspan="2">Gas leak detector</th>
            </tr>
        </thead>
        <tbody>
            <tr class="titles">
                <td>Current value</td>
            </tr>
            <tr>
                <td class="value" id="value">0</td>
            </tr>
            <tr class="titles">
                <td>Settings</td>
            </tr>
            <tr>
                <td class="settings">
                    <form>
                        <p class="labels">
                            <label for="threshold">Threshold</label>
                        </p>
                        <input id="threshold" type="number" name="threshold">
        
                        <p class="labels">
                            <label for="phone">Phone</label>
                        </p>
                        <input id="phone" type="tel" name="phone">
                        
                        <p>
                            <input class="save" type="button" value="save" onclick="postData()"></input>
                        </p>
                    </form>
                </td>
            </tr>
        </tbody>
        <tfoot>
            <tr>
                <td colspan="2">whatsapp: +79000000000<br>email: email@mail.ru</td>
            </tr>
        </tfoot>
    </table>
    <script>
        var baseUrl = "http://192.168.4.1/";
        function getValue() {
            var url = baseUrl + "api/value";

            var xhr = new XMLHttpRequest();
            xhr.open("GET", url);

            xhr.setRequestHeader("Accept", "*/*");

            xhr.onreadystatechange = function () {
                if (xhr.readyState === 4) {
                    if (xhr.status === 200) {
                        const dt = JSON.parse(xhr.responseText);
                        if (dt.status === "bad") {
                          document.getElementById("value").innerHTML = "error sensor connection";
                        } else {
                          document.getElementById("value").innerHTML = dt.value;
                        }
                    } else {
                        document.getElementById("value").innerHTML = "error WIFI connection";
                    }
                }
            };

            xhr.send();
        }
        function getData() {
            var url = baseUrl + "api/data";

            var xhr = new XMLHttpRequest();
            xhr.open("GET", url);

            xhr.setRequestHeader("Accept", "*/*");

            xhr.onreadystatechange = function () {
                if (xhr.readyState === 4) {
                    if (xhr.status === 200) {
                        const dt = JSON.parse(xhr.responseText);
                        document.getElementById("threshold").value = dt.threshold;
                        document.getElementById("phone").value = dt.phone;
                    } else {
                        document.getElementById("threshold").value = "error";
                        document.getElementById("phone").value = "error";
                    }
                }
            };

            xhr.send();
        }
        function postData() {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", baseUrl + "api/data");

            xhr.setRequestHeader("Accept", "application/json");
            xhr.setRequestHeader("Content-Type", "application/json");

            xhr.onreadystatechange = function () {
                if (xhr.readyState === 4) {
                    if (xhr.status != 201) {
                      alert("Can't update settings");
                    }
                }
            };

            let phone = document.getElementById("phone").value;
            let threshold = document.getElementById("threshold").value;

            var data = `{
                "threshold": "${threshold}",
                "phone": "${phone}"
            }`;

            if (phone.match(/\+?\d/g).length === 11 && (threshold > 0 && threshold <= 1000)) {
              xhr.send(data);
            } else {
              alert("Incorrect settings");
            }
        }
        getData();
        self.setInterval(() =>
            getValue()
            , 1000);
    </script>
</body>

</html>
)rawliteral";

/* Go to http://192.168.4.1 in a web browser */
ESP8266WebServer server(80);
SoftwareSerial sim900(rxPin,txPin);

void initRoute() {
  server.send(200, "text/html", indexHtml);
}

void getData() {
  String response = "{";

  response+= "\"threshold\": \""+String(threshold)+"\"";
  response+= ",\"phone\": \""+phone+"\"";
  response+= ",\"value\": \""+String(value)+"\"";
    
  response+= "}";

  server.send(200, F("text/json"), response);
}

void getValue() {
  String response = "{";
  
  response+= "\"value\": \""+String(value)+"\"";
  response+= "\"status\": \""+mqStatus+"\"";

  response+= "}";
    
  server.send(200, F("text/json"), response);
}

void postData() {
  const String postBody = server.arg("plain");
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, postBody);

  if (error) {
    server.send(400, F("text/html"), "Error");
  } else {
    JsonObject jsonData = doc.as<JsonObject>();
    if (server.method() == HTTP_POST) {
      String jsonPhone = jsonData["phone"];
      phone = jsonPhone;
      String jsonThreshold = jsonData["threshold"];
      threshold = jsonThreshold.toFloat();
      
      server.send(201, F("text/html"), "Success");
    }
  }
}

void sendSMS(String text)
{
  // mobile phone number to send message
  sim900.print(F("AT+CMGS=\""));
  sim900.print(phone);
  sim900.println(F("\"\r"));
  delay(1000);

  sim900.print(text);// messsage content
  delay(100);

  sim900.println((char)26);// ASCII code of CTRL+Z // finish
  delay(1000);
}

void initSIM900() {
  delay(1000);// Time to initialize module
  sim900.println(F("AT+CMGF=1\r"));// AT command to set SIM900 to SMS mode
  delay(100);
  sim900.println("AT+CMGD=1,4\r");// Deletes all SMS saved in SIM memory
  delay(100);
  sim900.println(F("AT+CNMI=2,2,0,0,0\r"));// Set module to send SMS data to serial out upon receipt
  delay(1000);
}

void checkTestCmd() {
  if (sim900.readString().indexOf("Test") > 0) {
    String msg = "Status: " + mqStatus + ", value: " + String(value);
    sendSMS(msg);
  }
}

void setup() {
  delay(1000);
  Serial.begin(9600);
  sim900.begin(9600);

  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  
  server.on("/", initRoute);
  server.on(F("/api/data"), HTTP_GET, getData);
  server.on(F("/api/data"), HTTP_POST, postData);
  server.on(F("/api/value"), HTTP_GET, getValue);
  
  server.begin();
  
  Serial.println(myIP);
  Serial.println("Server started");

  initSIM900();
}

void loop() {
  if (sim900.available() > 0) {
    checkTestCmd();
  }
  
  server.handleClient();

  value = analogRead(A0);

  if (isnan(value)) {
    mqStatus ="bad";
  } else {
    mqStatus ="good";
  }

  if (value > threshold && !isnan(value)) {
    if (!alarmTriggered) {
//      sendSMS("Gas leak detected!");
      alarmTriggered = true;
    }
  }

  if (value < threshold - threshold * 0.1) {
    alarmTriggered = false;
  }
}
