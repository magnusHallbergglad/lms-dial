#include <M5Dial.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "secrets.h"

long dialPosition = 0;
bool playerMuted = false;
bool wifiAvailable = false;
bool lmsAvailable = false;
bool volumeChangePending = false;
unsigned long lastVolumeChangeAt = 0;
unsigned long lastReconnectAttemptAt = 0;
unsigned long lastLmsSyncAt = 0;

constexpr unsigned long reconnectIntervalMs = 5000;
constexpr unsigned long lmsSyncIntervalMs = 10000;

void drawMessage(const String& title, const String& detail = "") {
  M5Dial.Display.fillScreen(BLACK);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextColor(WHITE, BLACK);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawString(title, M5Dial.Display.width() / 2,
                            M5Dial.Display.height() / 2 - 18);

  if (!detail.isEmpty()) {
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString(detail, M5Dial.Display.width() / 2,
                              M5Dial.Display.height() / 2 + 22);
  }
}

bool connectToWiFi() {
  WiFi.mode(WIFI_STA);

  drawMessage("Wi-Fi", "Soker nat...");
  const int networkCount = WiFi.scanNetworks();
  bool networkFound = false;
  for (int i = 0; i < networkCount; ++i) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      networkFound = true;
      Serial.printf("Configured SSID found: channel %d, RSSI %d dBm\n",
                    WiFi.channel(i), WiFi.RSSI(i));
      break;
    }
  }
  WiFi.scanDelete();

  if (!networkFound) {
    wifiAvailable = false;
    Serial.println("Configured SSID was not found by M5Dial");
    drawMessage("SSID saknas", "Kontrollera 2,4 GHz");
    return false;
  }

  drawMessage("Wi-Fi", "Ansluter...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  constexpr unsigned long timeoutMs = 15000;
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    wifiAvailable = false;
    Serial.printf("Connection failed, WiFi status: %d\n", WiFi.status());
    drawMessage("Wi-Fi nekad", "Kontrollera losenord");
    return false;
  }

  Serial.printf("Connected, IP address: %s\n",
                WiFi.localIP().toString().c_str());
  wifiAvailable = true;
  drawMessage("Wi-Fi ansluten", WiFi.localIP().toString());
  delay(2000);
  return true;
}

bool postLmsRequest(const String& body, String& responseBody) {
  HTTPClient http;
  const String url = "http://" + String(LMS_HOST) + ":" + String(LMS_PORT) +
                     "/jsonrpc.js";
  if (!http.begin(url)) {
    lmsAvailable = false;
    Serial.println("Could not initialize LMS request");
    return false;
  }

  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  const int responseCode = http.POST(body);
  if (responseCode != HTTP_CODE_OK) {
    lmsAvailable = false;
    Serial.printf("LMS returned HTTP status %d\n", responseCode);
    http.end();
    return false;
  }

  responseBody = http.getString();
  http.end();
  lmsAvailable = true;
  return true;
}

bool fetchLmsVolume(int& volume) {
  StaticJsonDocument<256> request;
  request["id"] = 1;
  request["method"] = "slim.request";
  JsonArray params = request.createNestedArray("params");
  params.add(LMS_PLAYER_ID);
  JsonArray command = params.createNestedArray();
  command.add("mixer");
  command.add("volume");
  command.add("?");

  String body;
  serializeJson(request, body);
  String responseBody;
  if (!postLmsRequest(body, responseBody)) {
    return false;
  }

  StaticJsonDocument<512> response;
  const DeserializationError error = deserializeJson(response, responseBody);
  if (error) {
    lmsAvailable = false;
    Serial.printf("Could not parse LMS response: %s\n", error.c_str());
    return false;
  }

  const char* volumeText = response["result"]["_volume"];
  char* parseEnd = nullptr;
  const long receivedVolume =
      volumeText == nullptr ? -1 : strtol(volumeText, &parseEnd, 10);
  if (volumeText == nullptr || parseEnd == volumeText || *parseEnd != '\0' ||
      receivedVolume < -100 || receivedVolume > 100) {
    lmsAvailable = false;
    Serial.println("LMS response did not contain a valid volume");
    return false;
  }

  volume = static_cast<int>(labs(receivedVolume));
  Serial.printf("LMS volume: %d\n", volume);
  return true;
}

bool fetchLmsMute(bool& muted) {
  StaticJsonDocument<256> request;
  request["id"] = 1;
  request["method"] = "slim.request";
  JsonArray params = request.createNestedArray("params");
  params.add(LMS_PLAYER_ID);
  JsonArray command = params.createNestedArray();
  command.add("mixer");
  command.add("muting");
  command.add("?");

  String body;
  serializeJson(request, body);
  String responseBody;
  if (!postLmsRequest(body, responseBody)) {
    return false;
  }

  StaticJsonDocument<512> response;
  const DeserializationError error = deserializeJson(response, responseBody);
  if (error) {
    lmsAvailable = false;
    Serial.printf("Could not parse LMS mute response: %s\n", error.c_str());
    return false;
  }

  const char* muteText = response["result"]["_muting"];
  if (muteText == nullptr || (strcmp(muteText, "0") != 0 &&
                              strcmp(muteText, "1") != 0)) {
    lmsAvailable = false;
    Serial.println("LMS response did not contain a valid mute state");
    return false;
  }

  muted = strcmp(muteText, "1") == 0;
  Serial.printf("LMS muted: %s\n", muted ? "yes" : "no");
  return true;
}

