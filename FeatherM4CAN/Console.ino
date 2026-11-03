// ================================================================
// Serial console
// ================================================================
//
// Line-based commands for bench testing the LED logic without a
// car. Open the serial monitor at 115200 baud with a newline line
// ending and type "help".
//
// Setting or sweeping a value suspends CAN polling so the PCM
// can't overwrite it. "can on" resumes polling and clears the
// manual values.
//
// While polling is suspended, every valid value has its timestamp
// refreshed at MANUAL_REFRESH_MS so expireStaleData() keeps it
// alive, the same rate a new RPM reading arrives from the car.
//
// ================================================================


// Commands
//
// can on / can off	// Turns CAN polling (and the CAN watchdog) back on or off. can on also stops all sweeps and clears the values you set.
// set <field> <value>	// Sets one value.
// sweep <field> <start> <end> <ms> [once|loop|bounce]	// Ramps a value from start to end over the given time. once stops at the end value, loop jumps back to the start and repeats, and bounce goes back and forth.
// stop [field|all]	// Stops a sweep and keeps the current value.
// invalid <field|all>	// Marks values as N/A, to test the “no data” state.
// clear	// Stops all sweeps and clears all values.
// status	// Prints the data once.
// print on|off	// Turns the 10 Hz data printout on or off. It floods the monitor, so turning it off makes typing easier.
//
// The fields are rpm, speed (km/h), gear (0 means neutral), ratio and oil (°C).

// LED overrides
//
// led 5 255 0 0	// Lights LED 5 red.
// led 6-21 0 255 0	// Lights LEDs 6 to 21 green. A reversed range like 21-6 also works.
// led all 0 0 255	// Lights the whole ring blue.
// led 6-10 off	// Turns those LEDs off.
// led auto	// Goes back to the normal display.


const uint8_t MANUAL_REFRESH_MS = 50;
uint32_t lastManualRefresh = 0;

// Field indexes. Plain ints rather than an enum so the Arduino
// generated prototypes compile.
const uint8_t FIELD_RPM   = 0;
const uint8_t FIELD_SPEED = 1;
const uint8_t FIELD_GEAR  = 2;
const uint8_t FIELD_RATIO = 3;
const uint8_t FIELD_OIL   = 4;
const uint8_t FIELD_COUNT = 5;

const char *fieldNames[FIELD_COUNT] = { "rpm", "speed", "gear", "ratio", "oil" };

// once   - start to end, then hold end
// loop   - start to end, jump back to start, repeat
// bounce - start to end to start, repeat
const uint8_t SWEEP_ONCE   = 0;
const uint8_t SWEEP_LOOP   = 1;
const uint8_t SWEEP_BOUNCE = 2;

struct Sweep
{
  bool active;
  float start;
  float end;
  uint32_t duration;
  uint32_t startTime;
  uint8_t mode;
};

Sweep sweeps[FIELD_COUNT];

// LED override. While active, updateLEDs() shows ledOverride[]
// instead of the vehicle data display.
bool ledOverrideActive = false;
uint32_t ledOverride[LED_COUNT];

const uint8_t CONSOLE_LINE_MAX = 80;
char consoleLine[CONSOLE_LINE_MAX];
uint8_t consoleLength = 0;


// ================================================================
// Read console input
// ================================================================
//
// Non-blocking: collects characters until CR or LF, then runs the
// line. Called every loop.
//
// ================================================================

void updateConsole()
{
  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '\r' || c == '\n')
    {
      if (consoleLength > 0)
      {
        consoleLine[consoleLength] = '\0';
        runCommand(consoleLine);
        consoleLength = 0;
      }
    }
    else if (consoleLength < CONSOLE_LINE_MAX - 1)
    {
      consoleLine[consoleLength++] = c;
    }
  }
}


// ================================================================
// Manual data
// ================================================================
//
// Called from updateVehicleData() in place of CAN polling.
//
// ================================================================

void updateManualData()
{
  if ((now - lastManualRefresh) >= MANUAL_REFRESH_MS)
  {
    lastManualRefresh = now;

    if (data.rpmValid)   data.rpmTime = now;
    if (data.speedValid) data.speedTime = now;
    if (data.gearValid)  data.gearTime = now;
    if (data.oilValid)   data.oilTime = now;
  }

  // In field order, so a ratio sweep overrides the ratio a gear
  // sweep sets.
  for (uint8_t i = 0; i < FIELD_COUNT; i++)
  {
    if (sweeps[i].active)
      updateSweep(i);
  }
}

