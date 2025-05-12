#include <SD.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Пингийн тодорхойлолт
#define NRF_CE_PIN 40           // NRF24L01 CE пин
#define NRF_CSN_PIN 42          // NRF24L01 CSN пин
#define SD_CS_PIN 53            // SD картын CS пин
#define MOTOR_FORWARD_LEFT 8    // Зүүн талын урагшаа мотор
#define MOTOR_FORWARD_RIGHT 9   // Баруун талын урагшаа мотор
#define MOTOR_REVERSE_LEFT 10   // Зүүн талын хойшоо мотор
#define MOTOR_REVERSE_RIGHT 11  // Баруун талын хойшоо мотор

// Радиогийн тохиргоо
RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
const byte address[6] = "00001";
unsigned long lastRecordTime = 0;
unsigned long lastPacketTime = 0;
unsigned long lastTimeoutCheck = 0;
File myfile;
bool ok = false;
// Дамжуулалтын төрлийг тодорхойлох enum
enum MessageType {
  JOYSTICK_DATA = 0,
  COMMAND = 1
};

// Дамжуулалтын пакетийн бүтэц
struct Packet {
  MessageType type;
  union {
    struct {
      int x;
      int y;
    } joystick;
    char command[32];
  } data;
};

// Point бүтэц ба массивыг нэмсэн
struct Point {
  int x;
  int y;
  unsigned long time;  // time утгыг хадгалах
};
Point points[100];  // Хамгийн ихдээ 100 координат хадгална
int pointCount = 0;

