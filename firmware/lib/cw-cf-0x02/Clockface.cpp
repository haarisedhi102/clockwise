
#include "Clockface.h"
#include <CWPreferences.h>

const char* FORMAT_TWO_DIGITS = "%02d";

EventBus eventBus;

unsigned long lastMillis = 0;

char hInWords[20];
char mInWords[20]; 
char formattedDate[20];

int temperature = 26;

DateI18nEN i18n;

Clockface::Clockface(Adafruit_GFX* display)
{
  _display = display;

  Locator::provide(display);
  Locator::provide(&eventBus);
}

void Clockface::setup(CWDateTime *dateTime) {
  this->_dateTime = dateTime;
  const auto* settings = ClockwiseParams::getInstance();
  const int16_t offsetMinutes = dateTime->getTimezoneOffset();
  const double baseOffsetHours = -(offsetMinutes +
                                   (dateTime->isDST() ? 60 : 0)) / 60.0;
  delete _location;
  _location = new Dusk2Dawn(settings->latitude, settings->longitude, baseOffsetHours);
  Locator::getDisplay()->setTextWrap(true);
  Locator::getDisplay()->fillRect(0, 0, 64, 64, 0x0000);  

  updateTime();

  updateSunTimes();
  updateDate();
  //updateTemperature();
}


void Clockface::update() 
{  
  if (millis() - lastMillis >= 1000) {

    if (_dateTime->getSecond() == 0) {
      updateTime();
      updateSunTimes();
    }

    if (_dateTime->getMinute() == 0 && _dateTime->getSecond() == 0) {
      updateDate();  
    }

    // if (_dateTime->getMinute() % 15 == 0 && _dateTime->getSecond() == 0) {
    //   updateTemperature();
    // }
    
    lastMillis = millis();
  }  
}


void Clockface::updateSunTimes() 
{
  const auto* settings = ClockwiseParams::getInstance();
  const int16_t offsetMinutes = _dateTime->getTimezoneOffset();
  const double baseOffsetHours = -(offsetMinutes +
                                   (_dateTime->isDST() ? 60 : 0)) / 60.0;
  // Get the current date
  int year = _dateTime->getYear();
  int month = _dateTime->getMonth();
  int day = _dateTime->getDay();
  int rawHour = _dateTime->getHour();
  int minute = _dateTime->getMinute();
  int second = _dateTime->getSecond();
  bool is24h = _dateTime->is24hFormat();
  bool isDST = _dateTime->isDST();
  // Calculate sunrise and sunset times
  int sunrise = _location->sunrise(year, month, day, isDST);
  int sunset = _location->sunset(year, month, day, isDST);

  bool isAM = _dateTime->isAM(); // Use isAM() to determine if it's AM
  int hour = rawHour;
  // Convert 12-hour format to 24-hour format if necessary
  if (!isAM && hour != 12) {
    hour += 12; // Add 12 hours for PM times (except 12 PM)
  } else if (isAM && hour == 12) {
    hour = 0; // Convert 12 AM to 0 hours
  }

  // Get the current time in minutes since midnight
  int currentMinutes = hour * 60 + minute;
  Serial.printf("[Sun] date=%04d-%02d-%02d local=%02d:%02d:%02d rawHour=%d is24h=%d isAM=%d DST=%d\n",
                year, month, day, hour, minute, second, rawHour, is24h, isAM, isDST);
  Serial.printf("[Sun] location lat=%.6f lon=%.6f offsetMinutes=%d baseOffsetHours=%.2f sunrise=%d sunset=%d currentMinutes=%d\n",
                settings->latitude, settings->longitude, offsetMinutes, baseOffsetHours,
                sunrise, sunset, currentMinutes);

  // Calculate time till next sunrise or sunset
  int timeTillEvent = 0;
  const char* eventLabel = ""; // Label for the event (sunrise or sunset)
  if (currentMinutes < sunrise) {
    timeTillEvent = sunrise - currentMinutes; // Time till sunrise
    eventLabel = "SUNRISE IN:";
  } else if (currentMinutes < sunset) {
    timeTillEvent = sunset - currentMinutes; // Time till sunset
    eventLabel = "SUNSET IN:";
  } else {
    // If it's after sunset, calculate time till next day's sunrise
    timeTillEvent = (1440 - currentMinutes) + sunrise;
    eventLabel = "SUNRISE IN:";
  }



  

  char time[6];
  bool formatted = Dusk2Dawn::min2str(time, timeTillEvent);
  Serial.printf("[Sun] event=%s eventMinutes=%d formatted=%s ok=%d\n",
                eventLabel, timeTillEvent, time, formatted);

  // Display the time till sunrise/sunset on the clockface
  Locator::getDisplay()->fillRect(0, 45, 64, 19, 0x0000); // Clear the area

  Locator::getDisplay()->setFont(&small4pt7b);
  Locator::getDisplay()->setCursor(0, 46);
  Locator::getDisplay()->setTextColor(0xF800);
  Locator::getDisplay()->print(eventLabel);
  Locator::getDisplay()->setFont(&hour8pt7b);
  Locator::getDisplay()->setCursor(0, 62);
  Locator::getDisplay()->println(time);
  
}