void updateSweep(uint8_t field)
{
  Sweep &s = sweeps[field];
  uint32_t elapsed = now - s.startTime;
  float progress = 1.0;

  if (s.duration > 0)
  {
    switch (s.mode)
    {
      case SWEEP_ONCE:
        progress = min(1.0f, (float)elapsed / s.duration);
        break;

      case SWEEP_LOOP:
        progress = (float)(elapsed % s.duration) / s.duration;
        break;

      case SWEEP_BOUNCE:
      {
        uint32_t phase = elapsed % (2 * s.duration);
        progress = (float)phase / s.duration;

        if (progress > 1.0)
          progress = 2.0 - progress;
        break;
      }
    }
  }

  setField(field, s.start + (s.end - s.start) * progress);

  if (s.mode == SWEEP_ONCE && elapsed >= s.duration)
  {
    s.active = false;
    Serial.print("Sweep ");
    Serial.print(fieldNames[field]);
    Serial.println(" done");
  }
}

// Stores a value as if it had just been read from the PCM.
void setField(uint8_t field, float value)
{
  switch (field)
  {
    case FIELD_RPM:
      data.rpm = constrain(value, 0, 16383.75);
      data.rpmValid = true;
      data.rpmTime = lastManualRefresh;
      break;

    case FIELD_SPEED:
      data.speed = constrain(lroundf(value), 0, 255);
      data.speedValid = true;
      data.speedTime = lastManualRefresh;
      break;

    // Also sets the ratio the car would report in that gear.
    case FIELD_GEAR:
    {
      uint8_t gear = constrain(lroundf(value), 0, 15);
      auto ratio = gearRatios.find(gear);

      data.gearStatus = gear;
      data.gearRatio = (ratio != gearRatios.end()) ? ratio->second : 0;
      data.gearValid = true;
      data.gearTime = lastManualRefresh;
      break;
    }

    case FIELD_RATIO:
      data.gearRatio = value;
      data.gearValid = true;
      data.gearTime = lastManualRefresh;
      break;

    case FIELD_OIL:
      data.oilC = value;
      data.oilValid = true;
      data.oilTime = lastManualRefresh;
      updateOilFlags();
      break;
  }
}

void invalidateField(uint8_t field)
{
  sweeps[field].active = false;

  switch (field)
  {
    case FIELD_RPM:
      data.rpmValid = false;
      break;

    case FIELD_SPEED:
      data.speedValid = false;
      break;

    case FIELD_GEAR:
    case FIELD_RATIO:
      data.gearValid = false;
      break;

    case FIELD_OIL:
      data.oilValid = false;
      data.oilWarming = false;
      data.oilOvertemp = false;
      break;
  }
}


// ================================================================
// LED override
// ================================================================
//
// Called at the start of updateLEDs(). Returns true if it drew
// the ring, so the normal display is skipped.
//
// ================================================================

bool drawLEDOverride()
{
  if (!ledOverrideActive)
    return false;

  for (uint8_t i = 0; i < LED_COUNT; i++)
    ring.setPixelColor(i, ledOverride[i]);

  ring.show();

  return true;
}

// Parses "n", "first-last" or "all" into an LED range.
bool parseLEDRange(const char *text, int &first, int &last)
{
  if (text == NULL)
    return false;

  if (strcasecmp(text, "all") == 0)
  {
    first = 0;
    last = LED_COUNT - 1;
    return true;
  }

  char *end;
  first = strtol(text, &end, 10);

  if (end == text)
    return false;

  if (*end == '\0')
  {
    last = first;
  }
  else if (*end == '-')
  {
    const char *lastText = end + 1;
    last = strtol(lastText, &end, 10);

    if (end == lastText || *end != '\0')
      return false;
  }
  else
  {
    return false;
  }

  if (first > last)
  {
    int swap = first;
    first = last;
    last = swap;
  }

  return first >= 0 && last < LED_COUNT;
}

bool parseColorChannel(const char *text, uint8_t &value)
{
  float number;

  if (!parseNumber(text, number) || number < 0 || number > 255)
    return false;

  value = (uint8_t)number;
  return true;
}


