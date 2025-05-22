#include <Wire.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi Configuration
const char* ssid = "Aa"; // Update with your WiFi SSID
const char* password = "87654321"; // Update with your WiFi password

// Server Configuration
const char* serverUrl = "http://192.168.228.45:3000/api/orders"; // Updated to your WiFi IP
const char* inventoryUrl = "http://192.168.228.45:3000/api/items"; // Updated to fetch menu items

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button Pins (active-HIGH)
#define BTN1_PIN 12  // Back/Exit
#define BTN2_PIN 13  // Select/Confirm
#define BTN3_PIN 14  // Down/Decrease
#define BTN4_PIN 27  // Up/Increase

// Checkmark Icon (16x16 pixels)
static const unsigned char PROGMEM checkmark_icon[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x03, 
  0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 
  0xFC, 0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// Menu Structure
struct MenuItem {
  String id;
  String name;
  float price;
  int quantity;
};

const int maxMenuItems = 20; // Maximum number of menu items we can handle
MenuItem menuItems[maxMenuItems];
int menuItemCount = 0;

// Application State
enum AppState {SPLASH, MENU, QUANTITY, ORDER, CONFIRMATION, WIFI_CONNECTING, FETCHING_MENU};
AppState currentState = SPLASH;
int cursorPos = 0;
int selectedIndex = -1;
const int visibleItems = 3;

// Order Tracking
float orderTotal = 0;
int itemCount = 0;

// WiFi Management
bool wifiConnected = false;
unsigned long wifiConnectStartTime = 0;
const unsigned long wifiTimeout = 30000; // Increased to 30 seconds for better WiFi connection

void setup() {
  Serial.begin(115200);
  
  // Initialize Display
  if(!display.begin(0x3C, true)) {
    Serial.println(F("Display initialization failed"));
    while(true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  // Initialize Buttons
  pinMode(BTN1_PIN, INPUT);
  pinMode(BTN2_PIN, INPUT);
  pinMode(BTN3_PIN, INPUT);
  pinMode(BTN4_PIN, INPUT);
  
  // Start WiFi Connection
  connectToWiFi();
  
  showSplashScreen();
}

void loop() {
  if (currentState == WIFI_CONNECTING) {
    checkWiFiConnection();
    delay(500);
    return;
  }

  if (currentState == FETCHING_MENU) {
    fetchMenuItems();
    return;
  }

  bool btn1 = digitalRead(BTN1_PIN) == HIGH;
  bool btn2 = digitalRead(BTN2_PIN) == HIGH;
  bool btn3 = digitalRead(BTN3_PIN) == HIGH;
  bool btn4 = digitalRead(BTN4_PIN) == HIGH;

  if (btn1 || btn2 || btn3 || btn4) {
    delay(200); // Debounce delay
    
    switch(currentState) {
      case SPLASH:
        if (btn1) {
          if (menuItemCount == 0) {
            fetchMenuItems();
          } else {
            currentState = MENU;
            showMenuScreen();
          }
        }
        break;
        
      case MENU:
        handleMenuScreenButtons(btn1, btn2, btn3, btn4);
        break;
        
      case QUANTITY:
        handleQuantityScreenButtons(btn1, btn2, btn3, btn4);
        break;
        
      case ORDER:
        handleOrderScreenButtons(btn1, btn2);
        break;
        
      case CONFIRMATION:
        if (btn1 || btn2) {
          resetOrder();
          currentState = MENU;
          showMenuScreen();
        }
        break;
    }
  }
  delay(10);
}

void fetchMenuItems() {
  if (!wifiConnected) {
    showMessageScreen("No WiFi", "Cannot fetch menu");
    Serial.println("WiFi not connected in fetchMenuItems");
    delay(1500);
    currentState = SPLASH;
    showSplashScreen();
    return;
  }

  currentState = FETCHING_MENU;
  showMessageScreen("Fetching menu...", "Please wait");
  
  HTTPClient http;
  Serial.print("Attempting to fetch menu from: ");
  Serial.println(inventoryUrl);
  http.begin(inventoryUrl);
  http.setTimeout(10000); // 10 seconds timeout

  int httpResponseCode = http.GET();
  
  if (httpResponseCode <= 0) {
    Serial.print("Menu fetch failed with code: ");
    Serial.println(httpResponseCode);
    Serial.print("Error description: ");
    Serial.println(http.errorToString(httpResponseCode));
    showMessageScreen("Menu fetch failed", "Error: " + String(httpResponseCode));
    delay(1500);
    http.end();
    currentState = SPLASH;
    showSplashScreen();
    return;
  }

  String response = http.getString();
  Serial.print("Server response: ");
  Serial.println(response);

  DynamicJsonDocument doc(2048); // Adjust size based on expected response
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    showMessageScreen("Menu fetch failed", "Invalid response");
    delay(1500);
    http.end();
    currentState = SPLASH;
    showSplashScreen();
    return;
  }

  // Clear existing menu items
  menuItemCount = 0;
  
  for (JsonObject item : doc.as<JsonArray>()) {
    if (menuItemCount >= maxMenuItems) break;
    
    menuItems[menuItemCount].id = item["_id"].as<String>();
    menuItems[menuItemCount].name = item["name"].as<String>();
    menuItems[menuItemCount].price = item["price"].as<float>();
    menuItems[menuItemCount].quantity = 0;
    menuItemCount++;
  }

  Serial.print("Loaded ");
  Serial.print(menuItemCount);
  Serial.println(" menu items");
  
  http.end();
  
  currentState = MENU;
  showMenuScreen();
}

void handleMenuScreenButtons(bool btn1, bool btn2, bool btn3, bool btn4) {
  Serial.print("Menu Screen - cursorPos: ");
  Serial.println(cursorPos);
  
  if (btn1) {
    currentState = SPLASH;
    showSplashScreen();
  }
  else if (btn2) {
    if (cursorPos == menuItemCount) { // Place order
      if (prepareOrder()) {
        currentState = ORDER;
        showOrderScreen();
      }
    } else if (cursorPos == menuItemCount + 1) { // Cancel order
      resetOrder();
      showMenuScreen();
    } else { // Select item
      selectedIndex = cursorPos;
      currentState = QUANTITY;
      showQuantityScreen();
    }
  }
  else if (btn3) { // Move down
    cursorPos = min(cursorPos + 1, menuItemCount + 1);
    showMenuScreen();
  }
  else if (btn4) { // Move up
    cursorPos = max(cursorPos - 1, 0);
    showMenuScreen();
  }
}

void handleQuantityScreenButtons(bool btn1, bool btn2, bool btn3, bool btn4) {
  if (btn1 || btn2) {
    currentState = MENU;
    showMenuScreen();
  }
  else if (btn3) { // Decrease quantity
    menuItems[selectedIndex].quantity = max(menuItems[selectedIndex].quantity - 1, 0);
    showQuantityScreen();
  }
  else if (btn4) { // Increase quantity
    menuItems[selectedIndex].quantity++;
    showQuantityScreen();
  }
}

bool sendingOrder = false;
void handleOrderScreenButtons(bool btn1, bool btn2) {
  if (btn1) {
    currentState = MENU;
    showMenuScreen();
  }
  else if (btn2 && !sendingOrder) {
    sendingOrder = true;
    if (sendOrder()) {
      currentState = CONFIRMATION;
      showConfirmationScreen();
    }
    sendingOrder = false;
  }
}

void connectToWiFi() {
  currentState = WIFI_CONNECTING;
  showWifiConnectingScreen();
  
  WiFi.begin(ssid, password);
  wifiConnectStartTime = millis();
  
  Serial.println("Connecting to WiFi...");
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiConnectStartTime > wifiTimeout) {
      Serial.println("Failed to connect to WiFi");
      wifiConnected = false;
      currentState = SPLASH;
      showSplashScreen();
      return;
    }
    return;
  }
  
  wifiConnected = true;
  Serial.println("WiFi connected");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  
  currentState = SPLASH;
  showSplashScreen();
}

