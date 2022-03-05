
// Software written by: Michael Pogodin
// Contact: miker2808@gmail.com
// Last Edit: 24/12/2021

#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Wire.h>      // this is needed even tho we aren't using it
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>

#include <Fonts/FreeMonoBold12pt7b.h>

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 150
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define BLACK      0x0000
#define BLUE       0x001F
#define RED        0xF800
#define GREEN      0x07E0
#define CYAN       0x07FF
#define MAGENTA    0xF81F
#define YELLOW     0xFFE0
#define WHITE      0xFFFF
#define DARK_GRAY  0x1111
#define LIGHT_GRAY 0XCCCC

// used to remember what key was touched last iteration
int pressed_j = 0;
int pressed_i = 0;
char pressed_char = 0;
bool was_pressed = false;
String input_pass = "";
int stay_awake_counter = 250; // turn screen off when reaching 0

void draw_keypad_char(int i, int j, int input_char, int color_block, int color_text, int text_size = 2) {
  tft.fillRect(11 + j * 75, 75 + i * 60, 70, 55, color_block);
  tft.drawRect(11 - 2 + j * 75, 75 - 2 + i * 60, 70 + 2, 55 + 2, color_text);
  tft.drawChar(11 + 20 + j * 75, 75 + 42 + i * 60, input_char, color_text, 0, text_size);

}

void draw_keypad_string(int i, int j, int color_block, int color_text, String input_text, int text_size = 1) {
  tft.fillRect(11 + j * 75, 75 + i * 60, 70, 55, color_block);
  tft.drawRect(11 - 2 + j * 75, 75 - 2 + i * 60, 70 + 2, 55 + 2, BLACK);
  tft.setCursor(11 + 12 + j * 75, 75 + 35 + i * 60);
  tft.setTextColor(color_text);
  tft.setTextSize(text_size);
  tft.print(input_text);
}

void reset_screen() {
  // Clean the screen and rest to default look

  int screen_width = tft.width();
  int screen_height = tft.height();

  tft.setRotation(0);

  // fill the screen with black
  tft.fillScreen(WHITE);

  char count_char = 49;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j++) {
      if (i == 3) {
        if (j == 0) {
          draw_keypad_string(i, j, BLACK, WHITE, "DEL", 1);

        }
        else if (j == 1) {
          draw_keypad_char(i, j, 48, BLACK, WHITE);

        }
        else if (j == 2) {
          draw_keypad_string(i, j, BLACK, WHITE, "ENT", 1);
        }
      }
      else {
        draw_keypad_char(i, j, count_char, BLACK, WHITE);
        count_char += 1;
      }
    }
  }
}

void screenWrongPassword() {
  // create image that shows invalid password
  tft.fillScreen(WHITE);
  tft.setCursor(20, 150);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print("INVALID");
  tft.setCursor(10, 200);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print("PASSWORD");
  delay(2500);
  reset_screen();
  delay(500);
  stay_awake_counter = 250;

}

void screenCommError() {
  // create image that shows Comm Error
  tft.fillScreen(WHITE);
  tft.setCursor(60, 100);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print("Comm");
  tft.setCursor(50, 150);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print("Error");
  tft.setCursor(30, 250);
  tft.setTextColor(BLACK);
  tft.setTextSize(1);
  tft.print("C:0x465F550A");
  delay(2500);
  reset_screen();
  delay(500);
}

void screenDebugPrint(String input_str) {
  // create image that shows the input string (For debugging)
  tft.fillScreen(WHITE);
  tft.setCursor(10, 100);
  tft.setTextColor(BLACK);
  tft.setTextSize(1);
  tft.print(input_str);
  
}

void screenSetFinger(String finger_action){
  tft.fillScreen(WHITE);
  tft.setCursor(20, 140);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print(finger_action);
  tft.setCursor(20, 200);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print("FINGER");
  stay_awake_counter = 500;
}

