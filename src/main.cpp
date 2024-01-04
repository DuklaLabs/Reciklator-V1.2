#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <FastLED.h>
#include <ItemInput.h>
#include <ItemToggle.h>
#include <MenuItem.h>
#include <ItemSubMenu.h>
#include <ItemList.h>
#include <ItemCommand.h>
#include <LcdMenu.h>
#include <thermistor.h>
#include <OneButton.h>
#include <AccelStepper.h>
#include <SimpleRotary.h>
#include <EncoderButton.h>
#include <EEPROM.h>


//pinout
#define heater 28
#define filamentFan1 22
#define filamentFan2 24
#define electronicsFan 26

#define CutterStep 2
#define CutterDir 5
#define PullerStep 3
#define PullerDir 6
#define EncoderA 51
#define EncoderB 52
#define EButton 53




long encoderPosition  = 0;
uint8_t setTemperture = 0;
bool heaterState = false;

uint8_t cursorIcon = 0x7E;

//obejcts
EncoderButton enc = EncoderButton(EncoderA, EncoderB, EButton); 
SimpleRotary encoder(EncoderA, EncoderB, EButton);
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);
LcdMenu menu(4, 20);
extern MenuItem* settingsMenu[];
Thermistor *thermistor;
AccelStepper cutterStepper(1, CutterStep, CutterDir);
AccelStepper pullerStepper(1, PullerStep, PullerDir);


#define CHARSET_SIZE 10
char charset[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
uint8_t charsetPosition;


void toggleHeater(uint16_t isOn);
void setHeaterTemp(char* value) {
    // Do stuff with value
    Serial.print(F("# "));
    Serial.println(value);
}

MAIN_MENU(
    ITEM_TOGGLE("Heater: ", toggleHeater),
    ITEM_INPUT("Temp: ", setHeaterTemp),
    ITEM_BASIC("this is a text"),
    ITEM_BASIC("this is also a text"),
    ITEM_SUBMENU("Settings", settingsMenu)

);

SUB_MENU(settingsMenu, mainMenu,
    ITEM_BASIC("Backlight"),
    ITEM_BASIC("Contrast")
);


void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  menu.setupLcdWithMenu(0x27, mainMenu);
  menu.update();
  cutterStepper.setMaxSpeed(1000);
  pullerStepper.setMaxSpeed(1000);
  cutterStepper.setSpeed(1000);
  pullerStepper.setSpeed(1000);

  thermistor = new Thermistor(A8, 5.0, 5.0, 1023.0, 100000, 100000, 25, 3950, 5, 20);
}


int ctr = 0;
byte lastDir = 0;
 
void loop(){
  int rDir = encoder.rotate();
  int rBtn = encoder.push();
  int rLBtn = encoder.pushLong(1000);

  // Check direction
  if ( rDir == 1  ) {

    if (menu.isInEditMode()) {  // Update the position only in edit mode
      charsetPosition = (charsetPosition + 1) % CHARSET_SIZE;
      menu.drawChar(charset[charsetPosition]);  // Works only in edit mode
    }
      // CW
      ctr++;
      lastDir = rDir;
      menu.up();
  }
  if ( rDir == 2 ) {
    if (menu.isInEditMode()) { // Update the position only in edit mode
      charsetPosition = constrain(charsetPosition - 1, 0, CHARSET_SIZE);
      menu.drawChar(charset[charsetPosition]);  // Works only in edit mode
    }
      // CCW
      ctr--;
      lastDir = rDir;
      menu.down();
  }

  if ( rBtn == 1 ) {
    if (menu.isInEditMode())
    {
      menu.type(charset[charsetPosition]);
    }
   
    menu.enter();
  }

  if ( rLBtn == 1 ) {
    ctr = 0;
    menu.back();
  }


  if (ctr > 20)
  {
    pullerStepper.runSpeed();
  } else if (ctr < -20)
  {
    cutterStepper.runSpeed();
  }


  if (heaterState == false)
  {
    digitalWrite(heater, LOW);
  } else if(thermistor->readTempC()>setTemperture+1 && heaterState == true) {
    digitalWrite(heater, LOW);
  } else if (thermistor->readTempC()<setTemperture-1 && heaterState == true)
  {
    digitalWrite(heater, HIGH);
  }

}

void toggleHeater(uint16_t isOn) { heaterState = isOn; }

/**
 * Funkce pro vyhledání adresy v EEPROM podle klíče.
 *
 * Tato funkce prochází EEPROM a hledá zadaný klíč. Klíč se považuje za
 * řetězec znaků, který je zapisován do paměti EEPROM na počátku hledání.
 * Funkce vrací adresu v EEPROM, kde byl klíč nalezen.
 *
 * @param key řetězec klíče, který se hledá v EEPROM
 * @return adresa v EEPROM, kde byl klíč nalezen
 */
int findInEEPROM(String key) {
  int addr = 0;
  int len = key.length();
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addr, key[i]);
    addr++;
  }
  return addr;
}

/**
 * Funkce pro zápis hodnot do EEPROM podle klíče.
 *
 * Tato funkce hledá v EEPROM zadaný klíč a zapíše dvě 16bitové hodnoty
 * (int) z pole 'value'. Pro každou hodnotu používá dvě buňky EEPROM.
 *
 * @param key řetězec klíče, podle kterého se hledá v EEPROM
 * @param value pole int hodnot, které budou zapsány do EEPROM (2 hodnoty)
 */
void writeInEEPROM(String key, int value[2]) {
  int addr = findInEEPROM(key);
  int len = 2;
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addr, ((value[i] >> 8) & 0xFF));
    EEPROM.write(addr, (value[i] & 0xFF));
    addr+=2;
  }
}

/**
 * Funkce pro čtení hodnot z EEPROM podle klíče.
 *
 * Tato funkce hledá v EEPROM zadaný klíč a načítá dvě 16bitové hodnoty
 * (int) spojené v poli 'value'. Pro každou hodnotu používá dvě buňky EEPROM.
 *
 * @param key řetězec klíče, podle kterého se hledá v EEPROM
 * @param value pole int hodnot, kam budou uloženy přečtené hodnoty (2 hodnoty)
 */
void readFromEEPROM(String key, int value[2]) {
  int addr = findInEEPROM(key);
  int len = 2;
  for (int i = 0; i < len; i++)
  {
    value[i] = (EEPROM.read(addr) << 8) + EEPROM.read(addr+1);
    addr+=2;
  }
}