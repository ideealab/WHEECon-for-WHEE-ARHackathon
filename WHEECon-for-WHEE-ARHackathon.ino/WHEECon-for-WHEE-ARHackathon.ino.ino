#include <BleKeyboard.h>
#include <M5Core2.h>
#include <Preferences.h>
#include <VL53L0X.h>
#include <Wire.h>

// Unit related constants
const int I2C_ADDR_VL53L0X = 0x29;
const int PIN_PORT_B_A = 33;
const int PIN_PORT_B_B = 32;
const int DIST_RANGE_MIN = 0;
const int DIST_RANGE_MAX = 2000;

const int GYRO_RANGE_MIN = -2000;
const int GYRO_RANGE_MAX = 2000;

const int ACC_RANGE_MIN = -80;
const int ACC_RANGE_MAX = 80;

const int UNIT_NONE = 0;
const int UNIT_DUAL_BUTTON = 1;

// Unit related variables
VL53L0X rangingSensor;
bool isDualButtonConnected = false;
bool isRangingSensorConnected = false;
bool isIMUSensorConnected = false;


int unitOnPortB = UNIT_NONE;
int distRangeMin = DIST_RANGE_MIN;
int distRangeMax = DIST_RANGE_MAX;
int gyroRangeMin = GYRO_RANGE_MIN;
int gyroRangeMax = GYRO_RANGE_MAX;
int accRangeMin = ACC_RANGE_MIN;
int accRangeMax = ACC_RANGE_MAX;

float gyroX = 0.0F;
float gyroY = 0.0F;
float gyroZ = 0.0F;

float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;

// Screen related constants
const int LAYOUT_GYROZ_CH_TOP = 40;
const int LAYOUT_ANALOG_CH_TOP = 140;
const int LAYOUT_BUTTONS_CH_TOP = 160;
const int LAYOUT_LINE_HEIGHT = 20;
const int SCREEN_MAIN = 0;
const int SCREEN_PREFS_SELECT = 1;
const int SCREEN_PREFS_EDIT = 2;
const int PREFS_MENU_NUM_ITEMS = 7;
const int PREFS_MENU_INC_DEC_UNIT = 10;
const int PREFS_MENU_ACC_INC_DEC_UNIT = 1;

// Screen related variables
int currentMenuItem = 0;
int currentScreenMode = SCREEN_MAIN;
Preferences preferences;
char analogStatus[30];
char imuStatus[120];
char buttonsStatus1[30];
char buttonsStatus2[30];

// Protocol related constants
const byte KEYS_FOR_ANALOG_CH[] = {'`', '1', '2', '3', '4', '5',
                                   '6', '7', '8', '9', '0'
                                  };
const byte KEYS_FOR_BUTTON_CH[] = {'v', 'b'};

//gyro,
//x['m', 'n', ',', 'o', 'p'], y['u', 'i', 'h', 'j', 'k'], z['f', 'g', 'r', 't', 'y']
//accel
//x['d', 'x', 'z', 'c', '['], y['q', 'w', 'e', 'a', 's'], z['x', '-', '=', '.', '/']

const byte KEYS_FOR_IMU_CH[] = {'m', 'n', ',', 'o', 'p', //gyro x
                                'u', 'i', 'h', 'j', 'k', //gyro y
                                'f', 'g', 'r', 't', 'y', //gyro z
                                'd', 'x', 'z', 'c', '[', //accel x
                                'q', 'w', 'e', 'a', 's', //accel y
                                'l', '-', '=', '.', '/'  //accel z
                               };


// ESP32 BLE Keyboard related variables
// Note: the device name should be within 15 characters;
// otherwise, macOS and iOS devices can't discover
// https://github.com/T-vK/ESP32-BLE-Keyboard/issues/51#issuecomment-733886764
BleKeyboard bleKeyboard("alt. controller");
bool isConnected = false;
bool isSending = false;


int BtnB_count = 0;


