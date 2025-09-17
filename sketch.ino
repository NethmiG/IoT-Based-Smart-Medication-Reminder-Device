#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>

// Define OLED parameters
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER      5
#define LED_1      15
#define PB_CANCEL  34
#define PB_OK      32
#define PB_UP      33
#define PB_DOWN    35
#define DHTPIN     12
#define LDR_PIN    36
#define SERVO_PIN  18

// Wi-Fi / MQTT
const char* ssid     = "Wokwi-GUEST";
const char* wifiPass = "";
WiFiClient     espClient;
PubSubClient   mqttClient(espClient);

// Display & sensors
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
Servo shutter;

// Time & alarms
#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     19800 // Sri Lanka
#define UTC_OFFSET_DST 0

int days=0, hours=0, minutes=0, seconds=0;
unsigned long timeNow=0, timeLast=0, snoozeStartTime=0;

// Default offset
int utc_offset = 19800; // For Sri Lanka

// LDR sampling & aggregation
unsigned long ts_ms      = 5000;    // sample every 5s
unsigned long tu_ms      = 120000;  // publish every 120s
unsigned long lastSample = 0, lastPublish=0;
float        sumB        = 0;
uint16_t     cntB        = 0;       // unsigned 16-bit integer(only no-negative values)
float avgIntensity = 0;
float T = 0;

// Servo control params
float theta_offset = 30.0;   // minimum angle (°)
float gammaFactor  = 0.75;   // control factor
float Tmed         = 30.0;   // ideal storage temp (°C)

// Alarm music notes
int n_notes = 8;
int C=262, D=294, E=330, F=349, G=392, A=440, B=494, C_H=523;
int notes[]={C,D,E,F,G,A,B,C_H};

// Menu & alarms
bool   alarm_enabled = true;
int    n_alarms = 0;
int    alarm_hours[]   = {0,0}, alarm_minutes[]  = {0,0};
bool   alarm_triggered[] = {false,false};
int    current_mode=0, max_modes=5;
String modes[]={"1- Set Time Zone","2- Set Alarm 1","3- Set Alarm 2","4- View Active Alarms","5- Disable Alarms"};

// Prototypes
void print_line(String,int,int,int);
void update_time_with_check_alarms();
void go_to_menu();
void check_temp();
int wait_for_button_press();
void run_mode(int);
void receiveCallback(char*,byte*,unsigned int);
void connectToBroker();
void rotateServo();

void setup() {
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  Serial.begin(115200); // Increased baud rate for faster debugging
  dhtSensor.setup(DHTPIN, DHTesp::DHT22); // Use DHT22 for Wokwi
  
  shutter.attach(SERVO_PIN, 500, 2400); // Explicitly set pulse width range
  analogReadResolution(12);  // reading analog values at 12-bit resolution
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 failed"); for(;;);
  }
  display.display(); delay(500);

  // Setting the servo to minimum position
  shutter.write(0); 
  delay(1000);

  WiFi.begin(ssid, wifiPass);
  while(WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI",0,0,2);
  }
  display.clearDisplay();
  print_line("Connected to WIFI",0,0,2);

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  display.clearDisplay();
  print_line("Welcome to Medibox",10,20,2);
  delay(1000);
  display.clearDisplay();

  mqttClient.setServer("broker.hivemq.com", 1883);
  mqttClient.setCallback(receiveCallback);    
}