// Бүх мөрүүдийг хойноос эхлэн хэвлэх функц
void printLastLines() {
  Serial.println("Last lines from file (from newest to oldest):");
  for (int i = pointCount - 1; i >= 0; i--) {
    Serial.print("x:");
    Serial.print(points[i].x);
    Serial.print(",y:");
    Serial.print(points[i].y);
    Serial.print(",time:");
    Serial.println(points[i].time);
    motor(points[i].x, points[i].y, 0, points[i].time);
    if (radio.available()) {
      Packet packet;
      radio.read(&packet, sizeof(Packet));

      if (packet.type == COMMAND) {
        if (strcmp(packet.data.command, "stop") == 0) {
          Serial.println("Received 'start' command.");
          stop();
          printLastLines();  // Бүх мөрүүдийг хойноос хэвлэх
        } else if (strcmp(packet.data.command, "record") == 0) {
          Serial.println("Received 'record' command.");
          SD.remove("data.txt");
          myfile = SD.open("data.txt", FILE_WRITE);
          myfile.close();
        }
      }
    }
  }
}
void revers() {
  Serial.println("Starting revers()...");
  File file = SD.open("data.txt", FILE_READ);
  if (!file) {
    Serial.println("Error opening file for reading.");
    ok = true;
    return;
  }
  Serial.println("File opened successfully.");

  long fileSize = file.size();
  Serial.print("File size: ");
  Serial.println(fileSize);
  if (fileSize == 0) {
    Serial.println("File is empty.");
    file.close();
    ok = true;
    return;
  }

  pointCount = 0;
  while (file.available() && pointCount < 100) {
    String line = file.readStringUntil('\n');
    if (line.length() == 0) {
      Serial.println("Empty line encountered, skipping...");
      continue;
    }

    Serial.print("Read line: ");
    Serial.println(line);

    int x, y;
    unsigned long time;
    if (sscanf(line.c_str(), "x:%d,y:%d,%lu", &x, &y, &time) == 3) {
      points[pointCount].x = x;
      points[pointCount].y = y;
      points[pointCount].time = time;
      pointCount++;
      Serial.print("Parsed point: x=");
      Serial.print(x);
      Serial.print(", y=");
      Serial.print(y);
      Serial.print(", time=");
      Serial.println(time);
    } else {
      Serial.println("Failed to parse line, skipping...");
    }

    if (pointCount >= 100) {
      Serial.println("Reached 100 points limit.");
      ok = true;
      break;
    }
  }

  file.close();
  Serial.println("File closed.");
  if (pointCount == 0) {
    Serial.println("No valid points to print.");
    ok = true;
    return;
  }
  Serial.println("revers() completed.");
}
void stop() {
  Serial.println("Stopping motors and clearing data...");
  digitalWrite(MOTOR_FORWARD_LEFT, LOW);
  digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
  digitalWrite(MOTOR_REVERSE_LEFT, LOW);
  digitalWrite(MOTOR_REVERSE_RIGHT, LOW);
  while (1) {
    if (radio.available()) {
      Packet packet;
      radio.read(&packet, sizeof(Packet));

      if (packet.type == COMMAND) {
        if (strcmp(packet.data.command, "start") == 0) {
          Serial.println("Received 'start' command.");
          return;  // Бүх мөрүүдийг хойноос хэвлэх
        } else if (strcmp(packet.data.command, "record") == 0) {
          Serial.println("Received 'record' command.");
          SD.remove("data.txt");
          myfile = SD.open("data.txt", FILE_WRITE);
          myfile.close();
          check();
        }
      }
    }
  }
}
void check() {
  // Serial.println("check ok");
  if (radio.available()) {
    Packet packet;
    radio.read(&packet, sizeof(Packet));

    if (packet.type == COMMAND) {
      if (strcmp(packet.data.command, "start") == 0) {
        Serial.println("Received 'start' command.");
        revers();
        printLastLines();
      } else if (strcmp(packet.data.command, "stop") == 0) {
        Serial.println("Received 'stop' command.");
        stop();
      } else if (strcmp(packet.data.command, "record") == 0) {
        Serial.println("Received 'record' command.");
        SD.remove("data.txt");
        myfile = SD.open("data.txt", FILE_WRITE);
        myfile.close();
        pointCount = 0;
      }
    } else if (packet.type == JOYSTICK_DATA) {
      int x = packet.data.joystick.x;
      int y = packet.data.joystick.y;

      if ((x >= 1 && x <= 1023) && (y >= 1 && y <= 1023)) {
        lastPacketTime = millis();
          unsigned long timeDiff = (lastRecordTime == 0) ? 0 : (lastPacketTime - lastRecordTime);
        Serial.print("Received -> X: ");
        Serial.print(x);
        Serial.print(" | Y: ");
        Serial.println(y);
               Serial.print(" | T: ");
          Serial.println(timeDiff);

       lastRecordTime = lastPacketTime;
  
        if (myfile) {
          myfile.print("x:");
          myfile.print(x);
          myfile.print(",y:");
          myfile.print(y);
          myfile.print(",");
          
          myfile.println(timeDiff);
          myfile.close();
          // Serial.print("Wrote to file: x:");
          // Serial.print(x);
          // Serial.print(",y:");
          // Serial.print(y);
          // Serial.print(",t:");
          // Serial.println(timeDiff);
   
          motor(x, y, 1, 0);  // Жишээ болгон 100ms delay
        } else {
          // Serial.println("Error opening file for writing.");
        }
      }
    }

    while (radio.available()) {
      char dummy[32];
      radio.read(&dummy, sizeof(dummy));
    }
  }
}
void setup() {
  Serial.begin(115200);

  // Моторын пинүүдийг тохируулах
  pinMode(MOTOR_FORWARD_LEFT, OUTPUT);
  pinMode(MOTOR_FORWARD_RIGHT, OUTPUT);
  pinMode(MOTOR_REVERSE_LEFT, OUTPUT);
  pinMode(MOTOR_REVERSE_RIGHT, OUTPUT);
  digitalWrite(MOTOR_FORWARD_LEFT, LOW);
  digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
  digitalWrite(MOTOR_REVERSE_LEFT, LOW);
  digitalWrite(MOTOR_REVERSE_RIGHT, LOW);
  if (!radio.begin()) {
    Serial.println("Radio hardware is not responding!");
    while (1) {}
  }
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(0x4c);
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(sizeof(Packet));
  radio.openReadingPipe(1, address);
  radio.startListening();

  // SD картыг эхлүүлэх
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
    while (1) {}
  }

  if (SD.exists("data.txt")) {
    SD.remove("data.txt");
  }
  File file = SD.open("data.txt", FILE_WRITE);
  if (file) {
    Serial.println("data.txt created.");
  } else {
    Serial.println("Error creating data.txt!");
  }
  Serial.println("Initialization done.");
}



