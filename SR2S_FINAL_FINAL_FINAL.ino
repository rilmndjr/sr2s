#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

#define SS_PIN 10
#define RST_PIN 9

#define LEDGreen 5
#define LEDRed 4
#define Buzzer 2
#define Relay 3
#define Btn 6

#define MAX_ATTEMPTS 3
#define ACCESS_DELAY 2000
#define DENIED_DELAY 1000

MFRC522 mfrc522(SS_PIN, RST_PIN);

RTC_DS3231 rtc;

byte TAG1[] = {0xB4, 0x15, 0x89, 0xA1};
byte TAG2[] = {0xD4, 0x6B, 0x10, 0xA0};
byte TAG3[] = {0xE0, 0x97, 0xC7, 0x13};
byte ADMIN_TAG[] = {0x10, 0x24, 0xD8, 0x11}; // Admin tag

struct User {
  byte tag[4];
  String name;
  int assignedDay;
};

// Define a new user for admin access
User adminUser = {{0x10, 0x24, 0xD8, 0x11}, "Admin", -1}; // Assigned to no specific day

User users[] = {
  {{0xB4, 0x15, 0x89, 0xA1}, "Sharon Winner", 1}, 
  {{0xD4, 0x6B, 0x10, 0xA0}, "STRANGE", 1},  
  {{0x62, 0x37, 0x33, 0x51}, "COLET", 0},
  adminUser,                                // Add the admin user
};

LiquidCrystal_I2C lcd(0x27, 16, 2);

int attempts = 0;

void setup() {
  lcd.begin(16, 2);
  lcd.backlight();
  
  pinMode(LEDRed, OUTPUT);
  pinMode(LEDGreen, OUTPUT);
  pinMode(Buzzer, OUTPUT);
  pinMode(Relay, OUTPUT);
  noTone(Buzzer);
  pinMode(Btn, INPUT);
  digitalWrite(Btn, HIGH);
  digitalWrite(Relay, HIGH);
  
  Serial.begin(9600);
  
  SPI.begin();
  mfrc522.PCD_Init();
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TAP YOUR ID");
  
  DateTime now = rtc.now();
  int currentDay = now.dayOfTheWeek();

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scanned Card UID:");
    lcd.setCursor(0, 1);
    
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      lcd.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      lcd.print(mfrc522.uid.uidByte[i], HEX);
    }
    
    bool accessStatus = false;
    String userName = "";
    bool hasScheduleToday = false;
    int assignedDay = -1; // Default assigned day when no match is found
    for (int i = 0; i < sizeof(users) / sizeof(users[0]); i++) {
      if (checkTag(users[i].tag, mfrc522.uid.uidByte)) {
        assignedDay = users[i].assignedDay; // Assign the user's assigned day
        if (assignedDay == -1 || isAssignedDay(users[i].assignedDay, currentDay)) {
          accessStatus = true;
          userName = users[i].name;
          break;
        } else {
          hasScheduleToday = true;
          break;
        }
      }
    }
    
    if (accessStatus) {
      accessGranted(userName);
    } else {
      accessDenied(hasScheduleToday, assignedDay); // Pass assigned day to accessDenied function
    }
  }
  
  if (digitalRead(Btn) == LOW) {
    buttonPressed(now.hour()); // Pass the current hour to the buttonPressed function
  }
}

bool checkTag(byte tag[], byte tagUID[]) {
  return memcmp(tag, tagUID, sizeof(tag)) == 0;
}

bool isAssignedDay(int assignedDay, int currentDay) {
  return assignedDay == currentDay;
}

void accessGranted(String userName) {
  attempts = 0;
  lcd.clear();
  digitalWrite(LEDGreen, HIGH);
  digitalWrite(Relay, LOW);
  lcd.print("Access Granted");
  lcd.setCursor(0, 1);
  delay(ACCESS_DELAY);
  digitalWrite(LEDGreen, LOW);
  digitalWrite(Relay, HIGH);
  lcd.clear();
  lcd.print("Welcome " + userName);
  delay(1000);
  lcd.clear();
  digitalWrite(LEDGreen, LOW); // Turn off green LED
}

void accessDenied(bool hasScheduleToday, int assignedDay) {
  attempts++;
  lcd.clear();
  lcd.print("Access Denied");
  lcd.setCursor(0, 1);
  delay(DENIED_DELAY);
  digitalWrite(LEDRed, HIGH);
  
  if (attempts >= MAX_ATTEMPTS) {
    activateAlarm();
    attempts = 0; // Reset attempts counter after triggering alarm
  } else {
    tone(Buzzer, 300);
    delay(500);
    noTone(Buzzer);
    delay(100);
    tone(Buzzer, 300);
    delay(500);
    noTone(Buzzer);
    delay(100);
    tone(Buzzer, 300);
    delay(5000);
    noTone(Buzzer);
  }
  
  if (hasScheduleToday) {
    lcd.clear();
    lcd.print("Access Denied");
    lcd.setCursor(0, 1);
    lcd.print("Schedule: ");
    String assignedDayString = displayAssignedDay(assignedDay);
    lcd.print(assignedDayString); // Print the assigned day here
    delay(1000);
  }
  
  digitalWrite(LEDRed, LOW); // Turn off red LED
}

String displayAssignedDay(int dayOfWeek) {
  String day;

  switch (dayOfWeek) {
    case 1:
      day = "MON";
      break;
    case 2:
      day = "TUE";
      break;
    case 3:
      day = "WED";
      break;
    case 4:
      day = "THU";
      break;
    case 5:
      day = "FRI";
      break;
    case 6:
      day = "SAT";
      break;
    case 7:
      day = "SUN";
      break;
    default:
      day = "No Record";
      break;
  }

  return day;
}

void buttonPressed(int currentHour) { // Receive the current hour as a parameter
  // Check if the scanned card UID matches the UID of the admin user
  if (checkTag(adminUser.tag, mfrc522.uid.uidByte)) {
    digitalWrite(LEDRed, LOW);
    digitalWrite(LEDGreen, HIGH);
    digitalWrite(Relay, LOW);
    lcd.clear();
    lcd.print(" Button Pressed ");
    lcd.setCursor(0, 1);
    lcd.print(" Door Un-Locked ");
    delay(3000);
    digitalWrite(Relay, HIGH);
    digitalWrite(LEDGreen, LOW);
    delay(50);
    lcd.clear();
    lcd.setCursor(0, 0);
    return; 
  }
  
  // Check if the current time is within the allowed hours (6 AM to 9 PM Philippine time)
  if (currentHour >= 6 && currentHour < 21) {
    digitalWrite(LEDRed, LOW);
    digitalWrite(LEDGreen, HIGH);
    digitalWrite(Relay, LOW);
    lcd.clear();
    lcd.print(" Button Pressed ");
    lcd.setCursor(0, 1);
    lcd.print(" Door Un-Locked ");
    delay(3000);
    digitalWrite(Relay, HIGH);
    digitalWrite(LEDGreen, LOW);
    delay(50);
    lcd.clear();
    lcd.setCursor(0, 0);
  } else {
    // If the current time is not within the allowed hours, display a message indicating that the button is disabled
    lcd.clear();
    lcd.print("Button Disabled");
    lcd.setCursor(0, 1);
    lcd.print("Outside Hours");
    delay(2000);
    lcd.clear();
  }
}

void activateAlarm() {
  digitalWrite(LEDRed, HIGH);
  tone(Buzzer, 1000);
  delay(5000);
  noTone(Buzzer);
  digitalWrite(LEDRed, LOW);
}
