#include <WiFi.h>
#include <esp_wifi.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <string>

// LCD Configuration (I2C)
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// Button Pins
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_SELECT 25
#define BTN_BACK 26

// Device Limits
#define MAX_WIFI_DEVICES 25
#define MAX_BLE_DEVICES 25

// --- Enums for State Management ---
enum MenuState {
  MAIN_MENU,
  WIFI_SCAN_LIST,
  BLE_SCAN_LIST,
  WIFI_DETAILS,
  BLE_DETAILS
};

// --- Structures for Device Information ---
struct WiFiDeviceInfo {
  String ssid;
  String mac;
  int channel;
  int rssi;
  wifi_auth_mode_t security;
};

struct BLEDeviceInfo {
  String name;
  String address;
  int rssi;
  int txPower;
  std::string serviceUUID = "";
};

// --- Global Variables ---
WiFiDeviceInfo wifiDevices[MAX_WIFI_DEVICES];
BLEDeviceInfo bleDevices[MAX_BLE_DEVICES];
int wifiDeviceCount = 0;
int bleDeviceCount = 0;

MenuState currentState = MAIN_MENU;
int listIndex = 0;       // For scrolling through device lists
int detailPage = 0;      // For scrolling through detail pages
unsigned long lastScanTime = 0;
const unsigned long SCAN_INTERVAL = 10000; // 10 seconds

// Button Debounce
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 200;

// --- Function Prototypes ---
void updateDisplay();
void handleButtons();
bool isButtonPressed(int pin);
void refreshScan();
void scanWiFi();
void scanBLE();
String getWifiSecurityString(wifi_auth_mode_t security);
void drawMainMenu();
void drawWifiList();
void drawBleList();
void drawWifiDetails();
void drawBleDetails();

// =================================================================
// SETUP
// =================================================================
void setup() {
  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Scanner Starting");
  delay(1000);

  // Setup buttons with internal pull-ups
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Initialize BLE
  BLEDevice::init("ESP32-Scanner");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  updateDisplay();
}

// =================================================================
// MAIN LOOP
// =================================================================
void loop() {
  handleButtons();

  // Auto-refresh scan lists
  if ((currentState == WIFI_SCAN_LIST || currentState == BLE_SCAN_LIST) && 
      (millis() - lastScanTime > SCAN_INTERVAL)) {
    refreshScan();
  }
  
  delay(50); // Small delay to prevent hammering the CPU
}

// =================================================================
// CORE LOGIC
// =================================================================

void refreshScan() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scanning...");
  
  if (currentState == WIFI_SCAN_LIST) {
    scanWiFi();
  } else if (currentState == BLE_SCAN_LIST) {
    scanBLE();
  }
  
  listIndex = 0; // Reset index after scan
  lastScanTime = millis();
  updateDisplay();
}

void handleButtons() {
  // --- UP Button ---
  if (isButtonPressed(BTN_UP)) {
    if (currentState == WIFI_DETAILS || currentState == BLE_DETAILS) {
      detailPage--; // Navigate up through detail pages
    } else {
      listIndex--; // Navigate up through device list
    }
    updateDisplay();
  }

  // --- DOWN Button ---
  if (isButtonPressed(BTN_DOWN)) {
    if (currentState == WIFI_DETAILS || currentState == BLE_DETAILS) {
      detailPage++; // Navigate down through detail pages
    } else {
      listIndex++; // Navigate down through device list
    }
    updateDisplay();
  }

  // --- SELECT Button ---
  if (isButtonPressed(BTN_SELECT)) {
    detailPage = 0; // Reset detail page on select
    if (currentState == MAIN_MENU) {
      currentState = (listIndex == 0) ? WIFI_SCAN_LIST : BLE_SCAN_LIST;
      refreshScan(); // Initial scan
    } else if (currentState == WIFI_SCAN_LIST && wifiDeviceCount > 0) {
      currentState = WIFI_DETAILS;
    } else if (currentState == BLE_SCAN_LIST && bleDeviceCount > 0) {
      currentState = BLE_DETAILS;
    }
    updateDisplay();
  }

  // --- BACK Button ---
  if (isButtonPressed(BTN_BACK)) {
    detailPage = 0; // Reset detail page on back
    if (currentState == WIFI_DETAILS) {
      currentState = WIFI_SCAN_LIST;
    } else if (currentState == BLE_DETAILS) {
      currentState = BLE_SCAN_LIST;
    } else {
      currentState = MAIN_MENU;
    }
    listIndex = 0;
    updateDisplay();
  }
}

bool isButtonPressed(int pin) {
  if (digitalRead(pin) == LOW) {
    if (millis() - lastDebounceTime > DEBOUNCE_DELAY) {
      lastDebounceTime = millis();
      return true;
    }
  }
  return false;
}

// =================================================================
// SCANNING FUNCTIONS
// =================================================================

void scanWiFi() {
  wifiDeviceCount = 0;
  int n = WiFi.scanNetworks(false, true); // (async, show_hidden)
  if (n > 0) {
    wifiDeviceCount = min(n, MAX_WIFI_DEVICES);
    for (int i = 0; i < wifiDeviceCount; ++i) {
      wifiDevices[i].ssid = WiFi.SSID(i);
      wifiDevices[i].mac = WiFi.BSSIDstr(i);
      wifiDevices[i].channel = WiFi.channel(i);
      wifiDevices[i].rssi = WiFi.RSSI(i);
      wifiDevices[i].security = WiFi.encryptionType(i);
    }
  }
  WiFi.scanDelete(); // Clear results from memory
}

