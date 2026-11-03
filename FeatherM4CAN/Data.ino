

// ================================================================
// Clear data
// ================================================================

void clearData()
{
  data.rpm = 0;
  data.speed = 0;
  data.gearStatus = 0;
  data.gearRatio = 0;
  data.oilC = 0;

  data.rpmValid = false;
  data.speedValid = false;
  data.gearValid = false;
  data.oilValid = false;
  data.oilWarming = false;
  data.oilOvertemp = false;

  data.rpmTime = 0;
  data.speedTime = 0;
  data.gearTime = 0;
  data.oilTime = 0;
}

// ================================================================
// Expire stale data
// ================================================================
//
// A value stays valid until DATA_STALE_MS without a good response,
// so a single dropped frame doesn't blank or switch the display.
//
// ================================================================

void expireStaleData()
{
  if (data.rpmValid && (now - data.rpmTime) > DATA_STALE_MS)
    data.rpmValid = false;

  if (data.speedValid && (now - data.speedTime) > DATA_STALE_MS)
    data.speedValid = false;

  if (data.gearValid && (now - data.gearTime) > DATA_STALE_MS)
    data.gearValid = false;

  if (data.oilValid && (now - data.oilTime) > DATA_STALE_MS)
  {
    data.oilValid = false;
    data.oilWarming = false;
    data.oilOvertemp = false;
  }
}

// ================================================================
// Oil temperature flags
// ================================================================

void updateOilFlags()
{
  data.oilOvertemp = data.oilC >= ENGINE_OVERTEMP;
  data.oilWarming = data.oilC <= ENGINE_OPERATING_TEMP && !data.oilOvertemp;
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
    "Speed",
    data.speed,
    "",
    data.speedValid);
  
  printValue(
    "Gear",
    data.gearStatus,
    "",
    data.gearValid);

  printValue(
    "Ratio",
    data.gearRatio,
    "",
    data.gearValid);

  printValue(
    "Oil",
    data.oilC,
    "C",
    data.oilValid);

  if (!data.oilValid)
  {
    Serial.println("Oil Temperature Unknown");
  }
  else if (data.oilWarming)
  {
    Serial.println("Oil Warming...");
  }
  else if (!data.oilWarming && data.oilValid && !data.oilOvertemp)
  {
    Serial.println("Oil At Operating Temperature");
  }
  else if (data.oilOvertemp)
  {
    Serial.println("OIL OVERTEMP!!");
  }
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
// 1  Speed
// 2  RPM
// 3  Gear
// 4  RPM
// 5  Oil
//
// ================================================================

const uint8_t PID_SLOTS = 6;
uint8_t pidIndex = 0;

void readNextPID()
{
  switch (pidIndex)
  {
    case 0:
    case 2:
    case 4:
      readRPM();
      break;

    case 1:
      readSpeed();
      break;

    case 3:
      readGear();
      break;

    case 5:
      readOilTemp();
      break;
  }

  pidIndex++;

  if (pidIndex >= PID_SLOTS)
    pidIndex = 0;
}

uint32_t lastRequest = 0;
uint32_t lastPrint = 0;

void updateVehicleData()
{
  // Poll one PID at a time
  if (!canPolling)
  {
    updateManualData();
  }
  else if ((now - lastRequest) >= REQUEST_INTERVAL_MS)
  {
    lastRequest = now;
    readNextPID();
  }

  expireStaleData();


  //Print complete dataset at 10Hz
  if (printEnabled && (now - lastPrint) >= 100)
  {
    lastPrint = now;
    printData();
  }
}