// ================================================================
// LED Animations
// ================================================================
void testAnimation()
{
  ring.fill(ring.Color(255, 0, 0), 0, LED_COUNT);
  ring.show();
  delay(250);

  ring.fill(ring.Color(0, 255, 0), 0, LED_COUNT);
  ring.show();
  delay(250);

  ring.fill(ring.Color(0, 0, 255), 0, LED_COUNT);
  ring.show();
  delay(250);

  ring.clear();

  for(int i=6;i < LED_COUNT;i++)
  {
    ring.setPixelColor(i, ring.Color(255,0,0));
    ring.show();
    delay(30);
  }

  for(int i=6;i < LED_COUNT;i++)
  {
    ring.setPixelColor(i, ring.Color(0,255,0));
    ring.show();
    delay(30);
  }
    
  for(int i=6;i < LED_COUNT;i++)
  {
    ring.setPixelColor(i, ring.Color(0,0,255));
    ring.show();
    delay(30);
  }
}


// ================================================================
// Animation Primitives
// ================================================================

//  Full Ring Pulse
//  Used by "Boot Up" and "Overtemp"

int pulseLevel = 0;
int pulseDirection = 1;
uint32_t lastPulseUpdate = 0;

void updateFullRingPulse(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t maxLevel = 255, uint8_t stepInterval = 1.0)
{
  // Only step the level on the interval, but always draw it:
  // updateLEDs() clears the ring every frame.
  if ((now - lastPulseUpdate) >= stepInterval)
  {
    lastPulseUpdate = now;
    pulseLevel += pulseDirection;

    if (pulseLevel >= maxLevel)
    {
      pulseLevel = maxLevel;
      pulseDirection = -1;
    }
    else if (pulseLevel <= 0)
    {
      pulseLevel = 0;
      pulseDirection = 1;
    }
  }

  ring.fill(ring.Color(
    map(pulseLevel, 0, 255, 0, r), 
    map(pulseLevel, 0, 255, 0, g), 
    map(pulseLevel, 0, 255, 0, b)
    ), 0, LED_COUNT);
}

void resetFullRingPulse()
{
  pulseLevel = 0;
  pulseDirection = 1;
  lastPulseUpdate = 0;
}


//Shift Flash Animation
bool shiftFlashOn = false;
uint32_t lastShiftFlash = 0;

void updateFullRingFlash(uint8_t r, uint8_t g, uint8_t b, uint32_t flashinterval = 100)
{
  if ((now - lastShiftFlash) >= flashinterval)
  {
    lastShiftFlash = now;
    shiftFlashOn = !shiftFlashOn;
  }

  if (shiftFlashOn)
  {
    ring.fill(ring.Color(r, g, b), 0, LED_COUNT);
  }
  else
  {
    ring.clear();
  }
}

void resetFullRingFlash()
{
  shiftFlashOn = false;
  lastShiftFlash = now;
}



// ================================================================
// Animation States
// ================================================================

void bootAnimation()
{
  uint8_t cycles = 1;
  uint32_t count = 0;
  uint32_t lastStep = 0;
  uint8_t stepInterval = 3;
  while (count < (255 * 2 * cycles))
  {
    now = millis();
    if ((now - lastStep) > stepInterval)
    {
      lastStep = now;
      updateFullRingPulse(255, 255, 255, 255, stepInterval);
      count++;
      ring.show();
    }
  }
  resetFullRingPulse();
}


int lastOilTemp = -1;

// position is how many LEDs are off, counting from LED 0. The
// fractional part fades the next LED out, so the bar shrinks
// smoothly instead of stepping one LED at a time.
void updateOilWarmingAnimation(float position)
{
  position = constrain(position, 0, LED_COUNT);

  int fullyOff = (int)position;
  float fade = position - fullyOff;

  for (int i = 0; i < LED_COUNT; i++)
  {
    // Turn LEDs off from left to right
    if (i < fullyOff)
    {
      ring.setPixelColor(i, 0, 0, 0);
    }
    else if (i == fullyOff)
    {
      ring.setPixelColor(i, (uint8_t)(255 * (1.0 - fade)), 0, 0);
    }
    else
    {
      ring.setPixelColor(i, 255, 0, 0);
    }
  }
}

// RPM scale: 0 to RPM_SCALE_MAX across LEDs RPM_LED_FIRST to
// RPM_LED_LAST.
const uint8_t RPM_LED_FIRST = 6;
const uint8_t RPM_LED_LAST = 21;
const uint16_t RPM_SCALE_MAX = 7500;

// Markers are summed here before drawing, so markers that share
// an LED add together instead of overwriting each other.
uint8_t markerR[LED_COUNT];
uint8_t markerG[LED_COUNT];
uint8_t markerB[LED_COUNT];

void clearMarkers()
{
  memset(markerR, 0, sizeof(markerR));
  memset(markerG, 0, sizeof(markerG));
  memset(markerB, 0, sizeof(markerB));
}

void addMarkerPixel(int led, uint8_t r, uint8_t g, uint8_t b, float level)
{
  // Off the scale: just the part of the marker still on it is drawn.
  if (led < RPM_LED_FIRST || led > RPM_LED_LAST)
    return;

  markerR[led] = min(255, markerR[led] + (int)(r * level));
  markerG[led] = min(255, markerG[led] + (int)(g * level));
  markerB[led] = min(255, markerB[led] + (int)(b * level));
}

// Same fractional fade as updateOilWarmingAnimation(): a value
// between two LEDs lights both, each in proportion to how close
// the value is, so the marker glides instead of stepping.
void addMarker(float rpm, uint8_t r, uint8_t g, uint8_t b)
{
  float position = RPM_LED_FIRST + rpm * (RPM_LED_LAST - RPM_LED_FIRST) / RPM_SCALE_MAX;

  int led = (int)floorf(position);
  float fade = position - led;

  addMarkerPixel(led, r, g, b, 1.0 - fade);
  addMarkerPixel(led + 1, r, g, b, fade);
}

