/*
 * Project Coop_Control
 * Description:
 * Author:
 * Date:
 */

#include "Particle.h"
#include "linear_actuator.h"

PRODUCT_ID(15773);
PRODUCT_VERSION(4);

SerialLogHandler logHandler(115200, LOG_LEVEL_ALL);

const int HUTCH_DOOR_OPEN_PIN = D0;
const int HUTCH_DOOR_CLOSE_PIN = D1;
const int HUTCH_DOOR_BUTTON_PIN = D2;

// 15 seconds to fully open/close actuator
const int HUTCH_DOOR_DURATION_SECS = 15;

LinearActuator HutchDoor(HUTCH_DOOR_DURATION_SECS, HUTCH_DOOR_OPEN_PIN, HUTCH_DOOR_CLOSE_PIN, LinearActuatorState::OPEN);

// Millis of last time button was pressed to debounce
unsigned int last_button_press = 0;
int last_button_state = LOW;

system_tick_t loopTimeCheckTick = 0;

String door_state = "";

// Converts door state to string
void updateDoorState() {
  switch (HutchDoor.getState()) {
    case LinearActuatorState::OPEN:
      door_state = "OPEN";
      break;
    case LinearActuatorState::CLOSED:
      door_state = "CLOSED";
      break;
    case LinearActuatorState::OPENING:
      door_state = "OPENING";
      break;
    case LinearActuatorState::CLOSING:
      door_state = "CLOSING";
      break;
  }
  Log.info("Door state changed to %s", door_state.c_str());
}

// Cloud function to open/close hutch
int setDoor(String state) {
  state = state.toUpperCase();
  Log.info("Setting door to %s", state.c_str());
  if (state == "OPEN") {
    HutchDoor.open();
    return 1;
  }
  if (state == "CLOSE") {
    HutchDoor.close();
    return 1;
  }
  return -1;
}

struct door_schedule_t {
  int open_hour;
  int open_min;
  int close_hour;
  int close_min;
}

door_schedule = door_schedule_t {
  .open_hour = 0,
  .open_min = 0,
  .close_hour = 0,
  .close_min = 0,
};

int setOpenTime(String time) {
  if (time.length() != 5) {
    return -1;
  }
  int hour = time.substring(0,1).toInt();
  int minute = time.substring(3,4).toInt();
  if (hour < 0 || hour > 24 || minute < 0 || minute > 59) {return -1;}
  Log.info("Setting door open time to %i:%i.", hour, minute);
  door_schedule.open_hour = hour;
  door_schedule.open_min = minute;
  return 1;
}

int setCloseTime(String time) {
  if (time.length() != 5) {
    return -1;
  }
  int hour = time.substring(0,1).toInt();
  int minute = time.substring(3,4).toInt();
  if (hour < 0 || hour > 24 || minute < 0 || minute > 59) {return -1;}
  Log.info("Setting door close time to %i:%i.", hour, minute);
  door_schedule.close_hour = hour;
  door_schedule.close_min = minute;
  return 1;
}

void setup() {
  // Setup button to control door manually
  pinMode(HUTCH_DOOR_BUTTON_PIN, INPUT);
  // This updates the global string variable door_state whenver the LinearActuator state changes
  HutchDoor.registerOnChange(updateDoorState);

  // Particle specific from here on:

  Particle.connect();
  // Set time zone to eastern
  Time.zone(-4);  
  // Reg variable to see door state from the cloud
  Particle.variable("hutch_door_state", door_state);
  // Reg function to control door state from the cloud
  Particle.function("set_door", setDoor);
  // Set open/closed times
  Particle.function("set_open_time", setOpenTime);
  Particle.function("set_close_time", setCloseTime);
}

void loop() {
  // Check button for presses
  int button_state = digitalRead(HUTCH_DOOR_BUTTON_PIN);
  if (button_state != last_button_state) {
    last_button_press = System.millis();
  }
  if ((System.millis() - last_button_press) > 50) {
    if (button_state != last_button_state) {
      last_button_state = button_state;
      // If button was pressed toggle door state
      if (button_state == HIGH) {
        Log.info("Toggling door state due to button press...");
        HutchDoor.toggle();
      }
    }
  }

  HutchDoor.loop();

  // Rest only runs once per minute
  // Every minute check time and position
  if (System.millis() - loopTimeCheckTick < 60000) {
      return;
  }
  loopTimeCheckTick = System.millis();

  if (Time.hour() == door_schedule.open_hour && Time.minute() == door_schedule.open_min) {
    Log.info("Opening door due to set time.");
    HutchDoor.open();
  }
  if (Time.hour() == door_schedule.close_hour && Time.minute() == door_schedule.close_min) {
    Log.info("Closing door due to set time.");
    HutchDoor.close();
  }
}