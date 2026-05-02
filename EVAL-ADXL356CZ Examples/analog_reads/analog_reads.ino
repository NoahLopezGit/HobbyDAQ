const float ADC_COUNTS = 1023.0;
const float VREF = 5.0;   // Arduino reference voltage (adjust if using 3.3V or INTERNAL)

// calibrations - follow 6-point static calibration using gravity
float xOffset = -0.03;
float xScale  = 1.087;

float yOffset = -0.06;
float yScale  = 1.087;

float zOffset = -0.085;
float zScale  = 1.058;

void setup() {
  Serial.begin(9600);
}

void loop() {
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

  // Serial Plotter friendly output
  Serial.print(xout);
  Serial.print(",");
  Serial.print(yout);
  Serial.print(",");
  Serial.print(zout);
  Serial.print(",");
  Serial.print(xCal);
  Serial.print(",");
  Serial.print(yCal);
  Serial.print(",");
  Serial.println(zCal);

  delay(50);
}