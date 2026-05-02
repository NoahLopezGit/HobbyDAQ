#include <Wire.h>

#define MPU_ADDR 0x68

#define SAMPLE_RATE_HZ 2000
#define SAMPLE_PERIOD_US (1000000UL / SAMPLE_RATE_HZ)

#define SERIAL_BAUD 500000
#define BUFFER_SAMPLES 32

#define PACKET_MAGIC 0xA55A

struct __attribute__((packed)) Sample {
  uint32_t t_us;
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
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
bool bufferReady = false;

uint32_t packetCounter = 0;
uint32_t nextSampleTime = 0;

void writeMPURegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void setupMPU6050() {
  // Wake up MPU6050
  writeMPURegister(0x6B, 0x00);

  // Gyro scale: ±500 deg/s
  writeMPURegister(0x1B, 0x08);

  // Accel scale: ±4g
  writeMPURegister(0x1C, 0x08);

  // Low-pass filter
  writeMPURegister(0x1A, 0x03);
}

bool readMPU6050(Sample& s) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(MPU_ADDR, 14) != 14) {
    return false;
  }

  s.t_us = micros();

  s.ax = (Wire.read() << 8) | Wire.read();
  s.ay = (Wire.read() << 8) | Wire.read();
  s.az = (Wire.read() << 8) | Wire.read();

  // Temperature, ignored
  Wire.read();
  Wire.read();

  s.gx = (Wire.read() << 8) | Wire.read();
  s.gy = (Wire.read() << 8) | Wire.read();
  s.gz = (Wire.read() << 8) | Wire.read();

  return true;
}

void swapBuffers() {
  Sample* temp = sendBuffer;
  sendBuffer = fillBuffer;
  fillBuffer = temp;

  fillIndex = 0;
  bufferReady = true;
}

void sendReadyBuffer() {
  PacketHeader header;
  header.magic = PACKET_MAGIC;
  header.sample_count = BUFFER_SAMPLES;
  header.packet_counter = packetCounter++;

  Serial.write((uint8_t*)&header, sizeof(header));
  Serial.write((uint8_t*)sendBuffer, sizeof(Sample) * BUFFER_SAMPLES);

  bufferReady = false;
}

Sample sample;

void setup() {
  Serial.begin(SERIAL_BAUD);

  Wire.begin();
  Wire.setClock(400000);

  setupMPU6050();

  delay(500);
  nextSampleTime = micros();
}

void loop() {
  uint32_t t0 = micros();
  bool ok = readMPU6050(sample);
  uint32_t dt = micros() - t0;

  Serial.println(dt);
  delay(100);
}