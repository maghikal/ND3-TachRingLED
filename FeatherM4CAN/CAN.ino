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

// ------------------------------------------------
// Get CAN Response
//
// Reads a single CAN frame from the PCM.
//
// Returns:
//   true  - PCM response received
//   false - Timeout
//
// Response includes the ISO-TP length byte.
//
// ------------------------------------------------

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

    // Ignore frames that are not from the PCM.
    if (id != PCM_RESPONSE_ID)
    {
      while (CAN.available())
        CAN.read();

      continue;
    }

    // Read the CAN payload.
    uint8_t count = 0;

    while (CAN.available())
    {
      uint8_t value = CAN.read();

      if (count < 8)
        response[count++] = value;
    }

    // Reject empty or invalid-length frames.
    if (count == 0 || count != packetSize)
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
//   04 41 PID A B
// Used by:
//   PID 04 41 - RPM
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
  //   04 41 PID A

  if (length < 5)
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

// ------------------------------------------------
// Request Mode 01 - 4 data bytes
//
// Response:
//   41 PID A B C D
// Used by:
//   PID 01 A4 - Gear detection
//
// ------------------------------------------------

bool requestMode01(
  uint8_t pid,
  uint8_t &A,
  uint8_t &B,
  uint8_t &C,
  uint8_t &D)
{
  uint8_t response[8];
  uint8_t length;

  sendMode01(pid);

  if (!getResponse(response, length))
    return false;

  // Minimum expected:
  //   06 41 PID A B C D

  if (length < 7)
    return false;

  // Positive Mode 01 response.
  if (response[1] != 0x41)
    return false;

  if (response[2] != pid)
    return false;

  A = response[3];
  B = response[4];
  C = response[5];
  D = response[6];

  return true;
}


// ================================================================
// Request Mazda Mode 22
// ================================================================
//
// Response:
//   04 62 13 10 A B
// Used by:
//   PID 04 62 - Oil Temp
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
    uint16_t raw = ((uint16_t)A << 8) | B;

    data.rpm = raw / 4.0;
    data.rpmValid = true;
  }
  else
  {
    data.rpmValid = false;
  }
}


// ------------------------------------------------
// Current Gear + Gear Ratio - 01 A4
//
// Response:
//   41 A4 AA BB CC DD
//
// A = Support bitmap
// B = Transmission gear status
// C, D = Gear ratio
//
// Gear ratio = ((C * 256) + D) / 1000
//
// Note:
//   Ratio is undefined when vehicle speed is 0.
//   Gate detection on PID 01 0D > 0.
//
// ------------------------------------------------

void readGear()
{
  uint8_t A, B, C, D;
  uint8_t speed, unused;

  //Gear ratio is not meaningful when stopped.
  if (requestMode01(0x0D, speed, unused))
  {
    data.speed = speed;
    data.speedValid = true;
  }
  else
  {
    data.speedValid = false;
  }

  uint16_t raw = 0;
  if (requestMode01(0xA4, A, B, C, D))
  {
    raw = ((uint16_t)C << 8) | D;

    data.gearRatio = raw / 1000.0;
    data.gearStatus = B / 16;
    data.gearValid = true;
  }
  else
  {
    data.gearValid = false;
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
    data.oilOvertemp = data.oilC >= ENGINE_OVERTEMP;
    data.oilWarming = data.oilC <= ENGINE_OPERATING_TEMP && !data.oilOvertemp;
  }
  else
  {
    data.oilValid = false;
    data.oilWarming = false;
    data.oilOvertemp = false;
  }
}

