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
#define TRIGGER_PIN A2   //trigger pin of ultrasonic sensor
#define ECHO_PIN A3      //echo pin of ultrasonic sensor
#define RELAY_PIN A1     // Pin where pump relay is connected

// all in centimeters
#define MAX_DISTANCE 95 // this is the max distance, the sensor should detect of which is equivalent to cylinder height
#define DIAMETER 53


/*============================
 ************ Global variables*****************
   ============================
*/
const int numRows = 2, numCols = 16; //LCD variables cols and rowa
int selected_block = 1, CustomerBlock[2] = {1, 2};

byte AdminBlock[16] = {"Admin"}, Customer[2][16] = {{"Customer"}, {"00000"}};
bool Relay_state = true;
float current_volume;

//This array is used for reading out a block.
byte data_read[18], data_read_1[18], readbackblock1[18];

int price_address = 0, pass_address = 1;   //Address for eeprom

//KEYPAD
const byte ROWS = 4;
const byte COLS = 3;
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {5, 6, 7, 8}, colPins[COLS] = {4, 3, 2};


/*=========================
 ************Function declaration*********
  =========================
*/
void readBlock(int blockNumber, byte arrayAddress[]);
void writeBlock(int selected_block, byte arrayAddress[]); void options();
void AdminMenu();
void CustomerMenu(int cash);
void UpdateScreen(int col, int row, String message, bool clear_screen);
float GetVolume();
String read_keypad(bool Is_password);
bool Authenticate();


/*========================
 **********Objects inistances************
  ========================
*/
LiquidCrystal_I2C Screen(0x27, numCols, numRows);
NewPing WaterLevel(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
Keypad KeyBoard = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
MFRC522 RFID(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;


void setup() {
  SPI.begin();
  RFID.PCD_Init();
  Screen.init();
  Screen.backlight();

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

}


void loop() {
  UpdateScreen(2, 0, "Water System", false);
  UpdateScreen(0, 1, "Scan the Tag", false);

  if ( ! RFID.PICC_IsNewCardPresent()) {
    return;
  }
  if ( ! RFID.PICC_ReadCardSerial()) {
    return;
  }

  readBlock(selected_block, data_read);
  String UserLevel = String((char*)data_read);

  Screen.clear();
  UpdateScreen(0, 0, UserLevel, false);
  delay(1000);
  Screen.clear();

  if (UserLevel == "Admin") {
    bool is_authenticated = Authenticate();
    if (is_authenticated) {
      AdminMenu();
    }
  }
  else {
    readBlock(CustomerBlock[1], readbackblock1);
    int credit = String((char*)readbackblock1).toInt();
    CustomerMenu(credit);
    delay(1000);
    Screen.clear();
  }

}


/* ========================
 * *********custom function definitions***
   ========================
*/


void readBlock(int blockNumber, byte arrayAddress[]) {
  int largestModulo4Number = blockNumber / 4 * 4;
  int trailerBlock = largestModulo4Number + 3;
  byte buffersize = 18;

  //authentication of the desired block for access
  byte status = RFID.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(RFID.uid));

  if (status != MFRC522::STATUS_OK) {
    Screen.clear();
    UpdateScreen(0, 0, "Authenication failure", false);
  }

  status = RFID.MIFARE_Read(blockNumber, arrayAddress, &buffersize);
  if (status != MFRC522::STATUS_OK) {
    Screen.clear();
    UpdateScreen(0, 0, "Failed to read", false);
  }
  RFID.PCD_StopCrypto1();
}


void writeBlock(int selected_block, byte arrayAddress[]) {
  int largestModulo4Number = selected_block / 4 * 4;
  int trailerBlock = largestModulo4Number + 3;

  //authentication of the desired block for access
  byte status = RFID.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(RFID.uid));
  if (status != MFRC522::STATUS_OK) {
    Screen.clear();
    UpdateScreen(0, 0, "Authenication failure", false);
  }

  //writing the block
  status = RFID.MIFARE_Write(selected_block, arrayAddress, 16);
  if (status != MFRC522::STATUS_OK) {
    Screen.clear();
    UpdateScreen(0, 0, "Write failed: ", false);
  }
  RFID.PCD_StopCrypto1();
}


void options() {
  Screen.clear();
  Screen.setCursor(0, 0);
  Screen.print("ADMIN");
  String options[3] = {"[1]:Recharge Customer.. ", "[2]:Set one liter price", " [3]:Change Password"};

  for (int j = 0; j < 3; j++) {
    Screen.setCursor(0, 1);
    Screen.print(options[j]);
    delay(500);
    for (int PositionCount = 0; PositionCount < 20; PositionCount++)
    {
      Screen.scrollDisplayLeft();
      delay(400);
    }
    Screen.clear();
  }
  //    UpdateScreen(0,0,"Make selection:",false);
}