void setup() {
  // A workaround to avoid noise regarding Button A
  adc_power_acquire();

  M5.begin();
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.setTextColor(BLUE, WHITE);
  M5.Lcd.setTextSize(2);

  preferences.begin("alt. controller", false);
  unitOnPortB = preferences.getInt("unitOnPortB", UNIT_NONE);
  distRangeMin = preferences.getInt("distRangeMin", DIST_RANGE_MIN);
  distRangeMax = preferences.getInt("distRangeMax", DIST_RANGE_MAX);
  gyroRangeMin = preferences.getInt("gyroRangeMin", GYRO_RANGE_MIN);
  gyroRangeMax = preferences.getInt("gyroRangeMax", GYRO_RANGE_MAX);
  accRangeMin = preferences.getInt("accRangeMin", ACC_RANGE_MIN);
  accRangeMax = preferences.getInt("accRangeMax", ACC_RANGE_MAX);


  updateFlagsRegardingPortB();

  pinMode(PIN_PORT_B_A, INPUT);
  pinMode(PIN_PORT_B_B, INPUT);

  Wire.begin();

  rangingSensor.setTimeout(500);
  if (rangingSensor.init()) {
    isRangingSensorConnected = true;
    rangingSensor.setMeasurementTimingBudget(20000);
  }

  M5.IMU.Init();

  bleKeyboard.begin();

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Bluetooth: Not connected");

  drawButtons(currentScreenMode);
  Serial.begin(9600);
}

void loop() {
  const unsigned long LOOP_INTERVAL = 25;
  unsigned long start = millis();

  M5.update();
  handleButtons();

  isConnected = bleKeyboard.isConnected();
  bool requestToSend = isConnected && isSending;

  if (isDualButtonConnected) {
    handleDualButton(requestToSend);
  }

  if (isIMUSensorConnected) {
    handleIMUSensor(requestToSend);
  }

  if (isRangingSensorConnected) {
    handleRangingSensor(requestToSend);
  }

  if (currentScreenMode == SCREEN_MAIN) {
    drawMainScreen();
  }

  unsigned long now = millis();
  unsigned long elapsed = now - start;

  if (currentScreenMode == SCREEN_MAIN) {
    //M5.Lcd.setTextSize(1);
    //    M5.Lcd.setCursor(0, 200);
    //    M5.Lcd.printf("%s", ESP.getSdkVersion());
    //    M5.Lcd.setCursor(160, 200);

    //    M5.Lcd.setCursor(0, 200);
    //    M5.Lcd.printf("%3d ms", elapsed);
    //    M5.Lcd.setTextSize(2);
  }

  if (elapsed < LOOP_INTERVAL) {
    delay(LOOP_INTERVAL - elapsed);
  }
}

