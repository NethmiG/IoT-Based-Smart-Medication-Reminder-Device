// Project By Pathirana P.N.A (220449F)

#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

// Define OLED parameters
#define SCREEN_WIDTH 128 //OLED display width, in pixels
#define SCREEN_HEIGHT 64 //OLED display height, in pixels
#define OLED_RESET -1 //Reset pin
#define SCREEN_ADDRESS 0x3C

#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

// Declare Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

// Default offset
int utc_offset = 19800; // For Sri Lanka

unsigned long timeNow = 0;
unsigned long timeLast = 0;
unsigned long snoozeStartTime = 0;

bool alarm_enabled = true;
int n_alarms = 0;
int alarm_hours[] = {0, 0};
int alarm_minutes[] = {0, 0};
bool alarm_triggered[] = {false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 5;
String modes[] = {"1- Set Time Zone", "2- Set Alarm 1", "3- Set Alarm 2", "4- View Active Alarms", "5- Disable Alarms"};

// Functions
void print_line(String text, int column, int row, int text_size);
void update_time_with_check_alarms(void);
void go_to_menu();
void check_temp();
int wait_for_button_press();
void run_mode(int mode);

void setup() {
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't procedd, loop forever
  }

  //Show initial display buffer contents on the screen --
  //the library initializes this with an Adafruit splash screen
  display.display();
  delay(500);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);
  }

  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);

  configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER);

  //Clear the buffer
  display.clearDisplay();

  print_line("Welcome to Medibox", 10, 20, 2);
  delay(1000);
  display.clearDisplay();
}

void loop() {
  // put your main code here, to run repeatedly:
  update_time_with_check_alarms();
  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }
  check_temp();
}

void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row); //(column,row )
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

  // Ring the buzzer
  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        display.clearDisplay();
        print_line("Alarm Dismissed!", 0, 0, 2);
        digitalWrite(LED_1, LOW);
        delay(1000);
        return; // Exit the function completely
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
    // Calculate the new alarm time (5 minutes later)
    int snooze_minutes = minutes + 5;
    int snooze_hours = hours;

    // Handle hour rollover
    if (snooze_minutes >= 60) {
      snooze_minutes -= 60;
      snooze_hours = (snooze_hours + 1) % 24;
    }

    // Update the current alarm with the new snooze time
    alarm_hours[alarm_index] = snooze_hours;
    alarm_minutes[alarm_index] = snooze_minutes;

    // Reset the triggered state so it will ring again
    alarm_triggered[alarm_index] = false;

    // Clear the display and show snooze confirmation
    display.clearDisplay();
    print_line("Snoozed to", 0, 0, 2);
    print_line(String(snooze_hours) + ":" +
               (snooze_minutes < 10 ? "0" : "") + String(snooze_minutes),
               0, 20, 2);

    delay(1000);
  }
}
void update_time_with_check_alarms(void) {
  update_time();
  print_time_now();

  // Reset all alarms at midnight
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
  int temp_offset_hours = utc_offset / 3600;  // Convert stored offset to hours
  int temp_offset_minutes = (utc_offset % 3600) / 60;  // Extract minutes

  // Set UTC Offset (Hours)
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

  // Set UTC Offset (Minutes)
  while (true) {
    display.clearDisplay();
    print_line("UTC Offset (M): " + String(temp_offset_minutes), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_offset_minutes = (temp_offset_minutes == 30) ? 0 : 30;  // Toggle between 0 and 30
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_offset_minutes = (temp_offset_minutes == 30) ? 0 : 30;  // Toggle between 0 and 30
    }
    else if (pressed == PB_OK) {
      delay(200);
      utc_offset = (temp_offset_hours * 3600) + (temp_offset_minutes * 60); // Convert to seconds
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
      delay(100);  // Wait for cancel button press
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

  // Loop through alarms for deletion
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
          alarm_hours[i] = -1;  // Mark alarm as deleted
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

  // Check Temperature Warning
  if (data.temperature < 24) {
    print_line("WARNING: TEMP LOW!", 0, 40, 1);
    warning = true;
  }
  else if (data.temperature > 32) {
    print_line("WARNING: TEMP HIGH!", 0, 40, 1);
    warning = true;
  }

  // Check Humidity Warning
  if (data.humidity < 65) {
    print_line("WARNING: HUMIDITY LOW!", 0, 50, 1);
    warning = true;
  }
  else if (data.humidity > 80) {
    print_line("WARNING: HUMIDITY HIGH!", 0, 50, 1);
    warning = true;
  }

  // If a warning is triggered, turn on LED or buzzer
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

