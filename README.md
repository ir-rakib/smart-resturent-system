### 🍽️ Smart Ordering & Management System
A comprehensive system for Bistro92, combining an ESP32-based customer ordering interface and a web-based management dashboard for handling inventory and orders. Built for streamlined restaurant operations using embedded hardware, REST APIs, and responsive web design.

## 📦 Project Structure
bistro92-system/
├── esp32-ordering/         # ESP32 code for menu display & ordering
├── web-dashboard/          # Web-based management interface
├── backend-api/            # (Assumed) Node.js + Express + MongoDB API server
└── README.md               # This file

## 🧠 Features
- ✅ ESP32 Menu Ordering (Customer Interface)
- OLED-based UI with 4-button navigation
- Connects to WiFi and fetches live menu from REST API
-  Lets users:
-  rowse menu (2 items per screen)
- Increase/decrease quantity
- Place orders (POSTed to /api/orders)
-  Validates available inventory before order is placed
- Handles connection errors & server issues gracefully
- ✅ Web-Based Dashboard (Admin Interface)
## Inventory Management:
- Add new items
- Update item name, price, quantity
- Delete items
- Increment/decrement quantity (+1/-1 buttons)
- Order Management:
- View all received orders in real-time
- Each order shows items, quantity, date, and total
- Debug Panel:
- Check server health (/health)
- Custom IP input to test ESP32 access
- Responsive Bootstrap-based UI

## 🧰 Technologies Used
- Component	Tech Stack
- ESP32 Interface	C++ (Arduino) + OLED + Buttons
- Web Dashboard	HTML5, Bootstrap 5, JavaScript
- Backend (Assumed)	Node.js + Express.js + MongoDB
- Communication	REST API (JSON over HTTP)
- Hosting (Optional)	Firebase Hosting / Local server

## 🚀 Getting Started
1. Backend Server Setup (Node.js + MongoDB)
- If you haven’t already, set up the backend REST API with the following endpoints:
- GET    /api/items          → Get all inventory
- POST   /api/items          → Add new item
- PUT    /api/items/:id      → Update item (name/price/qty)
- DELETE /api/items/:id      → Delete item
- GET    /api/orders         → Get all orders
- POST   /api/orders         → Submit order (from ESP32)
- GET    /health             → Server and MongoDB status
2. ESP32 Setup (Customer Interface)
Upload your Arduino sketch to ESP32
Define:
WiFi SSID & password
API base URL (e.g., http://192.168.228.45:3000)
Use OLED (I2C), 4 buttons (Up, Down, OK, Back)

3. Web Dashboard (Admin Interface)
Open web-dashboard/index.html in browser

Use the "Custom Server URL" field to connect to your local or hosted backend

View/add/edit inventory

View real-time customer orders

## ⚠️ Error Handling
- ✅ ESP32 checks server health before operations

- ✅ Web UI shows debug messages if:

- API server unreachable

- Invalid data submission

- Inventory load fails

- 🚫 Orders not placed if item quantity < requested

## 📌 To-Do / Enhancements
 - Authentication for dashboard access

 - Order status update (e.g., served, pending)

 - Real-time updates using WebSockets or polling

 - Export order reports (CSV/PDF)

## 🤝 Acknowledgements
Developed as part of an IoT + Web full-stack solution for restaurant automation.
Thanks to Bootstrap, MongoDB, and ESP32 Community.