// ================================================================
// CAN polling control
// ================================================================

void suspendCANPolling()
{
  if (!canPolling)
    return;

  canPolling = false;
  lastManualRefresh = now;
  Serial.println("CAN polling suspended");
}

void resumeCANPolling()
{
  stopAllSweeps();
  clearData();

  // Give the PCM a full watchdog period before re-initializing.
  lastCANResponse = now;
  canPolling = true;
  Serial.println("CAN polling resumed");
}

void stopAllSweeps()
{
  for (uint8_t i = 0; i < FIELD_COUNT; i++)
    sweeps[i].active = false;
}


// ================================================================
// Commands
// ================================================================

void printHelp()
{
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  help                                   this list");
  Serial.println("  status                                 print data once");
  Serial.println("  print on|off                           10 Hz data printout");
  Serial.println("  can on|off                             resume/suspend CAN polling");
  Serial.println("  set <field> <value>                    set a value");
  Serial.println("  sweep <field> <start> <end> <ms> [once|loop|bounce]");
  Serial.println("                                         ramp a value over ms");
  Serial.println("  stop [field|all]                       stop sweeps, hold values");
  Serial.println("  invalid <field|all>                    mark value(s) N/A");
  Serial.println("  clear                                  stop sweeps, clear all data");
  Serial.println("  led <n|first-last|all> <r> <g> <b>     light LEDs (0-255)");
  Serial.println("  led <n|first-last|all> off             turn LEDs off");
  Serial.println("  led auto                               back to the data display");
  Serial.println();
  Serial.print("LEDs are numbered 0 to ");
  Serial.print(LED_COUNT - 1);
  Serial.println(". The first led command blanks the data display.");
  Serial.println();
  Serial.println("Fields: rpm, speed (km/h), gear (0 = neutral), ratio, oil (C)");
  Serial.println("set/sweep suspend CAN polling. Setting gear also sets ratio.");
  Serial.println();
}

// Returns the field index, or -1 if the name is unknown.
int parseField(const char *name)
{
  if (name == NULL)
    return -1;

  for (uint8_t i = 0; i < FIELD_COUNT; i++)
  {
    if (strcasecmp(name, fieldNames[i]) == 0)
      return i;
  }

  return -1;
}

bool parseNumber(const char *text, float &value)
{
  if (text == NULL)
    return false;

  char *end;
  value = strtof(text, &end);

  return end != text && *end == '\0';
}

void printUnknownField(const char *name)
{
  Serial.print("Unknown field: ");
  Serial.println(name ? name : "(none)");
  Serial.println("Fields: rpm, speed, gear, ratio, oil");
}

