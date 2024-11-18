#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <IRremote.hpp>
#include <DHT.h>
#include <Keypad.h>
#include "SafeState.h"
#include <DHT_U.h>

/* Locking DOOR */
#define SERVO_PIN 12
#define SERVO_LOCK_POS   20
#define SERVO_UNLOCK_POS 90
Servo lockServo;

#define DHTPIN 16
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

#define IR_RECEIVE_PIN 13

/* Display */
LiquidCrystal_I2C lcd(0x27, 20, 4);

/* Keypad setup */
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = {9, 8, 7, 6};
byte colPins[KEYPAD_COLS] = {5, 4, 3, 2};
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

bool newCodeNeeded = true;
bool readyToLock = true;
bool isLocked = false;

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
SafeState safeState;

void lock() {
  lockServo.write(SERVO_LOCK_POS);
  safeState.lock();
}

void unlock() {
  lockServo.write(SERVO_UNLOCK_POS);
}

void showStartupMessage() {
  
  lcd.setCursor(5, 1);
  lcd.print("CT060301");
  
  lcd.setCursor(4, 2);
  String message = "SMART HOME";
  for (byte i = 0; i < message.length(); i++) {
    lcd.print(message[i]);
    delay(100);
  }
  delay(500);
}

String inputSecretCode() {
  lcd.setCursor(4, 1);
  lcd.print("[____]");
  lcd.setCursor(5, 1);
  String result = "";
  while (result.length() < 4) {
    char key = keypad.getKey();
    if (key >= '0' && key <= '9') {
      lcd.print('*');
      result += key;
    }
  }
  return result;
}

bool setNewCode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter new code:");
  String newCode = inputSecretCode();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Confirm new code");
  String confirmCode = inputSecretCode();

  if (newCode.equals(confirmCode)) {
    safeState.setCode(newCode);
    newCodeNeeded = false;
    isLocked = true;
    return true;
  } else {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Code mismatch");
    lcd.setCursor(0, 1);
    lcd.print("Safe not locked!");
    delay(2000);
    return false;
  }
}

void showUnlockMessage() {
  lcd.clear();
  lcd.setCursor(4, 1);
  lcd.print("Unlocked!");
  delay(2000);
  lcd.clear();
}


void showWaitScreen(int delayMillis) {
  lcd.setCursor(3, 1);
  lcd.print("[..........]");
  lcd.setCursor(4, 1);
  for (byte i = 0; i < 10; i++) {
    delay(delayMillis);
    lcd.print("=");
  }
}

void checkIRInput() {
  // Kiểm tra tín hiệu IR
  if (IrReceiver.decode()) {
    // Kiểm tra mã nút POWER (mã là 162)

    if (IrReceiver.decodedIRData.command == 162 && newCodeNeeded) {
      readyToLock = setNewCode();
    } else if (IrReceiver.decodedIRData.command == 162 && !newCodeNeeded) {
      isLocked = true;
    }

    IrReceiver.resume(); // Tiếp tục nhận tín hiệu tiếp theo
  }
}

void safeUnlockedLogic() {
  lcd.clear();

  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    lcd.setCursor(3, 0);
    lcd.print("Temp: ");
    lcd.print(event.temperature);
    lcd.print(F("\xdf"));
    lcd.print(F("C"));
  }
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) {
    lcd.setCursor(3, 1);
    lcd.print("Humi: ");
    lcd.print(event.relative_humidity);
    lcd.print(F("%"));
  }

  lcd.setCursor(0, 2);
  lcd.print("# or Power to lock");



  while (!isLocked) {
    checkIRInput(); // Kiểm tra tín hiệu IR
    char key = keypad.getKey();
    if (key == 'A') {
      readyToLock = setNewCode();
    }
    if (key == '#' && newCodeNeeded) {
      readyToLock = setNewCode();
    } else if(key == '#' && !newCodeNeeded){
      isLocked = true;
    }
  }

  if (readyToLock) {
    lcd.clear();
    lcd.setCursor(5, 0);
    lcd.print("Locking...");
    lock();
    showWaitScreen(10);
    isLocked = false;
  }
}

void safeLockedLogic() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Door Locked!");

  String userCode = inputSecretCode();
  bool unlockedSuccessfully = safeState.unlock(userCode);
  showWaitScreen(200);

  if (unlockedSuccessfully) {
    showUnlockMessage();
    unlock();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wrong code!");
    showWaitScreen(1000);
  }
}

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECEIVE_PIN);

  lcd.begin(20, 4);
  lockServo.attach(SERVO_PIN);

  if (safeState.locked()) {
    lock();
  } else {
    unlock();
  }
  delay(1000);
  showStartupMessage();
  lcd.clear();
  dht.begin();
}

void loop() {
  if (safeState.locked()) {
    safeLockedLogic();
  } else {
    safeUnlockedLogic();
  }
}
