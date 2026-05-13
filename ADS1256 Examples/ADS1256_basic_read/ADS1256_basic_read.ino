#include <SPI.h>

// Pin definitions
#define CS_PIN 10  // Chip select pin
#define DRDY_PIN 9 // Data ready pin

void setup() {
  pinMode(CS_PIN, OUTPUT);
  pinMode(DRDY_PIN, INPUT);
  digitalWrite(CS_PIN, HIGH); // Set CS high initially

  SPI.begin(); // Initialize SPI
  SPI.setDataMode(SPI_MODE1); // ADS1256 uses SPI mode 1
  SPI.setClockDivider(SPI_CLOCK_DIV16); // Set SPI clock speed
  Serial.begin(9600); // Initialize serial communication
}

void loop() {
  if (digitalRead(DRDY_PIN) == LOW) { // Check if data is ready
    digitalWrite(CS_PIN, LOW); // Select the ADS1256
    byte command = 0x01; // Example command to read data
    SPI.transfer(command); // Send command
    byte dataHigh = SPI.transfer(0x00); // Read high byte
    byte dataMid = SPI.transfer(0x00); // Read middle byte
    byte dataLow = SPI.transfer(0x00); // Read low byte
    digitalWrite(CS_PIN, HIGH); // Deselect the ADS1256

    // Combine the bytes into a 24-bit value
    long result = ((long)dataHigh << 16) | ((long)dataMid << 8) | dataLow;
    Serial.println(result); // Print the result

    delay(1000);
  }
}