void screenSetFingerResult(bool set_finger_result){
  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);
  tft.setTextSize(1);
  tft.setCursor(65, 150);
  tft.print("FINGER");
  tft.setCursor(50, 180);
  tft.print("ENROLLMENT");
  tft.setCursor(60, 210);
  tft.fillRect(48, 193, 129, 24, BLACK);
  tft.fillRect(50, 195, 125, 20, WHITE);
  if(set_finger_result == true){
    tft.print("FINISHED");
  }
  else{
    tft.print("FAILED");
  }
  
  delay(3000);
  reset_screen();
  delay(500);
  return;
  
}

void screenValidUser(String user_name) {
  // create image that shows valid finger with name ("Welcome {user_name}")
  // create image that shows invalid password
  
  tft.fillScreen(WHITE);
  tft.setCursor(20, 140);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print("WELCOME");
  tft.fillRect(0, 145, 240, 3, BLACK);

  // change cursor so text of name is in the middle
  if(user_name.length() <= 8){
    int cursor_x = 120 - ((user_name.length()/2)*28);
    tft.setCursor(cursor_x, 200);
    tft.setTextColor(BLACK);
    tft.setTextSize(2);
    tft.print(user_name);
    delay(5000);
    reset_screen();
    delay(500);
    return;
  }
  else{
    int space_index = 0;
    for(int i = 0; i < user_name.length(); i++){
      if(user_name[i] == ' ' or user_name[i] == '+'){
        space_index = i;
        break;
      }
    }
    String first_name = user_name.substring(0, space_index);
    String surname = user_name.substring(space_index+1);
    int cursor_x_first = 120 - ((first_name.length()/2)*28);
    int cursor_x_surname = 120 - ((surname.length()/2)*15);
    // print first name
    tft.setCursor(cursor_x_first, 200);
    tft.setTextColor(BLACK);
    tft.setTextSize(2);
    tft.print(first_name);
    // print surname
    tft.fillRect(0, 210, 240, 30, BLACK);
    tft.setCursor(cursor_x_surname, 230);
    tft.setTextColor(WHITE);
    tft.setTextSize(1.5);
    tft.print(surname);
    delay(5000);
    reset_screen();
    delay(500);
    return;
  }
}

void handleReceivingCommand() {
  if (Serial.available() > 0) {
    String received_input = Serial.readStringUntil('\n');
    if(received_input.startsWith("PSWD:GOOD")) {
      digitalWrite(3, HIGH);
      screenValidUser("GUEST");
    }
    else if(received_input.startsWith("PSWD:BAD")) {
      digitalWrite(3, HIGH);
      screenWrongPassword();
    }

    else if(received_input.startsWith("PSWD:LOCK")) {
      digitalWrite(3, HIGH);
      screenCommError();
    }

    else if (received_input.startsWith("FNGR:")) {
      digitalWrite(3, HIGH);
      String user_name = received_input.substring(5);
      screenValidUser(user_name);
    }

    else if(received_input.startsWith("SMSG:SETFNGR-PLACE")){
      screenSetFinger("PLACE");
    }
    else if(received_input.startsWith("SMSG:SETFNGR-RELEASE")){
      screenSetFinger("RELEASE");
    }
    else if(received_input.startsWith("SMSG:SETFNGR-FINISH")){
      screenSetFingerResult(true);
    }
    else if(received_input.startsWith("SMSG:SETFNGR-FAILED")){
      screenSetFingerResult(false);
    }
    
  }
  Serial.flush();
}

void setup() {

  // delay to fix tft not initiating without USB
  delay(2000);

  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  // object for LCD screen (To control pixels)
  tft.begin();

  // object for touch controller (To read touch input)
  if (!ts.begin()) {
    //Serial.println("Couldn't start touchscreen controller");
    // set reboot
  }
  //Serial.println("Touchscreen started");

  tft.setFont(&FreeMonoBold12pt7b);

  reset_screen();

  Serial.begin(115200);

}


