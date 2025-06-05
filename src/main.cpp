#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>


// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Rotary encoder pins
#define ENC_CLK 27
#define ENC_DT  26
#define ENC_SW  25

// Debounce and encoder timing
#define ENC_DEBOUNCE_MS 2
#define SW_DEBOUNCE_MS  50
#define SW_LONGPRESS_MS 600

// Menu system
enum class ScreenState {
  MAIN_MENU,
  BLE_MENU,
  WIFI_MENU,
  NFC_MENU,
  SETTINGS_MENU,
  // Add more as needed
};

const char* mainMenuItems[] = {
  "BLE Attacks",
  "WiFi Tools",
  "NFC Tools",
  "Settings"
};
const uint8_t mainMenuCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);

const char* bleMenuItems[] = {
  "Beacon Flood",
  "BLE Scanner",
  "HID Emulation",
  "Back"
};
const uint8_t bleMenuCount = sizeof(bleMenuItems) / sizeof(bleMenuItems[0]);

// State variables
ScreenState currentScreen = ScreenState::MAIN_MENU;
uint8_t selectedIndex = 0;

// Encoder state
volatile int8_t encDelta = 0;
int lastEncA = HIGH, lastEncB = HIGH;
unsigned long lastEncTime = 0;

// Button state
bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;
unsigned long buttonPressTime = 0;
bool buttonLongPressHandled = false;

// Forward declarations
void drawMenu();
void handleEncoder();
void handleButton();
void changeScreen(ScreenState newScreen);

void MenuNavigation(int8_t delta);
void MenuSelect();

void setup() {
  Serial.begin(115200);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("🚀 Flipper Pro Booting...");
  display.display();
  delay(1000);

  drawMenu();
}

void loop() {
  handleEncoder();
  handleButton();
}

// --- Rotary Encoder Handling ---
void handleEncoder() {
  int encA = digitalRead(ENC_CLK);
  int encB = digitalRead(ENC_DT);

  if (encA != lastEncA && encA == LOW) { // Only on falling edge
    if (encB == HIGH) {
      encDelta++;
    } else {
      encDelta--;
    }
    lastEncTime = millis();
  }
  lastEncA = encA;
  lastEncB = encB;

  if (encDelta != 0) {
    MenuNavigation(encDelta);
    encDelta = 0;
    drawMenu();
  }
}

// --- Encoder Button Handling ---
void handleButton() {
  bool buttonState = digitalRead(ENC_SW);
  unsigned long now = millis();

  // Debounce
  if (buttonState != lastButtonState) {
    lastButtonChange = now;
    lastButtonState = buttonState;
  }

  // Button pressed
  if (buttonState == LOW && (now - lastButtonChange) > SW_DEBOUNCE_MS) {
    if (buttonPressTime == 0) {
      buttonPressTime = now;
      buttonLongPressHandled = false;
    }
    // Long press to go back
    if (!buttonLongPressHandled && (now - buttonPressTime) > SW_LONGPRESS_MS) {
      if (currentScreen != ScreenState::MAIN_MENU) {
        changeScreen(ScreenState::MAIN_MENU);
        drawMenu();
      }
      buttonLongPressHandled = true;
    }
  }
  // Button released
  else if (buttonState == HIGH && buttonPressTime != 0) {
    if (!buttonLongPressHandled && (now - buttonPressTime) < SW_LONGPRESS_MS) {
      MenuSelect();
      drawMenu();
    }
    buttonPressTime = 0;
    buttonLongPressHandled = false;
  }
}

// all functions and attacks are defined here

void scanBLEDevices() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Scanning BLE...");
  display.display();

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  BLEScanResults foundDevices = pBLEScan->start(3, false); // Scan for 3 seconds

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("BLE Devices:");
  int shown = 0;
  for (int i = 0; i < foundDevices.getCount() && shown < 5; i++) { // Show up to 5 devices
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    String name = d.getName().c_str();
    if (name.length() == 0) name = d.getAddress().toString().c_str();
    display.setCursor(0, 12 + shown * 10);
    display.println(name);
    shown++;
  }
  if (shown == 0) {
    display.setCursor(0, 12);
    display.println("None found.");
  }
  display.display();
  delay(2000); // Show results for 2 seconds

  pBLEScan->clearResults();
}


// --- Menu Navigation ---
void MenuNavigation(int8_t delta) {
  uint8_t count = 0;
  switch (currentScreen) {
    case ScreenState::MAIN_MENU: count = mainMenuCount; break;
    case ScreenState::BLE_MENU:  count = bleMenuCount;  break;
    // Add other menus here
    default: count = mainMenuCount; break;
  }
  if (delta > 0) {
    selectedIndex = (selectedIndex + 1) % count;
  } else if (delta < 0) {
    selectedIndex = (selectedIndex == 0) ? count - 1 : selectedIndex - 1;
  }
}

// --- Menu Selection ---
void MenuSelect() {
  switch (currentScreen) {
    case ScreenState::MAIN_MENU:
      switch (selectedIndex) {
        case 0: changeScreen(ScreenState::BLE_MENU); break;
        case 1: changeScreen(ScreenState::WIFI_MENU); break;
        case 2: changeScreen(ScreenState::NFC_MENU); break;
        case 3: changeScreen(ScreenState::SETTINGS_MENU); break;
      }
      break;
    case ScreenState::BLE_MENU:
      if (selectedIndex == 1) { // BLE Scanner
        scanBLEDevices();
      } else if (selectedIndex == bleMenuCount - 1) { // "Back"
        changeScreen(ScreenState::MAIN_MENU);
      } else {
        // Placeholder for other BLE actions
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Selected: ");
        display.println(bleMenuItems[selectedIndex]);
        display.display();
        delay(800);
      }
      break;
    // Add other submenu logic here
    default:
      break;
  }
}

// --- Change Screen ---
void changeScreen(ScreenState newScreen) {
  currentScreen = newScreen;
  selectedIndex = 0;
}

// --- Draw Menu ---
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  const char** items = nullptr;
  uint8_t count = 0;
  const char* title = "";

  switch (currentScreen) {
    case ScreenState::MAIN_MENU:
      items = mainMenuItems; count = mainMenuCount; title = "Main Menu"; break;
    case ScreenState::BLE_MENU:
      items = bleMenuItems; count = bleMenuCount; title = "BLE Attacks"; break;
    // Add other menus here
    default:
      items = mainMenuItems; count = mainMenuCount; title = "Menu"; break;
  }

  display.setCursor(0, 0);
  display.println(title);

  for (uint8_t i = 0; i < count; i++) {
    display.setCursor(0, 14 + i * 11);
    if (i == selectedIndex) display.print("> ");
    else display.print("  ");
    display.println(items[i]);
  }
  display.display();
}