/*
 * ================================================================
 * Mazda MX-5 ND3 (2024+) Multi-PID OBD-II Reader
 * ================================================================
 *
 * Hardware:
 *   Adafruit Feather M4 CAN
 *
 * CAN:
 *   500 kbit/s
 *
 * Standard OBD-II requests:
 *   Functional request ID = 0x7DF
 *   PCM response          = 0x7E8
 *
 * Mazda-specific request:
 *   PCM request ID        = 0x7E0
 *   PCM response          = 0x7E8
 *
 * Confirmed ND3 PIDs:
 *
 *   01 0C  Engine RPM
 *   01 0D  Vehicle speed
 *   01 05  Coolant temperature
 *   01 11  Throttle position
 *   01 04  Engine load
 *   01 0F  Intake air temperature
 *   01 10  MAF
 *   01 0B  Intake manifold pressure
 *   01 46  Ambient temperature
 *   01 42  Battery voltage
 *   01 2F  Fuel level
 *   01 0E  Ignition timing
 *   01 5E  Engine fuel rate
 *
 * Mazda-specific:
 *
 *   22 13 10  Engine oil temperature
 *
 * ================================================================
 */

#include <CANSAME5x.h>

CANSAME5x CAN;

#include <Adafruit_NeoPixel.h>
#include <math.h>
#define RING_PIN 9
#define LED_COUNT 24
Adafruit_NeoPixel ring(Adafruit_NeoPixel(LED_COUNT, RING_PIN, NEO_GRB + NEO_KHZ800));

// ================================================================
// CAN IDs
// ================================================================

const uint32_t OBD_FUNCTIONAL_ID = 0x7DF;
const uint32_t PCM_REQUEST_ID    = 0x7E0;
const uint32_t PCM_RESPONSE_ID   = 0x7E8;


// ================================================================
// Timing
// ================================================================

// Time between PID requests.
// 50 ms = maximum of ~20 requests/sec.

const uint32_t REQUEST_INTERVAL_MS = 50;

const uint32_t RESPONSE_TIMEOUT_MS = 40;


// ================================================================
// Data structure
// ================================================================

struct VehicleData
{
  float rpm;
  float oilC;

  bool rpmValid;
  bool oilValid;
};

VehicleData data;


// ================================================================
// Clear data
// ================================================================

void clearData()
{
  data.rpm = 0;
  data.oilC = 0;

  data.rpmValid = false;
  data.oilValid = false;
}


// ================================================================
// Send standard Mode 01 PID request
// ================================================================
//
// Example:
//
//   sendMode01(0x0C)
//
// sends:
//
//   7DF  02 01 0C 00 00 00 00 00
//
// ================================================================

void sendMode01(uint8_t pid)
{
  CAN.beginPacket(OBD_FUNCTIONAL_ID);

  CAN.write(0x02);       // Number of following bytes
  CAN.write(0x01);       // Mode 01
  CAN.write(pid);

  // Padding
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);

  CAN.endPacket();
}


// ================================================================
// Send Mazda Mode 22 request
// ================================================================
//
// Oil temperature:
//
//   7E0  03 22 13 10 00 00 00 00
//
// ================================================================

void sendMode22(uint8_t pidHigh, uint8_t pidLow)
{
  CAN.beginPacket(PCM_REQUEST_ID);

  CAN.write(0x03);       // 3 bytes follow
  CAN.write(0x22);       // Mode 22
  CAN.write(pidHigh);
  CAN.write(pidLow);

  // Padding
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);

  CAN.endPacket();
}


// ================================================================
// Receive CAN response
// ================================================================
//
// Returns true if a valid response from 0x7E8 is received.
//
// The data bytes are copied into response[].
//
// ================================================================

bool getResponse(uint8_t *response, uint8_t &length)
{
  uint32_t start = millis();

  while ((millis() - start) < RESPONSE_TIMEOUT_MS)
  {
    int packetSize = CAN.parsePacket();

    if (!packetSize)
      continue;

    // Ignore extended frames.
    if (CAN.packetExtended())
    {
      while (CAN.available())
        CAN.read();

      continue;
    }

    uint32_t id = CAN.packetId();

    uint8_t count = 0;

    while (CAN.available() && count < 8)
    {
      response[count++] = CAN.read();
    }

    // We only want the PCM response.
    if (id != PCM_RESPONSE_ID)
      continue;

    length = count;

    return true;
  }

  return false;
}


// ================================================================
// Request Mode 01 and return payload
// ================================================================
//
// Standard positive response:
//
//   04 41 PID A B
//
// Example RPM:
//
//   04 41 0C AA BB
//
// ================================================================

bool requestMode01(uint8_t pid, uint8_t &A, uint8_t &B)
{
  uint8_t response[8];
  uint8_t length;

  sendMode01(pid);

  if (!getResponse(response, length))
    return false;

// Minimum expected:
//
//   04 41 PID A
//
// For 2-byte PIDs we need at least 5 bytes.

  if (length < 4)
    return false;

  // Positive Mode 01 response.
  if (response[1] != 0x41)
    return false;

  if (response[2] != pid)
    return false;

  A = response[3];

  if (length >= 5)
    B = response[4];
  else
    B = 0;

  return true;
}


