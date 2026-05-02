// Test how Serial.write() time changes around TX buffer size
// Focus around the buffer boundary
const int numTests = 10;
const int testSizes[numTests] = {8, 16, 32, 48, 64, 72, 96, 128, 256, 512};

uint8_t buffer[512];

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  for (int i = 0; i < 512; i++) {
    buffer[i] = i;
  }

  delay(2000);
  Serial.println("Serial Buffer Behavior Test");
}

void loop() {
  for (int t = 0; t < numTests; t++) {
    int size = testSizes[t];

    // Make sure buffer is empty before test
    Serial.flush();
    delay(10);

    unsigned long startTime = micros();

    // SINGLE write call (this is the key difference)
    Serial.write(buffer, size);

    unsigned long endTime = micros();

    unsigned long writeTime = endTime - startTime;

    Serial.print("Size: ");
    Serial.print(size);
    Serial.print(" bytes | write() time: ");
    Serial.print(writeTime);
    Serial.println(" us");

    delay(200); // spacing for readability
  }

  Serial.println("\n--- Repeat ---\n");
  delay(3000);
}