#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <ESPmDNS.h>


#define PIN_C1OFFTOUCH    12
#define PIN_C1AUTOTOUCH   15
#define PIN_C1ADVTOUCH    14
#define PIN_C1PLUSTOUCH   33
#define PIN_C1OFFLED      23
#define PIN_C1AUTOLED     5
#define PIN_C1ADVLED      16
#define PIN_C1PLUSLED     19
#define PIN_C1STATUSLED   25

#define PIN_C2OFFTOUCH    13
#define PIN_C2AUTOTOUCH   4
#define PIN_C2ADVTOUCH    27
#define PIN_C2PLUSTOUCH   32
#define PIN_C2OFFLED      22
#define PIN_C2AUTOLED     18
#define PIN_C2ADVLED      17
#define PIN_C2PLUSLED     21
#define PIN_C2STATUSLED   26

int C1LEDS[] = {PIN_C1OFFLED, PIN_C1AUTOLED, PIN_C1ADVLED, PIN_C1PLUSLED, PIN_C1STATUSLED};
int C2LEDS[] = {PIN_C2OFFLED, PIN_C2AUTOLED, PIN_C2ADVLED, PIN_C2PLUSLED, PIN_C2STATUSLED};


const char *ssid = "Kingdom";
const char *password = "password";
const char *admin_username = "admin";
const char *admin_password = "admin";
const char* const day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
WebServer server(80);


class Schedule
{
  static const int SCHED_SIZE = sizeof(uint16_t)*8;
  static const uint16_t SCHED_NULL = 0xffff;
  
  char *part_name;
  uint16_t *sched[7];
  bool state;
  bool sched_state;
  int channel;
  uint8_t day;
  uint16_t now;
  bool advance;
  bool plus_one;
  uint8_t plus_day;
  uint16_t plus_now;
  bool off;

  public:
  String location;

  
  Schedule(char *part_name, int channel, String location)
  {
    this->part_name = part_name;
    this->state = false;
    this->set_advance(false);
    this->sched_state = false;
    this->channel = channel;
    this->location = location;
    this->set_plus_one(false);
    this->off = false;
  }


  void begin()
  {
    load();
  }


  void update(uint8_t day, uint16_t now)
  {
    this->day = day;
    this->now = now;

    if (this->get_plus_one())
    {
      // Has the hour expired?
      if (this->day == this->plus_day && this->now >= this->plus_now)
        this->set_plus_one(false);
      else
        this->state = true;
    }

    if (!this->get_plus_one())
    {
      if (this->off)
      {
        this->state = false;
      }
      else
      {
        this->state = false;
        for (int i = 0; i < 4; ++i)
        {
          if (this->state = (now >= this->sched[day][i*2] && now < this->sched[day][i*2+1]))
            break;
        }
      
        if (this->state != this->sched_state)
        {
          this->sched_state = this->state;
          this->set_advance(false);
        }
      
        if (this->get_advance())
          this->state = !this->state;
      }
    }
  }


  bool get_state()
  {
    return this->state;
  }


  int get_channel()
  {
    return this->channel;
  }


  void set_advance(bool adv)
  {
    if (this->off)
      return;

    if (this->advance = adv)
      this->set_plus_one(false);
  }


  bool get_advance()
  {
    return this->advance;
  }


  void set_plus_one(bool plus)
  {
    if (this->plus_one = plus)
    {
      this->set_advance(false);
      this->plus_day = this->day;
      this->plus_now = this->now + 100;
      if (this->plus_now > 2359)
      {
        this->plus_now -= 2359;
        this->plus_day = (this->plus_day + 1) % 7;
      }
    }
    else
    {
      this->plus_day = SCHED_NULL;
      this->plus_now = SCHED_NULL;
    }
  }


  bool get_plus_one()
  {
    return this->plus_one;
  }


  bool get_off()
  {
    return this->off;
  }


