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

const uint32_t OPERATING_TEMP = 85;

// ================================================================
// Timing
// ================================================================

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
  bool oilWarming;
  bool oilOvertemp;
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
  data.oilWarming = true;
  data.oilOvertemp = false;
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

const uint32_t OBD_FUNCTIONAL_ID = 0x7DF;

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

const uint32_t PCM_RESPONSE_ID   = 0x7E8;

const uint32_t PCM_REQUEST_ID    = 0x7E0;

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
// Reading one PID at a time to not overwhelm CAN traffic.
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
// LED Animations
// ================================================================
void testAnimation()
{
  ring.fill(ring.Color(255, 0, 0), 0, LED_COUNT);
  ring.show();
  delay(200);
  
  ring.fill(ring.Color(0, 0, 255), 0, LED_COUNT);
  ring.show();
  delay(200);

  for(int i=0;i < LED_COUNT;i++)
  {
    ring.setPixelColor(i, ring.Color(255,0,0));
    ring.show();
    delay(10);
  }
    

  for(int i=0;i < LED_COUNT;i++)
  {
    ring.setPixelColor(i, ring.Color(0,0,255));
    ring.show();
    delay(10);
  }

  for (int brightness = 0; brightness <= 255; brightness++)
  {
    ring.fill(ring.Color(brightness, 0, 0), 0, LED_COUNT);
    ring.show();
    delay(1);
  }
  for (int brightness = 255; brightness >= 0; brightness--)
  {
    ring.fill(ring.Color(brightness, 0, 0), 0, LED_COUNT);
    ring.show();
    delay(1);
  }
}


//Overtemp Animation
int overtempBrightness = 0;
int overtempDirection = 1;
uint32_t lastOvertempUpdate = 0;
const uint32_t OVERTEMP_INTERVAL_MS = 5;

void updateOvertempAnimation(uint32_t now)
{
  if ((uint32_t)(now - lastOvertempUpdate) < OVERTEMP_INTERVAL_MS)
    return;

  lastOvertempUpdate = now;
  overtempBrightness += overtempDirection;

  if (overtempBrightness >= 255)
  {
    overtempBrightness = 255;
    overtempDirection = -1;
  }
  else if (overtempBrightness <= 0)
  {
    overtempBrightness = 0;
    overtempDirection = 1;
  }
  ring.fill(ring.Color(overtempBrightness, 0, 0), 0, LED_COUNT);
}



//Shift Flash Animation
bool shiftFlashOn = false;
uint32_t lastShiftFlash = 0;

void updateFullRingFlash(uint8_t r, uint8_t g, uint8_t b, uint32_t now, uint32_t flashinterval = 100)
{
  if ((uint32_t)(now - lastShiftFlash) >= flashinterval)
  {
    lastShiftFlash = now;
    shiftFlashOn = !shiftFlashOn;

    if (shiftFlashOn)
    {
      ring.fill(ring.Color(r, g, b), 0, LED_COUNT);
    }
    else
    {
      ring.clear();
    }
  }
}

void resetShiftFlash()
{
  shiftFlashOn = false;
  lastShiftFlash = 0;
}

int lastOilTemp = -1;

void updateOilWarmingAnimation(int oilTemp)
{
  for (int i = 0; i < LED_COUNT; i++)
  {
    // Turn LEDs off from left to right
    if (i < oilTemp)
    {
      ring.setPixelColor(i, 0, 0, 0);
    }
    else
    {
      ring.setPixelColor(i, 255, 0, 0);
    }
  }
}



// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  // Enable CAN transceiver.
  pinMode(PIN_CAN_STANDBY, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false);

  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_BOOSTEN, true);

  /*
  // ND3 OBD-II CAN bus is 500 kbit/s.
  if (!CAN.begin(500000))
  {
    Serial.println("CAN initialization FAILED!");

    while (1)
      delay(100);
  }

  Serial.println("CAN initialized at 500 kbit/s");
  Serial.println("Starting PID polling...");
  */

  clearData();

  //Disable board NeoPixel
  pinMode(PIN_NEOPIXEL_POWER, OUTPUT);
  digitalWrite(PIN_NEOPIXEL_POWER, LOW);


  //setup ring LED
  pinMode(RING_PIN, OUTPUT);
  ring.begin(); 
  ring.setBrightness(30); // Set global brightness for the whole strip 0-255  
  ring.show();

  testAnimation();
}


// ================================================================
// LOOP
// ================================================================

uint32_t lastRequest = 0;
uint32_t lastPrint = 0;

void loop()
{
  uint32_t now = millis();

  // Poll one PID at a time
  if ((now - lastRequest) >= REQUEST_INTERVAL_MS)
  {
    lastRequest = now;
    readNextPID();
  }


  // Print complete dataset at 10Hz
  if ((now - lastPrint) >= 100)
  {
    lastPrint = now;
    printData();
  }


  // Oil temp animation
  if(!data.oilValid)
  {
    data.oilWarming = false;
    data.oilOvertemp = false;
    ring.clear();
  }
  else if (data.oilValid)
  {
    // Pulse LED ring on overtemp as a warning
    if (data.oilC >= 125)
    {
      data.oilOvertemp = true;
      data.oilWarming = false;
      updateOvertempAnimation(now);
    }
    //Animate increasing redline with inreasing temp
    else if (data.oilC <= OPERATING_TEMP)
    {
      data.oilOvertemp = false;
      data.oilWarming = true;
      //int oilTempScaled = round(map(data.oilC, 0, OPERATING_TEMP, 0, LED_COUNT));
      int oilTempScaled = constrain((long)data.oilC * LED_COUNT / OPERATING_TEMP, 0, LED_COUNT);
      updateOilWarmingAnimation(oilTempScaled);
    } 
    else
    {
      data.oilOvertemp = false;
      data.oilWarming = false;
      ring.clear();
    }
  }

  // Shift indicator flash animation
  if (data.rpmValid && !data.oilWarming && !data.oilOvertemp)
  {
    if (data.rpm > 7200)
    {
      updateFullRingFlash(0, 0, 255, now, 100);
    }
    else if (data.rpm > 6900)
    {
      ring.fill(ring.Color(255, 0, 0), 0, LED_COUNT);
      resetShiftFlash();
    }
    else if (data.rpm > 6500)
    {
      ring.fill(ring.Color(0, 255, 0), 0, LED_COUNT);
      resetShiftFlash();
    } 
    else
    {
      ring.clear();
      resetShiftFlash();
    }
  }
  else if (!data.oilValid && !data.rpmValid)
  {
    ring.clear();
  }
ring.show();
}
