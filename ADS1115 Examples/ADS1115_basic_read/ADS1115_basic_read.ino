#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
// Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */

// calibrations - follow 6-point static calibration using gravity
float xOffset = -0.03;
float xScale  = 1.087;

float yOffset = -0.06;
float yScale  = 1.087;

float zOffset = -0.085;
float zScale  = 1.058;

void setup(void)
{
  Serial.begin(9600);
  Serial.println("Hello!");

  Serial.println("Getting single-ended readings from AIN0..3");
  Serial.println("ADC Range: +/- 6.144V (1 bit = 3mV/ADS1015, 0.1875mV/ADS1115)");

  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
}

void loop(void)
{
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

  // delay(50);
}
