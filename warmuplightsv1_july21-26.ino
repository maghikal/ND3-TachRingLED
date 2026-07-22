#include <Adafruit_NeoPixel.h>
#include <math.h>
#define PIN 1
#define LED_COUNT 23
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRBW + NEO_KHZ800);

int ThermistorPin = 8;
int Vo;
float R1 = 2000;
float logR2, R2, T, temp;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

int two[] = {7,8,9,10,11,12,13,14,15,16,17,18};    //LEDs indicate redline at 2000rpm
int three[] = {7,8,9,10,11,12,13,14,15,16};           //LEDs indicate redline at 3000rpm
int four[] = {7,8,9,10,11,12,13,14};                     //LEDs indicate redline at 4000rpm
int five[] = {7,8,9,10,11,12};                              //LEDs indicate redline at 5000rpm
int six[] = {7,8,9,10};                                        //LEDs indicate redline at 6000rpm
int seven[] = {7,8};                                               //LEDs indicate redline at 7000rpm
int redline[] = {7};                                                   //LEDs correspond to actual redline
int warning[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22}; //entire ring of LEDs illuminates to indicate over-temp

/*
int tg1[] = {0};  //temperature gauge segment at lowest
int tg2[] = {0,1}; //temperature gauge up one tick
int tg3[] = {0,1,2}; //temperature gauge up two ticks
int tg4[] = {0,1,2,3}; //temperature gauge at middle tick
int tg5[] = {0,1,2,3,4}; //temperature gauge one tick above middle
int tg6[] = {0,1,2,3,4,5}; //temperature gauge one tick from maximum
int tg7[] = {0,1,2,3,4,5,6}; //temperature gauge at maximum
*/

void setup()
{
  leds.begin();  // Call this to start up the LEDs.
  leds.clear();   // This function, defined below, turns all LEDs off.
  leds.setBrightness(32); // Set global brightness for the whole strip 0-255  
}

void loop() {

  Vo = analogRead(ThermistorPin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  temp = T - 273.15;
  delay(500);

  if (temp < 0)
  {
    leds.clear();
    for(int i=0;i <= 12;i++)
    {
    leds.setPixelColor(two[i], leds.Color(255,0,0,0));
    //leds.setPixelColor(tg1[i], leds.Color(0,0,255,0));
    }
  }
  else if(temp >=0 && temp < 20)
  {
    leds.clear();
    for(int i=0;i <= 10;i++)
    {
    leds.setPixelColor(three[i], leds.Color(255,0,0,0));
    //leds.setPixelColor(tg1[i], leds.Color(0,0,0,255));
    }
  }

  else if(temp >= 20 && temp < 60)
  {
    leds.clear();
    for(int i=0;i <= 8;i++)
    {
    leds.setPixelColor(four[i], leds.Color(255,0,0,0));
    //leds.setPixelColor(tg2[i], leds.Color(0,0,0,255));
    }
  }

  else if(temp >= 60 && temp < 75)
  {
    leds.clear();
    for(int i=0;i <= 6;i++)
    {
    leds.setPixelColor(five[i], leds.Color(255,0,0,0));
    //leds.setPixelColor(tg4[i], leds.Color(0,0,0,255));
    }
  }

  else if(temp >= 75 && temp < 80)
  {
    leds.clear();
    for(int i=0;i <= 4;i++)
    {
    leds.setPixelColor(six[i], leds.Color(255,0,0,0));
    //leds.setPixelColor(tg3[i], leds.Color(0,0,0,255));
    }
  }

  else if(temp >= 80 && temp < 85)
  {
    leds.clear();
    for(int i=0;i <= 2;i++)
    {
    leds.setPixelColor(seven[i], leds.Color(255,0,0,0));
    //leds.setPixelColor(tg3[i], leds.Color(0,0,0,255));
    }
  }

  /*else if(temp >= 85 && temp < 100)
  {
    leds.clear();
    for(int i=0;i <= 5;i++)
    {
    leds.setPixelColor(redline[i], leds.Color(255,0,0,0));
    leds.setPixelColor(tg4[i], leds.Color(0,0,0,255));
    }
  }

  else if(temp >= 100 && temp < 115)
  {
    leds.clear();
    for(int i=0;i <= 6;i++)
    {
    leds.setPixelColor(redline[i], leds.Color(255,0,0,0));
    leds.setPixelColor(tg5[i], leds.Color(0,0,0,255));
    }
  }*/

  else if(temp >= 115 && temp < 125)
  {
    leds.clear();
    for(int i=0;i <= 23;i++)
    {
    leds.setPixelColor(warning[i], leds.Color(255,20,0,0));
    }
  }

  else if(temp > 125)
  {
    leds.clear();
    for(int i=0;i <= 23;i++)
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
    //leds.setPixelColor(tg4[i], leds.Color(0,0,0,255));
    }
  }  
leds.show();
} 
   