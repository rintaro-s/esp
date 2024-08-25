#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <stdio.h>
#include <stdlib.h>

/*
Uncomment and set up if you want to use custom pins for the SPI communication
#define REASSIGN_PINS
int sck = -1;
int miso = -1;
int mosi = -1;
int cs = -1;
*/

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

String readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }

  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  return content;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char *path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %lu ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
  file.close();
}

int kaonashi_count;     // 無限カオナシ防止用変数
String token;
String ssid;
String pass;
String mode0; // stringで一時保存
int mode; // 1なら親探知、2ならインターホン

const char* serverName = "http://10.5.4.170:5000/trigger";//PCのIP

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

#ifdef REASSIGN_PINS
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {
#else
  if (!SD.begin()) {
#endif
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  ssid = readFile(SD, "/ssid.txt");
  pass = readFile(SD, "/pass.txt");
  mode0 = readFile(SD, "/mode.txt");
  token = readFile(SD, "/token.txt");
  mode = mode0.toInt();
  Serial.print(token);
  // mode = strtol(mode0, &endptr, 10); // 文字列を整数に変換

  WiFi.begin(ssid, pass);
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to Wi-Fi");
  if (mode == 1) {
    Serial.println("親探知モード");
    sendMessage("親探知モードとして起動しました");
  } else {
    Serial.println("インターホンモード");
    sendMessage("インターホンモードとして起動しました");
  }
}

void loop() {
  switch (mode) {

    case 1: // 親探知モード

      if (Serial.available() > 0) {
        Serial.println("受信！");
        kaonashi_count = 0;
        String data = Serial.readStringUntil('\n');
        data.trim(); // 余分な空白や改行を削除

        Serial.print("Received data: ");
        Serial.println(data); // 受信データを表示

        if (data.indexOf("name") != -1) {
          if (data.indexOf("face1") != -1) {
            Serial.println("いた");
            sendMessage("奴が来た");
              HTTPClient http;
    http.begin(serverName);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    } else {
      Serial.printf("Error code: %d\n", httpResponseCode);
    }

    http.end();
            delay(5000);
          } else {
            Serial.println("いない");
          }

        } else if (data == "config") {
          Serial.print("送信完了");

          // SSID、パスワード、モードの設定を読み込み
          while (Serial.available() == 0) {}
          String data = Serial.readStringUntil('\n');
          ssid = data;
          Serial.print("ssidは");
          Serial.print(ssid);
          writeFile(SD, "/ssid.txt", data.c_str());

          while (Serial.available() == 0) {}
          data = Serial.readStringUntil('\n');
          pass = data;
          Serial.print("パスワードは");
          Serial.print(pass);
          writeFile(SD, "/pass.txt", data.c_str());

          while (Serial.available() == 0) {}
          data = Serial.readStringUntil('\n');
          if (data == "oya") {
            mode = 1;
          } else {
            mode = 2;
          }
          writeFile(SD, "/mode.txt", String(mode).c_str());
          
          WiFi.begin(ssid, pass);
          Serial.print("Connecting...");
          while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
          }
          Serial.println();
          Serial.println("Connected to Wi-Fi");
          
          ssid = readFile(SD, "/ssid.txt");
          pass = readFile(SD, "/pass.txt");
          String mode0 = readFile(SD, "/mode.txt");
          mode = mode0.toInt();
          
          if (mode == 1) {
            Serial.println("親探知モード");
            sendMessage("親探知モードとして起動しました");
          } else {
            Serial.println("インターホンモード");
            sendMessage("インターホンモードとして起動しました");
          }
        }
      }
      break;

    case 2: // インターフォンモード

      // シリアルポートに到着しているデータのバイト数
      if (Serial.available() > 0) {
        kaonashi_count = 0;
        // シリアルデータの受信 (改行まで)
        String data = Serial.readStringUntil('\n');
        if (data.indexOf("name") != -1) {
          if (data.indexOf("face2") != -1) {
            Serial.println("いた2");
            sendMessage("2番が来た");
            delay(5000);
          } else if (data.indexOf("face3") != -1) {
            Serial.println("いた3");
            sendMessage("3番が来た");
            delay(5000);
          } else if (data.indexOf("face4") != -1) {
            Serial.println("いた4");
            sendMessage("4番が来た");
            delay(5000);
          } else if (data.indexOf("face5") != -1) {
            Serial.println("いた5");
            sendMessage("5番が来た");
            delay(5000);
          } else {
            Serial.println("知らない人が来た");
          }
        }
      } else {
        String data = Serial.readStringUntil('\n');

        if (kaonashi_count == 1000) {
          Serial.println("カオナシ3");
          kaonashi_count = 0;
        } else {
          kaonashi_count++;
          delay(1);
        }
        if (data == "config") {
          Serial.print("送信完了");
          while (Serial.available() == 0) {
            // Wait for data
          }
          String data = Serial.readStringUntil('\n');
          ssid = data;
          Serial.print("ssidは");
          Serial.print(ssid);
          writeFile(SD, "/ssid.txt", data.c_str());
          while (Serial.available() == 0) {
            // Wait for data
          }
          data = Serial.readStringUntil('\n');
          pass = data;
          Serial.print("パスワードは");
          Serial.print(pass);
          writeFile(SD, "/pass.txt", data.c_str());
          while (Serial.available() == 0) {
            // Wait for data
          }
          data = Serial.readStringUntil('\n');
          if (data == "oya") { // 1なら親探知、2ならインターホン
            mode = 1;
          } else {
            mode = 2;
          }
          WiFi.begin(ssid, pass);
          Serial.print("Connecting...");
          while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
          }
          Serial.println();
          Serial.println("Connected to Wi-Fi");
          ssid = readFile(SD, "/ssid.txt");
          pass = readFile(SD, "/pass.txt");
          mode0 = readFile(SD, "/mode.txt");
          mode = mode0.toInt();
          if (mode == 1) {
            Serial.println("親探知モード");
            sendMessage("親探知モードとして起動しました");
          } else {
            Serial.println("インターホンモード");
            sendMessage("インターホンモードとして起動しました");
          }
        }
      }
      break;

    default:
      Serial.println("エラー");
      delay(1000);
      break;
  }
  delay(200);
}

void sendMessage(String message) { 
  HTTPClient client;

  String url = "https://notify-api.line.me/api/notify";
  client.begin(url);
  client.addHeader("Content-Type", "application/x-www-form-urlencoded");
  client.addHeader("Authorization", "Bearer " + token);

  String query = "message=" + message;
  client.POST(query);

  String body = client.getString();
  Serial.println("Sent the message");
  Serial.println(body);
  client.end();
}