void runCommand(char *line)
{
  char *command = strtok(line, " \t");

  if (command == NULL)
    return;

  char *arg1 = strtok(NULL, " \t");
  char *arg2 = strtok(NULL, " \t");
  char *arg3 = strtok(NULL, " \t");
  char *arg4 = strtok(NULL, " \t");
  char *arg5 = strtok(NULL, " \t");

  if (strcasecmp(command, "help") == 0 || strcmp(command, "?") == 0)
  {
    printHelp();
  }
  else if (strcasecmp(command, "status") == 0)
  {
    printData();
    Serial.print("CAN polling: ");
    Serial.println(canPolling ? "on" : "off");
  }
  else if (strcasecmp(command, "print") == 0)
  {
    if (arg1 && strcasecmp(arg1, "on") == 0)
      printEnabled = true;
    else if (arg1 && strcasecmp(arg1, "off") == 0)
      printEnabled = false;
    else
      Serial.println("Usage: print on|off");
  }
  else if (strcasecmp(command, "can") == 0)
  {
    if (arg1 && strcasecmp(arg1, "on") == 0)
      resumeCANPolling();
    else if (arg1 && strcasecmp(arg1, "off") == 0)
      suspendCANPolling();
    else
      Serial.println("Usage: can on|off");
  }
  else if (strcasecmp(command, "set") == 0)
  {
    int field = parseField(arg1);
    float value;

    if (field < 0)
    {
      printUnknownField(arg1);
    }
    else if (!parseNumber(arg2, value))
    {
      Serial.println("Usage: set <field> <value>");
    }
    else
    {
      suspendCANPolling();
      sweeps[field].active = false;
      setField(field, value);

      Serial.print(fieldNames[field]);
      Serial.print(" = ");
      Serial.println(value, 2);
    }
  }
  else if (strcasecmp(command, "sweep") == 0)
  {
    int field = parseField(arg1);
    float start, end, duration;
    uint8_t mode = SWEEP_ONCE;
    bool modeOk = true;

    if (arg5 == NULL || strcasecmp(arg5, "once") == 0)
      mode = SWEEP_ONCE;
    else if (strcasecmp(arg5, "loop") == 0)
      mode = SWEEP_LOOP;
    else if (strcasecmp(arg5, "bounce") == 0)
      mode = SWEEP_BOUNCE;
    else
      modeOk = false;

    if (field < 0)
    {
      printUnknownField(arg1);
    }
    else if (!parseNumber(arg2, start) || !parseNumber(arg3, end) ||
             !parseNumber(arg4, duration) || duration < 0 || !modeOk)
    {
      Serial.println("Usage: sweep <field> <start> <end> <ms> [once|loop|bounce]");
    }
    else
    {
      suspendCANPolling();

      Sweep &s = sweeps[field];
      s.start = start;
      s.end = end;
      s.duration = (uint32_t)duration;
      s.startTime = now;
      s.mode = mode;
      s.active = true;
      setField(field, start);

      Serial.print("Sweeping ");
      Serial.print(fieldNames[field]);
      Serial.print(" ");
      Serial.print(start, 2);
      Serial.print(" -> ");
      Serial.print(end, 2);
      Serial.print(" over ");
      Serial.print(s.duration);
      Serial.println(" ms");
    }
  }
  else if (strcasecmp(command, "stop") == 0)
  {
    if (arg1 == NULL || strcasecmp(arg1, "all") == 0)
    {
      stopAllSweeps();
      Serial.println("All sweeps stopped");
    }
    else
    {
      int field = parseField(arg1);

      if (field < 0)
      {
        printUnknownField(arg1);
      }
      else
      {
        sweeps[field].active = false;
        Serial.print("Sweep ");
        Serial.print(fieldNames[field]);
        Serial.println(" stopped");
      }
    }
  }
  else if (strcasecmp(command, "invalid") == 0)
  {
    if (arg1 && strcasecmp(arg1, "all") == 0)
    {
      for (uint8_t i = 0; i < FIELD_COUNT; i++)
        invalidateField(i);
      Serial.println("All values N/A");
    }
    else
    {
      int field = parseField(arg1);

      if (field < 0)
      {
        printUnknownField(arg1);
      }
      else
      {
        invalidateField(field);
        Serial.print(fieldNames[field]);
        Serial.println(" = N/A");
      }
    }
  }
  else if (strcasecmp(command, "led") == 0)
  {
    int first, last;
    uint8_t r, g, b;

    if (arg1 && strcasecmp(arg1, "auto") == 0)
    {
      ledOverrideActive = false;
      Serial.println("LEDs back to the data display");
    }
    else if (!parseLEDRange(arg1, first, last))
    {
      Serial.println("Usage: led <n|first-last|all> <r> <g> <b>");
      Serial.println("       led <n|first-last|all> off");
      Serial.println("       led auto");
    }
    else
    {
      bool off = arg2 && strcasecmp(arg2, "off") == 0;

      if (!off && (!parseColorChannel(arg2, r) ||
                   !parseColorChannel(arg3, g) ||
                   !parseColorChannel(arg4, b)))
      {
        Serial.println("Colors are 0-255: led <n|first-last|all> <r> <g> <b>");
      }
      else
      {
        // Start from a blank ring, not the data display.
        if (!ledOverrideActive)
        {
          memset(ledOverride, 0, sizeof(ledOverride));
          ledOverrideActive = true;
        }

        uint32_t color = off ? 0 : ring.Color(r, g, b);

        for (int i = first; i <= last; i++)
          ledOverride[i] = color;

        Serial.print("LED ");
        Serial.print(first);
        if (last != first)
        {
          Serial.print("-");
          Serial.print(last);
        }
        Serial.println(off ? " off" : " set");
      }
    }
  }
  else if (strcasecmp(command, "clear") == 0)
  {
    stopAllSweeps();
    clearData();
    Serial.println("Data cleared");
  }
  else
  {
    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type \"help\" for a list of commands");
  }
}
