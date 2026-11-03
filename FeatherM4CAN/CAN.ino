// ================================================================
// CAN initialization
// ================================================================
//
// ND3 OBD-II CAN bus is 500 kbit/s.
//
// Only accept frames from the PCM so unrelated traffic can't
// fill the RX FIFO and push out the responses we need.
//
// ================================================================

const uint32_t PCM_RESPONSE_ID   = 0x7E8;

const uint32_t PCM_REQUEST_ID    = 0x7E0;

const uint32_t OBD_FUNCTIONAL_ID = 0x7DF;

bool initCAN()
{
  if (!CAN.begin(500000))
    return false;

  CAN.filter(PCM_RESPONSE_ID, 0x7FF);

  // Give the PCM a full watchdog period to answer.
  lastCANResponse = millis();

  return true;
}


// ================================================================
// CAN watchdog
// ================================================================
//
// If the Feather boots before the car, nothing on the bus ACKs our
// requests. The controller goes error passive and keeps retrying
// the unacknowledged frame forever (the library never cancels it,
// and ACK errors never reach bus-off, so its auto-recovery never
// runs). Sometimes it does not recover when the car wakes up.
//
// Re-initializing the controller clears the error counters and
// the stuck TX buffer. Repeats every CAN_WATCHDOG_MS until the
// PCM answers.
//
// ================================================================

void checkCANWatchdog()
{
  if ((now - lastCANResponse) < CAN_WATCHDOG_MS)
    return;

  if ((now - lastCANReset) < CAN_WATCHDOG_MS)
    return;

  lastCANReset = now;

  Serial.println("No CAN response, re-initializing CAN...");

  CAN.end();

  if (!initCAN())
    Serial.println("CAN re-initialization FAILED!");
}


// ================================================================
// Discard received frames
// ================================================================
//
// Called before every request so a late response to a previous
// request can't be mistaken for the answer to this one.
//
// ================================================================

void discardPacket()
{
  while (CAN.available())
    CAN.read();
}

void drainRx()
{
  while (CAN.parsePacket())
    discardPacket();
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
  drainRx();

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
  drainRx();

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


// ------------------------------------------------
// Get CAN Response
//
// Waits for a PCM frame whose bytes after the
// ISO-TP length byte start with match[].
//
//   Mode 01:  match = 41 PID
//   Mode 22:  match = 62 PIDH PIDL
//
// Frames that don't match (e.g. a late reply to an
// earlier request) are skipped, not treated as a
// failure.
//
// Returns:
//   true  - Matching PCM response received
//   false - Timeout or negative response
//
// Response includes the ISO-TP length byte.
//
// ------------------------------------------------

bool getResponse(
  const uint8_t *match,
  uint8_t matchLength,
  uint8_t *response,
  uint8_t &length)
{
  uint32_t start = millis();

  while ((millis() - start) < RESPONSE_TIMEOUT_MS)
  {
    int packetSize = CAN.parsePacket();

    if (!packetSize)
      continue;

    // Ignore extended frames and frames not from the PCM.
    if (CAN.packetExtended() || CAN.packetId() != PCM_RESPONSE_ID)
    {
      discardPacket();
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

    // Negative response:
    //   03 7F SID NRC
    // NRC 78 = response pending, keep waiting.
    if (count >= 4 && response[1] == 0x7F && response[2] == match[0] - 0x40)
    {
      if (response[3] == 0x78)
        continue;

      return false;
    }

    if (count < 1 + matchLength)
      continue;

    if (memcmp(&response[1], match, matchLength) != 0)
      continue;

    length = count;
    lastCANResponse = millis();

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
//   PID 01 0C - RPM
//   PID 01 0D - Speed (B unused)
//
// ================================================================

bool requestMode01(uint8_t pid, uint8_t &A, uint8_t &B)
{
  uint8_t response[8];
  uint8_t length;
  const uint8_t match[] = { 0x41, pid };

  sendMode01(pid);

  if (!getResponse(match, sizeof(match), response, length))
    return false;

  // Minimum expected:
  //   04 41 PID A B

  if (length < 5)
    return false;

  A = response[3];
  B = response[4];

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
  const uint8_t match[] = { 0x41, pid };

  sendMode01(pid);

  if (!getResponse(match, sizeof(match), response, length))
    return false;

  // Minimum expected:
  //   06 41 PID A B C D

  if (length < 7)
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
//   PID 22 13 10 - Oil Temp
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
  // Positive Mode 22 response: 62 = 22 + 0x40
  const uint8_t match[] = { 0x62, pidHigh, pidLow };

  sendMode22(pidHigh, pidLow);

  if (!getResponse(match, sizeof(match), response, length))
    return false;

  if (length < 6)
    return false;

  A = response[4];
  B = response[5];

  return true;
}


// ================================================================
// PID functions
// ================================================================
//
// On success each function stores the value and its timestamp.
// On failure the last good value is kept; expireStaleData()
// invalidates it once it is older than DATA_STALE_MS.
//
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
    data.rpmTime = now;
  }
}


// ------------------------------------------------
// Vehicle speed - 01 0D
// Speed = A (km/h)
// ------------------------------------------------

void readSpeed()
{
  uint8_t speed, unused;

  if (requestMode01(0x0D, speed, unused))
  {
    data.speed = speed;
    data.speedValid = true;
    data.speedTime = now;
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
//   updateShiftPoints() gates on speed > 0.
//
// ------------------------------------------------

void readGear()
{
  uint8_t A, B, C, D;

  if (requestMode01(0xA4, A, B, C, D))
  {
    uint16_t raw = ((uint16_t)C << 8) | D;

    data.gearRatio = raw / 1000.0;
    data.gearStatus = B / 16;
    data.gearValid = true;
    data.gearTime = now;
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
    data.oilTime = now;
    updateOilFlags();
  }
}