  void set_off(bool off)
  {
    this->set_plus_one(false);
    this->set_advance(false);
    this->off = off;

    Preferences preferences;
    preferences.begin(this->part_name, false);
    if (this->off != preferences.getBool("off", false))
      if (preferences.putBool("off", this->off) != sizeof(bool))
        Serial.println("Failed to store schedule");
    preferences.end();
  }

  
  String get_edit_form()
  {
    String t;
    t += "<form action=\"saveschedule?channel=";
    t += this->channel;
    t += "\" method=\"post\"><table><tr><th/>";
    for (int i = 0; i < 4; ++i)
      t += "<th>On</th><th>Off</th>";
    t += "</tr>";

    for (int d = 0; d < 7; ++d)
    {
      t += "<tr><th>";
      t += day_names[d];
      t += "</th>";

      for (int e = 0; e < 8; ++e)
      {
        t += "<td><input type=\"time\" name=\"";
        t += day_names[d];
        t += e;
        t += "\"";
        if (this->sched[d][e] != SCHED_NULL)
        {
          char buf[10];
          t += " value=\"";
          snprintf(buf, 10, "%02d:%02d", this->sched[d][e] / 100, this->sched[d][e] % 100);
          t += buf;
          t += "\"";
        }
        t += "></td>";
      }
      t += "</tr>";
    }
  
    t += "</table><input type=\"submit\" value=\"Save\"></form>";

    return t;
  }


  void parse_edit_form()
  {
    // Parse arguments from server connection
    for (int d = 0; d < 7; ++d)
      for (int e = 0; e < 8; ++e)
      {
        String a;
        a += day_names[d];
        a += e;
  
        uint16_t val = SCHED_NULL;
  
        if (server.hasArg(a))
        {
          String v = server.arg(a);
          if (v.length() > 0)
          {
            int hr = v.substring(0, 2).toInt();
            int mn = v.substring(3, 5).toInt();
            val = hr * 100 + mn;
            Serial.println(String(a) + " " + String(val));
          }
        }
  
        this->sched[d][e] = val;
      }

    // Validate
    for (int d = 0; d < 7; ++d)
      for (int e = 0; e < 4; ++e)
      {
        uint16_t start = this->sched[d][e*2];
        uint16_t end = this->sched[d][e*2+1];

        if (start == SCHED_NULL || end == SCHED_NULL || start >= end)
          start = end = SCHED_NULL;

        this->sched[d][e*2] = start;
        this->sched[d][e*2+1] = end;
      }

    store();
  }


  bool next_state_change(uint16_t& day, uint16_t& tm)
  {
    if (this->off)
      return false;
    
    if (this->state)
    {
      // Look for the next off
      uint16_t now = this->now;
      int next = -1;
      for (int d = 0; d <= 7; ++d)
      {
        int di = (this->day + d) % 7;
  
        uint16_t next = SCHED_NULL, next_start = SCHED_NULL;
        for (int i = 0; i < 4; ++i)
        {
          uint16_t e = this->sched[di][i*2+1];
          if (e >= now && e < next)
          {
            next = e;
            next_start = this->sched[di][i*2];
          }
        }

        if (next != SCHED_NULL)
        {
          day = di;
          tm = next;

          if (this->get_plus_one())
          {
            if (time_before(this->plus_day, this->plus_now, di, next_start))
            {
              day = this->plus_day;
              tm = this->plus_now;
            }
          }
          return true;
        }
        now = 0;
      }

      if (this->get_plus_one())
      {
        day = this->plus_day;
        tm = this->plus_now;
        return true;
      }
    }
    else
    {
      // Look for the next on
      uint16_t now = this->now;
      for (int d = 0; d <= 7; ++d)
      {
        int di = (this->day + d) % 7;
  
        uint16_t next = SCHED_NULL;
        for (int i = 0; i < 4; ++i)
        {
          uint16_t e = this->sched[di][i*2];
          if (e >= now && e < next)
            next = e;
        }

        if (next != SCHED_NULL)
        {
          day = di;
          tm = next;
          return true;
        }
        now = 0;
      }
    }

    return false;
  }


  private:
  
  bool time_before(uint16_t daya, uint16_t tma, uint16_t dayb, uint16_t tmb)
  {
    if (daya < this->day || daya == this->day && tma < this->now)
      daya += 7;
    if (dayb < this->day || dayb == this->day && tmb < this->now)
      dayb += 7;

    return daya < dayb || daya == dayb && tma < tmb;
  }


