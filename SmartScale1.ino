/*
   -------------------------------------------------------------------------------------
   ECE387 Final Group Project
   Colin Sellers, Dhruv Birla, Saad Halail, Qin Huai
   5/13/2022
   -------------------------------------------------------------------------------------
*/


#include <SoftwareSerial.h>
#include <ArduinoBlue.h>
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

//pins:
const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin
const int BLUETOOTH_TX = 8; // Bluetooth TX -> Arduino D8
const int BLUETOOTH_RX = 7; // Bluetooth RX -> Arduino D7

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

//bluetooth contructor: 
SoftwareSerial bluetooth(BLUETOOTH_TX, BLUETOOTH_RX);
ArduinoBlue phone(bluetooth);

const int calVal_eepromAdress = 0;
unsigned long t = 0;
int button;
String str;
float itemWeight;
String item;
bool itemSet = false;

void setup() {
 Serial.begin(57600); bluetooth.begin(9600); delay(10);
  String str = "Please Calibrate before using";
  phone.sendMessage(str);

  LoadCell.begin();
  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    str = "Could not connect to scale";
    phone.sendMessage(str);
    while (1);
  }
  else {
    LoadCell.setCalFactor(1.0); // user set calibration value (float), initial value 1.0 may be used for this sketch
    Serial.println("Startup is complete");
  }
  
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity
  button = phone.getButton();
  str = phone.getText();
  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      Serial.print("Load_cell output val: ");
      Serial.println(i);
      newDataReady = 0;
      t = millis();
    }
  }

  // receive command from serial terminal
  if (!str.equals("")) {
    if (str.equals("t") || str.equals("T")){
      LoadCell.tareNoDelay(); //tare
      str = "Tare complete";
      Serial.print("t sent");
      phone.sendText(str);
    }
    else if (str.equals("r") || str.equals("R")) calibrate(); //calibrate
  }

  // check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }
  if(button == 1) {
    while (!LoadCell.update());
    calibrate(); //start calibration procedure
  }
  if(button == 2) {
    str = LoadCell.getData();  
    str += " grams";
    phone.sendText(str);
  }
  if(button == 3) {
    setItem(); 
    itemSet = true; 
  }
  if(button == 4) {
    if(itemSet) {
      float currentWeight = LoadCell.getData();
      float percentage = (currentWeight / itemWeight) * 100;
      str = item + " is at: " + percentage + "%";
      phone.sendText(str);
    }  else {
        str = "Set item before checking";
        phone.sendText(str);
      }
  }

}

void setItem() {
  str = "Enter name of item";
  phone.sendMessage(str);
  
  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    str = phone.getText();
    if (!str.equals("")) {
      item = str;
      itemWeight = LoadCell.getData();
      str = item;
      str += " weight saved";
      phone.sendText(str);
      _resume = true;
      }
  }
}

void calibrate() {
  str = "Send 't' from text box to set the tare offset";
  phone.sendMessage(str);

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    str = phone.getText();
    if (!str.equals("")) {
      if (phone.getText() == 't' || phone.getText() == 'T') LoadCell.tareNoDelay();
    }
    if (LoadCell.getTareStatus() == true) {
      _resume = true;
    }
  }
  str = "Place known mass on the board";
  phone.sendMessage(str);
  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (!str.equals("")) {
      known_mass = phone.getText().toFloat();
      if (known_mass != 0) {
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value

  str = "Board is now Calibrated";
  phone.sendMessage(str);
}


void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  boolean _resume = false;
  Serial.println("***");
  Serial.print("Current value is: ");
  Serial.println(oldCalibrationValue);
  Serial.println("Now, send the new value from serial monitor, i.e. 696.0");
  float newCalibrationValue;
  while (_resume == false) {
    if (Serial.available() > 0) {
      newCalibrationValue = Serial.parseFloat();
      if (newCalibrationValue != 0) {
        Serial.print("New calibration value is: ");
        Serial.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  _resume = false;
  Serial.print("Save this value to EEPROM adress ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");
  while (_resume == false) {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'y') {
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        _resume = true;
      }
      else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }
  Serial.println("End change calibration value");
  Serial.println("***");
}
