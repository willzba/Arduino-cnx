#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>

// Variables globales para SSID y contraseña
String ssid;
String password;
String serverURL = "http://192.168.2.101/esp32/guardar_datos.php"; //192.168.2.101 Cambia la IP por la de tu servidor

// Pines y frecuencia de LoRa
#define SS 18
#define RST 14
#define DIO 26
#define BAND 915E6

// ID del receptor esperado
#define RECEIVER_ID "9525"

// Prototipos de funciones
void initializeSerial();
void requestWiFiCredentials();
void connectToWiFi();
void initializeLoRa();
void initializeTasks();

// Tarea para la reconexión automática
void WiFiReconnectTask(void *parameter) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado. Reconectando...");
      connectToWiFi();
    }
    vTaskDelay(pdMS_TO_TICKS(10000));  // Revisa cada 10 segundos
  }
}

// Tarea para recibir datos por LoRa y enviarlos al servidor
void receiveDataLoRaTask(void *parameter) {
  while (true) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String receivedData = "";
      while (LoRa.available()) {
        receivedData += (char)LoRa.read();
      }
      Serial.println("Datos recibidos por LoRa: " + receivedData);

      // Verificar si el paquete contiene el ID de recepción correcto
      if (receivedData.startsWith(RECEIVER_ID)) {
        Serial.println("ID de recepción válido detectado.");

        // Dividir los datos en partes (ID|temperature|humidity|lightState)
        int separatorIndex1 = receivedData.indexOf('|');
        int separatorIndex2 = receivedData.indexOf('|', separatorIndex1 + 1);
        int separatorIndex3 = receivedData.indexOf('|', separatorIndex2 + 1);

        if (separatorIndex1 != -1 && separatorIndex2 != -1 && separatorIndex3 != -1) {
          String temperature = receivedData.substring(separatorIndex1 + 1, separatorIndex2);
          String humidity = receivedData.substring(separatorIndex2 + 1, separatorIndex3);
          String ledState = receivedData.substring(separatorIndex3 + 1);

          // Enviar los datos al servidor
          if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            String url = serverURL + "?temperature=" + temperature + "&humidity=" + humidity + "&ledState=" + ledState;
            http.begin(url);
            int httpCode = http.GET();

            if (httpCode > 0) {
              Serial.println("Datos enviados al servidor correctamente");
              Serial.println(http.getString());
            } else {
              Serial.println("Error al enviar los datos al servidor");
            }
            http.end();
          } else {
            Serial.println("No se puede enviar los datos: WiFi no conectado");
          }
        } else {
          Serial.println("Error: formato de datos no válido");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Espera antes de revisar el siguiente paquete
}
}

void initializeSerial() {
  Serial.begin(9600);
}

void requestWiFiCredentials() {
  Serial.println("Introduce el nombre de la red Wi-Fi:");
  while (Serial.available() == 0) {}
  ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.println("Introduce la contraseña de la red Wi-Fi:");
  while (Serial.available() == 0) {}
  password = Serial.readStringUntil('\n');
  password.trim();
}

void connectToWiFi() {
  Serial.println("Intentando conectar a WiFi...");
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 5000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado a WiFi");
  } else {
    Serial.println("\nConexión fallida. Se solicitarán nuevamente los datos de la red.");
    requestWiFiCredentials();
    connectToWiFi();
  }
}

void initializeLoRa() {
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DIO);
  if (!LoRa.begin(BAND)) {
    Serial.println("Iniciando LoRa falló!");
    while (1);
  }
  Serial.println("LoRa inicializado correctamente");
}

void initializeTasks() {
  xTaskCreatePinnedToCore(WiFiReconnectTask, "WiFiReconnect", 4096, NULL, 1, NULL, 0);      // Núcleo 0
  xTaskCreatePinnedToCore(receiveDataLoRaTask, "ReceiveDataLoRa", 4096, NULL, 1, NULL, 1);  // Núcleo 1
}

void setup() {
  initializeSerial();
  requestWiFiCredentials();
  connectToWiFi();
  initializeLoRa();
  initializeTasks();
}

void loop() {
  // No se necesita código en el loop, ya que las tareas se manejan en segundo plano
}