void scanBLE() {
  bleDeviceCount = 0;
  BLEScan* pBLEScan = BLEDevice::getScan();
  BLEScanResults foundDevices = pBLEScan->start(2, false); // Scan for 2 seconds
  int count = foundDevices.getCount();
  
  for (int i = 0; i < count && bleDeviceCount < MAX_BLE_DEVICES; i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    
    // Prevent duplicates
    bool found = false;
    for(int j=0; j < bleDeviceCount; j++) {
      if (bleDevices[j].address.equals(device.getAddress().toString().c_str())) {
        found = true;
        break;
      }
    }

    if (!found) {
      bleDevices[bleDeviceCount].name = device.haveName() ? device.getName().c_str() : "N/A";
      bleDevices[bleDeviceCount].address = device.getAddress().toString().c_str();
      bleDevices[bleDeviceCount].rssi = device.haveRSSI() ? device.getRSSI() : 0;
      bleDevices[bleDeviceCount].txPower = device.haveTXPower() ? device.getTXPower() : 0;
      if(device.haveServiceUUID()) {
        bleDevices[bleDeviceCount].serviceUUID = device.getServiceUUID().toString();
      } else {
        bleDevices[bleDeviceCount].serviceUUID = "None";
      }
      bleDeviceCount++;
    }
  }
  pBLEScan->clearResults();
}

// =================================================================
// DISPLAY & UI FUNCTIONS
// =================================================================

void updateDisplay() {
  lcd.clear();
  switch (currentState) {
    case MAIN_MENU:
      drawMainMenu();
      break;
    case WIFI_SCAN_LIST:
      drawWifiList();
      break;
    case BLE_SCAN_LIST:
      drawBleList();
      break;
    case WIFI_DETAILS:
      drawWifiDetails();
      break;
    case BLE_DETAILS:
      drawBleDetails();
      break;
  }
}

void drawMainMenu() {
  // Handle index wrapping
  if (listIndex < 0) listIndex = 1;
  if (listIndex > 1) listIndex = 0;

  lcd.setCursor(0, 0);
  lcd.print(listIndex == 0 ? "-> WiFi Scanner" : "   WiFi Scanner");
  lcd.setCursor(0, 1);
  lcd.print(listIndex == 1 ? "-> BLE Scanner" : "   BLE Scanner");
}

void drawWifiList() {
  lcd.setCursor(0, 0);
  lcd.print("WiFi Networks ");
  lcd.print(wifiDeviceCount);
  
  if (wifiDeviceCount == 0) {
    lcd.setCursor(0, 1);
    lcd.print("No networks found");
    return;
  }
  
  // Handle index wrapping
  if (listIndex < 0) listIndex = wifiDeviceCount - 1;
  if (listIndex >= wifiDeviceCount) listIndex = 0;
  
  lcd.setCursor(0, 1);
  String ssid = wifiDevices[listIndex].ssid;
  if (ssid.length() == 0) ssid = "Hidden Network";
  String line = "-> " + ssid;
  lcd.print(line.substring(0, 16));
}

void drawBleList() {
  lcd.setCursor(0, 0);
  lcd.print("BLE Devices   ");
  lcd.print(bleDeviceCount);

  if (bleDeviceCount == 0) {
    lcd.setCursor(0, 1);
    lcd.print("No devices found");
    return;
  }
  
  // Handle index wrapping
  if (listIndex < 0) listIndex = bleDeviceCount - 1;
  if (listIndex >= bleDeviceCount) listIndex = 0;
  
  lcd.setCursor(0, 1);
  String name = bleDevices[listIndex].name;
  String line = "-> " + name;
  lcd.print(line.substring(0, 16));
}

void drawWifiDetails() {
  const int totalPages = 3;
  // Handle page wrapping
  if (detailPage < 0) detailPage = totalPages - 1;
  if (detailPage >= totalPages) detailPage = 0;

  String top_line = wifiDevices[listIndex].ssid;
  if (top_line.length() == 0) top_line = "Hidden Network";
  top_line.trim();
  lcd.setCursor(0, 0);
  lcd.print(top_line.substring(0, 16));

  lcd.setCursor(0, 1);
  switch (detailPage) {
    case 0: // RSSI
      lcd.print("RSSI: ");
      lcd.print(wifiDevices[listIndex].rssi);
      lcd.print(" dBm");
      break;
    case 1: // MAC Address
      lcd.print(wifiDevices[listIndex].mac);
      break;
    case 2: // Channel and Security
      lcd.print("Ch: ");
      lcd.print(wifiDevices[listIndex].channel);
      lcd.print(" Sec: ");
      lcd.print(getWifiSecurityString(wifiDevices[listIndex].security));
      break;
  }
}

void drawBleDetails() {
  const int totalPages = 4;
  // Handle page wrapping
  if (detailPage < 0) detailPage = totalPages - 1;
  if (detailPage >= totalPages) detailPage = 0;

  String top_line = bleDevices[listIndex].name;
  top_line.trim();
  lcd.setCursor(0,0);
  lcd.print(top_line.substring(0, 16));

  lcd.setCursor(0, 1);
  switch (detailPage) {
    case 0: // RSSI
      lcd.print("RSSI: ");
      lcd.print(bleDevices[listIndex].rssi);
      lcd.print(" dBm");
      break;
    case 1: // Full BLE Address
      lcd.print(bleDevices[listIndex].address);
      break;
    case 2: // TX Power
      lcd.print("TX Power: ");
      lcd.print(bleDevices[listIndex].txPower);
      lcd.print(" dB");
      break;
    case 3: // Service UUID (first part)
      lcd.print("UUID:");
      lcd.print(bleDevices[listIndex].serviceUUID.c_str());
      break;
  }
}

String getWifiSecurityString(wifi_auth_mode_t security) {
  switch (security) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-E";
    default:
      return "Unknown";
  }
}