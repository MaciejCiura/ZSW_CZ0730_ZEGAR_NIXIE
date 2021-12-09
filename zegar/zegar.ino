#include "Timers.h";
#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <SPI.h>
#include "HX711.h"
#include <SD.h>

#define SD_CS 4

#define TFT_CS     10   
#define TFT_RST    9  
#define TFT_DC     8

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);



unsigned long clockTick=0;
int hours, minutes, seconds;           //initialize these to current time before loop starts

Timer Timer1;


void setup()
{
    pinMode(4,INPUT_PULLUP);
    


    tft.initR(INITR_BLACKTAB);  // You will need to do this in every sketch
    tft.fillScreen(ST7735_BLACK); 
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(2);

    //setup timer
    
    //readin current hr:min:sec
    Timer1.begin(1000); 
}

void loop()
{


    while(!digitalRead(4))
    {

      clockTick = map(analogRead(A3),0,1023,0,86400);
      clockTick-=clockTick%60;
      ComputeTime();
      TftPrintTime(hours,minutes,seconds);
      delay(50);
    }
    if(Timer1.available()){
     
      ComputeTime();
      TftPrintTime(hours,minutes,seconds);
      clockTick++;
      Timer1.restart();
    }
}

void ComputeTime()
{
    seconds = clockTick%60;
    minutes = clockTick/60%60;
    if(minutes%60==0 && seconds%60==0) clockTick++;
    hours = clockTick/3600%60;
}

void TftPrint(int val)
{
  tft.fillRect(10,100,120,30, ST7735_BLACK);
  
  tft.setCursor(10,100);
  tft.println(val);



}

void TftPrintTime(int a, int b , int c)
{
  tft.fillRect(10,100,120,30, ST7735_BLACK);
  
  tft.setCursor(10,100);

  String h=CheckZero(a);
  String m=CheckZero(b);
  String s=CheckZero(c);
  
  tft.print(h+":"+m+":"+s);

}

String CheckZero(int a)
{
  if(a<10) return ('0'+(String)a);
  else return (String)a;
}
