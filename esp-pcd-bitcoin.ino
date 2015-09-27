/*********************************************************************
 * Show Bitcoin exchange rates for US dollars, GB pounds, and EU euros
 * from CoinDesk. Display on PCD8544/Nokia 5110 LCD display.
 *
 * Depends on:
 * - Fork of Adafruit PCD8544 library with changes for ESP8266. Use the
 *   esp8266 branch!
 *   https://github.com/bbx10/Adafruit-PCD8544-Nokia-5110-LCD-library.git
 * - Adafruit GFX library, no changes required
 *   Use the Arduino IDE Library Manager to get the latest version
 * - Arduino JSON library
 *   Use the Arduino IDE Library Manager to get the latest version
 *
 *********************************************************************/

/*
The MIT License (MIT)

Copyright (c) 2015 by bbx10node@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <ESP8266WiFi.h>
#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif
#ifndef max
#define max(x,y) (((x)>(y))?(x):(y))
#endif
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

/******************************************************************
ESP8266 with PCD8544 display

== Parts ==

* Adafruit Huzzah ESP8266 https://www.adafruit.com/products/2471

* Adafruit PCD8544/5110 display https://www.adafruit.com/product/338

* Adafruit USB to TTL serial cable https://www.adafruit.com/products/954

== Connection ==

USB TTL     Huzzah      Nokia 5110  Description
            ESP8266     PCD8544

            GND         GND         Ground
            3V          VCC         3.3V from Huzzah to display
            14          CLK         Output from ESP SPI clock
            13          DIN         Output from ESP SPI MOSI to display data input
            12          D/C         Output from display data/command to ESP
            #5          CS          Output from ESP to chip select/enable display
            #4          RST         Output from ESP to reset display
                        LED         3.3V to turn backlight on, GND off

GND (blk)   GND                     Ground
5V  (red)   V+                      5V power from PC or charger
TX  (green) RX                      Serial data from IDE to ESP
RX  (white) TX                      Serial data to ESP from IDE
******************************************************************/

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 14 on Huzzah ESP8266
// MOSI is LCD DIN - this is pin 13 on an Huzzah ESP8266
// pin 12 - Data/Command select (D/C) on an Huzzah ESP8266
// pin 5 - LCD chip select (CS)
// pin 4 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(12, 5, 4);

const char SSID[]       = "<your SSID>";
const char PASSWORD[]   = "<your password>";

// Looks a capital C with two horizontal lines
const uint8_t EURO_SYMBOL[] PROGMEM = {
  B00110000,
  B01001000,
  B11100000,
  B01000000,
  B11100000,
  B01001000,
  B00110000,
};

// 60 second delay between normal updates
#define DELAY_NORMAL    (60*1000)
// 10 minute delay between updates after an error
#define DELAY_ERROR     (10*60*1000)

#define COINDESK    "api.coindesk.com"
const char COINDESK_REQ[] =
    "GET /v1/bpi/currentprice.json HTTP/1.1\r\n"
    "Host: " COINDESK "\r\n"
    "Connection: close\r\n\r\n";

void setup()
{
  Serial.begin(115200);

  display.begin();
  // init done

  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(50);

  display.display(); // show splashscreen

  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());

  // text display tests
  display.setTextSize(1);
  display.setTextColor(BLACK);

  // Prices provided by CoinDesk so give them credit.
  // See http://www.coindesk.com/api/ for details.
  display.clearDisplay();   // clears the screen and buffer
  display.println(F("  Powered by"));
  display.println(F("   CoinDesk"));
  display.println();
  display.println(F("http://www.coindesk.com/price"));
  display.display();
  delay(2000);
}

void loop()
{
  char respBuf[1024];

  // http://api.coindesk.com/v1/bpi/currentprice.json
  // TBD https://api.coindesk.com/v1/bpi/currentprice.json
  WiFiClient httpclient;
  Serial.println(F("open socket to " COINDESK " port 80"));
  if (!httpclient.connect(COINDESK, 80)) {
    Serial.println(F("socket connect to coindesk.com failed"));
    //return;
    delay(100);
  }
  Serial.println(httpclient.connected() ?
      F("connected") : F("not connected"));

  Serial.print(F("Requesting "));
  Serial.println(COINDESK_REQ);
  httpclient.print(COINDESK_REQ);
  httpclient.flush();

  // Collect http response headers and content from CoinDesk
  // Headers are discarded.
  // The content is formatted in JSON and is left in respBuf.
  int respLen = 0;
  bool skip_headers = true;
  while (httpclient.connected()) {
    if (skip_headers) {
      String aLine = httpclient.readStringUntil('\n');
      // Blank line denotes end of headers
      if (aLine.length() <= 1) {
        skip_headers = false;
      }
    }
    else {
      int bytesIn;
      bytesIn = httpclient.read((uint8_t *)&respBuf[respLen], sizeof(respBuf) - respLen);
      Serial.print(F("bytesIn ")); Serial.println(bytesIn);
      if (bytesIn > 0) {
        respLen += bytesIn;
        if (respLen > sizeof(respBuf)) respLen = sizeof(respBuf);
      }
      else if (bytesIn < 0) {
        Serial.print(F("read error "));
        Serial.println(bytesIn);
      }
    }
    delay(1);
  }
  httpclient.stop();

  if (respLen >= sizeof(respBuf)) {
    Serial.print(F("respBuf overflow "));
    Serial.println(respLen);
    delay(DELAY_ERROR);
    return;
  }
  // Terminate the C string
  respBuf[respLen++] = '\0';
  Serial.print(F("respLen "));
  Serial.println(respLen);
  Serial.println(respBuf);

  // Parse JSON and show results
  if (showBitcoin(respBuf)) {
    delay(DELAY_NORMAL);
  }
  else {
    delay(DELAY_ERROR);
  }
}

bool showBitcoin(char *json)
{
  StaticJsonBuffer<1024> jsonBuffer;

  // JSON from CoinDesk in json
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
    return false;
  }

  const char *time_updatedISO = root["time"]["updatedISO"];
  Serial.println(time_updatedISO);
  // Display date on 1 line and time on next
  //2015-09-25T09:14:00+00:00
  //012345678901234567890
  //          1         2
  char yyyymmdd[10+1];
  const char *hhmmss;
  strncpy(yyyymmdd, time_updatedISO, 10);
  yyyymmdd[10] = '\0';
  hhmmss = &time_updatedISO[11];
  display.clearDisplay();
  display.println(yyyymmdd);
  display.println(hhmmss);

  // Show bitcoin exchange rate in US dollars
  const char *bpi_usd_rate = root["bpi"]["USD"]["rate"];
  Serial.println(bpi_usd_rate);
  display.print(F("USD $"));
  display.println(bpi_usd_rate);

  // Show bitcoin exchange rate in Great Britain pounds
  const char *bpi_gbp_rate = root["bpi"]["GBP"]["rate"];
  Serial.println(bpi_gbp_rate);
  display.print(F("GBP \x9C"));
  display.println(bpi_gbp_rate);

  // Show bitcoin exchange rate in EU Euros
  const char *bpi_eur_rate = root["bpi"]["EUR"]["rate"];
  Serial.println(bpi_eur_rate);
  display.print(F("EUR  "));
  display.drawBitmap(24, 40, EURO_SYMBOL, 5, 7, 1);
  display.println(bpi_eur_rate);

  display.display();
  return true;
}
