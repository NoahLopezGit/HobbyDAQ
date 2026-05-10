#include <Adafruit_ADS1X15.h>

#define SAMPLE_RATE_HZ 500
#define SAMPLE_PERIOD_US (1000000UL / SAMPLE_RATE_HZ)
#define SERIAL_BAUD 1000000
#define BUFFER_SAMPLES 3
#define PACKET_MAGIC 0xA55A

// timing variables
uint32_t lastReadTime;

// buffer stuff; 128 / 8 = 16 bytes
struct __attribute__((packed)) Sample {
  uint32_t t_us;
  float ax;
  float ay;
  float az;
};

struct __attribute__((packed)) PacketHeader {
  uint16_t magic;
  uint16_t sample_count;
  uint32_t packet_counter;
};

Sample bufferA[BUFFER_SAMPLES];
Sample bufferB[BUFFER_SAMPLES];

Sample* fillBuffer = bufferA;
Sample* sendBuffer = bufferB;

uint16_t fillIndex = 0;
uint32_t packetCounter = 0;


Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
// Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */

// calibrations - follow 6-point static calibration using gravity
float xOffset = -0.03;
float xScale  = 1.087;

float yOffset = -0.06;
float yScale  = 1.087;

float zOffset = -0.085;
float zScale  = 1.058;


bool readAccel(Sample& s, uint32_t t_us) {
  s.t_us = t_us;

  int16_t adc0, adc1, adc2, adc3;
  float volts0, volts1, volts2, volts3;

  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);

  float v1p8 = ads.computeVolts(adc0);
  float xV = ads.computeVolts(adc1);
  float yV = ads.computeVolts(adc2);
  float zV = ads.computeVolts(adc3);

  // protect div by 0
  float denom = v1p8 - 0.06;
  if (denom < 0.01) denom = 0.01;

  // Apply your acceleration equation (±10g scale)
  float xout = (20.0 / (denom)) * (xV - v1p8 / 2.0);
  float yout = (20.0 / (denom)) * (yV - v1p8 / 2.0);
  float zout = (20.0 / (denom)) * (zV - v1p8 / 2.0);

  float xCal = (xout - xOffset) * xScale;
  float yCal = (yout - yOffset) * yScale;
  float zCal = (zout - zOffset) * zScale;

  s.ax = xCal;
  s.ay = yCal;
  s.az = zCal;

  return true;
}

void swapBuffers() {
  Sample* temp = sendBuffer;
  sendBuffer = fillBuffer;
  fillBuffer = temp;

  fillIndex = 0;
}

void sendReadyBuffer() {
  PacketHeader header;
  header.magic = PACKET_MAGIC;
  header.sample_count = BUFFER_SAMPLES;
  header.packet_counter = packetCounter++;

  Serial.write((uint8_t*)&header, sizeof(header));
  Serial.write((uint8_t*)sendBuffer, sizeof(Sample) * BUFFER_SAMPLES);
}


void setup(void)
{
  Serial.begin(SERIAL_BAUD);
  Serial.println("Hello!");

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
}

void loop() {
  // maybe read accel data
  if (micros() - lastReadTime >= SAMPLE_PERIOD_US) { // TODO will mess up with rollover?
    lastReadTime = micros();
    uint32_t sampleTime = lastReadTime;
    readAccel(fillBuffer[fillIndex], sampleTime);
    fillIndex++;
  }

  // maybe send accel data over serial
  if (fillIndex >= BUFFER_SAMPLES) { 
    swapBuffers();
    sendReadyBuffer();
  }
}