void handleButtons() {
  if (currentScreenMode != SCREEN_MAIN) {
    M5.Lcd.setCursor(0, 0 + LAYOUT_LINE_HEIGHT * currentMenuItem);
    M5.Lcd.print(">");
  }

  if (M5.BtnA.wasPressed()) {
    switch (currentScreenMode) {
      case SCREEN_MAIN:
        currentScreenMode = SCREEN_PREFS_SELECT;
        M5.Lcd.clear(TFT_WHITE);
        drawButtons(currentScreenMode);
        break;
      case SCREEN_PREFS_SELECT:
        currentScreenMode = SCREEN_MAIN;
        M5.Lcd.clear(TFT_WHITE);
        drawButtons(currentScreenMode);
        break;
      case SCREEN_PREFS_EDIT:
        switch (currentMenuItem) {
          case 0:
            unitOnPortB = unitOnPortB - 1;
            if (unitOnPortB < 0) {
              unitOnPortB = UNIT_DUAL_BUTTON;
            }
            preferences.putInt("unitOnPortB", unitOnPortB);
            updateFlagsRegardingPortB();
            break;

          case 1:
            distRangeMin = constrain((distRangeMin - PREFS_MENU_INC_DEC_UNIT),
                                     DIST_RANGE_MIN,
                                     distRangeMax - PREFS_MENU_INC_DEC_UNIT);
            preferences.putInt("distRangeMin", distRangeMin);
            break;
          case 2:
            distRangeMax = constrain((distRangeMax - PREFS_MENU_INC_DEC_UNIT),
                                     distRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                     DIST_RANGE_MAX);
            preferences.putInt("distRangeMax", distRangeMax);
            break;

          case 3:
            gyroRangeMin = constrain((gyroRangeMin + PREFS_MENU_INC_DEC_UNIT),
                                     GYRO_RANGE_MIN,
                                     gyroRangeMax - PREFS_MENU_INC_DEC_UNIT);
            preferences.putInt("gyroRangeMin", gyroRangeMin);
            break;
          case 4:
            gyroRangeMax = constrain((gyroRangeMax - PREFS_MENU_INC_DEC_UNIT),
                                     gyroRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                     GYRO_RANGE_MAX);
            preferences.putInt("gyroRangeMax", gyroRangeMax);
            break;

          case 5:
            accRangeMin = constrain((accRangeMin + PREFS_MENU_ACC_INC_DEC_UNIT),
                                    ACC_RANGE_MIN,
                                    accRangeMax - PREFS_MENU_ACC_INC_DEC_UNIT);
            preferences.putInt("accRangeMin", accRangeMin);
            break;
          case 6:
            accRangeMax = constrain((accRangeMax - PREFS_MENU_ACC_INC_DEC_UNIT),
                                    accRangeMin + PREFS_MENU_ACC_INC_DEC_UNIT,
                                    ACC_RANGE_MAX);
            preferences.putInt("accRangeMax", accRangeMax);
            break;

          default:
            break;
        }
        break;
      default:
        break;
    }
  }

  if (M5.BtnC.wasPressed()) {
    switch (currentScreenMode) {
      case SCREEN_MAIN:
        if (isSending) {
          isSending = false;
          drawButtons(currentScreenMode);
        } else {
          isSending = true;
          drawButtons(currentScreenMode);
        }
        break;
      case SCREEN_PREFS_SELECT:
        M5.Lcd.setCursor(0, 0 + LAYOUT_LINE_HEIGHT * currentMenuItem);
        M5.Lcd.print(" ");
        currentMenuItem = (currentMenuItem + 1) % PREFS_MENU_NUM_ITEMS;
        M5.Lcd.setCursor(0, 0 + LAYOUT_LINE_HEIGHT * currentMenuItem);
        M5.Lcd.print(">");
        drawButtons(currentScreenMode);
        break;

      case SCREEN_PREFS_EDIT:
        switch (currentMenuItem) {
          case 0:
            unitOnPortB = unitOnPortB + 1;
            if (unitOnPortB > UNIT_DUAL_BUTTON) {
              unitOnPortB = UNIT_NONE;
            }
            preferences.putInt("unitOnPortB", unitOnPortB);
            updateFlagsRegardingPortB();
            break;

          case 1:
            distRangeMin = constrain((distRangeMin + PREFS_MENU_INC_DEC_UNIT),
                                     DIST_RANGE_MIN,
                                     distRangeMax - PREFS_MENU_INC_DEC_UNIT);
            preferences.putInt("distRangeMin", distRangeMin);
            break;
          case 2:
            distRangeMax = constrain((distRangeMax + PREFS_MENU_INC_DEC_UNIT),
                                     distRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                     DIST_RANGE_MAX);
            preferences.putInt("distRangeMax", distRangeMax);
            break;

          case 3:
            gyroRangeMin = constrain((gyroRangeMin - PREFS_MENU_INC_DEC_UNIT),
                                     GYRO_RANGE_MIN,
                                     gyroRangeMax - PREFS_MENU_INC_DEC_UNIT);
            preferences.putInt("gyroRangeMin", gyroRangeMin);
            break;
          case 4:
            gyroRangeMax = constrain((gyroRangeMax + PREFS_MENU_INC_DEC_UNIT),
                                     gyroRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                     GYRO_RANGE_MAX);
            preferences.putInt("gyroRangeMax", gyroRangeMax);
            break;

          case 5:
            accRangeMin = constrain((accRangeMin - PREFS_MENU_ACC_INC_DEC_UNIT),
                                    ACC_RANGE_MIN,
                                    accRangeMax - PREFS_MENU_ACC_INC_DEC_UNIT);
            preferences.putInt("accRangeMin", accRangeMin);
            break;
          case 6:
            accRangeMax = constrain((accRangeMax + PREFS_MENU_ACC_INC_DEC_UNIT),
                                    accRangeMin + PREFS_MENU_ACC_INC_DEC_UNIT,
                                    ACC_RANGE_MAX);
            preferences.putInt("accRangeMax", accRangeMax);
            break;

          default:
            break;
        }
        break;
      default:
        break;
    }
  }

  if (M5.BtnA.pressedFor(500)) {
    if (currentScreenMode == SCREEN_PREFS_EDIT) {
      switch (currentMenuItem) {
        case 1:
          distRangeMin = constrain((distRangeMin - PREFS_MENU_INC_DEC_UNIT),
                                   DIST_RANGE_MIN,
                                   distRangeMax - PREFS_MENU_INC_DEC_UNIT);
          break;
        case 2:
          distRangeMax = constrain((distRangeMax - PREFS_MENU_INC_DEC_UNIT),
                                   distRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                   DIST_RANGE_MAX);
          break;

        case 3:
          gyroRangeMin = constrain((gyroRangeMin + PREFS_MENU_INC_DEC_UNIT),
                                   GYRO_RANGE_MIN,
                                   gyroRangeMax - PREFS_MENU_INC_DEC_UNIT);
          break;
        case 4:
          gyroRangeMax = constrain((gyroRangeMax - PREFS_MENU_INC_DEC_UNIT),
                                   gyroRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                   GYRO_RANGE_MAX);
          break;

        case 5:
          accRangeMin = constrain((accRangeMin + PREFS_MENU_ACC_INC_DEC_UNIT),
                                  ACC_RANGE_MIN,
                                  accRangeMax - PREFS_MENU_ACC_INC_DEC_UNIT);
          preferences.putInt("accRangeMin", accRangeMin);
          break;
        case 6:
          accRangeMax = constrain((accRangeMax - PREFS_MENU_ACC_INC_DEC_UNIT),
                                  accRangeMin + PREFS_MENU_ACC_INC_DEC_UNIT,
                                  ACC_RANGE_MAX);
          preferences.putInt("accRangeMax", accRangeMax);
          break;
        default:
          break;
      }
    }
  }

  if (M5.BtnC.pressedFor(500)) {
    if (currentScreenMode == SCREEN_PREFS_EDIT) {
      switch (currentMenuItem) {
        case 1:
          distRangeMin = constrain((distRangeMin + PREFS_MENU_INC_DEC_UNIT),
                                   DIST_RANGE_MIN,
                                   distRangeMax - PREFS_MENU_INC_DEC_UNIT);
          break;
        case 2:
          distRangeMax = constrain((distRangeMax + PREFS_MENU_INC_DEC_UNIT),
                                   distRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                   DIST_RANGE_MAX);
          break;

        case 3:
          gyroRangeMin = constrain((gyroRangeMin - PREFS_MENU_INC_DEC_UNIT),
                                   GYRO_RANGE_MIN,
                                   gyroRangeMax - PREFS_MENU_INC_DEC_UNIT);
          break;
        case 4:
          gyroRangeMax = constrain((gyroRangeMax + PREFS_MENU_INC_DEC_UNIT),
                                   gyroRangeMin + PREFS_MENU_INC_DEC_UNIT,
                                   GYRO_RANGE_MAX);
          break;

        case 5:
          accRangeMin = constrain((accRangeMin - PREFS_MENU_ACC_INC_DEC_UNIT),
                                  ACC_RANGE_MIN,
                                  accRangeMax - PREFS_MENU_ACC_INC_DEC_UNIT);
          preferences.putInt("accRangeMin", accRangeMin);
          break;
        case 6:
          accRangeMax = constrain((accRangeMax + PREFS_MENU_ACC_INC_DEC_UNIT),
                                  accRangeMin + PREFS_MENU_ACC_INC_DEC_UNIT,
                                  ACC_RANGE_MAX);
          preferences.putInt("accRangeMax", accRangeMax);
          break;

        default:
          break;
      }
    }
  }

  if (M5.BtnB.wasPressed()) {
    switch (currentScreenMode) {

      case SCREEN_MAIN:
        BtnB_count ++;
        if (BtnB_count == 1)
        {
          isIMUSensorConnected = true;
        } else {
          isIMUSensorConnected = false;
          BtnB_count = 0;
        }

        break;

      case SCREEN_PREFS_SELECT:

        currentScreenMode = SCREEN_PREFS_EDIT;

        break;

      case SCREEN_PREFS_EDIT:
        currentScreenMode = SCREEN_PREFS_SELECT;
        break;
      default:
        break;
    }

    drawButtons(currentScreenMode);
  }

  if (currentScreenMode != SCREEN_MAIN) {
    drawPreferencesScreen();
  }
}