// ================================================================
// Request Mazda Mode 22
// ================================================================
//
// Oil temperature response:
//
//   04 62 13 10 AA BB
//
// ================================================================

bool requestMode22(
  uint8_t pidHigh,
  uint8_t pidLow,
  uint8_t &A,
  uint8_t &B)
{
  uint8_t response[8];
  uint8_t length;

  sendMode22(pidHigh, pidLow);

  if (!getResponse(response, length))
    return false;

  if (length < 6)
    return false;

  // Positive Mode 22 response:
  //
  // 62 = 22 + 0x40
  //
  if (response[1] != 0x62)
    return false;

  if (response[2] != pidHigh)
    return false;

  if (response[3] != pidLow)
    return false;

  A = response[4];
  B = response[5];

  return true;
}


// ================================================================
// PID functions
// ================================================================


// ------------------------------------------------
// Engine RPM - 01 0C
// RPM = ((A * 256) + B) / 4
// ------------------------------------------------

void readRPM()
{
  uint8_t A, B;

  if (requestMode01(0x0C, A, B))
  {
    uint16_t raw =
      ((uint16_t)A << 8) | B;

    data.rpm = raw / 4.0;
    data.rpmValid = true;
  }
  else
  {
    data.rpmValid = false;
  }
}


// ------------------------------------------------
// Engine oil temperature
//
// Mazda Mode 22:
//
// Request:
//   7E0  03 22 13 10 00 00 00 00
//
// Response:
//   7E8  04 62 13 10 AA BB
//
// Formula:
//   (((A * 256) + B) / 100) - 40
//
// ------------------------------------------------

void readOilTemp()
{
  uint8_t A, B;

  if (requestMode22(0x13, 0x10, A, B))
  {
    uint16_t raw =
      ((uint16_t)A << 8) | B;

    data.oilC =
      (raw / 100.0) - 40.0;

    data.oilValid = true;
  }
  else
  {
    data.oilValid = false;
  }
}


// ================================================================
// Print a floating point value
// ================================================================

void printValue(
  const char *name,
  float value,
  const char *units,
  bool valid)
{
  Serial.print(name);
  Serial.print(": ");

  if (valid)
  {
    Serial.print(value, 1);
    Serial.print(" ");
    Serial.println(units);
  }
  else
  {
    Serial.println("N/A");
  }
}


// ================================================================
// Print all current data
// ================================================================

void printData()
{
  Serial.println();
  Serial.println("================================");

  printValue(
    "RPM",
    data.rpm,
    "rpm",
    data.rpmValid);

  printValue(
    "Oil",
    data.oilC,
    "C",
    data.oilValid);

  Serial.println("================================");
}


// ================================================================
// Read one PID per loop
// ================================================================
//
// Reading one PID at a time keeps the CAN traffic predictable.
//
// The sequence is:
//
// 0  RPM
// 1  Oil
//
// ================================================================

uint8_t pidIndex = 0;

void readNextPID()
{
  switch (pidIndex)
  {
    case 0:
      readRPM();
      break;

    case 1:
      readOilTemp();
      break;
  }

  pidIndex++;

  if (pidIndex >= 2)
    pidIndex = 0;
}


// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  /*
  Serial.println();
  Serial.println("========================================");
  Serial.println(" Mazda MX-5 ND3 OBD-II Tach Light");
  Serial.println(" Feather M4 CAN");
  Serial.println("========================================");
  */

  // Enable CAN transceiver.

  pinMode(PIN_CAN_STANDBY, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false);

  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_BOOSTEN, true);


  // ND3 OBD-II CAN bus is 500 kbit/s.

  if (!CAN.begin(500000))
  {
    Serial.println("CAN initialization FAILED!");

    while (1)
      delay(100);
  }

  Serial.println("CAN initialized at 500 kbit/s");
  Serial.println("Starting PID polling...");

  clearData();

  //Disable board NeoPixel
  pinMode(PIN_NEOPIXEL_POWER, OUTPUT);
  digitalWrite(PIN_NEOPIXEL_POWER, LOW);


  //setup ring LED
  pinMode(RING_PIN, OUTPUT);
  ring.begin(); 
  ring.setBrightness(30); // Set global brightness for the whole strip 0-255  
  ring.show();
}


// ================================================================
// LOOP
// ================================================================

uint32_t lastRequest = 0;
uint32_t lastPrint = 0;

void loop()
{
  uint32_t now = millis();

  // ------------------------------------------------
  // Poll one PID
  // ------------------------------------------------

  if ((now - lastRequest) >= REQUEST_INTERVAL_MS)
  {
    lastRequest = now;

    readNextPID();
  }


  // ------------------------------------------------
  // Print complete dataset once per second
  // ------------------------------------------------

  if ((now - lastPrint) >= 10)
  {
    lastPrint = now;

    printData();
  }

}
