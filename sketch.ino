// C W Dissanayake - 200005601938

// Initially set time using wifi
// Then using counter to update the time

#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHTesp.h"
#include "time.h"

// time sync
#define NTP_SERVER    "pool.ntp.org"
#define UTC_OFFSET     19800
#define UTC_OFFSET_DST 0

// push buttons
#define CANCEL 13
#define OK 27
#define DOWN 26
#define UP 25

// menu modes
uint8_t current_mode = 0;
uint8_t max_modes = 4;

enum mode {
  SET_TIME,
  SET_ALARM_1,
  SET_ALARM_2,
  DISABLE_ALARMS,
};

String mode_labels[] = {"1-Set Time", "2-Set Alarm 1", "3-Set Alarm 2", "4-Disable Alarms"};

// buzzer and LEDs
#define LED_ALARM 18
#define LED_TEMP 4
#define BUZZER 19

// buzzer notes
#define NUM_NOTES 8
#define C 262
#define D 294
#define E 330
#define F 349
#define G 392
#define A 440
#define B 494
#define C_H 523

int notes[] = {C, D, E, F, G, A, B, C_H};

// DHT sensor
#define DHT 15
DHTesp dhtSensor;

// OLED screen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDR 0x3C

Adafruit_SSD1306 disp(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Device time data
struct Time {
  uint16_t days;
  uint16_t hours;
  uint16_t minutes;
  uint16_t seconds;
};

Time device_time = {0, 0, 0, 0};

uint64_t time_now = 0;
uint64_t time_last = 0;

// Alarm data
#define NUM_ALARMS 2
boolean alarms_enabled = false;

struct Alarm {
  uint16_t hour;
  uint16_t minute;
  boolean triggered;
};

Alarm alarm_1 = {0, 0, true};
Alarm alarm_2 = {0, 0, true};

struct Alarm alarms[] = {alarm_1, alarm_2};

void print_line(String text, uint8_t text_size = 1, uint8_t cursor_x = 0, uint8_t cursor_y = 0, boolean inverted = false) {
  disp.clearDisplay();

  if (inverted) disp.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else disp.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

  disp.setTextSize(text_size);
  disp.setCursor(cursor_x, cursor_y);

  disp.println(text);

  disp.display();
}

void print_time_now() {
  char buffer[10];
  sprintf(buffer, "%02u:%02u:%02u", device_time.hours % 24, device_time.minutes % 60, device_time.seconds % 60);

  print_line(buffer, 2);
}

void update_time(void) {
  time_now = millis() / 1000;
  device_time.seconds = time_now - time_last;

  if (device_time.seconds > 59) {
    device_time.minutes++;
    time_last += 60;
  }

  if (device_time.minutes > 59) {
    device_time.minutes = 0;
    device_time.hours++;
  }

  if (device_time.hours > 23) {
    device_time.days++;
    device_time.hours = 0;

    // reset alarm triggered state
    // in order to ring on next day at same time
    alarm_1.triggered = false;
    alarm_2.triggered = false;
  }
}

void update_time_w_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarms_enabled) {
    for (auto i = 0; i < NUM_ALARMS; i++) {
      if (!alarms[i].triggered && alarms[i].hour == device_time.hours && alarms[i].minute == device_time.minutes) {
        ring_alarm();
        alarms[i].triggered = true;
      }
    }
  }
}

void set_time_w_wifi() {
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  print_line("Syncing time..");
  Serial.print("INFO: Retrieving time");

  // wait for time to be synced
  while (time(nullptr) <= 100000) {
    Serial.print(".");
    delay(1000);
  }

  struct tm* timeinfo;
  time_t now;
  time(&now);

  timeinfo = localtime(&now);
  Serial.println(timeinfo, "\n%A, %B %d %Y %H:%M:%S");

  device_time.hours = timeinfo->tm_hour;
  device_time.minutes = timeinfo->tm_min;
  // syncing seconds is ok if it happens within a minute

  Serial.println("INFO: Time has been synced");
}

