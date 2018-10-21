#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <ESPmDNS.h>


#define CHAN1_PIN 26
#define CHAN2_PIN 25

const char *ssid = "Kingdom";
const char *password = "37b8ecd9a6";
const char* const DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
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

  public:
  bool advance;
  String location;

  
  Schedule(char *part_name, int channel, String location)
  {
    this->part_name = part_name;
    this->state = false;
    this->advance = false;
    this->sched_state = false;
    this->channel = channel;
    this->location = location;
  }


  void begin()
  {
    load();
  }


  void update(uint8_t day, uint16_t now)
  {
    this->day = day;
    this->now = now;
    
    bool now_state = false;
    
    for (int i = 0; i < 4; ++i)
    {
      if (now_state = (now >= this->sched[day][i*2] && now < this->sched[day][i*2+1]))
        break;
    }
  
    if (now_state != this->sched_state)
    {
      this->sched_state = now_state;
      this->advance = false;
    }
  
    if (this->advance)
      now_state = !now_state;

    this->state = now_state;
  }


  bool get_state()
  {
    return this->state;
  }


  int get_channel()
  {
    return this->channel;
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
      t += DAYS[d];
      t += "</th>";

      for (int e = 0; e < 8; ++e)
      {
        t += "<td><input type=\"time\" name=\"";
        t += DAYS[d];
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
        a += DAYS[d];
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

        if (start == SCHED_NULL || end == SCHED_NULL || start > end)
          start = end = SCHED_NULL;

        this->sched[d][e*2] = start;
        this->sched[d][e*2+1] = end;
      }

    store();
  }


  bool next_state_change(uint16_t& day, uint16_t& tm)
  {
    if (this->state)
    {
      // Look for the next off
      uint16_t now = this->now;
      for (int d = 0; d <= 7; ++d)
      {
        int di = (this->day + d) % 7;
  
        uint16_t next = SCHED_NULL;
        for (int i = 0; i < 4; ++i)
        {
          uint16_t e = this->sched[di][i*2+1];
          if (this->sched[di][i*2] < e && e > now && e < next)
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

      return false;
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
          if (e < this->sched[di][i*2+1] && e > now && e < next)
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

      return false;
    }
  }


  private:

  void load()
  {
    Preferences preferences;
    preferences.begin(this->part_name, false);
    for (int day = 0; day < 7; ++day)
    {
      this->sched[day] = (uint16_t*)calloc(SCHED_SIZE, 1);
      
      if (preferences.getBytes(DAYS[day], this->sched[day], SCHED_SIZE) != SCHED_SIZE)
      {
        Serial.print("Schedule for ");
        Serial.print(DAYS[day]);
        Serial.print(" failed to load.\n");
        for (int i = 0; i < 8; ++i)
          this->sched[day][i] = SCHED_NULL;
      }
    }
    preferences.end();
  }


  void store()
  {
    Preferences preferences;
    preferences.begin(this->part_name, false);
    for (int day = 0; day < 7; ++day)
      if (preferences.putBytes(DAYS[day], this->sched[day], SCHED_SIZE) != SCHED_SIZE)
        Serial.println("Failed to store schedule");
    preferences.end();
  }
};


class StateController
{
  bool cstate;
  bool first;
  int pin;

  public:
  
  StateController(int outPin)
  {
    this->cstate = false;
    this->first = true;
    this->pin = outPin;
  }


  void begin()
  {
    pinMode(this->pin, OUTPUT);
  }


  void update(bool state)
  {
    if (this->cstate != state || this->first)
    {
      this->cstate = state;
      if (this->cstate)
      {
        Serial.println("On");
        digitalWrite(this->pin, HIGH);
      }
      else
      {
        Serial.println("Off");
        digitalWrite(this->pin, LOW);
      }
    }

    this->first = false;
  }
};


Schedule sched1("schedule", 1, "First floor heating");
Schedule sched2("schedule2", 2, "Ground floor heating");
StateController state1(CHAN1_PIN);
StateController state2(CHAN2_PIN);


void setup()
{
  Serial.begin(115200);
  Serial.println();


  WiFi.begin(ssid, password);
  Serial.print("WiFi.");
  while (WiFi.status() != WL_CONNECTED)
  {
      delay(500);
      Serial.print(".");
  }
  Serial.println("Connected");


  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", "GMT0BST,M3.5.0,M10.5.0", 1);
  tzset();


  sched1.begin();
  state1.begin();
  sched2.begin();
  state2.begin();
  

  if (MDNS.begin("chctrl"))
    Serial.println("MDNS responder started");


  server.on("/", handleRoot);
  server.on("/editschedule", editSchedule);
  server.on("/saveschedule", saveSchedule);
  server.begin();
}


void loop()
{
  struct tm info;
  getLocalTime(&info);

  uint16_t now = info.tm_hour * 100 + info.tm_min;

  sched1.update(info.tm_wday, now);
  state1.update(sched1.get_state());

  sched2.update(info.tm_wday, now);
  state2.update(sched2.get_state());
  
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
  if (sched.advance)
    t += " (advance)";
  t += ".";
  uint16_t nd, nt;
  if (sched.next_state_change(nd, nt))
  {
    t += " Next ";
    t += sched.get_state() ? "off " : "on ";
    if (nd != info.tm_wday || nt < info.tm_hour * 100 + info.tm_min)
    {
      t += DAYS[nd];
      t += " ";
    }
    t += format_time(nt);
    t += ".";
  }
  t += "<div><a href=\"?channel=";
  t += sched.get_channel();
  t += "\">Advance</a></div>";
  t += "<div><a href=\"editschedule?channel=";
  t += sched.get_channel();
  t += "\">Edit schedule</a></div>";
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


void handleRoot()
{
  struct tm info;
  getLocalTime(&info);

  Schedule *s;
  if (s = get_schedule())
  {
    s->advance = !s->advance;
    s->update(info.tm_wday, info.tm_hour * 100 + info.tm_min);
  }
  
  String t;
  t += "<!DOCTYPE html>\
  <html>\
  <head>\
    <meta charset=\"UTF-8\">\
    <title>Central Heating Home</title>\
  </head>\
  <body>\
    <div><a href=\"\\\">Refresh</a></div>\
    <h1>Central Heating Home</h1>";

  char buf[32];
  snprintf(buf, 32, "<p>%s %02d:%02d:%02d</p>", DAYS[info.tm_wday], info.tm_hour, info.tm_min, info.tm_sec);
  t += buf;

  t += schedule_summary(sched1, info);
  t += schedule_summary(sched2, info);

  t += "</body></html>";

  server.send(200, "text/html", t);
}


void editSchedule()
{
  String t;

  t += 
  "<!DOCTYPE html>\
  <html>\
  <head>\
    <meta charset=\"UTF-8\">\
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


void saveSchedule()
{
  String t;
  t += "<!DOCTYPE html>\
  <html>\
  <head>\
    <meta charset=\"UTF-8\">\
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