void AdminMenu() {
  int amount, cash ;
  String User;
  options();
  String  key_pressed = read_keypad(false);

  if (key_pressed == "1") {
    UpdateScreen(0, 0, "Enter Amount: ", false);
    amount = (read_keypad(false)).toInt();
    delay(100);
    Screen.clear();
    UpdateScreen(0, 0, "Place Customer Card: ", false);

    while (! RFID.PICC_IsNewCardPresent() && ! RFID.PICC_ReadCardSerial()) {
      readBlock(selected_block, readbackblock1);
      delay(1000);
      Screen.clear();
      User = String((char*)data_read_1);
      if (User == "Customer") {
        for (int i = 0; i < 18; i++) {
          readbackblock1[i] = 0;
        }

        readBlock(CustomerBlock[1], readbackblock1);
        int cash = String((char*)readbackblock1).toInt();
        break;
      }
    }

    int updated_cash = cash + amount;
    delay(1000);
    UpdateScreen(0, 0, String(updated_cash), false);
    UpdateScreen(4, 0, String(cash), false);

    // Update card
    //       writeBlock(CustomerBlock[1], updated_cash);

    delay(1000);
    UpdateScreen(0, 1, "Remove the tag", false);
    delay(1000);
    Screen.clear();

  }
  else if (key_pressed == "2") {
    Screen.clear();
    UpdateScreen(0, 0, "Enter Price: ", false);
    amount = (read_keypad(false)).toInt();
    Screen.clear();
    UpdateScreen(0, 1, String(amount), false);
    //    EEPROM.update(price_address, amount);
  }
  else {
    options();
    UpdateScreen(0, 0, "Make selection:", false);
    key_pressed = read_keypad(false);
  }
}


void CustomerMenu(int cash) {
  Screen.clear();
  UpdateScreen(0, 0, "Enter amount in ltrs:", false);

  int ltrs = (read_keypad(false)).toInt();

  float current_price = EEPROM.read(price_address);

  if (GetVolume() < ltrs) {
    UpdateScreen(0, 0, "No Enough water", true);
  } else {

    if ((current_price * ltrs) > cash) {
      UpdateScreen(0, 0, "Please Recharge", true);
      UpdateScreen(0, 1, "Cash: ", false);
      UpdateScreen(8, 1, String(cash), false);
    }
    else {
      current_volume = GetVolume();
      UpdateScreen(0, 0, "Pumping water", true);

      while ((current_volume - GetVolume()) <= ltrs) {
        //  Turn on the pump relay
        if (Relay_state) {
          digitalWrite(RELAY_PIN, HIGH);
          Relay_state = false;
        }
      }
      UpdateScreen(0, 1, "Scan card again", false);

      int updated_cash = cash - (current_price * ltrs);
      writeBlock(CustomerBlock[1], updated_cash);
      digitalWrite(RELAY_PIN, LOW);
      current_volume = GetVolume();
      Relay_state = true;
    }
  }
}

float GetVolume() {
  float volume = (22 / 7) * (DIAMETER * DIAMETER) * WaterLevel.ping_cm() / 4;     //PI*r2h
  return volume;
}



String read_keypad(bool Is_password = false) {
  int cursor_position = 0;
  char amount[10];
  char key;
  while (true) {
    if (KeyBoard.getKeys())
    {
      for (int i = 0; i < LIST_MAX; i++) // Scan the whole key list.
      {
        if ( KeyBoard.key[i].stateChanged )   // Only find keys that have changed state.
        {
          if (KeyBoard.key[i].kstate == PRESSED) {
            key = KeyBoard.key[i].kchar;

            if (String(key) == "#") {
              // print all pressed keys
              if (Is_password) {
                for (int i = 0; i < cursor_position; i++) {
                  amount[i] = amount[i];
                }
                return (amount);
              } else {
                for (int i = 0; i < cursor_position; i++) {
                  amount[i] = amount[i];
                  UpdateScreen(i, 1, String(amount[i]), false);
                }
                return (amount);
              }
            }
            else if (String(key) == "*") {
              cursor_position -= 1;
              UpdateScreen(cursor_position, 1, " ", false);
            }
            else {
              if (Is_password) {
                UpdateScreen(cursor_position, 1, "*", false);
              } else {
                UpdateScreen(cursor_position, 1, String(key), false);
              }

              amount[cursor_position] = key;
              cursor_position += 1;
              cursor_position = 0 ? cursor_position < 0 : cursor_position;
            }
          }
        }
      }
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

bool Authenticate() {
  Screen.clear();
  UpdateScreen(0, 0, "Enter Password: ", false);
  int pass = read_keypad(true).toInt(), stored_pass = EEPROM.read(pass_address);
  int trials = 3;

  if (stored_pass != 255) {
    //      while (trials > 0) {
    //        if (stored_pass == pass) {
    //          return true;
    //        } else {
    //          UpdateScreen(0, 0, "Enter Password: ", false);
    //          UpdateScreen(0, 1, char(trials-1)+"Remaining", false);
    //          pass = read_keypad(true);
    //        }
    //        trials -= 1;
    //      }
    //      return false;
    Screen.print(stored_pass);

  }
  else {
    Screen.clear();

    UpdateScreen(0, 0, "Set Password: ", false);
    int new_pass = read_keypad(true).toInt();

    UpdateScreen(6, 1, String(new_pass), false);
    delay(1000);
    Screen.clear();
    UpdateScreen(0, 0, "Enter Password again: ", false);

    int new_pass_1 = read_keypad(true).toInt();
    UpdateScreen(6, 1, String(new_pass_1), false);
    delay(1000);
    Screen.clear();
    if (new_pass == new_pass_1) {
      //        EEPROM.update(pass_address,new_pass);
      UpdateScreen(0, 0, "password set", false);
      delay(1000);
      return true;
    } else {
      //      set password to default
      UpdateScreen(0, 0, "Wrong password:", false);
      UpdateScreen(0, 1, "Password :" + String(1234), false);
      delay(3000);
      //      EEPROM.update(pass_address,1234);
      return true;
    }
  }
}