void Clockface::updateTime() 
{
  Locator::getDisplay()->fillRect(0, 0, 64, 30, 0x0000);  
  Locator::getDisplay()->fillRect(0, 30, 38, 12, 0x0000); 
  i18n.timeInWords(_dateTime->getHour(), _dateTime->getMinute(), hInWords, mInWords);  
  
  // Hour
  Locator::getDisplay()->setFont(&hour8pt7b);  
  Locator::getDisplay()->setCursor(1, 14);
  Locator::getDisplay()->setTextColor(0xF800);
  Locator::getDisplay()->println(hInWords);
  
  // Minute
  Locator::getDisplay()->setFont(&minute7pt7b);
  Locator::getDisplay()->setCursor(0, 25);
  Locator::getDisplay()->setTextColor(0xF800);
  Locator::getDisplay()->println(mInWords);

  // Separator line
  // Locator::getDisplay()->drawFastHLine(1, 40, 62, 0xffff);

  // if (WiFi.status() == WL_CONNECTED) {
  //   Locator::getDisplay()->drawRGBBitmap(1, 55, WIFI, 8, 8);
  // } else {
  //   Locator::getDisplay()->fillRect(1, 55, 8, 8, 0x0000);
  // }
}

void Clockface::updateDate() 
{
  Locator::getDisplay()->fillRect(39, 30, 25, 11, 0x0000);

  // Date
  Locator::getDisplay()->setFont(&small4pt7b);
  Locator::getDisplay()->setCursor(39, 34);
  Locator::getDisplay()->setTextColor(0xF800);
    
  const char* fmt = i18n.formatDate(_dateTime->getDay(), _dateTime->getMonth());

  Locator::getDisplay()->print(fmt);

  uint16_t dateWidth, h = 0;
  int16_t x1, y1 = 0;
  Locator::getDisplay()->getTextBounds(fmt, 0, 0, &x1, &y1, &dateWidth, &h);

  // Weekday
  Locator::getDisplay()->setFont(&small4pt7b);
  //Locator::getDisplay()->setFont(&minute7pt7b);
  Locator::getDisplay()->setCursor(39, 40);
  Locator::getDisplay()->setTextColor(0xF800);
  Locator::getDisplay()->println(i18n.weekDayName(_dateTime->getWeekday()));  
}

void Clockface::updateTemperature() 
{

  Locator::getDisplay()->fillRect(46, 41, 18, 13, 0x0000);
  Locator::getDisplay()->setFont(&minute7pt7b);

  // Temperature
  // TODO get temperature
  temperature++;
  if (temperature > 30) temperature = 20;

  char buffer[4];  
  sprintf(buffer, "%d~", temperature);
  
  uint16_t tempWidth, h = 0;
  int16_t x1,y1 = 0;
  Locator::getDisplay()->getTextBounds(buffer, 0, 0, &x1, &y1, &tempWidth, &h);
 
  Locator::getDisplay()->setCursor(62-tempWidth, 52);
  Locator::getDisplay()->setTextColor(0xF800);
  Locator::getDisplay()->println(buffer);
  
  Locator::getDisplay()->drawRGBBitmap(12, 55, MAIL, 8, 8);
  Locator::getDisplay()->drawRGBBitmap(55, 55, WEATHER_CLOUDY_SUN, 8, 8);
}