void loop() {
  check();
}
int motor(int x, int y, int mode, unsigned long time) {
  // Моторын пинүүдийг унтраах (эхний байдал)
  digitalWrite(MOTOR_FORWARD_LEFT, LOW);
  digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
  digitalWrite(MOTOR_REVERSE_LEFT, LOW);
  digitalWrite(MOTOR_REVERSE_RIGHT, LOW);

  if ((1 <= x && x <= 400) && (400 <= y && y <= 600) && (mode == 1)) {  // урагшаа
    Serial.println("uragshaa");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, HIGH);
    digitalWrite(MOTOR_REVERSE_LEFT, LOW);
    digitalWrite(MOTOR_REVERSE_RIGHT, LOW);
    delay(time);  // time хугацаагаар ажиллана
  } else if ((1 <= x && x <= 400) && (400 <= y && y <= 600) && (mode == 0)) {
    Serial.println("SD uragshaa");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_REVERSE_RIGHT, HIGH);
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, HIGH);
    delay(time);
  } else if ((600 <= x && x <= 1023) && (400 <= y && y <= 600) && (mode == 1)) {  // хойшоо
    Serial.println("hoyashoo");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, HIGH);
    digitalWrite(MOTOR_REVERSE_LEFT, HIGH);
    digitalWrite(MOTOR_REVERSE_RIGHT, HIGH);
    delay(time);
  } else if ((600 <= x && x <= 1023) && (400 <= y && y <= 600) && (mode == 0)) {
    Serial.println("SD hoyashoo");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, HIGH);
    digitalWrite(MOTOR_REVERSE_LEFT, LOW);
    digitalWrite(MOTOR_REVERSE_RIGHT, LOW);
    delay(time);
  } else if ((400 <= x && x <= 600) && (1 <= y && y <= 400) && (mode == 1)) {  // баруун
    Serial.println("baruun");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
    digitalWrite(MOTOR_REVERSE_LEFT, LOW);
    digitalWrite(MOTOR_REVERSE_RIGHT, LOW);
    delay(time);
  } else if ((400 <= x && x <= 600) && (1 <= y && y <= 400) && (mode == 0)) {
    Serial.println("SD baruun");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_REVERSE_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
    digitalWrite(MOTOR_REVERSE_RIGHT, LOW);

    delay(time);
  } else if ((400 <= x && x <= 600) && (600 <= y && y <= 1023) && (mode == 1)) {  // зүүн
    Serial.println("zuun");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_REVERSE_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
    digitalWrite(MOTOR_REVERSE_RIGHT, LOW);

    delay(time);
  } else if ((400 <= x && x <= 600) && (600 <= y && y <= 1023) && (mode == 0)) {
    Serial.println("SD zuun");
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_REVERSE_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
    digitalWrite(MOTOR_REVERSE_RIGHT, LOW);


    delay(time);
  } else {
    Serial.println("stop");
    // Мотор зогсоно (аль хэдийн LOW байгаа)
    digitalWrite(MOTOR_FORWARD_LEFT, HIGH);
    digitalWrite(MOTOR_REVERSE_LEFT, HIGH);
    digitalWrite(MOTOR_FORWARD_RIGHT, LOW);
    digitalWrite(MOTOR_REVERSE_RIGHT, LOW);
  }
  // Моторыг дахин унтраах
  return 0;
}