bool checkItemAvailability() {
  if (!wifiConnected) {
    showMessageScreen("No WiFi", "Cannot check stock");
    delay(1500);
    return false;
  }

  HTTPClient http;
  String checkUrl = String(inventoryUrl) + "/check";
  http.begin(checkUrl);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  JsonArray items = doc.createNestedArray("items");
  
  for (int i = 0; i < menuItemCount; i++) {
    if (menuItems[i].quantity > 0) {
      JsonObject item = items.createNestedObject();
      item["_id"] = menuItems[i].id;
      item["name"] = menuItems[i].name;
      item["price"] = menuItems[i].price;
      item["quantity"] = menuItems[i].quantity;
    }
  }

  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println("Sending: " + jsonString);

  showMessageScreen("Checking stock...", "Please wait");
  int httpCode = http.POST(jsonString);

  if (httpCode <= 0) {
    http.end();
    showMessageScreen("Network Error", String(httpCode));
    delay(1500);
    return false;
  }

  String response = http.getString();
  Serial.println("Response: " + response);
  http.end();

  DynamicJsonDocument resDoc(512);
  deserializeJson(resDoc, response);

  bool success = resDoc["success"];
  

  return success;
}

bool updateDatabase() {
  HTTPClient http;
  String updateUrl = String(inventoryUrl) + "/update";
  http.begin(updateUrl);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  JsonArray items = doc.createNestedArray("items");

  for (int i = 0; i < menuItemCount; i++) {
    if (menuItems[i].quantity > 0) {
      JsonObject item = items.createNestedObject();
      item["_id"] = menuItems[i].id; // Use "_id"
      item["quantity"] = menuItems[i].quantity;
    }
  }

  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println("Sending update: " + jsonString);
  
  int httpCode = http.POST(jsonString);
  if (httpCode != 200) {
    Serial.print("Update failed with code: ");
    Serial.println(httpCode);
    showMessageScreen("Update failed", String(httpCode));
    delay(1500);
    http.end();
    return false;
  }

  Serial.println("Update successful");
  http.end();
  return true;
}

