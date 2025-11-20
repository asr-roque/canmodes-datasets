#include <SPI.h>
#include "mcp2515_can.h"

const int CAN_INT_PIN = 2;
const int SPI_CS_PIN = 10;
mcp2515_can CAN(SPI_CS_PIN);

void setup() {
    Serial.begin(115200);
    if (CAN.begin(CAN_500KBPS) != CAN_OK) {
        Serial.println("Failed to initialize CAN");
        while (1);
    }
    Serial.println("Scanning for OBD-II broadcast addresses...");
}

void scanBroadcastAddresses() {
    uint8_t obdRequest[8] = { 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Request for Supported PIDs
    
    for (uint32_t id = 0x7DF; id <= 0x7EF; id++) {  // Extended diagnostic address range
        CAN.sendMsgBuf(id, 0, 8, obdRequest);
        delay(150);

        uint32_t receivedId;
        uint8_t len = 0;
        uint8_t buf[8];

        if (CAN_MSGAVAIL == CAN.checkReceive()) {
            CAN.readMsgBufID(&receivedId, &len, buf);
            Serial.print("Response received from ID: 0x");
            Serial.println(receivedId, HEX);
        }
    }
}

void loop() {
    scanBroadcastAddresses();
    delay(5000); // Run scan every 5 seconds
}
