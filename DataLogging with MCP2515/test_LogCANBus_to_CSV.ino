#include <SPI.h>    // SPI communication for SD and CAN Shield
#include <Wire.h>   // Wire library for I2C communication - for RTC
#include "RTClib.h" // Specific library for RTC1307 and others from Adafruit
#include <SD.h>     // Include the SD library

const int SD_CS_PIN = 4;   // Define SD card CS pin as digital pin 4
const int CAN_INT_PIN = 2;
const int SPI_CS_PIN = 10; // Define CAN shield CS pin as digital pin 10

#include "mcp2515_can.h"
mcp2515_can CAN(SPI_CS_PIN);

#define MAX_DATA_SIZE 8

uint32_t canId = 0x000;
uint8_t len = 0;
uint8_t buf[8];

RTC_DS1307 rtc; // Instantiate the RTC object

const int buttonPin = 3; // Pin connected to the button
const int buttonStatus = 6; // Pin connected to the button
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 50; // Adjust as needed
bool loggingActive = true; // Flag to indicate if logging is active
File logFile;

void dateTime(uint16_t* date, uint16_t* time) {
    DateTime now = rtc.now();
    *date = FAT_DATE(now.year(), now.month(), now.day());
    *time = FAT_TIME(now.hour(), now.minute(), now.second());
}

void setup() {
    Serial.begin(115200);

    pinMode(buttonPin, INPUT_PULLUP); // Configure the button pin
    pinMode(buttonStatus, OUTPUT);
    digitalWrite(buttonStatus, HIGH);

    // Initialize SD card
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println(F("SD card initialization failed!"));
        return;
    } else {
        Serial.println(F("SD card initialized."));
    }

    // Initialize RTC
    if (!rtc.begin()) {
        Serial.println(F("Couldn't find RTC"));
        while (1);
    }

    if (!rtc.isrunning()) {
        Serial.println(F("RTC is NOT running!"));
    }

    // Set the date and time callback for SD card to use RTC values
    SdFile::dateTimeCallback(dateTime);

    // Create new log file with datetime
    DateTime now = rtc.now();
    char filename[13]; // 8.3 format: 8 characters for name + 1 dot + 3 characters for extension
    snprintf(filename, sizeof(filename), "LOG%02d%02d.CSV",
             now.hour(), now.minute());

    logFile = SD.open(filename, FILE_WRITE);
    if (!logFile) {
        Serial.print(F("Error opening file: "));
        Serial.println(filename); // Print the filename to help debug
        return;
    } else {
        Serial.print(F("CSV file opened successfully: "));
        Serial.println(filename); // Print the filename for confirmation
        logFile.println("TimestampEpoch;BusChannel;ID;IDE;DLC;DataLength;Dir;EDL;BRS;DataBytes");
    }

    while (CAN.begin(CAN_500KBPS) != CAN_OK) {
        Serial.println(F("CAN BUS Shield init fail, retrying..."));
        delay(100);
    }
    Serial.println(F("CAN BUS Shield init ok!"));
}

void loop() {
    // Check if the button for close SD file is pressed
    if (digitalRead(buttonPin) == LOW && millis() - lastButtonPress > debounceDelay) {
        lastButtonPress = millis(); // Record the time of the button press
        logFile.close(); // Close the file
        loggingActive = false; // Set loggingActive to false
        digitalWrite(buttonStatus, LOW);
        Serial.println(F("Logging stopped. Safe to power down."));
        while (digitalRead(buttonPin) == LOW); // Wait until the button is released
    }

    if (loggingActive && CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBufID(&canId, &len, buf);
        
        // Write CAN data to CSV file with timestamp
        DateTime now = rtc.now();
        unsigned long timestamp = now.unixtime();
        logFile.print(timestamp);
        logFile.print(F("."));
        logFile.print(millis() % 1000); // Add milliseconds
        logFile.print(F(";1;")); // BusChannel, example value

        // Format CAN ID to 3 digits
        // 0x100 is 256 dec, If ID < 256 means that an ID with two digits must receive an additional 0. Example: 0x1D - 0x01D or 255 dec is 0xFF - 0x0FF
        logFile.print(canId < 0x100 ? "0" : "");
        logFile.print(canId < 0x10 ? "0" : "");
        logFile.print(canId, HEX);

        logFile.print(F(";0;")); // IDE, example value
        logFile.print(len);
        logFile.print(F(";")); // DLC
        logFile.print(len);
        logFile.print(F(";0;0;0;")); // Dir, EDL, BRS, example values

        // Format data bytes to two digits each
        // 0x10 is 16 Dec, If ID < 16 means that all IDs below need an additional 0 digit, example: F (15 dec) - 0F, E - 0E and so on. 
        for (int i = 0; i < len; i++) {
            logFile.print(buf[i] < 0x10 ? "0" : "");
            logFile.print(buf[i], HEX);
        }
        
        logFile.println();
        logFile.flush(); // Ensure data is written to the SD card
    }
}