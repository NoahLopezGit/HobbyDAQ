#include <SPI.h>

#define MPU_CS_PIN 10

#define SAMPLE_RATE_HZ 1000
#define SAMPLE_PERIOD_US (1000000UL / SAMPLE_RATE_HZ)

#define SERIAL_BAUD 1000000
#define BUFFER_SAMPLES 32

#define PACKET_MAGIC 0xA55A

// MPU6500 registers
#define REG_PWR_MGMT_1   0x6B
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_CONFIG       0x1A
#define REG_ACCEL_XOUT_H 0x3B
#define REG_WHO_AM_I     0x75

#define SPI_READ_FLAG 0x80

SPISettings mpuSPI(8000000, MSBFIRST, SPI_MODE3);

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

void selectMPU() {
  digitalWrite(MPU_CS_PIN, LOW);
}

void deselectMPU() {
  digitalWrite(MPU_CS_PIN, HIGH);
}

uint8_t readMPURegister(uint8_t reg) {
  SPI.beginTransaction(mpuSPI);
  selectMPU();

  SPI.transfer(reg | SPI_READ_FLAG);
  uint8_t value = SPI.transfer(0x00);

  deselectMPU();
  SPI.endTransaction();

  return value;
}

void writeMPURegister(uint8_t reg, uint8_t value) {
  SPI.beginTransaction(mpuSPI);
  selectMPU();

  SPI.transfer(reg & 0x7F);
  SPI.transfer(value);

  deselectMPU();
  SPI.endTransaction();
}

void readMPURegisters(uint8_t startReg, uint8_t* data, uint8_t len) {
  SPI.beginTransaction(mpuSPI);
  selectMPU();

  SPI.transfer(startReg | SPI_READ_FLAG);

  for (uint8_t i = 0; i < len; i++) {
    data[i] = SPI.transfer(0x00);
  }

  deselectMPU();
  SPI.endTransaction();
}

void setupMPU6500() {
  // Wake up device
  writeMPURegister(REG_PWR_MGMT_1, 0x00);
  delay(100);

  // Optional: select PLL clock source
  writeMPURegister(REG_PWR_MGMT_1, 0x01);
  delay(10);

  // Gyro scale: ±500 deg/s
  // 0x00 = ±250 dps
  // 0x08 = ±500 dps
  // 0x10 = ±1000 dps
  // 0x18 = ±2000 dps
  writeMPURegister(REG_GYRO_CONFIG, 0x08);

  // Accel scale: ±4g
  // 0x00 = ±2g
  // 0x08 = ±4g
  // 0x10 = ±8g
  // 0x18 = ±16g
  writeMPURegister(REG_ACCEL_CONFIG, 0x08);

  // Digital low-pass filter
  // 0x03 is similar to your previous MPU6050 code, around ~40 Hz bandwidth
  // For higher bandwidth, try 0x00
  writeMPURegister(REG_CONFIG, 0x03);
}

bool readMPU6500(Sample& s) {
  uint8_t raw[14];

  readMPURegisters(REG_ACCEL_XOUT_H, raw, 14);

  s.t_us = micros();

  s.ax = ((int16_t)raw[0]  << 8) | raw[1];
  s.ay = ((int16_t)raw[2]  << 8) | raw[3];
  s.az = ((int16_t)raw[4]  << 8) | raw[5];

  // raw[6], raw[7] are temperature, ignored

  s.gx = ((int16_t)raw[8]  << 8) | raw[9];
  s.gy = ((int16_t)raw[10] << 8) | raw[11];
  s.gz = ((int16_t)raw[12] << 8) | raw[13];

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

void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(MPU_CS_PIN, OUTPUT);
  deselectMPU();

  SPI.begin();

  delay(100);

  setupMPU6500();

  uint8_t whoami = readMPURegister(REG_WHO_AM_I);
  // MPU6500 WHO_AM_I is commonly 0x70.
  // You can uncomment for debugging:
  // Serial.print("WHO_AM_I = 0x");
  // Serial.println(whoami, HEX);

  delay(500);
  nextSampleTime = micros();
}

void loop() {
  uint32_t now = micros();

  if ((int32_t)(now - nextSampleTime) >= 0) {
    nextSampleTime += SAMPLE_PERIOD_US;

    if (!bufferReady) {
      if (readMPU6500(fillBuffer[fillIndex])) {
        fillIndex++;

        if (fillIndex >= BUFFER_SAMPLES) {
          swapBuffers();
        }
      }
    }
  }

  if (bufferReady) {
    sendReadyBuffer();
  }
}