void updateFlagsRegardingPortB() {
  switch (unitOnPortB) {
    case UNIT_NONE:
      isDualButtonConnected = false;
      break;
    case UNIT_DUAL_BUTTON:
      isDualButtonConnected = true;
      break;

    default:
      break;
  }
}

void drawMainScreen() {
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("Bluetooth: %s",
                isConnected ? "Connected    " : "Disconnected ");
  //imu
  M5.Lcd.setCursor(0, LAYOUT_GYROZ_CH_TOP);
  M5.Lcd.print(imuStatus);

  M5.Lcd.setCursor(0, LAYOUT_ANALOG_CH_TOP);
  M5.Lcd.print(analogStatus);

  M5.Lcd.setCursor(0, LAYOUT_BUTTONS_CH_TOP);
  M5.Lcd.print("BUTTONS:");

  if (isDualButtonConnected) {
    M5.Lcd.setCursor(0, LAYOUT_BUTTONS_CH_TOP + LAYOUT_LINE_HEIGHT);
    M5.Lcd.print(buttonsStatus1);
    M5.Lcd.setCursor(0, LAYOUT_BUTTONS_CH_TOP + LAYOUT_LINE_HEIGHT * 2);
    M5.Lcd.print(buttonsStatus2);
  }

}

void drawPreferencesScreen() {
  M5.Lcd.setCursor(20, 0 + LAYOUT_LINE_HEIGHT * 0);
  switch (unitOnPortB) {
    case UNIT_NONE:
      M5.Lcd.print("Port: NONE       ");
      break;
    case UNIT_DUAL_BUTTON:
      M5.Lcd.print("Port: DUAL BUTTON");
      break;
    default:
      break;
  }


  M5.Lcd.setCursor(20, 0 + LAYOUT_LINE_HEIGHT * 1);
  M5.Lcd.printf("Dist Range Min: %5d", distRangeMin);
  M5.Lcd.setCursor(20, 0 + LAYOUT_LINE_HEIGHT * 2);
  M5.Lcd.printf("           Max: %5d", distRangeMax);

  M5.Lcd.setCursor(20, 0 + LAYOUT_LINE_HEIGHT * 3);
  M5.Lcd.printf("gyro Range Min: %5d", gyroRangeMin);
  M5.Lcd.setCursor(20, 0 + LAYOUT_LINE_HEIGHT * 4);
  M5.Lcd.printf("           Max: %5d", gyroRangeMax);

  M5.Lcd.setCursor(20, 0 + LAYOUT_LINE_HEIGHT * 5);
  M5.Lcd.printf("acc Range  Min: %5d", accRangeMin);
  M5.Lcd.setCursor(20, 0 + LAYOUT_LINE_HEIGHT * 6);
  M5.Lcd.printf("           Max: %5d", accRangeMax);
}

