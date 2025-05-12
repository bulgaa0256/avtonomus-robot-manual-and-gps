/**
 * RC Vehicle Transmitter - Using NRF24L01
 * 
 * Adapted based on user's actual implementation with packet-based communication
 * for joystick control and commands.
 * 
 * Components:
 * - Arduino board
 * - NRF24L01+ radio module
 * - Joystick module (analog)
 * - Button for control on pin 8
 */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Pin definitions
#define JOY_X_PIN A3       // Joystick X-axis
#define JOY_Y_PIN A2       // Joystick Y-axis
#define BUTTON_PIN 8       // Control button
#define NRF_CE_PIN 9       // NRF24L01 CE pin
#define NRF_CSN_PIN 10     // NRF24L01 CSN pin

// Radio configuration
RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
const byte address[6] = "00001"; // Address for communication channel
int stop = 0; // Transmission state flag
unsigned long buttonPressTime = 0; // Товчлуур дарсан хугацаа
bool buttonPressed = false; // Товчлуур дарсан эсэх

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

// Retry mechanism for packet sending
bool sendPacket(Packet* packet, int retries = 5) {
  for (int i = 0; i < retries; i++) {
    if (radio.write(packet, sizeof(Packet))) {
      return true;
    }
    delay(50); // Хүлээх хугацаа
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  Serial.println("RC Vehicle Transmitter Starting...");
  
  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize radio
  if (!radio.begin()) {
    Serial.println("Radio hardware not responding!");
    while (1) {}
  }
  
  // Radio setup - using same configuration as user's code
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(0x4c);
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(32);
  radio.openWritingPipe(address);
  radio.stopListening();
  
  Serial.println("Transmitter started");
}

void loop() {
  Packet packet;
  
  // Check button state
  int buttonState = digitalRead(BUTTON_PIN);
  Serial.print("button: ");
  Serial.println(buttonState);
  
  // Button pressed
  if (buttonState == LOW && !buttonPressed) {
    buttonPressed = true;
    buttonPressTime = millis(); // Дарсан хугацааг хадгалах
    delay(50); // Debounce
  }
  
  // Button released
  if (buttonState == HIGH && buttonPressed) {
    if (millis() - buttonPressTime < 5000) {
      // Short press: toggle start/stop
      but();
    }
    buttonPressed = false; // Төлөвийг шинэчлэх
  }
  
  // Long press detection (5 seconds or more)
  if (buttonPressed && (millis() - buttonPressTime >= 5000)) {
    packet.type = COMMAND;
    strncpy(packet.data.command, "record", sizeof(packet.data.command));
    if (sendPacket(&packet)) {
      Serial.println("Товчлуур 5 секундээс удаан дарлагдлаа! 'record' илгээгдлээ");
    } else {
      Serial.println("Failed to send 'record' after retries");
    }
    buttonPressed = false; // Төлөвийг шинэчлэх
  }
  
  // If not stopped, send joystick data
  if (stop == 0) {
    packet.type = JOYSTICK_DATA;
    packet.data.joystick.x = map(analogRead(JOY_Y_PIN), 0, 1023, 1, 1023); // Map to ensure values are 1-1023
    packet.data.joystick.y = map(analogRead(JOY_X_PIN), 0, 1023, 1, 1023);
    
    if (sendPacket(&packet)) {
      Serial.print("Sent -> X: ");
      Serial.print(packet.data.joystick.x);
      Serial.print(" | Y: ");
      Serial.println(packet.data.joystick.y);
    } else {
      Serial.println("Failed to send");
      Serial.print("Radio state: ");
      Serial.println(radio.isChipConnected() ? "Connected" : "Disconnected");
    }
  }
  
  delay(100); // Shorter delay for more responsive control
}

// Handle short button press to toggle between start/stop commands
void but() {
  Packet packet;
  packet.type = COMMAND;
  
  if (stop == 0) {
    // If currently transmitting, send stop command and stop transmission
    strncpy(packet.data.command, "start", sizeof(packet.data.command));
    if (sendPacket(&packet)) {
      stop = 1;
      Serial.println("Transmission stopped and 'stop' sent");
    } else {
      Serial.println("Failed to send 'stop' after retries");
    }
  } else {
    // If currently stopped, send start command and resume transmissiNon
    strncpy(packet.data.command, "stop", sizeof(packet.data.command));
    if (sendPacket(&packet)) {
      stop = 0;
      Serial.println("Transmission resumed and 'start' sent");
    } else {
      Serial.println("Failed to send 'start' after retries");
    }
  }
}