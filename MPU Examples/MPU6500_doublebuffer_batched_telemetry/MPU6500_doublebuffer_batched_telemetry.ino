#include <Arduino.h>
#include <SPI.h>

#define MPU_CS_PIN   10
#define MPU_INT_PIN  2
#define BAUD_RATE    1e6

#define MPU_READ   0x80
#define MPU_WRITE  0x00

#define REG_SMPLRT_DIV        0x19
#define REG_CONFIG            0x1A
#define REG_GYRO_CONFIG       0x1B
#define REG_ACCEL_CONFIG      0x1C
#define REG_ACCEL_CONFIG2     0x1D
#define REG_FIFO_EN           0x23
#define REG_INT_PIN_CFG       0x37
#define REG_INT_ENABLE        0x38
#define REG_INT_STATUS        0x3A
#define REG_ACCEL_XOUT_H      0x3B
#define REG_USER_CTRL         0x6A
#define REG_PWR_MGMT_1        0x6B
#define REG_PWR_MGMT_2        0x6C
#define REG_SIGNAL_PATH_RESET 0x68
#define REG_FIFO_COUNTH       0x72
#define REG_FIFO_COUNTL       0x73
#define REG_FIFO_R_W          0x74
#define REG_WHO_AM_I          0x75

#define SYNC_WORD             0xAABBCCDD

#define SAMPLE_SIZE_BYTES     12      // accel XYZ + gyro XYZ, no temp
#define MAX_SAMPLES_PER_PKT   32
#define FIFO_READ_MAX_BYTES   (SAMPLE_SIZE_BYTES * MAX_SAMPLES_PER_PKT)

SPISettings mpuSPI(10000000, MSBFIRST, SPI_MODE0);

volatile bool mpuInterruptFlag = false;

uint32_t sequenceNumber = 0;

struct __attribute__((packed)) ImuSample {
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
};

struct __attribute__((packed)) ImuPacketHeader {
  uint32_t sync;
  uint32_t seq;
  uint32_t timestamp_us;
  uint16_t sample_count;
  uint16_t sample_size;
};

ImuSample sampleBuffer[MAX_SAMPLES_PER_PKT];

void mpuWrite(uint8_t reg, uint8_t value) {
  SPI.beginTransaction(mpuSPI);
  digitalWrite(MPU_CS_PIN, LOW);
  SPI.transfer(reg | MPU_WRITE);
  SPI.transfer(value);
  digitalWrite(MPU_CS_PIN, HIGH);
  SPI.endTransaction();
}

uint8_t mpuRead8(uint8_t reg) {
  SPI.beginTransaction(mpuSPI);
  digitalWrite(MPU_CS_PIN, LOW);
  SPI.transfer(reg | MPU_READ);
  uint8_t value = SPI.transfer(0x00);
  digitalWrite(MPU_CS_PIN, HIGH);
  SPI.endTransaction();
  return value;
}

void mpuReadBytes(uint8_t reg, uint8_t *buf, size_t len) {
  SPI.beginTransaction(mpuSPI);
  digitalWrite(MPU_CS_PIN, LOW);
  SPI.transfer(reg | MPU_READ);

  for (size_t i = 0; i < len; i++) {
    buf[i] = SPI.transfer(0x00);
  }

  digitalWrite(MPU_CS_PIN, HIGH);
  SPI.endTransaction();
}

uint16_t mpuReadFIFOCount() {
  uint8_t buf[2];
  mpuReadBytes(REG_FIFO_COUNTH, buf, 2);
  return ((uint16_t)buf[0] << 8) | buf[1];
}

int16_t makeInt16(uint8_t hi, uint8_t lo) {
  return (int16_t)((hi << 8) | lo);
}

void resetMPUFIFO() {
  mpuWrite(REG_USER_CTRL, 0x04);  // FIFO reset
  delay(1);
  mpuWrite(REG_USER_CTRL, 0x40);  // FIFO enable
}

void mpuISR() {
  mpuInterruptFlag = true;
}