void handleDualButton(bool updateRequested) {
  const int KEY_ID_RED_BUTTON = 0;
  const int KEY_ID_BLUE_BUTTON = 1;

  static bool wasRedButtonPressed = false;
  static bool wasBlueButtonPressed = false;

  bool isRedButtonPressed = digitalRead(PIN_PORT_B_B) == LOW;
  bool isBlueButtonPressed = digitalRead(PIN_PORT_B_A) == LOW;

  sprintf(buttonsStatus1, "Red:%d  Blue:%d", isRedButtonPressed,
          isBlueButtonPressed);

  if (updateRequested) {
    if (!wasRedButtonPressed && isRedButtonPressed) {
      bleKeyboard.press(KEYS_FOR_BUTTON_CH[KEY_ID_RED_BUTTON]);
    } else if (wasRedButtonPressed && !isRedButtonPressed) {
      bleKeyboard.release(KEYS_FOR_BUTTON_CH[KEY_ID_RED_BUTTON]);
    }

    if (!wasBlueButtonPressed && isBlueButtonPressed) {
      bleKeyboard.press(KEYS_FOR_BUTTON_CH[KEY_ID_BLUE_BUTTON]);
    } else if (wasBlueButtonPressed && !isBlueButtonPressed) {
      bleKeyboard.release(KEYS_FOR_BUTTON_CH[KEY_ID_BLUE_BUTTON]);
    }
  }

  wasRedButtonPressed = isRedButtonPressed;
  wasBlueButtonPressed = isBlueButtonPressed;
}