bool prepareOrder() {
  orderTotal = 0;
  itemCount = 0;
  
  for (int i = 0; i < menuItemCount; i++) {
    if (menuItems[i].quantity > 0) {
      orderTotal += menuItems[i].price * menuItems[i].quantity;
      itemCount += menuItems[i].quantity;
    }
  }
  
  if (itemCount == 0) {
    showMessageScreen("No items selected!", "Add items first");
    delay(1500);
    showMenuScreen();
    return false;
  }
  return true;
}

bool sendOrder() {
  Serial.println("Starting sendOrder()...");
  
  if (!checkItemAvailability()) {
    Serial.println("Item not available - returning to menu");
    currentState = MENU;
    showMenuScreen();
    return false;
  }
  
  DynamicJsonDocument doc(1024);
  JsonArray items = doc.createNestedArray("items");
  
  for (int i = 0; i < menuItemCount; i++) {
    if (menuItems[i].quantity > 0) {
      JsonObject item = items.createNestedObject();
      item["_id"] = menuItems[i].id; // Ensure menuItems[i].id contains the correct ObjectId
      item["name"] = menuItems[i].name;
      item["price"] = menuItems[i].price;
      item["quantity"] = menuItems[i].quantity;
    }
  }
  
  doc["total"] = orderTotal;
  doc["itemCount"] = itemCount;
  
  serializeJsonPretty(doc, Serial);
  Serial.println();
  
  if (wifiConnected) {
    if (sendOrderToServer(doc)) {
      if (!updateDatabase()) {
        Serial.println("ERROR: Stock update failed");
        showMessageScreen("Order sent", "Stock update failed");
        delay(1500);
        return false;
      }
      return true;
    } else {
      return false;
    }
  } else {
    showMessageScreen("Order saved", "WiFi not connected");
    delay(1500);
    return false;
  }
}

bool sendOrderToServer(DynamicJsonDocument& doc) {
  HTTPClient http;
  
  Serial.print("Attempting to connect to: ");
  Serial.println(serverUrl);
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.setTimeout(10000);
  
  // Use the original JSON without transformation (since it already uses "_id")
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.print("Sending JSON: ");
  Serial.println(jsonString);
  
  showMessageScreen("Sending order...", "Please wait");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected before sending order");
    wifiConnected = false;
    showMessageScreen("WiFi Error", "Disconnected");
    delay(1500);
    http.end();
    return false;
  }
  
  int httpResponseCode = http.POST(jsonString);
  
  String response = http.getString();
  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);
  Serial.print("Server response: ");
  Serial.println(response);
  
  if (httpResponseCode != 200) {
    DynamicJsonDocument errorDoc(512);
    deserializeJson(errorDoc, response);
    String errorMsg = errorDoc["message"] | "Unknown error";
    Serial.print("Error message: ");
    Serial.println(errorMsg);
    showMessageScreen("Send failed", errorMsg);
    delay(1500);
    http.end();
    return false;
  }
  
  http.end();
  return true;
}

void resetOrder() {
  for (int i = 0; i < menuItemCount; i++) {
    menuItems[i].quantity = 0;
  }
  cursorPos = 0;
  orderTotal = 0;
  itemCount = 0;
}

void showWifiConnectingScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 10);
  display.println("Connecting to WiFi");
  display.setCursor(10, 30);
  display.print("SSID: ");
  display.print(ssid);
  display.setCursor(10, 45);
  display.println("Please wait...");
  display.display();
}

void showSplashScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("Bistro92");
  display.setTextSize(1);
  display.setCursor(20, 35);
  display.println("Smart Order");
  display.setCursor(10, 50);
  display.print("WiFi: ");
  display.print(wifiConnected ? "Connected" : "Offline");
  display.setCursor(10, 56);
  display.println("Press BTN1 for Menu");
  display.display();
}

void showMenuScreen() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Menu (Select: BTN2)");
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  
  int firstVisible = max(0, min(cursorPos - (visibleItems - 1), menuItemCount + 2 - visibleItems));
  
  for (int i = 0; i < visibleItems; i++) {
    int itemIdx = firstVisible + i;
    if (itemIdx >= menuItemCount + 2) continue;
    
    int yPos = 15 + (i * 15);
    
    if (itemIdx == cursorPos) {
      display.fillRect(0, yPos - 2, 128, 12, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    } else {
      display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    }
    
    if (itemIdx < menuItemCount) {
      display.setCursor(5, yPos);
      display.print(menuItems[itemIdx].name);
      display.setCursor(80, yPos);
      display.print("$");
      display.print(menuItems[itemIdx].price, 2);
      
      if (menuItems[itemIdx].quantity > 0) {
        display.setCursor(100, yPos);
        display.print("x");
        display.print(menuItems[itemIdx].quantity);
      }
    } else if (itemIdx == menuItemCount) {
      display.setCursor(5, yPos);
      display.print("> PLACE ORDER");
    } else if (itemIdx == menuItemCount + 1) {
      display.setCursor(5, yPos);
      display.print("> CANCEL ORDER");
    }
  }
  
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.drawLine(0, 54, 128, 54, SH110X_WHITE);
  
  int totalItems = 0;
  for (int i = 0; i < menuItemCount; i++) {
    totalItems += menuItems[i].quantity;
  }
  
  display.setCursor(5, 56);
  if (totalItems > 0) {
    display.print("Items: ");
    display.print(totalItems);
    display.print(" $");
    display.print(orderTotal, 2);
  } else {
    display.print("Select items to order");
  }
  
  display.display();
  Serial.print("Menu Screen - firstVisible: ");
  Serial.print(firstVisible);
  Serial.print(", cursorPos: ");
  Serial.println(cursorPos);
}

void showQuantityScreen() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Adjust Quantity:");
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  
  display.setCursor(5, 15);
  display.print("Item: ");
  display.print(menuItems[selectedIndex].name);
  display.setCursor(5, 25);
  display.print("Price: $");
  display.print(menuItems[selectedIndex].price, 2);
  
  display.fillRect(0, 35, 128, 15, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(5, 40);
  display.print("Quantity: ");
  display.print(menuItems[selectedIndex].quantity);
  
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(5, 55);
  display.print("Subtotal: $");
  display.print(menuItems[selectedIndex].price * menuItems[selectedIndex].quantity, 2);
  
  display.setCursor(70, 15);
  display.print("BTN3: -1");
  display.setCursor(70, 25);
  display.print("BTN4: +1");
  display.setCursor(70, 35);
  display.print("BTN2: Back");
  
  display.display();
}

void showOrderScreen() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ORDER SUMMARY");
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  
  int yPos = 15;
  for (int i = 0; i < menuItemCount; i++) {
    if (menuItems[i].quantity > 0) {
      display.setCursor(5, yPos);
      display.print(menuItems[i].name);
      display.print(" x");
      display.print(menuItems[i].quantity);
      display.setCursor(80, yPos);
      display.print("$");
      display.print(menuItems[i].price * menuItems[i].quantity, 2);
      yPos += 10;
    }
  }
  
  display.drawLine(0, yPos + 2, 128, yPos + 2, SH110X_WHITE);
  display.setCursor(5, yPos + 5);
  display.print("TOTAL: $");
  display.print(orderTotal, 2);
  
  display.setCursor(5, 56);
  display.print("BTN1:Back  BTN2:Confirm");
  
  display.display();
}

void showConfirmationScreen() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(30, 10);
  display.println("ORDER CONFIRMED!");
  display.drawBitmap(56, 25, checkmark_icon, 16, 16, SH110X_WHITE);
  display.setCursor(10, 56);
  display.print("Press any btn to continue");
  display.display();
}

void showMessageScreen(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println(line1);
  display.setCursor(10, 40);
  display.println(line2);
  display.display();
}