void setupMPU6500() {
  pinMode(MPU_CS_PIN, OUTPUT);
  digitalWrite(MPU_CS_PIN, HIGH);

  delay(100);

  mpuWrite(REG_PWR_MGMT_1, 0x80);   // device reset
  delay(100);

  mpuWrite(REG_PWR_MGMT_1, 0x01);   // PLL clock
  delay(10);

  mpuWrite(REG_PWR_MGMT_2, 0x00);   // enable accel + gyro
  delay(10);

  mpuWrite(REG_SIGNAL_PATH_RESET, 0x07);
  delay(10);

  mpuWrite(REG_CONFIG, 0x03);        // gyro DLPF
  mpuWrite(REG_ACCEL_CONFIG2, 0x03); // accel DLPF

  mpuWrite(REG_SMPLRT_DIV, 0x00);    // sample rate divider... means sample rate is 1 kHz (acutal = 1000 / (1 + SMPLRT_DIV))

  mpuWrite(REG_GYRO_CONFIG, 0x18);   // ±2000 dps
  mpuWrite(REG_ACCEL_CONFIG, 0x10);  // ±8g

  mpuWrite(REG_USER_CTRL, 0x00);
  delay(1);

  resetMPUFIFO();

  /*
    FIFO_EN bits:
    bit 7 TEMP_OUT
    bit 6 GYRO_XOUT
    bit 5 GYRO_YOUT
    bit 4 GYRO_ZOUT
    bit 3 ACCEL
  */
  mpuWrite(REG_FIFO_EN, 0x78);       // accel + gyro XYZ into FIFO

  /*
    INT_PIN_CFG:
    0x10 = active high, push-pull, pulse
  */
  mpuWrite(REG_INT_PIN_CFG, 0x10);

  /*
    INT_ENABLE:
    bit 0 = DATA_RDY_EN
    bit 4 = FIFO_OFLOW_EN
  */
  mpuWrite(REG_INT_ENABLE, 0x11);

  delay(10);

  volatile uint8_t who = mpuRead8(REG_WHO_AM_I);
  (void)who;
}

uint16_t readSamplesFromFIFO() {
  uint16_t fifoCount = mpuReadFIFOCount();

  if (fifoCount == 0) {
    return 0;
  }

  if (fifoCount >= 1024) {
    resetMPUFIFO();
    return 0;
  }

  uint16_t completeSamples = fifoCount / SAMPLE_SIZE_BYTES;

  if (completeSamples == 0) {
    return 0;
  }

  if (completeSamples > MAX_SAMPLES_PER_PKT) {
    completeSamples = MAX_SAMPLES_PER_PKT;
  }

  uint16_t bytesToRead = completeSamples * SAMPLE_SIZE_BYTES;

  uint8_t raw[FIFO_READ_MAX_BYTES];
  mpuReadBytes(REG_FIFO_R_W, raw, bytesToRead);

  for (uint16_t i = 0; i < completeSamples; i++) {
    uint8_t *p = &raw[i * SAMPLE_SIZE_BYTES];

    sampleBuffer[i].ax = makeInt16(p[0],  p[1]);
    sampleBuffer[i].ay = makeInt16(p[2],  p[3]);
    sampleBuffer[i].az = makeInt16(p[4],  p[5]);

    sampleBuffer[i].gx = makeInt16(p[6],  p[7]);
    sampleBuffer[i].gy = makeInt16(p[8],  p[9]);
    sampleBuffer[i].gz = makeInt16(p[10], p[11]);
  }

  return completeSamples;
}

void streamSamples(uint16_t sampleCount) {
  ImuPacketHeader header;

  header.sync = SYNC_WORD;
  header.seq = sequenceNumber++;
  header.timestamp_us = micros();
  header.sample_count = sampleCount;
  header.sample_size = sizeof(ImuSample);

  Serial.write((uint8_t *)&header, sizeof(header));
  Serial.write((uint8_t *)sampleBuffer, sampleCount * sizeof(ImuSample));
}

void setup() {
  Serial.begin(BAUD_RATE);
  while (!Serial && millis() < 3000) {}

  SPI.begin();

  setupMPU6500();

  pinMode(MPU_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), mpuISR, RISING);
}

void loop() {
  if (mpuInterruptFlag) {
    noInterrupts();
    mpuInterruptFlag = false;
    interrupts();

    uint8_t intStatus = mpuRead8(REG_INT_STATUS);

    if (intStatus & 0x10) {
      resetMPUFIFO();
      return;
    }

    if (intStatus & 0x01) {
      uint16_t n = readSamplesFromFIFO();

      if (n > 0) {
        streamSamples(n);
      }
    }
  }

  uint16_t fifoCount = mpuReadFIFOCount();

  if (fifoCount >= SAMPLE_SIZE_BYTES * MAX_SAMPLES_PER_PKT) {
    uint16_t n = readSamplesFromFIFO();

    if (n > 0) {
      streamSamples(n);
    }
  }
}