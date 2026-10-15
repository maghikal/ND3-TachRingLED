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
  if ((now - lastPulseUpdate) < stepInterval)
    return;

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
  ring.fill(ring.Color(
    round(map(pulseLevel, 0, 255, 0, r)), 
    round(map(pulseLevel, 0, 255, 0, g)), 
    round(map(pulseLevel, 0, 255, 0, b))
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

void updateShiftPoints()
{

}

void updateLEDs()
{
  // Priority 1: Oil overtemperature
  if (data.oilValid && data.oilOvertemp)
  {
    updateFullRingPulse(255, 0, 0);
  }

  // Priority 2: Oil warming
  else if (data.oilValid && data.oilWarming)
  {
    int oilTempScaled = round(map(data.oilC, 0, ENGINE_OPERATING_TEMP, 10, LED_COUNT));
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
      ring.fill(ring.Color(255, 0, 0), 0, LED_COUNT);
    }
    else if (data.rpm > SHIFT_POINT_MID)
    {
      resetFullRingFlash();
      ring.fill(ring.Color(255, 255, 0), 0, LED_COUNT);
    }
    else if (data.rpm > SHIFT_POINT_LOW)
    {
      resetFullRingFlash();
      ring.fill(ring.Color(0, 255, 0), 0, LED_COUNT);
    }
    else
    {
      resetFullRingFlash();
      ring.clear();
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