  void load()
  {
    Preferences preferences;
    preferences.begin(this->part_name, false);
    for (int day = 0; day < 7; ++day)
    {
      this->sched[day] = (uint16_t*)calloc(SCHED_SIZE, 1);
      
      if (preferences.getBytes(day_names[day], this->sched[day], SCHED_SIZE) != SCHED_SIZE)
      {
        Serial.print("Schedule for ");
        Serial.print(day_names[day]);
        Serial.print(" failed to load.\n");
        for (int i = 0; i < 8; ++i)
          this->sched[day][i] = SCHED_NULL;
      }
    }
    this->off = preferences.getBool("off", false);
    preferences.end();
  }


  void store()
  {
    Preferences preferences;
    preferences.begin(this->part_name, false);
    for (int day = 0; day < 7; ++day)
      if (preferences.putBytes(day_names[day], this->sched[day], SCHED_SIZE) != SCHED_SIZE)
        Serial.println("Failed to store schedule");
    if (preferences.putBool("off", this->off) != sizeof(bool))
      Serial.println("Failed to store schedule");
    preferences.end();
  }
};


class TouchMonitor
{
  #define TOUCH_THRESHOLD   50
  #define TOUCH_SAMPLES     5
  #define TIME_THRESHOLD    20
  
  static int touch_pins[8];
  byte stats[sizeof(touch_pins)/sizeof(*touch_pins)][TOUCH_SAMPLES];
  int stat_idx;
  int touch_idx;
  bool triggered;
  int touch_time;
  void (*handler)(int);

  public:
  
  TouchMonitor(void (*handler)(int))
  {
    for (int i = 0; i < sizeof(touch_pins)/sizeof(*touch_pins); ++i)
      for (int j = 0; j < TOUCH_SAMPLES; ++j)
        this->stats[i][j] = 80;

    this->stat_idx = 0;
    this->touch_idx = -1;
    this->triggered = false;
    this->handler = handler;
  }


  void update()
  {
    for (int i = 0; i < sizeof(touch_pins)/sizeof(*touch_pins); ++i)
      stats[i][stat_idx] = touchRead(touch_pins[i]);
    stat_idx = (stat_idx + 1) % TOUCH_SAMPLES;
  
    if (touch_idx >= 0)
    {
      int sum = 0;
      for (int j = 0; j < TOUCH_SAMPLES; ++j)
        sum += stats[touch_idx][j];
      if (sum / TOUCH_SAMPLES < TOUCH_THRESHOLD)
      {
        if ((millis() - touch_time) > TIME_THRESHOLD && !triggered)
        {
          triggered = true;
          this->handler(this->touch_pins[touch_idx]);
        }
      }
      else
      {
        touch_idx = -1;
        triggered = false;
      }
    }
  
    if (touch_idx < 0)
    {
      for (int i = 0; i < sizeof(touch_pins)/sizeof(*touch_pins); ++i)
      {
        int sum = 0;
        for (int j = 0; j < TOUCH_SAMPLES; ++j)
          sum += stats[i][j];
        if (sum / TOUCH_SAMPLES < TOUCH_THRESHOLD)
        {
          touch_idx = i;
          touch_time = millis();
        }
      }
    }
  }
};
int TouchMonitor::touch_pins[8] = {PIN_C1OFFTOUCH, PIN_C1AUTOTOUCH, PIN_C1ADVTOUCH, PIN_C1PLUSTOUCH, PIN_C2OFFTOUCH, PIN_C2AUTOTOUCH, PIN_C2ADVTOUCH, PIN_C2PLUSTOUCH};


void cycle_leds()
{
  for (int i = 0; i <= 5; ++i)
  {
    if (i > 0)
    {
      digitalWrite(C1LEDS[i-1], LOW);
      digitalWrite(C2LEDS[i-1], LOW);
    }

    if (i < 5)
    {
      digitalWrite(C1LEDS[i], HIGH);
      digitalWrite(C2LEDS[i], HIGH);
      delay(150);
    }
  }
}


Schedule sched1("schedule", 1, "First floor heating");
Schedule sched2("schedule2", 2, "Ground floor heating");
TouchMonitor touch_monitor(touch_handler);


