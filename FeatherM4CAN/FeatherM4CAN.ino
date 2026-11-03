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
const uint16_t SHIFT_POINT_LOW = 5300;
const uint16_t SHIFT_POINT_MID = 5900;
const uint16_t SHIFT_POINT_HIGH = 6500;
const uint16_t SHIFT_POINT_MAX = 7100;
const uint8_t GEAR_COUNT = 6;
// Keep showing the last gear in neutral above this speed.
// OBD speed is km/h: 32 km/h = 20 mph.
const uint8_t NEUTRAL_HOLD_MIN_KMH = 32;
// Starting estimate of engine RPM per km/h at a 1.000 gear ratio:
// 2.866 final drive, 205/45R17 (1.936 m circumference).
//   (1000 / 60) / 1.936 * 2.866 = 24.7
// Refined at runtime while driving in gear.
const float RPM_PER_KMH_PER_RATIO = 24.7;

// ================================================================
// Timing
// ================================================================

// One request per slot. RPM gets every other slot, so it updates
// every 2 * REQUEST_INTERVAL_MS (20 Hz).
const uint8_t REQUEST_INTERVAL_MS = 25;
const uint8_t RESPONSE_TIMEOUT_MS = 40;
// Keep showing the last good value through a few missed responses.
const uint16_t DATA_STALE_MS = 500;
// Re-initialize the CAN controller if no PCM response for this long.
const uint16_t CAN_WATCHDOG_MS = 2000;
uint32_t now = 0;
uint32_t lastCANResponse = 0;
uint32_t lastCANReset = 0;

// Cleared by the serial console (Console.ino) to test the display
// with manual values instead of PCM responses.
bool canPolling = true;
// 10 Hz data printout, toggled by the console "print" command.
bool printEnabled = true;

// ================================================================
// Data
// ================================================================

#include <map>
std::map<int, float> gearRatios {
    { 1, 5.087 },
    { 2, 2.991 },
    { 3, 2.035 },
    { 4, 1.594 },
    { 5, 1.286 },
    { 6, 1.000 },
};

struct VehicleData
{
  float rpm;
  uint8_t speed;
  uint8_t gearStatus;
  float gearRatio;
  float oilC;

  bool rpmValid;
  bool speedValid;
  bool gearValid;
  bool oilValid;
  bool oilWarming;
  bool oilOvertemp;

  // millis() of the last good response
  uint32_t rpmTime;
  uint32_t speedTime;
  uint32_t gearTime;
  uint32_t oilTime;
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

  testAnimation();
  bootAnimation();

  // Blink the ring magenta and keep retrying if CAN won't start.
  while (!initCAN())
  {
    Serial.println("CAN initialization FAILED! Retrying...");

    ring.fill(ring.Color(255, 0, 255), 0, LED_COUNT);
    ring.show();
    delay(500);

    ring.clear();
    ring.show();
    delay(500);
  }
  Serial.println("CAN initialized at 500 kbit/s");
  Serial.println("Starting PID polling...");
  Serial.println("Type \"help\" for console commands");
}


// ================================================================
// LOOP
// ================================================================
void loop()
{
  now = millis();

  updateConsole();

  if (canPolling)
    checkCANWatchdog();

  updateVehicleData();

  updateLEDs();
}
