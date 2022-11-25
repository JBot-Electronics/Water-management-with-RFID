/*
  Note: i2c pins on arduino uno and nano are
   sck---> A5
   dt----> A4
*/
#include "NewPing.h" // library for ultrasonic sensor(easy ping)
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>

//RFID pins
#define SS_PIN 10
#define RST_PIN 9
//Ultrasonic sensor pins
#define TRIGGER_PIN 6   //trigger pin of ultrasonic sensor
#define ECHO_PIN 7      //echo pin of ultrasonic sensor
#define RELAY_PIN 8     // Pin where pump relay is connected
// all in centimeters
#define MAX_DISTANCE 95 // this is the max distance, the sensor should detect of which is equivalent to cylinder height
#define DIAMETER 53

const int numRows = 2, numCols = 16; //LCD variables cols and rowa
int selected_block = 1, CustomerBlock[2] = {1, 2};

byte AdminBlock[16] = {"Admin"};  byte Customer[2][16] = {{"Customer"}, {"00000"}};
bool Relay_state = true;

//This array is used for reading out a block.
byte data_read[18], readbackblock1[18];

int addr = 0;       //Address for eeprom

//KEYPAD
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {3, 2, 1, 0}, colPins[COLS] = {7, 6, 5, 4};
int amount;
float current_volume;
String tag_cash;


//Function declaration
void AdminMenu();
void CustomerMenu(int);
void UpdateScreen(int, int, String,bool);

//Objects inistances
LiquidCrystal_I2C Screen(0x27, numCols, numRows);
NewPing WaterLevel(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
Keypad KeyBoard = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
MFRC522 RFID(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;


void setup() {
  // put your setup code here, to run once:
  Screen.init();
  Screen.backlight();
  UpdateScreen(0, 0, "Water System",true);
  UpdateScreen(0, 1, "Scan the Tag",true);
  delay(100);
  Screen.clear();

}

void loop() {
  // put your main code here, to run repeatedly:
  if ( ! RFID.PICC_IsNewCardPresent()) {
    return;
  }

  if ( ! RFID.PICC_ReadCardSerial())
  {
    return;
  }

  //  Read the first block to determine user's level
  readBlock(selected_block, data_read);
  String UserLevel = String((char*)data_read);

  if (UserLevel == "Admin") {
    AdminMenu();
  }
  else {
    readBlock(CustomerBlock[1], readbackblock1);
    int credit = String((char*)readbackblock1).toInt();
    CustomerMenu(credit);
  }
}


//custom functions
int readBlock(int blockNumber, byte arrayAddress[])
{
  int largestModulo4Number = blockNumber / 4 * 4;
  int trailerBlock = largestModulo4Number + 3; //determine trailer block for the sector

  //authentication of the desired block for access
  byte status = RFID.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(RFID.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print("PCD_Authenticate() failed (read): ");
    Serial.println(RFID.GetStatusCodeName(status));
    return 3;//return "3" as error message
  }

  byte buffersize = 18;
  status = RFID.MIFARE_Read(blockNumber, arrayAddress, &buffersize);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("MIFARE_read() failed: ");
    Serial.println(RFID.GetStatusCodeName(status));
    return 4;//return "4" as error message
  }
}


int writeBlock(int selected_block, byte arrayAddress[]) {
  int largestModulo4Number = selected_block / 4 * 4;
  int trailerBlock = largestModulo4Number + 3;

  //authentication of the desired block for access
  byte status = RFID.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(RFID.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed: ");
    Serial.println(RFID.GetStatusCodeName(status));
    return 3;
  }

  //writing the block
  status = RFID.MIFARE_Write(selected_block, arrayAddress, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Write failed: ");
    Serial.println(RFID.GetStatusCodeName(status));
    return 4;
  }
}


void options() {
  UpdateScreen(0, 0, "ADMIN",true);
  UpdateScreen(0, 1, "[1]- Recharge Customer.. [2]-- Set one liter price",true);
  Screen.noAutoscroll();
}

void AdminMenu() {
  options();
  char* key_pressed = KeyBoard.getKey();
  Screen.clear();

  if (key_pressed == "1") {
    UpdateScreen(0, 0, (key_pressed),true);
    UpdateScreen(0, 1, "Enter Amount: ",true);

    int cursor_position = 0;
    readKeypad();
    UpdateScreen(0, 1, "Scan Customer Card: ",true);
    readBlock(CustomerBlock[1], readbackblock1);
    int cash = String((char*)readbackblock1).toInt();
    int updated_cash = cash + amount;

    // Update card
    writeBlock(CustomerBlock[1], updated_cash);
    UpdateScreen(0, 1, "Remove the tag",true);

  }
  else if (key_pressed == "2") {
    amount = readKeypad();
    UpdateScreen(0, 1, String(amount),true);
    EEPROM.update(addr, amount);
  }
  else {
    options();
  }
}


float GetVolume() {
  float volume = (22 / 7) * (DIAMETER * DIAMETER) * WaterLevel.ping_cm() / 4;     //PI*r2h
  return volume;
}

int readKeypad() {
  int cursor_position = 0;
  while (true) {
    char* key = KeyBoard.getKey();
//    Pressing any key other than a number confirms the amount entered
    if (key != "A" || key != "B" || key != "C" || key != "D" || key != "E" || key != "F") {
      
      UpdateScreen(cursor_position, 1,key,false);
      tag_cash += key;
      //    amount[cursor_position] = key;
      cursor_position += 1;
    }
    else {
      amount = tag_cash.toInt();
      tag_cash = "";
      return amount;
    }
  }
}


void CustomerMenu(int cash) {
  UpdateScreen(0,0,"Enter amount in ltrs",true);
  Screen.autoscroll();
  int ltrs = readKeypad();
  float current_price = EEPROM.read(addr);

  if (GetVolume() < ltrs) {
    UpdateScreen(0,0,"No Enough water",true);
  } else {

    if ((current_price * ltrs) > cash) {
      UpdateScreen(0,0,"Please Recharge",true);
      UpdateScreen(0,1,"Cash: ", false);
      UpdateScreen(8,1,String(cash),false);
    }
    else {
      current_volume = GetVolume();
      UpdateScreen(0,0,"Pumping water",true);

      while ((current_volume - GetVolume()) <= ltrs) {
        //  Turn on the pump relay
        if (Relay_state) {
          digitalWrite(RELAY_PIN, HIGH);
          Relay_state = false;
        }
      }
      UpdateScreen(0,1,"Scan card again",false);
      
      int updated_cash = cash - (current_price * ltrs);
      writeBlock(CustomerBlock[1], updated_cash);
      digitalWrite(RELAY_PIN, LOW);
      current_volume = GetVolume();
      Relay_state = true;
    }
  }
}

void UpdateScreen(int col, int row, String message, bool clear_screen = true) {
  if (clear_screen) {
    Screen.clear();
    Screen.setCursor(col, row);
    Screen.print(message);
  } else {
    Screen.setCursor(col, row);
    Screen.print(message);
  }
}