void touch_handler(int idx)
{
  switch (idx)
  {
    case PIN_C1OFFTOUCH:
      sched1.set_off(true);
      break;
    case PIN_C1AUTOTOUCH:
      sched1.set_off(false);
      break;
    case PIN_C1ADVTOUCH:
      sched1.set_advance(!sched1.get_advance());
      break;
    case PIN_C1PLUSTOUCH:
      sched1.set_plus_one(!sched1.get_plus_one());
      break;
    case PIN_C2OFFTOUCH:
      sched2.set_off(true);
      break;
    case PIN_C2AUTOTOUCH:
      sched2.set_off(false);
      break;
    case PIN_C2ADVTOUCH:
      sched2.set_advance(!sched2.get_advance());
      break;
    case PIN_C2PLUSTOUCH:
      sched2.set_plus_one(!sched2.get_plus_one());
      break;
  }
}


void setup()
{
  Serial.begin(115200);
  Serial.println();

  for (int i = 0; i <= 4; ++i)
  {
    pinMode(C1LEDS[i], OUTPUT);
    pinMode(C2LEDS[i], OUTPUT);
  }

  WiFi.begin(ssid, password);
  Serial.print("WiFi.");
  bool light = HIGH;
  for (int i = 0; WiFi.status() != WL_CONNECTED; ++i)
  {
    if (i == 120)  // If it doesn't connect in 60 sec.. it won't.
      ESP.restart();
    digitalWrite(PIN_C1STATUSLED, light);
    light = !light;
    delay(500);
    Serial.print(".");
  }
  digitalWrite(PIN_C1STATUSLED, LOW);
  Serial.println("Connected");
  Serial.println(WiFi.localIP());

  cycle_leds();

  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", "GMT0BST,M3.5.0,M10.5.0", 1);
  tzset();


  sched1.begin();
  sched2.begin();
  

  if (MDNS.begin("chcontrol"))
    Serial.println("MDNS responder started");


  server.on("/", handleRoot);
  server.on("/editschedule", handleEditSchedule);
  server.on("/saveschedule", handleSaveSchedule);
  server.on("/advance", handleAdvance);
  server.on("/plusone", handlePlusOne);
  server.on("/auto", handleAuto);
  server.on("/off", handleOff);
  server.begin();
}


void loop()
{
  touch_monitor.update();

  struct tm info;
  getLocalTime(&info);

  uint16_t now = info.tm_hour * 100 + info.tm_min;

  sched1.update(info.tm_wday, now);
  digitalWrite(PIN_C1OFFLED, sched1.get_off() ? HIGH : LOW);
  digitalWrite(PIN_C1AUTOLED, sched1.get_off() ? LOW : HIGH);
  digitalWrite(PIN_C1ADVLED, sched1.get_advance() ? HIGH : LOW);
  digitalWrite(PIN_C1PLUSLED, sched1.get_plus_one() ? HIGH : LOW);
  digitalWrite(PIN_C1STATUSLED, sched1.get_state() ? HIGH : LOW);

  sched2.update(info.tm_wday, now);
  digitalWrite(PIN_C2OFFLED, sched2.get_off() ? HIGH : LOW);
  digitalWrite(PIN_C2AUTOLED, sched2.get_off() ? LOW : HIGH);
  digitalWrite(PIN_C2ADVLED, sched2.get_advance() ? HIGH : LOW);
  digitalWrite(PIN_C2PLUSLED, sched2.get_plus_one() ? HIGH : LOW);
  digitalWrite(PIN_C2STATUSLED, sched2.get_state() ? HIGH : LOW);

  server.handleClient();
}


String format_time(uint16_t t)
{
  char buf[6];
  snprintf(buf, 6, "%02d:%02d", t / 100, t % 100);
  return String(buf);
}


String schedule_summary(Schedule& sched, struct tm &info)
{
  String t;
  t += "<p>";
  t += sched.location;
  t += " is ";
  t += sched.get_state() ? "on" : "off";
  if (sched.get_advance())
    t += " (advance)";
  if (sched.get_plus_one())
    t += " (+1)";
  t += ".";
  uint16_t nd, nt;
  if (sched.next_state_change(nd, nt))
  {
    t += " Next ";
    t += sched.get_state() ? "off" : "on";
    t += " at ";
    if (nd != info.tm_wday || nt < info.tm_hour * 100 + info.tm_min)
    {
      t += day_names[nd];
      t += " ";
    }
    t += format_time(nt);
    t += ".";
  }

  if (sched.get_off())
  {
    t += "<div><a href=\"plusone?channel=";
    t += sched.get_channel();
    t += "\">+1 hour</a></div>";
    t += "<div><a href=\"auto?channel=";
    t += sched.get_channel();
    t += "\">Turn on schedule timer</a></div>";
  }
  else
  {
    t += "<div><a href=\"advance?channel=";
    t += sched.get_channel();
    t += "\">Advance</a></div>";
    t += "<div><a href=\"plusone?channel=";
    t += sched.get_channel();
    t += "\">+1 hour</a></div>";
    t += "<div><a href=\"editschedule?channel=";
    t += sched.get_channel();
    t += "\">Edit schedule</a></div>";
    t += "<div><a href=\"off?channel=";
    t += sched.get_channel();
    t += "\">Turn off schedule timer</a></div>";
  }

  t += "</p>";
  return t;
}