void loop()
{
  if(stay_awake_counter > 0){
    stay_awake_counter -= 1;
    digitalWrite(3, HIGH);
  }
  if(stay_awake_counter <= 0){
    // Turns the screen OFF (physically turn off backlight), touch will still work
    tft.fillScreen(BLACK);
    digitalWrite(3, LOW);
  }
  handleReceivingCommand(); // used to check the serial to know if any commands arrived
  //TS_Point p = ts.getPoint();
  // See if there's any  touch data for us, cleans buffer as well
  if (!ts.bufferEmpty()) {
    digitalWrite(3, HIGH);
    if(stay_awake_counter <= 0){
      reset_screen();
      TS_Point p = ts.getPoint(); // to clean buffer
      stay_awake_counter = 250;
      input_pass = "";
      delay(500);
      return;
    }
    stay_awake_counter = 250;
    
    char pressed_value = 0;
    // Retrieve a point
    TS_Point p = ts.getPoint();

    // Scale from ~0->4000 to tft.width using the calibration #'s
    p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
    p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

    char count_char = 49;
    // The loop checks if one of the 12 keys was pressed.
    // i = col
    // j = row
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 3; j++) {
        if (p.x > (11 + j * 75) and p.x < (11 + j * 75 + 70) and p.y > (75 + i * 60) and p.y < (75 + i * 60 + 55)) {
          if (i == 3) {
            if (j == 0) { // check if the Delete "<" button was pressed
              draw_keypad_string(i, j, WHITE, BLACK, "DEL", 1);
              pressed_j = j;
              pressed_i = i;
              pressed_char = 60; // equal to char '<'
              was_pressed = true;
              break;
            }
            else if (j == 1) { // check if the 0 button was pressed
              draw_keypad_char(i, j, 48, WHITE, BLACK);
              pressed_j = j;
              pressed_i = i;
              pressed_char = 48;
              was_pressed = true;
              break;
            }
            else if (j == 2) { // check if the SEND ">" button was pressed
              draw_keypad_string(i, j, WHITE, BLACK, "ENT", 1);
              pressed_j = j;
              pressed_i = i;
              pressed_char = 62; // equal to char '>'
              was_pressed = true;
              break;
            }
          }
          else { // otherwise
            draw_keypad_char(i, j, count_char, WHITE, BLACK);
            // reset parameters if pressed
            pressed_j = j;
            pressed_i = i;
            pressed_char = count_char;
            was_pressed = true;
            break;
          }
        }
        count_char += 1;
      }
    }

    // continue if pressed
  }

  else {
    if (was_pressed) {
      // check what key was pressed to know how to restore its view (reset_screen() is too slow)
      if (pressed_i == 3 and (pressed_j == 0 or pressed_j == 2)) {
        if (pressed_j == 0) {
          draw_keypad_string(pressed_i, pressed_j, BLACK, WHITE, "DEL", 1);
        }
        else if (pressed_j == 2) {
          draw_keypad_string(pressed_i, pressed_j, BLACK, WHITE, "ENT", 1);
        }
      }
      else {
        draw_keypad_char(pressed_i, pressed_j, pressed_char, BLACK, WHITE);
      }

      // on "DEL" released:
      if (pressed_i == 3 and pressed_j == 0) {
        //mask the input with a white rectangle
        tft.fillRect(0, 0, 240, 75, WHITE);
        // remove one character from the password
        input_pass.remove(input_pass.length() - 1);
        // print the new password again
        tft.setCursor(30, 50);
        tft.setTextColor(BLACK);
        tft.setTextSize(2);
        tft.print(input_pass);
      }
      // on "ENT" released:
      else if (pressed_i == 3 and pressed_j == 2 and input_pass.length() > 0) {
        Serial.println(String("PSWD:" + input_pass));
        input_pass = "";
        //mask the input with a white rectangle
        tft.fillRect(0, 0, 240, 75, WHITE);
        //reset_screen();
        delay(50);
      }
      // on any other key released:
      else {
        if (input_pass.length() < 6 and pressed_char != '<' and pressed_char != '>') {
          input_pass += pressed_char;
          tft.setCursor(30, 50);
          tft.setTextColor(BLACK);
          tft.setTextSize(2);
          tft.print(input_pass);
        }
      }
      was_pressed = false;
    }
    // continue no touch received
    // if data is available in buffer, do accordingly
    //handleReceivingCommand();
  }
  // out of touch bounds

  delay(20);
}
