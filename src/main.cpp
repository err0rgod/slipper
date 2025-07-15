#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <esp_bt.h>

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Rotary encoder pins
#define ENC_CLK 27
#define ENC_DT 26
#define ENC_SW 25

// Debounce and encoder timing
#define ENC_DEBOUNCE_MS 2
#define SW_DEBOUNCE_MS 50
#define SW_LONGPRESS_MS 600

// Menu system
enum class ScreenState
{
  MAIN_MENU,
  BLE_MENU,
  WIFI_MENU,
  NFC_MENU,
  SETTINGS_MENU,
  BLE_SCAN_RESULTS,
  // Add more as needed
};

#define MAX_BLE_SCAN_RESULTS 5
BLEAdvertisedDevice *scannedDevices[MAX_BLE_SCAN_RESULTS];

int scannedDeviceCount = 0;

const char *mainMenuItems[] = {
    "BLE Attacks",
    "WiFi Tools",
    "NFC Tools",
    "Settings"};
const uint8_t mainMenuCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);

const char *bleMenuItems[] = {
    "Beacon Flood",
    "BLE Scanner",
    "HID Emulation",
    "Back"};
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

// --- Beacon Flood Globals ---
bool beaconFloodActive = false;
unsigned long lastFloodTime = 0;
unsigned int floodInterval = 200; // ms, can randomize between 100-500ms
BLEAdvertising *pAdvertising = nullptr;

// Forward declarations
void drawMenu();
void handleEncoder();
void handleButton();
void changeScreen(ScreenState newScreen);

void MenuNavigation(int8_t delta);
void MenuSelect();

// --- Helper: Generate random MAC address ---
void randomMac(uint8_t *mac)
{
  for (int i = 0; i < 6; i++)
    mac[i] = random(0, 256);
  // Set locally administered and unicast bits
  mac[0] = (mac[0] & 0xFE) | 0x02;
}

// --- Helper: Generate random device name ---
String randomName()
{
  char buf[12];
  sprintf(buf, "BLE_%04X", random(0x10000));
  return String(buf);
}

// --- Start Beacon Flood ---
void startBeaconFlood()
{
  beaconFloodActive = true;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Beacon Flood:");
  display.println("Active");
  display.display();

  BLEDevice::deinit(true); // Clean up previous BLE state
  BLEDevice::init("");     // Re-init BLE

  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);
  pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
  pAdvertising->setMinInterval(0x20); // 20ms
  pAdvertising->setMaxInterval(0x40); // 40ms

  lastFloodTime = millis();
}

// --- Stop Beacon Flood ---
void stopBeaconFlood()
{
  beaconFloodActive = false;
  if (pAdvertising)
  {
    pAdvertising->stop();
    pAdvertising = nullptr;
  }
  BLEDevice::deinit(true);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Beacon Flood:");
  display.println("Stopped");
  display.display();
}

// --- Call this in loop() ---
void beaconFloodLoop()
{
  if (!beaconFloodActive)
    return;
  unsigned long now = millis();
  if (now - lastFloodTime >= floodInterval)
  {
    // Randomize interval for next beacon
    floodInterval = random(100, 501);

    // Generate random MAC
    uint8_t mac[6];
    randomMac(mac);
    esp_ble_gap_set_rand_addr(mac);

    // Generate random name
    String name = randomName();

    //BLEDevice::setDeviceName(name.c_str());
    // BLEDevice::getAdvertising()->setName(name.c_str());
    //BLEAdvertisementData advData;
  

    // Prepare advertisement data
    BLEAdvertisementData advData;
    advData.setFlags(0x06); // General discoverable, BR/EDR not supported
    advData.setName(name.c_str());

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
    // Stop after a short burst to allow new adv
    delay(20);
    pAdvertising->stop();

    lastFloodTime = now;
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("OLED not found"));
    for (;;)
      ;
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

void loop()
{
  handleEncoder();
  handleButton();
  beaconFloodLoop();
}

// --- Rotary Encoder Handling ---
void handleEncoder()
{
  int encA = digitalRead(ENC_CLK);
  int encB = digitalRead(ENC_DT);

  if (encA != lastEncA && encA == LOW)
  { // Only on falling edge
    if (encB == HIGH)
    {
      encDelta++;
    }
    else
    {
      encDelta--;
    }
    lastEncTime = millis();
  }
  lastEncA = encA;
  lastEncB = encB;

  if (encDelta != 0)
  {
    MenuNavigation(encDelta);
    encDelta = 0;
    drawMenu();
  }
}

// --- Encoder Button Handling ---
void handleButton()
{
  bool buttonState = digitalRead(ENC_SW);
  unsigned long now = millis();

  // Debounce
  if (buttonState != lastButtonState)
  {
    lastButtonChange = now;
    lastButtonState = buttonState;
  }

  // Button pressed
  if (buttonState == LOW && (now - lastButtonChange) > SW_DEBOUNCE_MS)
  {
    if (buttonPressTime == 0)
    {
      buttonPressTime = now;
      buttonLongPressHandled = false;
    }
    // Long press to go back
    if (!buttonLongPressHandled && (now - buttonPressTime) > SW_LONGPRESS_MS)
    {
      if (currentScreen != ScreenState::MAIN_MENU)
      {
        changeScreen(ScreenState::MAIN_MENU);
        drawMenu();
      }
      buttonLongPressHandled = true;
    }
  }
  // Button released
  else if (buttonState == HIGH && buttonPressTime != 0)
  {
    if (!buttonLongPressHandled && (now - buttonPressTime) < SW_LONGPRESS_MS)
    {
      MenuSelect();
      drawMenu();
    }
    buttonPressTime = 0;
    buttonLongPressHandled = false;
  }
}

// --- Free Scanned Devices --- for avoiding memory leaks
void freeScannedDevices()
{
  for (int i = 0; i < scannedDeviceCount; i++)
  {
    delete scannedDevices[i];
    scannedDevices[i] = nullptr;
  }
  scannedDeviceCount = 0;
}

// all functions and attacks are defined here
void scanBLEDevices()
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Scanning BLE...");
  display.display();

  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  BLEScanResults foundDevices = pBLEScan->start(3, false); // Scan for 3 seconds

  // Store pointers to found devices
  scannedDeviceCount = 0;
  for (int i = 0; i < foundDevices.getCount() && scannedDeviceCount < MAX_BLE_SCAN_RESULTS; i++)
  {
    BLEAdvertisedDevice *d = new BLEAdvertisedDevice(foundDevices.getDevice(i));
    scannedDevices[scannedDeviceCount++] = d;
  }
  pBLEScan->clearResults();

  changeScreen(ScreenState::BLE_SCAN_RESULTS); // Go to device selection screen
  drawMenu();
  freeScannedDevices();
}
/*void scanBLEDevices() {
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
}*/