Schedule *get_schedule()
{
  if (!server.hasArg("channel"))
    return nullptr;

  if (server.arg("channel") == "1")
    return &sched1;
  if (server.arg("channel") == "2")
    return &sched2;

  return nullptr;
}


void check_authentication()
{
  if (!server.authenticate(admin_username, admin_password))
    return server.requestAuthentication();
}


void handleRoot()
{
  check_authentication();
  
  struct tm info;
  getLocalTime(&info);

  String t;
  t += "<!DOCTYPE html>\
  <html>\
  <head>\
    <meta charset=\"UTF-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
    <title>Central Heating</title>\
  </head>\
  <body>\
    <h1>Central Heating Home</h1>";

  char buf[32];
  snprintf(buf, 32, "<p>%s %02d:%02d:%02d</p>", day_names[info.tm_wday], info.tm_hour, info.tm_min, info.tm_sec);
  t += buf;

  t += schedule_summary(sched1, info);
  t += schedule_summary(sched2, info);

  t += "</body></html>";

  server.send(200, "text/html", t);
}


void handleEditSchedule()
{
  check_authentication();

  String t;
  t += 
  "<!DOCTYPE html>\
  <html>\
  <head>\
    <meta charset=\"UTF-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
    <title>Central Heating Control</title>\
  </head>\
  <body>\
    <div><a href=\"\\\">Home</a></div>";

  Schedule *s;
  if (s = get_schedule())
  {
    t += "<h1>";
    t += s->location;
    t += "</h1>";
    t += s->get_edit_form();
  }

  t += "</body></html>";
  
  server.send(200, "text/html", t);
}


void handleSaveSchedule()
{
  check_authentication();

  String t;
  t += "<!DOCTYPE html>\
  <html>\
  <head>\
    <meta charset=\"UTF-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
    <title>Central Heating Control</title>\
  </head>\
  <body>\
    <div><a href=\"\\\">Home</a></div>";

  Schedule *s;
  if (s = get_schedule())
  {
    t += "<h1>";
    t += s->location;
    t += "</h1>";
    
    s->parse_edit_form();
    
    t += "Schedule saved";
  }

  t += "</body></html>";

  server.send(200, "text/html", t);
}


void redirect_to_root()
{
  server.sendHeader("Location", "/");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(301);
}


void handleAdvance()
{
  check_authentication();

  Schedule *s;
  if (s = get_schedule())
  {
    struct tm info;
    getLocalTime(&info);

    s->set_advance(!s->get_advance());
    s->update(info.tm_wday, info.tm_hour * 100 + info.tm_min);
  }

  redirect_to_root();
}


void handlePlusOne()
{
  check_authentication();

  Schedule *s;
  if (s = get_schedule())
  {
    struct tm info;
    getLocalTime(&info);

    s->set_plus_one(!s->get_plus_one());
    s->update(info.tm_wday, info.tm_hour * 100 + info.tm_min);
  }

  redirect_to_root();
}


void handleAuto()
{
  check_authentication();

  Schedule *s;
  if (s = get_schedule())
  {
    struct tm info;
    getLocalTime(&info);

    s->set_off(false);
    s->update(info.tm_wday, info.tm_hour * 100 + info.tm_min);
  }

  redirect_to_root();
}


void handleOff()
{
  check_authentication();

  Schedule *s;
  if (s = get_schedule())
  {
    struct tm info;
    getLocalTime(&info);

    s->set_off(true);
    s->update(info.tm_wday, info.tm_hour * 100 + info.tm_min);
  }

  redirect_to_root();
}