void handleRangingSensor(bool updateRequested) {
  static int lastValue = -1;

  int range = constrain(rangingSensor.readRangeSingleMillimeters(),
                        distRangeMin, distRangeMax);

  if (rangingSensor.readRangeSingleMillimeters() == 8191)
  {
    range = distRangeMax;
  }
  // convert to 11 steps
  int currentValue = map(range, distRangeMin, distRangeMax, 0, 10);

  Serial.println(rangingSensor.readRangeSingleMillimeters());

  sprintf(analogStatus, "ANALOG:%2d (%4d mm)", currentValue, range);


  if (lastValue != currentValue) {

    if (updateRequested) {
      bleKeyboard.write(KEYS_FOR_ANALOG_CH[currentValue]);
    }
    lastValue = currentValue;
  }

}


void handleIMUSensor(bool updateRequested) {
  static int lastValue_gyroX = -1;
  static int lastValue_gyroY = -1;
  static int lastValue_gyroZ = -1;

  static int lastValue_accX = -1;
  static int lastValue_accY = -1;
  static int lastValue_accZ = -1;

  M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
  M5.IMU.getAccelData(&accX, &accY, &accZ);

  int gyroX_data = constrain(gyroX, gyroRangeMin, gyroRangeMax);
  int gyroY_data = constrain(gyroY, gyroRangeMin, gyroRangeMax);
  int gyroZ_data = constrain(gyroZ, gyroRangeMin, gyroRangeMax);

  int accX_data = constrain(accX * 10, accRangeMin, accRangeMax);
  int accY_data = constrain(accY * 10, accRangeMin, accRangeMax);
  int accZ_data = constrain(accZ * 10, accRangeMin, accRangeMax);


  // convert to 5 steps
  int currentValue_gyroX = map(gyroX_data, gyroRangeMin, gyroRangeMax, -2, 2);
  int currentValue_gyroY = map(gyroY_data, gyroRangeMin, gyroRangeMax, -2, 2);
  int currentValue_gyroZ = map(gyroZ_data, gyroRangeMin, gyroRangeMax, -2, 2);

  int currentValue_accX = map(accX_data, accRangeMin, accRangeMax, -2, 2);
  int currentValue_accY = map(accY_data, accRangeMin, accRangeMax, -2, 2);
  int currentValue_accZ = map(accZ_data, accRangeMin, accRangeMax, -2, 2);


  sprintf(imuStatus, "GryoX:%2d (%4d o/s)\nGryoY:%2d (%4d o/s)\nGryoZ:%2d (%4d o/s)\nAccX:%2d (%4d G/10)\nAccY:%2d (%4d G/10)\nAccZ:%2d (%4d G/10)", currentValue_gyroX, gyroX_data, currentValue_gyroY, gyroY_data, currentValue_gyroZ, gyroZ_data, currentValue_accX, accX_data, currentValue_accY, accY_data, currentValue_accZ, accZ_data);

  if (lastValue_gyroX != currentValue_gyroX) {
    if (updateRequested) {
      bleKeyboard.write(KEYS_FOR_IMU_CH[currentValue_gyroX + 2]);//+2: [-2,2] -> [0,4] move cursor to gyroX
    }
    lastValue_gyroX = currentValue_gyroX;
  }

  if (lastValue_gyroY != currentValue_gyroY) {
    if (updateRequested) {
      bleKeyboard.write(KEYS_FOR_IMU_CH[currentValue_gyroY + 2 + 5]);//+2: [-2,2] -> [0,4], +5: move cursor to gyroY
    }
    lastValue_gyroY = currentValue_gyroY;
  }


  if (lastValue_gyroZ != currentValue_gyroZ) {
    if (updateRequested) {
      bleKeyboard.write(KEYS_FOR_IMU_CH[currentValue_gyroZ + 2 + 10]);//+2: [-2,2] -> [0,4], +10: move cursor to gyroZ
    }
    lastValue_gyroZ = currentValue_gyroZ;
  }


  if (lastValue_accX != currentValue_accX) {
    if (updateRequested) {
      bleKeyboard.write(KEYS_FOR_IMU_CH[currentValue_accX + 2 + 15]); //+2: [-2,2] -> [0,4], +15: move cursor to accX
    }
    lastValue_accX = currentValue_accX;
  }

  if (lastValue_accY != currentValue_accY) {
    if (updateRequested) {
      bleKeyboard.write(KEYS_FOR_IMU_CH[currentValue_accY + 2 + 20]);//+2: [-2,2] -> [0,4], +20: move cursor to gyroY
    }
    lastValue_accY = currentValue_accY;
  }


  if (lastValue_accZ != currentValue_accZ) {
    if (updateRequested) {
      bleKeyboard.write(KEYS_FOR_IMU_CH[currentValue_accZ + 2 + 25]);//+2: [-2,2] -> [0,4], +10: move cursor to gyroZ
    }
    lastValue_accZ = currentValue_accZ;
  }

}