// --- Menu Navigation ---
void MenuNavigation(int8_t delta)
{
  uint8_t count = 0;
  switch (currentScreen)
  {
  case ScreenState::MAIN_MENU:
    count = mainMenuCount;
    break;
  case ScreenState::BLE_MENU:
    count = bleMenuCount;
    break;
  case ScreenState::BLE_SCAN_RESULTS:
    count = scannedDeviceCount;
    break;
  // Add other menus here
  default:
    count = mainMenuCount;
    break;
  }
  if (count == 0)
    return;
  if (delta > 0)
  {
    selectedIndex = (selectedIndex + 1) % count;
  }
  else if (delta < 0)
  {
    selectedIndex = (selectedIndex == 0) ? count - 1 : selectedIndex - 1;
  }
}

// --- Menu Selection ---
void MenuSelect()
{
  switch (currentScreen)
  {
  case ScreenState::MAIN_MENU:
    switch (selectedIndex)
    {
    case 0:
      changeScreen(ScreenState::BLE_MENU);
      break;
    case 1:
      changeScreen(ScreenState::WIFI_MENU);
      break;
    case 2:
      changeScreen(ScreenState::NFC_MENU);
      break;
    case 3:
      changeScreen(ScreenState::SETTINGS_MENU);
      break;
    }
    break;
  case ScreenState::BLE_MENU:
    if (selectedIndex == 0)
    { // Beacon Flood
      startBeaconFlood();
      // Wait for button press to stop
      while (digitalRead(ENC_SW) == HIGH)
      {
        beaconFloodLoop();
        delay(10);
      }
      stopBeaconFlood();
      changeScreen(ScreenState::BLE_MENU);
      drawMenu();
    }
    else if (selectedIndex == 1)
    { // BLE Scanner
      scanBLEDevices();
    }
    else if (selectedIndex == bleMenuCount - 1)
    { // "Back"
      changeScreen(ScreenState::MAIN_MENU);
    }
    else
    {
      // Placeholder for other BLE actions
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Selected: ");
      display.println(bleMenuItems[selectedIndex]);
      display.display();
      delay(800);
    }
    break;
  case ScreenState::BLE_SCAN_RESULTS:
    if (scannedDeviceCount > 0)
    {
      // Device selected, do something with scannedDevices[selectedIndex]
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Selected device:");
      String name = scannedDevices[selectedIndex]->getName().c_str();
      if (name.length() == 0)
        name = scannedDevices[selectedIndex]->getAddress().toString().c_str();
      display.println(name);
      display.display();
      delay(1500);
      // After selection, go back to BLE menu
      changeScreen(ScreenState::BLE_MENU);
    }
    break;
  // Add other submenu logic here
  default:
    break;
  }
}

// --- Change Screen ---
void changeScreen(ScreenState newScreen)
{
  currentScreen = newScreen;
  selectedIndex = 0;
}

// --- Draw Menu ---
void drawMenu()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  const char **items = nullptr;
  uint8_t count = 0;
  const char *title = "";

  switch (currentScreen)
  {
  case ScreenState::MAIN_MENU:
    items = mainMenuItems;
    count = mainMenuCount;
    title = "Main Menu";
    break;
  case ScreenState::BLE_MENU:
    items = bleMenuItems;
    count = bleMenuCount;
    title = "BLE Attacks";
    break;
  case ScreenState::BLE_SCAN_RESULTS:
    title = "Select Device";
    count = scannedDeviceCount;
    break;
  // Add other menus here
  default:
    items = mainMenuItems;
    count = mainMenuCount;
    title = "Menu";
    break;
  }

  display.setCursor(0, 0);
  display.println(title);

  if (currentScreen == ScreenState::BLE_SCAN_RESULTS)
  {
    for (uint8_t i = 0; i < count; i++)
    {
      display.setCursor(0, 14 + i * 11);
      if (i == selectedIndex)
        display.print("> ");
      else
        display.print("  ");
      String name = scannedDevices[i]->getName().c_str();
      if (name.length() == 0)
        name = scannedDevices[i]->getAddress().toString().c_str();
      display.println(name);
    }
    if (count == 0)
    {
      display.setCursor(0, 14);
      display.println("None found.");
    }
  }
  else
  {
    for (uint8_t i = 0; i < count; i++)
    {
      display.setCursor(0, 14 + i * 11);
      if (i == selectedIndex)
        display.print("> ");
      else
        display.print("  ");
      display.println(items[i]);
    }
  }
  display.display();
}
// Uncomment this to use the drawMenu function of previous versions without BLE scan results
/*void drawMenu() {
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
}*/