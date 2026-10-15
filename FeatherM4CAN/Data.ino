

// ================================================================
// Clear data
// ================================================================

void clearData()
{
  data.rpm = 0;
  data.gearStatus = 0;
  data.gearRatio = 0;
  data.oilC = 0;

  data.rpmValid = false;
  data.gearValid = false;
  data.oilValid = false;
  data.oilWarming = true;
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
    "Gear",
    data.gearStatus,
    "",
    data.gearValid);
  
  printValue(
    "Gear Ratio",
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