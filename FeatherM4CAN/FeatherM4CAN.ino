/*
 * ================================================================
 * Mazda MX-5 ND3 (2024+) Multi-PID OBD-II Reader
 * ================================================================
 *
 * Hardware:
 *   Adafruit Feather M4 CAN
 *
 * CAN Research:
 *   https://github.com/drewid74/2024-nd3-mazda-obdii
 *
 * ================================================================
 */

#include <CANSAME5x.h>
CANSAME5x CAN;

#include <Adafruit_NeoPixel.h>
#include <math.h>
#define RING_PIN 9
#define LED_COUNT 22
Adafruit_NeoPixel ring(Adafruit_NeoPixel(LED_COUNT, RING_PIN, NEO_GRB + NEO_KHZ800));

const uint8_t ENGINE_OPERATING_TEMP = 80;
const uint8_t ENGINE_OVERTEMP = 120;
const uint16_t SHIFT_POINT_LOW = 6000;
const uint16_t SHIFT_POINT_MID = 6600;
const uint16_t SHIFT_POINT_HIGH = 7000;
const uint16_t SHIFT_POINT_MAX = 7400;

// ================================================================
// Timing
// ================================================================

const uint32_t REQUEST_INTERVAL_MS = 50;
const uint32_t RESPONSE_TIMEOUT_MS = 40;
uint32_t now = 0;

// ================================================================
// Data structure
// ================================================================

struct VehicleData
{
  float rpm;
  uint8_t gearStatus;
  float gearRatio;
  float oilC;

  bool rpmValid;
  bool gearValid;
  bool oilValid;
  bool oilWarming;
  bool oilOvertemp;
};

VehicleData data;


// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);
  clearData();

  // Enable CAN transceiver.
  pinMode(PIN_CAN_STANDBY, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false);
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_BOOSTEN, true);

  //Disable board NeoPixel
  pinMode(PIN_NEOPIXEL_POWER, OUTPUT);
  digitalWrite(PIN_NEOPIXEL_POWER, LOW);


  //setup ring LED
  pinMode(RING_PIN, OUTPUT);
  ring.begin(); 
  ring.setBrightness(32); // Set global brightness for the whole strip 0-255  
  //ring.show();

  //testAnimation();
  bootAnimation();

  // ND3 OBD-II CAN bus is 500 kbit/s.
  if (!CAN.begin(500000))
  {
    Serial.println("CAN initialization FAILED!");

    while (1)
      delay(100);
  }
  Serial.println("CAN initialized at 500 kbit/s");
  Serial.println("Starting PID polling...");
}


// ================================================================
// LOOP
// ================================================================
void loop()
{
  now = millis();
  
  updateCANData();

  updateLEDs();
}