void loop() {
  unsigned long now = millis();
  if(!mqttClient.connected()){
    connectToBroker();
  } 
  mqttClient.loop();

  // Measure temperature and publish
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  T = data.temperature;
  mqttClient.publish("MEDIBOX-TEMP", String(T, 1).c_str(), true);
  delay(200);
  rotateServo();

  // LDR sampling
  if(now - lastSample >= ts_ms) {
    lastSample = now;
    float b = (4095.0 - analogRead(LDR_PIN)) / 4095.0;
    sumB += b;
    cntB++;
    Serial.print("LDR reading: "); 
    Serial.println(b, 2);
  }

  // Sending the average value
  if(now - lastPublish >= tu_ms && cntB > 0) {
    lastPublish = now;
    avgIntensity = sumB / cntB;
    mqttClient.publish("MEDIBOX-LDR", String(avgIntensity, 2).c_str(), true);
    delay(200);
    rotateServo();
    sumB = 0; cntB = 0;
  }

  update_time_with_check_alarms();
  if(digitalRead(PB_OK) == LOW) { delay(200); go_to_menu(); }
  check_temp();
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    display.clearDisplay();
    print_line("MQTT connecting…", 0, 0, 2);
    if (mqttClient.connect("ESP32-465778289")) {
      display.clearDisplay();
      print_line("MQTT Connected", 0, 20, 2);
      mqttClient.subscribe("LDR-TS");
      mqttClient.subscribe("LDR-TU");
      mqttClient.subscribe("SERVO-THETA");
      mqttClient.subscribe("SERVO-GAMMA");
      mqttClient.subscribe("SERVO-TMED");
    } else {
      Serial.print("Failed ");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  static char buf[16];
  if (length >= sizeof(buf)) length = sizeof(buf) - 1;
  memcpy(buf, payload, length);
  buf[length] = '\0';

  if (strcmp(topic, "LDR-TS") == 0) {
    unsigned long ts = strtoul(buf, nullptr, 10) * 1000UL;
    ts_ms = constrain(ts, 1000UL, 60000UL); // 1–60 seconds
    Serial.print("Received t_s: "); Serial.println(ts_ms / 1000.0);
  }
  else if (strcmp(topic, "LDR-TU") == 0) {
    unsigned long tu = strtoul(buf, nullptr, 10) * 1000UL;
    tu_ms = constrain(tu, 30000UL, 600000UL); // 30 seconds–10 minutes
    Serial.print("Received t_u: "); Serial.println(tu_ms / 1000.0);
  }
  else if (strcmp(topic, "SERVO-THETA") == 0) {
    float theta = atof(buf);
    theta_offset = constrain(theta, 0.0, 120.0); // 0–120 degrees
    Serial.print("Received theta_offset: "); Serial.println(theta_offset);
  }
  else if (strcmp(topic, "SERVO-GAMMA") == 0) {
    float gamma = atof(buf);
    gammaFactor = constrain(gamma, 0.0, 1.0); // 0–1
    Serial.print("Received gammaFactor: "); Serial.println(gammaFactor);
  }
  else if (strcmp(topic, "SERVO-TMED") == 0) {
    float tmed = atof(buf);
    Tmed = constrain(tmed, 10.0, 40.0); // 10–40°C
    Serial.print("Received Tmed: "); Serial.println(Tmed);
  }
  rotateServo();
}

void rotateServo(){
  // Servo angle calculation
  float I = avgIntensity;
  float ts = ts_ms / 1000.0;
  float tu = tu_ms / 1000.0;
  float factor = log(ts/tu);
  float theta = theta_offset + (180.0 - theta_offset) * I * gammaFactor * factor * (T / Tmed);
  theta = constrain(theta, 0.0, 180.0);

  display.clearDisplay();
  print_line("Servo angle",0,0,1);
  print_line(String((int)theta),0,20,3);
  delay(1000);
  shutter.write((int)theta);
}

// Functions from the previous assignment
void print_line(String text, int x, int y, int s) {
  display.setTextSize(s);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.println(text);
  display.display();
}

void print_time_now() {
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
}

void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);
}

void ring_alarm(int alarm_index) {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);
  print_line("Snooze?", 0, 40, 2);
  display.display();
  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        display.clearDisplay();
        print_line("Alarm Dismissed!", 0, 0, 2);
        digitalWrite(LED_1, LOW);
        delay(1000);
        return;
      } else if (digitalRead(PB_OK) == LOW) {
        display.clearDisplay();
        print_line("Snoozed for 5 min!", 0, 0, 2);
        digitalWrite(LED_1, LOW);
        delay(1000);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  if (break_happened) {
    int snooze_minutes = minutes + 5;
    int snooze_hours = hours;

    if (snooze_minutes >= 60) {
      snooze_minutes -= 60;
      snooze_hours = (snooze_hours + 1) % 24;
    }

    alarm_hours[alarm_index] = snooze_hours;
    alarm_minutes[alarm_index] = snooze_minutes;
    alarm_triggered[alarm_index] = false;

    display.clearDisplay();
    print_line("Snoozed to", 0, 0, 2);
    print_line(String(snooze_hours) + ":" +
               (snooze_minutes < 10 ? "0" : "") + String(snooze_minutes),
               0, 20, 2);
    delay(1000);
  }
}

void update_time_with_check_alarms() {
  update_time();
  print_time_now();

  if (hours == 0 && minutes == 0 && seconds == 0) {
    for (int i = 0; i < n_alarms; i++) {
      alarm_triggered[i] = false;
    }
  }

  if (alarm_enabled == true) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        alarm_triggered[i] = true;
        ring_alarm(i);
      }
    }
  }
}

int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

