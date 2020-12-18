#include "RTClib.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <WiFiClient.h>
#include <FS.h>
#include <DNSServer.h>
#include <WiFiManager.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 RTC;
char t[16];
char x[16];
const int pinBuzzer = D3;
int button = D4;
int buttonip = D5;
String Mensaje = "";
WebSocketsServer websocket = WebSocketsServer(81);
int minutos = 0;
int estado_boton = 0;
int estado_botonip = 0;
ESP8266WebServer server(80);

void setup() {
  lcd.init();
  lcd.backlight();
  RTC.begin();
  // Ajusto la fecha actual
  //RTC.adjust(DateTime(2020, 2, 14, 17, 3 , 5));
  Serial.begin(115200);

  //Nos conectamos al Wifi
  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  lcd.setCursor(0, 1);
  if (!wifiManager.autoConnect("CONFIGURACION ESP8266", "c0nf1gur@c10n")) {
    Serial.println("Fallo en la conexión (timeout)");
    ESP.reset();
    delay(1000);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    lcd.print(".");
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  Serial.println("");
  Serial.println("Wifi Conectado");
  Serial.println(WiFi.localIP());
  delay(2000);
  // Inicia el puerto I2C
  Wire.begin(D2, D1);
  lcd.clear();
  //esto es para la transferencia de archivos
  SPIFFS.begin();
  websocket.begin();
  websocket.onEvent(webSocketEvent);
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "Archivo no encontrado");
  });
  server.begin();
  Serial.println("Servidor HTTP iniciado");
  pinMode(button, INPUT);
  pinMode(buttonip, INPUT);
}

void loop() {
  if (Mensaje.length() > 0) {
    estado_boton = digitalRead(button);
    if (estado_boton == LOW ) {
      Mensaje = "";
      minutos = 0;
      websocket.broadcastTXT("True");
      noTone(pinBuzzer);
    }
    else
    {
      lcd.clear();
      lcd.print(Mensaje);
      lcd.setCursor(0, 1);
      lcd.print("Recordatorio nuevo");
      tone(pinBuzzer, 440, 300);
      delay(1000);
      minutos++;
    }
    if (minutos == 300)
    {
      Mensaje = "";
      websocket.broadcastTXT("False");
      minutos = 0;
    }
  } else {
    //iniciamos el módulo RTC
    DateTime fecha = RTC.now();
    int hora = fecha.hour();
    int minuto = fecha.minute();
    int segundo = fecha.second();
    //iniciamos el web socket y el servidor
    websocket.loop();
    server.handleClient();
    //Con esta línea enviamos el valor de la fecha al servidor por medio del web socket para que haga la comparación y reciba luego el valor de la base de datos
    sprintf(t, "%02d-%02d-%02d %02d:%02d:%02d", fecha.year(), fecha.month(), fecha.day(),  fecha.hour(), fecha.minute(), fecha.second());
    //desde aqui se envia a la java
    websocket.broadcastTXT(t);
    estado_botonip = digitalRead(buttonip);
    if (estado_botonip == LOW) {
      delay(50);
      if (estado_botonip == LOW ) {
        lcd.setCursor(0, 0);
        lcd.print("IP:");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());

        //Actualización de 1 seg.
        delay(1000);

        //Limpiar pantalla
        lcd.clear();
      }
    } else {
      //Mostrar la hora en la pantalla
      lcd.setCursor(0, 0);
      lcd.print(fecha.year());
      lcd.print('-');
      lcd.print(fecha.month());
      lcd.print('-');
      lcd.print(fecha.day());
      lcd.print(' ');
      lcd.setCursor(4, 1);
      lcd.print(fecha.hour());
      lcd.print(':');
      lcd.print(fecha.minute());
      lcd.print(':');
      lcd.print(fecha.second());
      //Actualización de 1 seg.
      delay(1000);
      //Limpiar pantalla
      lcd.clear();
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch (type) {
    case WStype_DISCONNECTED: {
        Serial.printf("Usuario #%u - Desconectado\n", num);
        break;
      }
    case WStype_CONNECTED: {
        IPAddress ip = websocket.remoteIP(num);
        Serial.printf("Nueva conexión: %d.%d.%d.%d Nombre: %s ID: %u\n", ip[0], ip[1], ip[2], ip[3], payload, num);
        String msj = Mensaje;
        websocket.broadcastTXT(msj);
        break;
      }
    case WStype_TEXT: {
        //aqui recibes desde el webscoket o java
        Serial.printf("Numero de conexion: %u - %s\n ", num, payload);
        //payload es la variable con el mensaje que llega desde java puto
        websocket.sendTXT(num, payload);
        Mensaje = (char*)payload;
        break;
      }
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
#ifdef DEBUG
  Serial.println("handleFileRead: " + path);
#endif
  if (path.endsWith("/")) path += "index.html";
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, getContentType(path));
    file.close();
    return true;
  }
  return false;
}