void drawMarkers()
{
  for (int i = RPM_LED_FIRST; i <= RPM_LED_LAST; i++)
  {
    ring.setPixelColor(i, markerR[i], markerG[i], markerB[i]);
  }
}

uint32_t nextRPM = 0;
uint32_t previousRPM = 0;

// Gear used for the shift points. Held through neutral (e.g. clutch
// in mid-shift) until the next gear is detected or speed drops
// below NEUTRAL_HOLD_MIN_KMH. 0 = no gear to show.
uint8_t displayGear = 0;

// True while displayGear is being held through neutral.
bool gearHeld = false;

// Engine RPM per km/h per unit of gear ratio (final drive and tire
// size). Starts from the stock estimate and is learned while
// driving in gear, so it tracks the actual tires.
float rpmPerKmhPerRatio = RPM_PER_KMH_PER_RATIO;
uint32_t lastLearnRpmTime = 0;

void learnRpmPerKmh()
{
  // One sample per new RPM reading, not per LED frame.
  if (data.rpmTime == lastLearnRpmTime)
    return;

  lastLearnRpmTime = data.rpmTime;

  if (data.speed < NEUTRAL_HOLD_MIN_KMH || data.rpm < 1000)
    return;

  float sample = data.rpm / (data.speed * gearRatios[displayGear]);

  // Reject samples from clutch slip or a mis-reported gear.
  if (sample < RPM_PER_KMH_PER_RATIO * 0.8 || sample > RPM_PER_KMH_PER_RATIO * 1.2)
    return;

  rpmPerKmhPerRatio += (sample - rpmPerKmhPerRatio) * 0.05;
}

void updateDisplayGear()
{
  if (!data.gearValid || !data.speedValid)
  {
    displayGear = 0;
    gearHeld = false;
  }
  // Real gear (1..GEAR_COUNT) while moving; the reported gear is
  // meaningless when stopped.
  else if (data.gearStatus >= 1 && data.gearStatus <= GEAR_COUNT && data.speed > 0)
  {
    displayGear = data.gearStatus;
    gearHeld = false;
    learnRpmPerKmh();
  }
  else if (data.speed < NEUTRAL_HOLD_MIN_KMH)
  {
    displayGear = 0;
    gearHeld = false;
  }
  // Otherwise neutral above the hold speed: keep the last gear.
  else if (displayGear != 0)
  {
    gearHeld = true;
  }
}

void updateShiftPoints()
{
  nextRPM = 0;
  previousRPM = 0;

  if (displayGear != 0)
  {
    // In gear the engine RPM matches the road speed. In neutral it
    // doesn't, so use the RPM the held gear would have at this speed.
    float gearRPM = data.rpm;

    if (gearHeld)
      gearRPM = data.speed * rpmPerKmhPerRatio * gearRatios[displayGear];

    float rpmPerRatio = gearRPM / gearRatios[displayGear];

    if (displayGear < GEAR_COUNT)
      nextRPM = rpmPerRatio * gearRatios[displayGear + 1];

    if (displayGear > 1)
      previousRPM = rpmPerRatio * gearRatios[displayGear - 1];
  }

  clearMarkers();

  if (nextRPM > NEXT_GEAR_MIN_RPM)
  {
    float level = constrain((float)(nextRPM - NEXT_GEAR_MIN_RPM) / NEXT_GEAR_FADE_RPM, 0.0, 1.0);
    addMarker(nextRPM, 255 * level, 255 * level, 0);
  }
  if (previousRPM != 0)
  {
    addMarker(previousRPM, 255, 255, 0);
  }

  addMarker(data.rpm, 125, 125, 125);

  drawMarkers();
}

void updateLEDs()
{
  // Console "led" command
  if (drawLEDOverride())
    return;

  ring.clear();

  // Track the gear every frame, even when the shift points aren't
  // drawn, so a shift made above SHIFT_POINT_LOW isn't missed.
  updateDisplayGear();

  // Priority 1: Oil overtemperature
  if (data.oilValid && data.oilOvertemp)
  {
    updateFullRingPulse(255, 0, 0);
  }

  // Priority 2: Oil warming
  else if (data.oilValid && data.oilWarming)
  {
    // Same scale as before (0 C -> 10 LEDs off, operating temp ->
    // all off), but kept fractional for the fade.
    float oilTempScaled = 10 + data.oilC * (LED_COUNT - 10) / ENGINE_OPERATING_TEMP;
    updateOilWarmingAnimation(oilTempScaled);
  }

  // Priority 3: Shift indicator
  else if (data.rpmValid)
  {
    if (data.rpm > SHIFT_POINT_MAX)
    {
      updateFullRingFlash(0, 0, 255, 50);
    }
    else if (data.rpm > SHIFT_POINT_HIGH)
    {
      resetFullRingFlash();
      ring.fill(ring.Color(0, 255, 0), 0, LED_COUNT);
    }
    else if (data.rpm > SHIFT_POINT_MID)
    {
      resetFullRingFlash();
      ring.fill(ring.Color(255, 255, 0), 0, LED_COUNT);
    }
    else if (data.rpm > SHIFT_POINT_LOW)
    {
      resetFullRingFlash();
      ring.fill(ring.Color(255, 0, 0), 0, LED_COUNT);
    }
    else
    {
      resetFullRingFlash();
      updateShiftPoints();
    }
  }

  // Priority 4: No valid display condition
  else
  {
    resetFullRingFlash();
    ring.clear();
  }

  ring.show();
}