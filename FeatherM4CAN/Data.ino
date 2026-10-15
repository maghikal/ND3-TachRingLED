

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
      readGear();
      break;

    case 2:
      readOilTemp();
      break;
  }

  pidIndex++;

  if (pidIndex >= 3)
    pidIndex = 0;
}

uint32_t lastRequest = 0;
uint32_t lastPrint = 0;

void updateVehicleData()
{
  // Poll one PID at a time
  if ((now - lastRequest) >= REQUEST_INTERVAL_MS)
  {
    lastRequest = now;
    readNextPID();
  }


  //Print complete dataset at 10Hz
  if ((now - lastPrint) >= 100)
  {
    lastPrint = now;
    printData();
  }
}