void ring_alarm(void) {
  print_line("TAKE YOUR MEDICINE!", 2);

  digitalWrite(LED_ALARM, HIGH);

  bool is_cancelled = false;

  while (!is_cancelled) {
    for (auto i = 0; i < NUM_NOTES; i++) {

      if (digitalRead(CANCEL) == LOW) {
        delay(200);

        is_cancelled = true;
        break;
      };

      // Delay here causes a problem
      // Button press might not register
      // need to press and hold for a little
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_ALARM, LOW);
}

void go_to_menu() {
  Serial.println("INFO: Navigated to menu");

  while (true) {
    print_line(mode_labels[current_mode], 2);

    uint8_t pressed = wait_for_button_press();

    if (pressed == CANCEL) {
      delay(200);
      break;
    }

    if (pressed == UP) {
      delay(200);
      current_mode++;
      current_mode %= max_modes;
    }

    if (pressed == DOWN) {
      delay(200);

      // to prevent underflow
      if (current_mode - 1 < 0) {
        current_mode = max_modes - 1;
      }
      else current_mode--;
    }

    if (pressed == OK) {
      delay(200);
      run_mode(current_mode);
    }
  }

  current_mode = 0;
}

int wait_for_button_press(void) {
  while (true) {
    if (digitalRead(UP) == LOW) {
      delay(200);
      return UP;
    }
    if (digitalRead(DOWN) == LOW) {
      delay(200);
      return DOWN;
    }
    if (digitalRead(OK) == LOW) {
      delay(200);
      return OK;
    }
    if (digitalRead(CANCEL) == LOW) {
      delay(200);
      return CANCEL;
    }
  }
}

void run_mode(uint8_t current_mode) {

  if (current_mode == SET_TIME) {
    Serial.println("INFO: Navigated to set time mode");

    Time *input_time = get_time_input();

    if (input_time == NULL) {
      print_line("Time not set!", 2);
      delay(1500);
    }
    else {
      device_time.hours = input_time->hours;
      device_time.minutes = input_time->minutes;

      print_line("Time has been set!", 2);
      delay(1500);
    }

    free(input_time);
  }

  else if (current_mode == SET_ALARM_1 || current_mode == SET_ALARM_2) {
    Serial.println("INFO: Navigated to set alarm mode");
    Time* input_time = get_time_input();

    if (input_time == NULL) {
      print_line("Alarm not set!", 2);
      delay(1500);
    }
    else {
      alarms[current_mode - 1].hour = input_time->hours;
      alarms[current_mode - 1].minute = input_time->minutes;

      // triggered is true by default
      // to prevent 0,0 default alarm from ringing upon setting alarms_enabled = true;
      alarms[current_mode - 1].triggered = false;

      alarms_enabled = true;

      print_line("Alarm has been set!", 2);
      delay(1500);
    }
    free(input_time);
  }

  else if (current_mode == DISABLE_ALARMS) {
    alarms_enabled = false;
    print_line("All alarms disabled!", 2);
    delay(1500);
  }
}

Time *get_time_input(void) {
  Time *temp_time = new Time{0, 0, 0, 0};
  if (temp_time == NULL) return NULL;

  bool is_cancelled = false;

  // hour
  while (true) {
    print_line("Hours:" + String(temp_time->hours), 2);

    int pressed = wait_for_button_press();

    if (pressed == CANCEL) {
      delay(200);
      is_cancelled = true;
      break;
    }

    if (pressed == UP) {
      delay(200);
      temp_time->hours++;
      temp_time->hours %= 24;
    }

    else if (pressed == DOWN) {
      delay(200);
      if (temp_time->hours - 1 < 0) {
        temp_time->hours = 23;
      }
      else temp_time->hours--;
    }

    else if (pressed == OK) {
      delay(200);
      break;
    }
  }

  if (is_cancelled) {
    free(temp_time);
    return NULL;
  }

  // minute
  while (true) {
    print_line("Minutes:" + String(temp_time->minutes), 2);

    int pressed = wait_for_button_press();
    if (pressed == CANCEL) {
      delay(200);
      break;
    }

    if (pressed == UP) {
      delay(200);
      temp_time->minutes++;
      temp_time->minutes %= 60;
    }

    else if (pressed == DOWN) {
      delay(200);
      if (temp_time->minutes - 1 < 0) {
        temp_time->minutes = 59;
      }
      else temp_time->minutes--;
    }

    else if (pressed == OK) {
      delay(200);
      return temp_time;
    }
  }

  free(temp_time);
  return NULL;
}

void check_temp_n_humidity() {
  TempAndHumidity  data = dhtSensor.getTempAndHumidity();
  String message = "";
  bool all_good = true;

  if (data.temperature > 30) {
    all_good = false;
    message += "TEMP HIGH \n";
  }
  else if (data.temperature < 15) {
    all_good = false;
    message += "TEMP LOW \n";
  }

  if (data.humidity > 60) {
    all_good = false;
    message += "HUMIDITY HIGH \n";
  }

  if (message.length() > 0) {
    print_line(message, 1, 0, 20);
    delay(300);
  }

  if (!all_good) digitalWrite(LED_TEMP, HIGH);
  else digitalWrite(LED_TEMP, LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_ALARM, OUTPUT);
  pinMode(LED_TEMP, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(CANCEL, INPUT);
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(OK, INPUT);

  dhtSensor.setup(DHT, DHTesp::DHT22);

  if (!disp.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println("FATAL: SSD1306 ALLOCATION FAILED");
    for (;;);
  }

  disp.display();
  delay(2000);

  print_line("MEDIBOX", 2, 20, 26);
  delay(2000);

  WiFi.begin("Wokwi-GUEST", "", 6);
  Serial.print("INFO: Connecting to Wi-Fi");
  print_line("Connecting to Wi-Fi..");

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println("\nINFO: Connected to Wi-Fi");
  print_line("Connected to Wi-Fi!");
  delay(1000);

  set_time_w_wifi();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("INFO: Disconnected from Wi-Fi");
}

void loop() {
  update_time_w_check_alarm();
  check_temp_n_humidity();

  if (digitalRead(OK) == LOW) {
    delay(200);
    go_to_menu();
  }
}

