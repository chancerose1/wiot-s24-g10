#include <Arduino.h>
#include <WiFi.h> // arduino-esp32 wifi api
#include <esp_wifi.h> // esp-idf wifi api
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <string>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <WebServer.h>

BLECharacteristic* pCharacteristic;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

/*
* Group Members: Alby Alex, Chance Rose, Chance Woosley
*/

std::unordered_map<std::string, std::string> students;
std::unordered_map<std::string, unsigned long> codes;
std::unordered_map<std::string, unsigned long> attendance;
unsigned long time_value = 0;


WebServer server(80);
void handleRoot() {
  server.send(200, "text/html", "<h1>Hello from device!</h1>");
}

void handleAuth(){
    std::string userid = std::string((server.arg("userid")).c_str());
    Serial.println(String(userid.c_str()));
    std::string random = std::string((server.arg("randomval")).c_str());
    Serial.println(String(random.c_str()));
    auto it = codes.find(random);
    if(it==codes.end()){
        server.send(200, "text/html", "<h1>Invalid Code</h1>");
    }
    else{
        auto it2 = students.find(userid);
        if(it2==students.end()){
            server.send(200, "text/html", "<h1>Invalid User</h1>");
        }
        else{
            attendance[students[userid]] = codes[random];
            server.send(200, "text/html", "<h1>Success</h1>");
        }
    }
}
void handleShow(){
    std::string html = "<table border='1'>\n";
    html += "<tr><th>Student</th><th>Time (s since start)</th></tr>\n";
    
    for (const auto& pair : attendance) {
        html += "<tr><td>" + pair.first + "</td><td>" + std::to_string(pair.second/1000) + "</td></tr>\n";
    }
    
    html += "</table>";
    server.send(200, "text/html", html.c_str());
}

void handleForm(){
    String html = "<form action=\"#\" method=\"post\"><label for=\"userid\">User ID: </label><input type=\"text\" id=\"userid\" name=\"userid\" required><br><br><label for=\"randomval\">Random Code: </label><input type=\"text\" id=\"randomval\" name=\"randomval\" required><br><br><input type=\"submit\" value=\"Submit\"></form>";
    server.send(200, "text/html", html);
}

void setup()
{
    std::vector<std::pair<std::string, std::string>> itemsToInsert = {
        {"a_alex", "Alby Alex"},
        {"c_woosley", "Chance Woosley"},
        {"c_rose", "Chance Rose"}
    };
    students.insert(itemsToInsert.begin(), itemsToInsert.end());
    randomSeed(analogRead(0));
    Serial.begin(115200);
    delay(10);
    Serial.println("Starting BLE work!");
    BLEDevice::init("group10thing");
    BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setValue("No students.");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
    Serial.println("Started!");
    Serial.print("MAC Address is: ");
    Serial.println(WiFi.macAddress());

    Serial.println("Setting up Access Point...");
    if (!WiFi.softAP("Group 10", "group-10-wiot")) {
        Serial.println("Soft AP creation failed.");
        while(1);
    }
    

    Serial.println("AP Started!");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    time_value = millis();
  server.on("/hello", handleRoot);
  server.on("/attendance", handleShow);
  server.on("/present", HTTP_GET, handleForm);
  server.on("/present",HTTP_POST, handleAuth);
  server.begin();
  Serial.println("HTTP server started");
}

// void get_client_details() {
//     wifi_sta_list_t clients;
//     esp_wifi_ap_get_sta_list(&clients);
//     Serial.printf("Connected clients: %d\n", clients.num);

//     for (size_t i=0; i<clients.num; i++) {
//         wifi_sta_info_t* info = &(clients.sta[i]);

//         const char* phys = "Unknown";
//         if (info->phy_11b && info->phy_11g && info->phy_11n) {
//             phys = "802.11b/g/n";
//         }
//         char buffer[18];
//         snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
//              (info->mac)[0], (info->mac)[1], (info->mac)[2], (info->mac)[3], (info->mac)[4], (info->mac)[5]);

//         std::string macString = std::string(buffer);
//         auto it = students.find(macString);
//         if (it != students.end()){
//             auto it2 =peoplePresent.find(students[macString]);
//             if (it2 == peoplePresent.end()){
//                 peoplePresent.insert(students[macString]);
//             }
//         }
        
//         Serial.printf("%d\tRSSI=%d\tPHY=%s\tMAC=%s\n", i,
//             info->rssi, phys, macString.c_str());

//     }
// }

void loop()
{
    server.handleClient();
    if(millis() - time_value > 30000){
        time_value = millis();
        int randomnumber = random(100000, 1000000);
        codes[std::to_string(randomnumber)] = time_value;
        pCharacteristic -> setValue(String(randomnumber).c_str());
        pCharacteristic->notify();
        Serial.println("Code Changed: " + String(randomnumber));
    }
    
    //get_client_details();
    
    // if(count < peoplePresent.size()){
    //     String setString = "";
    //     for (const auto& element : peoplePresent) {
    //         setString += element.c_str(); 
    //         setString += ", ";
    //     }
    //     if (setString.length() >= 2) {
    //         setString.remove(setString.length() - 2);
    //     }
    //     count = peoplePresent.size(); 
    //     pCharacteristic->setValue(setString.c_str());
    //     pCharacteristic->notify();
    //     Serial.println(setString);
    // }
    // if(count % 10 == 0){
    //     pCharacteristic->setValue();
    //     Serial.println("changed");
    // }
    // if(count %  7 == 0){
    //     pCharacteristic->setValue("2");
    //     Serial.println("changed");
    // }
}
