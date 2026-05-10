#define SAMPLE_RATE_HZ 500
#define SAMPLE_PERIOD_US (1000000UL / SAMPLE_RATE_HZ)

#define SERIAL_BAUD 1000000
// at 1e6 baud, 56 bytes takes 56 bytes * 10 bits / 1e6 = 560 us
// 1000 Hz is 1000 us per period, so we can support
// the read takes 778 us
// 778 + 560 = 1338 > 1000 us, so we can't actually support 1000 Hz with consistent timing with the current approach
// 500 Hz (2000 us period) will work though

// header + sample size * num samples < 64 bytes...
// 64 - 8 - 16*n => n = 3
#define BUFFER_SAMPLES 3

#define PACKET_MAGIC 0xA55A

// calc definitions
const float ADC_COUNTS = 1023.0;
const float VREF = 5.0;   // Arduino reference voltage (adjust if using 3.3V or INTERNAL)

// calibrations - follow 6-point static calibration using gravity
float xOffset = -0.03;
float xScale  = 1.087;

float yOffset = -0.06;
float yScale  = 1.087;

float zOffset = -0.085;
float zScale  = 1.058;

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

bool readAccel(Sample& s, uint32_t t_us) {
  s.t_us = t_us;

  // Convert ADC readings to volts
  float v1p8 = analogRead(A0) * VREF / ADC_COUNTS;
  float xV   = analogRead(A1) * VREF / ADC_COUNTS;
  float yV   = analogRead(A2) * VREF / ADC_COUNTS;
  float zV   = analogRead(A3) * VREF / ADC_COUNTS;

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

void setup() {
  Serial.begin(SERIAL_BAUD);
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