void drawButtons(int currentScreenMode) {
  const int LAYOUT_BTN_A_CENTER = 64;
  const int LAYOUT_BTN_B_CENTER = 160;
  const int LAYOUT_BTN_C_CENTER = 256;

  switch (currentScreenMode) {
    case SCREEN_MAIN:
      if (!isSending) {
        drawButton(LAYOUT_BTN_A_CENTER, "Setup");
        if (isIMUSensorConnected) {
          drawButton(LAYOUT_BTN_B_CENTER, "IMUON");
        } else {
          drawButton(LAYOUT_BTN_B_CENTER, "IMU");
        }

        drawButton(LAYOUT_BTN_C_CENTER, "Send");
      } else {
        drawButton(LAYOUT_BTN_A_CENTER, "Setup");
        if (isIMUSensorConnected) {
          drawButton(LAYOUT_BTN_B_CENTER, "IMUON");
        } else {
          drawButton(LAYOUT_BTN_B_CENTER, "IMU");
        }        drawButton(LAYOUT_BTN_C_CENTER, "Stop");
      }
      break;
    case SCREEN_PREFS_SELECT:
      drawButton(LAYOUT_BTN_A_CENTER, "Exit");
      drawButton(LAYOUT_BTN_B_CENTER, "Enter");
      drawButton(LAYOUT_BTN_C_CENTER, "Next");
      break;
    case SCREEN_PREFS_EDIT:
      drawButton(LAYOUT_BTN_A_CENTER, "-");
      drawButton(LAYOUT_BTN_B_CENTER, "Done");
      drawButton(LAYOUT_BTN_C_CENTER, "+");
      break;
    default:
      break;
  }
}

void drawButton(int centerX, const String &title) {
  const int BUTTON_WIDTH = 72;
  const int BUTTON_HEIGHT = 24;

  M5.Lcd.setTextSize(2);

  int fontHeight = M5.Lcd.fontHeight();
  int rectLeft = centerX - BUTTON_WIDTH / 2;
  int rectTop = M5.Lcd.height() - BUTTON_HEIGHT;
  int rectWidth = BUTTON_WIDTH;
  int rectHeight = BUTTON_HEIGHT;
  int coordinateY = rectTop + (rectHeight - fontHeight) / 2;

  M5.Lcd.fillRect(rectLeft, rectTop, rectWidth, rectHeight, TFT_WHITE);
  M5.Lcd.drawRect(rectLeft, rectTop, rectWidth, rectHeight, TFT_BLUE);
  M5.Lcd.drawCentreString(title, centerX, coordinateY, 1);
}
