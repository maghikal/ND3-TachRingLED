#include <Adafruit_NeoPixel.h>
#include <math.h>
#define PIN 1
#define LED_COUNT 32
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRBW + NEO_KHZ800);

int ThermistorPin = 8;
int Vo;
float R1 = 2000;
float logR2, R2, T, temp;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

int two[] = {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};    //LEDs indicate redline at 2000rpm
int three[] = {10,11,12,13,14,15,16,17,18,19,20,21,22,23,24};           //LEDs indicate redline at 3000rpm
int four[] = {10,11,12,13,14,15,16,17,18,19,20,21};                     //LEDs indicate redline at 4000rpm
int five[] = {10,11,12,13,14,15,16,17,18};                              //LEDs indicate redline at 5000rpm
int six[] = {10,11,12,13,14,15};                                        //LEDs indicate redline at 6000rpm
int seven[] = {10,11,12};                                               //LEDs indicate redline at 7000rpm
int redline[] = {10};                                                   //LEDs correspond to actual redline
int warning[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}; //entire ring of LEDs illuminates to indicate over-temp

void setup()
{
  leds.begin();  // Call this to start up the LED strip.
  leds.clear();   // This function, defined below, turns all LEDs off...
  leds.setBrightness(32); // Set global brightness for the whole strip 0-255  
  //leds.show();   // ...but the LEDs don't actually update until you call this.
}

void loop() {

  Vo = analogRead(ThermistorPin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  temp = T - 273.15;
  //temp = (T * 9.0)/ 5.0 + 32.0; 
  delay(500);

  if (temp < 0)
  {
    leds.clear();
    for(int i=0;i <= 18;i++)
    {
    leds.setPixelColor(two[i], leds.Color(255,0,0,0));
    }
  }
  else if(temp >=0 && temp < 20)
  {
    leds.clear();
    for(int i=0;i <= 15;i++)
    {
    leds.setPixelColor(three[i], leds.Color(255,0,0,0));
    }
  }

  else if(temp >= 20 && temp < 60)
  {
    leds.clear();
    for(int i=0;i <= 12;i++)
    {
    leds.setPixelColor(four[i], leds.Color(255,0,0,0));
    }
  }

  else if(temp >= 60 && temp < 75)
  {
    leds.clear();
    for(int i=0;i <= 9;i++)
    {
    leds.setPixelColor(five[i], leds.Color(255,0,0,0));
    }
  }

  else if(temp >= 75 && temp < 80)
  {
    leds.clear();
    for(int i=0;i <= 6;i++)
    {
    leds.setPixelColor(six[i], leds.Color(255,0,0,0));
    }
  }

  else if(temp >= 80 && temp < 85)
  {
    leds.clear();
    for(int i=0;i <= 3;i++)
    {
    leds.setPixelColor(seven[i], leds.Color(255,0,0,0));
    }
  }

  else if(temp >= 115 && temp < 125)
  {
    leds.clear();
    for(int i=0;i <= 32;i++)
    {
    leds.setPixelColor(warning[i], leds.Color(255,35,0,0));
    }
  }

  else if(temp > 125)
  {
    leds.clear();
    for(int i=0;i <= 32;i++)
    {
    leds.setPixelColor(warning[i], leds.Color(255,0,0,0));
    }
  }

  else
  {
    leds.clear();
    for(int i=0;i <= 1;i++)
    {
    leds.setPixelColor(redline[i], leds.Color(255,0,0,0));
    }
  }  
leds.show(); // Okay, we have informed which colors we want. Now, it is time to flip the switch and let the magic happen. The pixels.show() command does that
} 
   