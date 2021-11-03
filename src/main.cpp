/*
 * Project Coop_Control
 * Description:
 * Author:
 * Date:
 */

#include "Particle.h"
#include "linear_actuator.h"

PRODUCT_ID(15773);
PRODUCT_VERSION(11);

SerialLogHandler logHandler(115200, LOG_LEVEL_ALL);

// Enable system thread so Photon still runs program when WiFi is lost
SYSTEM_THREAD(ENABLED)

const int HUTCH_DOOR_OPEN_PIN = D1;
const int HUTCH_DOOR_CLOSE_PIN = D0;
const int HUTCH_DOOR_BUTTON_PIN = D2;

// Store settings in EEPROM
const int DOOR_STATE_ADDRESS = 10;
const int DOOR_SCHEDULE_ADDRESS = 20;

// 15 seconds to fully open/close actuator
const int HUTCH_DOOR_DURATION_SECS = 15;

// Voltage divider settings
const int VDIV_ANALOG_PIN = A5;
const int VDIV_R1_OHMS = 5000;
const int VDIV_R2_OHMS = 330;
const float Aref = 3.3;
double batt_volts;

LinearActuatorState door_state_enum = UNKNOWN;
String door_state = "UNKNOWN";

LinearActuator HutchDoor(HUTCH_DOOR_DURATION_SECS, HUTCH_DOOR_OPEN_PIN, HUTCH_DOOR_CLOSE_PIN, LinearActuatorState::OPEN);

// Millis of last time button was pressed to debounce
uint64_t last_button_press = 0;

system_tick_t loopTimeCheckTick = 0;

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

void saveDoorSchedule() {
  EEPROM.put(DOOR_SCHEDULE_ADDRESS, door_schedule);
}

void loadDoorSchedule() {
  EEPROM.get(DOOR_SCHEDULE_ADDRESS, door_schedule);
}

int setOpenTime(String time) {
  if (time.length() != 5) {
    return -1;
  }
  int hour = time.substring(0,2).toInt();
  int minute = time.substring(3).toInt();
  if (hour < 0 || hour > 24 || minute < 0 || minute > 59) {return -1;}
  Log.info("Setting door open time to %i:%i.", hour, minute);
  door_schedule.open_hour = hour;
  door_schedule.open_min = minute;
  saveDoorSchedule();
  return 1;
}

int setCloseTime(String time) {
  if (time.length() != 5) {
    return -1;
  }
  int hour = time.substring(0,2).toInt();
  int minute = time.substring(3).toInt();
  if (hour < 0 || hour > 24 || minute < 0 || minute > 59) {return -1;}
  Log.info("Setting door close time to %i:%i.", hour, minute);
  door_schedule.close_hour = hour;
  door_schedule.close_min = minute;
  saveDoorSchedule();
  return 1;
}

void saveDoorState() {
  EEPROM.put(DOOR_STATE_ADDRESS, door_state_enum);
}

void loadDoorState() {
  EEPROM.get(DOOR_STATE_ADDRESS, door_state_enum);
  if (door_state_enum == 255) {
    Log.info("No door state saved in EEPROM.");
    door_state_enum = UNKNOWN;
  }
}

// Converts door state to string
void updateDoorState() {
  door_state_enum = HutchDoor.getState();
  switch (door_state_enum) {
    case OPEN:
      door_state = "OPEN";
      break;
    case CLOSED:
      door_state = "CLOSED";
      break;
    case OPENING:
      door_state = "OPENING";
      break;
    case CLOSING:
      door_state = "CLOSING";
      break;
    case UNKNOWN:
      door_state = "UNKNOWN";
      break;
  }
  Log.info("Door state changed to %s", door_state.c_str());
  saveDoorState();
}

void readBattVoltage() {
  unsigned int total = 0;
  // Average 64 voltage readings
  for (int x = 0; x < 64; x++) {
    total = total + analogRead(VDIV_ANALOG_PIN);
  }
  // Particle analog pins are 12-bit DAC, so reads 0-4095 corresponding to 0-3.3 volts
  batt_volts = (total / 64) * ((VDIV_R1_OHMS + VDIV_R2_OHMS)/VDIV_R2_OHMS) * Aref / 4095;

  // Check if max voltage on pin exceeded
  if (total >= (4094 * 64)) {
    Log.info("Battery voltage too high for analog pin!");
    batt_volts = 999;
  }
}

void setup() {
  // Setup button to control door manually
  pinMode(HUTCH_DOOR_BUTTON_PIN, INPUT_PULLDOWN);
  // This updates the global string variable door_state whenver the LinearActuator state changes
  HutchDoor.registerOnChange(updateDoorState);
  // Load door state from flash and set linear actuator class to this value; the actuator should already be
  // in this position, but since it has built in limit switches, this just makes sure the class is correctly
  // aligned with the actuator even if the relays are turned on
  loadDoorState();
  switch (door_state_enum) {
    case OPEN:
    case OPENING:
      HutchDoor.open();
      break;
    case CLOSED:
    case CLOSING:
      HutchDoor.close();
      break;
  }
  loadDoorSchedule();

  // Particle specific from here on:
  Particle.connect();
  // Set time zone to eastern
  Time.zone(-4);  
  // Reg variable to see door state from the cloud
  Particle.variable("hutch_door_state", door_state);
  Particle.variable("hutch_batt_volts", batt_volts);
  // Reg function to control door state from the cloud
  Particle.function("set_door", setDoor);
  // Set open/closed times
  Particle.function("set_open_time", setOpenTime);
  Particle.function("set_close_time", setCloseTime);
}

void loop() {
  // Check button for presses
  int button_state = digitalRead(HUTCH_DOOR_BUTTON_PIN);
  // If button pressed and more then one second since last press toggle door state
  if (button_state == HIGH && (System.millis() - last_button_press) > 1000) {
    Log.info("Toggling door state due to button press...");
    last_button_press = System.millis();
    HutchDoor.toggle();
  }

  HutchDoor.loop();

  readBattVoltage();

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