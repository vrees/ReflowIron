#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "max6675.h"

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int thermoDO = D6;
int thermoCS = D7;
int thermoCLK = D5;
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

const int button = D4;
const int solidstate = D8;
const int poti = A0;
const int temp_preheat = 150;
const int temp_reflow = 240;

int temp_now = 0;
int temp_next = 0;
int temp_poti = 0;
int temp_poti_old = 0;
String state[] = {"OFF", "PREHEAT", "REFLOW", "COOLING"};
int state_now = 0;

int time_count = 0;
int perc = 0;
int offset = 0;

long t = millis();
long t_solder = millis();

int X(int textgroesse, int n)
{
  return (0.5 * (display.width() - textgroesse * (6 * n - 1))); //end int X
}
int Y(int textgroesse, float f)
{
  return (f * display.height() - (textgroesse * 4)); //end int Y
}

void regulate_temp(int temp, int should)
{
  if (should <= temp - offset)
  {
    digitalWrite(solidstate, LOW);
  }
  else if (should > temp + offset)
  {
    digitalWrite(solidstate, HIGH);
  }
}

void PrintScreen(String state, int soll_temp, int ist_temp, int tim, int percentage)
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(state);
  display.setCursor(80, 0);
  String str = String(soll_temp) + " deg";
  display.println(str);

  if (tim != 0)
  {
    display.setCursor(0, 50);
    str = String(tim) + " sec";
    display.println(str);
  }
  if (percentage != 0)
  {
    display.setCursor(80, 50);
    str = String(percentage) + " %";
    display.println(str);
  }
  display.setTextSize(2);
  display.setCursor(30, 22);
  str = String(ist_temp) + " deg";
  display.println(str);
  display.display();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("setup: will init ports");

  pinMode(button, INPUT);
  pinMode(solidstate, OUTPUT);
  digitalWrite(solidstate, LOW);

  Serial.println("setup: will init display now ...");
  display.begin();
  //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  Serial.println("setup: will clear display now ...");
  display.clearDisplay();
  display.display();

  display.fillScreen(WHITE);
  display.setTextSize(1);
  display.setCursor(X(1, 6), Y(1, 0.1));
  display.println("REFLOW");

  delay(3000);
  Serial.println("setup: display done");
}

void loop()
{
  temp_now = thermocouple.readCelsius();
  Serial.print("temp_now=");
  Serial.println(temp_now);

  temp_poti = map(analogRead(poti), 1023, 0, temp_preheat, temp_reflow);
  Serial.print("temp_poti=");
  Serial.println(temp_poti);

  if (temp_poti != temp_poti_old)
  {
    long v = millis();
    display.fillScreen(WHITE);
    display.setTextSize(1);
    display.setCursor(X(1, 6), Y(1, 0.1));
    display.println("REFLOW");
    display.setTextSize(2);
    while (millis() < v + 2000)
    {
      yield();
      temp_poti = map(analogRead(poti), 1023, 0, temp_preheat, temp_reflow);
      if (temp_poti > temp_poti_old + 1 || temp_poti < temp_poti_old - 1)
      {
        display.setCursor(X(2, 3), Y(2, 0.5));
        display.println(String(temp_poti));
        display.display();
        temp_poti_old = temp_poti;
        v = millis();
      }
    }
    temp_poti_old = temp_poti;
  }

  if (millis() > t + 200 || millis() < t)
  {
    // PrintScreen(state[state_now], temp_next, temp_now, time_count, perc);
    t = millis();
  }

  if (digitalRead(button) == 0)
  {
    delay(100);
    long c = millis();
    while (digitalRead(button) == 0)
    {
      yield();

      if (millis() > c + 1500)
      {
        digitalWrite(solidstate, LOW);
        state_now = 0;
        display.fillScreen(WHITE);
        display.setTextColor(BLACK);
        display.setTextSize(2);
        display.setCursor(X(2, 3), Y(2, 0.5));
        display.println("OFF");
        display.display();
        while (digitalRead(button) == 0)
        {
          yield();
          delay(1);
        }
        return;
      }
    }

    t_solder = millis();
    perc = 0,
    state_now++;
    if (state_now == 0)
      temp_next = 0;
    else if (state_now == 1)
      temp_next = temp_preheat;
    else if (state_now == 2)
      temp_next = temp_poti;
    else if (state_now == 3)
      temp_next = 0;
    else if (state_now == 4)
    {
      state_now = 0;
      temp_next = 0;
    }
  }

  if (state_now == 1)
  { //PREHEAT
    regulate_temp(temp_now, temp_next);
    perc = int((float(temp_now) / float(temp_next)) * 100.00);
  }
  else if (state_now == 2)
  { //REFLOW
    regulate_temp(temp_now, temp_next);
    perc = int((float(temp_now) / float(temp_next)) * 100.00);
    if (perc >= 100)
    {
      state_now = 3;
      t_solder = millis();
      perc = 0;
      temp_next = 0;
    }
  }
  else if (state_now == 3)
  { //COOLING
    digitalWrite(solidstate, LOW);
    time_count = int((t_solder + 60000 - millis()) / 1000);
    if (time_count <= 0)
    {
      state_now = 0;
    }
  }
  else
  {
    digitalWrite(solidstate, LOW);
    time_count = 0;
  }
  delay(30);
}