void go_to_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode <= 0) {
        current_mode = max_modes - 1;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

void set_time() {
  int temp_offset_hours = UTC_OFFSET / 3600;
  int temp_offset_minutes = (UTC_OFFSET % 3600) / 60;

  while (true) {
    display.clearDisplay();
    print_line("UTC Offset (H): " + String(temp_offset_hours), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_offset_hours += 1;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_offset_hours -= 1;
    }
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      return;
    }
  }

  while (true) {
    display.clearDisplay();
    print_line("UTC Offset (M): " + String(temp_offset_minutes), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_offset_minutes = (temp_offset_minutes == 30) ? 0 : 30;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_offset_minutes = (temp_offset_minutes == 30) ? 0 : 30;
    }
    else if (pressed == PB_OK) {
      delay(200);
      utc_offset = (temp_offset_hours * 3600) + (temp_offset_minutes * 60);
      configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      return;
    }
  }

  display.clearDisplay();
  print_line("Time is set", 0, 0, 2);
  delay(1000);
}

void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  n_alarms = n_alarms + 1;
  delay(1000);
}

void view_active_alarms() {
  display.clearDisplay();

  if (n_alarms == 0) {
    print_line("No Alarms Set", 0, 0, 2);
    while (digitalRead(PB_CANCEL) == HIGH) {
      delay(100);
    }
    return;
  }

  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();

    int y_offset = 0;

    if (n_alarms == 1) {
      print_line("Alarm 1", 10, y_offset, 1);
      print_line(String(alarm_hours[0]) + ":" + (alarm_minutes[0] < 10 ? "0" : "") + String(alarm_minutes[0]), 10, y_offset + 15, 2);
      y_offset += 30;
    }
    else if (n_alarms == 2) {
      print_line("Alarm 1", 10, y_offset, 1);
      print_line(String(alarm_hours[0]) + ":" + (alarm_minutes[0] < 10 ? "0" : "") + String(alarm_minutes[0]), 10, y_offset + 15, 2);
      y_offset += 30;
      print_line("Alarm 2", 10, y_offset, 1);
      print_line(String(alarm_hours[1]) + ":" + (alarm_minutes[1] < 10 ? "0" : "") + String(alarm_minutes[1]), 10, y_offset + 15, 2);
    }

    display.display();
  }
}

void delete_alarm() {
  display.clearDisplay();

  if (n_alarms == 0) {
    print_line("No Alarms Set", 0, 0, 2);
    while (digitalRead(PB_CANCEL) == HIGH) {
      delay(100);
    }
    return;
  }

  for (int i = 0; i < n_alarms; i++) {
    if (alarm_hours[i] >= 0 && alarm_hours[i] < 24 && alarm_minutes[i] >= 0 && alarm_minutes[i] < 60) {
      while (true) {
        display.clearDisplay();
        print_line("Alarm " + String(i + 1) + ": ", 0, 0, 2);
        String alarmText = String(alarm_hours[i]) + ":" +
                           (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]);
        print_line(alarmText, 0, 20, 2);
        print_line("Delete?", 0, 40, 2);
        display.display();

        int pressed = wait_for_button_press();
        if (pressed == PB_OK) {
          delay(200);
          alarm_hours[i] = -1;
          alarm_minutes[i] = -1;
          alarm_triggered[i] = false;

          display.clearDisplay();
          print_line("Alarm " + String(i + 1) + " Deleted!", 0, 0, 2);
          n_alarms = n_alarms - 1;
          display.display();
          delay(1500);
          break;
        }
        else if (pressed == PB_CANCEL) {
          delay(200);
          break;
        }
      }
    }
  }
}

void run_mode(int mode) {
  if (mode == 0) {
    set_time();
  }
  else if (mode == 1 || mode == 2) {
    set_alarm(mode - 1);
  }
  else if (mode == 3) {
    view_active_alarms();
  }
  else if (mode == 4) {
    delete_alarm();
  }
}

void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();

  display.clearDisplay();
  String tempText = "Temp: " + String(data.temperature) + "C";
  String humidityText = "Humidity: " + String(data.humidity) + "%";

  print_line(tempText, 0, 0, 1);
  print_line(humidityText, 0, 20, 1);

  bool warning = false;

  if (data.temperature < 24) {
    print_line("WARNING: TEMP LOW!", 0, 40, 1);
    warning = true;
  }
  else if (data.temperature > 32) {
    print_line("WARNING: TEMP HIGH!", 0, 40, 1);
    warning = true;
  }

  if (data.humidity < 65) {
    print_line("WARNING: HUMIDITY LOW!", 0, 50, 1);
    warning = true;
  }
  else if (data.humidity > 80) {
    print_line("WARNING: HUMIDITY HIGH!", 0, 50, 1);
    warning = true;
  }

  if (warning) {
    digitalWrite(LED_1, HIGH);
    tone(BUZZER, 1000);
    delay(100);
    noTone(BUZZER);
  } else {
    digitalWrite(LED_1, LOW);
  }

  display.display();
  delay(1000);
}