bool setLmsVolume(int volume) {
  StaticJsonDocument<256> request;
  request["id"] = 1;
  request["method"] = "slim.request";
  JsonArray params = request.createNestedArray("params");
  params.add(LMS_PLAYER_ID);
  JsonArray command = params.createNestedArray();
  command.add("mixer");
  command.add("volume");
  command.add(String(volume));

  String body;
  serializeJson(request, body);
  String responseBody;
  const bool sent = postLmsRequest(body, responseBody);
  Serial.printf("Set LMS volume to %d: %s\n", volume,
                sent ? "ok" : "failed");
  return sent;
}

bool togglePlayback() {
  StaticJsonDocument<192> request;
  request["id"] = 1;
  request["method"] = "slim.request";
  JsonArray params = request.createNestedArray("params");
  params.add(LMS_PLAYER_ID);
  params.createNestedArray().add("pause");

  String body;
  serializeJson(request, body);
  String responseBody;
  const bool sent = postLmsRequest(body, responseBody);
  Serial.printf("Toggle LMS playback: %s\n", sent ? "ok" : "failed");
  return sent;
}

bool toggleMute() {
  StaticJsonDocument<192> request;
  request["id"] = 1;
  request["method"] = "slim.request";
  JsonArray params = request.createNestedArray("params");
  params.add(LMS_PLAYER_ID);
  JsonArray command = params.createNestedArray();
  command.add("mixer");
  command.add("muting");
  command.add("toggle");

  String body;
  serializeJson(request, body);
  String responseBody;
  const bool sent = postLmsRequest(body, responseBody);
  Serial.printf("Toggle LMS mute: %s\n", sent ? "ok" : "failed");
  return sent;
}

void drawDialPosition() {
  M5Dial.Display.fillScreen(BLACK);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextColor(WHITE, BLACK);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawString("lms-dial", M5Dial.Display.width() / 2,
                            M5Dial.Display.height() / 2 - 36);

  M5Dial.Display.setTextSize(4);
  const String value = playerMuted ? "MUTE" : String(dialPosition);
  M5Dial.Display.drawString(value, M5Dial.Display.width() / 2,
                            M5Dial.Display.height() / 2 + 14);
}

bool syncPlayerState() {
  const bool restoringConnection = !lmsAvailable;
  int volume = 0;
  bool muted = false;
  if (!fetchLmsVolume(volume) || !fetchLmsMute(muted)) {
    return false;
  }

  const bool stateChanged = dialPosition != volume || playerMuted != muted;
  dialPosition = volume;
  playerMuted = muted;
  M5Dial.Encoder.write(dialPosition);
  if (restoringConnection || stateChanged) {
    drawDialPosition();
  }
  return true;
}

void setup() {
  Serial.begin(115200);

  auto config = M5.config();
  M5Dial.begin(config, true, false);  // Enable encoder, disable RFID.

  M5Dial.Display.setRotation(0);
  if (connectToWiFi()) {
    drawMessage("LMS", "Hamtar volym...");
    if (!syncPlayerState()) {
      drawMessage("LMS saknas", "Kontrollera servern");
    }
  }
  lastReconnectAttemptAt = millis();
  lastLmsSyncAt = millis();
}

void loop() {
  M5Dial.update();

  const unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiAvailable) {
      wifiAvailable = false;
      lmsAvailable = false;
      drawMessage("Wi-Fi saknas", "Forsoker igen...");
    }

    if (now - lastReconnectAttemptAt >= reconnectIntervalMs) {
      lastReconnectAttemptAt = now;
      if (connectToWiFi()) {
        drawMessage("LMS", "Ateransluter...");
        if (!syncPlayerState()) {
          drawMessage("LMS saknas", "Forsoker igen...");
        }
        lastLmsSyncAt = millis();
      }
    }
    return;
  }

  if (!wifiAvailable) {
    wifiAvailable = true;
    drawMessage("LMS", "Ateransluter...");
    if (!syncPlayerState()) {
      drawMessage("LMS saknas", "Forsoker igen...");
    }
    lastLmsSyncAt = millis();
    return;
  }

  if (now - lastLmsSyncAt >= lmsSyncIntervalMs) {
    lastLmsSyncAt = now;
    if (!syncPlayerState()) {
      drawMessage("LMS saknas", "Forsoker igen...");
    }
  }

  const long newPosition = M5Dial.Encoder.read();
  if (newPosition != dialPosition) {
    const long clampedPosition = constrain(newPosition, 0L, 100L);
    if (clampedPosition != newPosition) {
      M5Dial.Encoder.write(clampedPosition);
    }
    if (clampedPosition != dialPosition) {
      dialPosition = clampedPosition;
      drawDialPosition();
      volumeChangePending = true;
      lastVolumeChangeAt = millis();
    }
  }

  if (volumeChangePending && millis() - lastVolumeChangeAt >= 100) {
    volumeChangePending = false;
    if (setLmsVolume(dialPosition)) {
      fetchLmsMute(playerMuted);
      drawDialPosition();
    }
  }

  if (M5Dial.BtnA.wasHold()) {
    if (toggleMute() && fetchLmsMute(playerMuted)) {
      drawDialPosition();
    }
  } else if (M5Dial.BtnA.wasClicked()) {
    togglePlayback();
  }
}
