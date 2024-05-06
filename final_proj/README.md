**This repo is for Group 10's final project for CS 4501 Spring 2024.** <br>
**Group Members:** <br>
Alby Alex, Chance Rose, Chance Woosley <br>
**Description**
The project is a NFC based attendance tracker designed to guarantee a student's physical presence in class.
A LoRa 32 board hosts a WiFi access point with a webpage for signing in. The board is also connected via BLE to a 
nrf52840 board which recieves random access codes every 30 seconds and writes the code to a NFC tag. A user joins the network hosted by the LoRa access point, and reads the NFC tag with their smartphone to get their code and a link to the login site. A user then logs in with their unique code and user id to display their attendance.<br>
<br>
**Future Changes**
* Develop smartphone application to abstract away the logging in process so a user cannot give their id to another user and have them log in for them.
* Piggyback the access point to the web so the database of student can be easily shared.

**Where is code?**
* LoRa code is in the ./lora directory.
* NRF code is the ./ble directory.

