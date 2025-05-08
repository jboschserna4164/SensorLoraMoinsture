#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "LoRaBoards.h"

#ifndef CONFIG_RADIO_FREQ
#define CONFIG_RADIO_FREQ 915.0
#endif
#ifndef CONFIG_RADIO_OUTPUT_POWER
#define CONFIG_RADIO_OUTPUT_POWER 17
#endif
#ifndef CONFIG_RADIO_BW
#define CONFIG_RADIO_BW 125.0
#endif

const char *ssid = "UPBWiFi";

void conectarWiFi()
{
    WiFi.begin(ssid);
    Serial.print("Conectando a WiFi");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado. IP local: " + WiFi.localIP().toString());
}

void esperarSincronizacionHora()
{
    configTime(-5 * 3600, 0, "pool.ntp.org"); // UTC-5 Colombia
    time_t now = time(nullptr);
    while (now < 1700000000)
    {
        delay(500);
        now = time(nullptr);
    }
}

String obtenerTimestampISO()
{
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", tm_info);
    return String(buffer);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    conectarWiFi();
    esperarSincronizacionHora();

    setupBoards();
    delay(1500);
    Serial.println("LoRa Receiver");

#ifdef RADIO_TCXO_ENABLE
    pinMode(RADIO_TCXO_ENABLE, OUTPUT);
    digitalWrite(RADIO_TCXO_ENABLE, HIGH);
#endif

    LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
    if (!LoRa.begin(CONFIG_RADIO_FREQ * 1000000))
    {
        Serial.println("Fallo al iniciar LoRa");
        while (1)
            ;
    }

    LoRa.setTxPower(CONFIG_RADIO_OUTPUT_POWER);
    LoRa.setSignalBandwidth(CONFIG_RADIO_BW * 1000);
    LoRa.setSpreadingFactor(10);
    LoRa.setPreambleLength(16);
    LoRa.setSyncWord(0xAB);
    LoRa.setCodingRate4(7);
    LoRa.disableCrc();
    LoRa.disableInvertIQ();
    LoRa.receive();
}

void loop()
{
    int packetSize = LoRa.parsePacket();
    if (!packetSize)
        return;

    String recv = "";
    while (LoRa.available())
    {
        recv += (char)LoRa.read();
    }
    Serial.println("Recibido: " + recv);

    // Extraer pares clave:valor
    String id = "", ip = "", tipo = "", valor = "", puerto = "";
    int start = 0;
    while (start < recv.length())
    {
        int sep = recv.indexOf(',', start);
        if (sep == -1)
            sep = recv.length();
        String par = recv.substring(start, sep);
        int colon = par.indexOf(':');
        if (colon > 0)
        {
            String key = par.substring(0, colon);
            String val = par.substring(colon + 1);
            if (key == "id")
                id = val;
            else if (key == "ip")
                ip = val;
            else if (key == "p")
                puerto = val;
            else if (key == "h" || key == "t" || key == "r" || key == "l" || key == "m")
            {
                tipo = key;
                valor = val;
            }
        }
        start = sep + 1;
    }

    if (id == "" || ip == "" || puerto == "" || tipo == "" || valor == "")
    {
        Serial.println("❌ Mensaje incompleto, no se envía.");
        return;
    }

    String timestamp = obtenerTimestampISO();

    // Traducir abreviatura del tipo a nombre legible
    String nombreTipo = "";

    if (tipo == "h")
        nombreTipo = "humedad";
    else if (tipo == "t")
        nombreTipo = "temperatura";
    else if (tipo == "r")
        nombreTipo = "radiacion";
    else if (tipo == "l")
        nombreTipo = "luz";
    else if (tipo == "m")
        nombreTipo = "moisture";
    else
        nombreTipo = tipo;

    // Construir JSON estructurado
    String json = "{";
    json += "\"id\":\"" + id + "\",";
    json += "\"type\":\"" + nombreTipo + "\",";
    json += "\"" + nombreTipo + "\":{";
    json += "\"value\":" + valor + ",";
    json += "\"type\":\"Float\"";
    json += "},";
    json += "\"timestamp\":\"" + timestamp + "\"";
    json += "}";

    String url = "http://" + ip + ":" + puerto + "/recibir";

    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(json);
        if (httpCode > 0)
        {
            Serial.printf("✅ Enviado a %s (HTTP %d)\n", url.c_str(), httpCode);
            Serial.println("📨 Respuesta: " + http.getString());
        }
        else
        {
            Serial.printf("❌ Error HTTP POST: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    }
    else
    {
        Serial.println("❌ WiFi desconectado");
    }

    LoRa.receive();
}
