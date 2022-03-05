//MAC address: A4:CF:12:8B:D6:34

// Software written by: Michael Pogodin
// Contact: miker2808@gmail.com
// Last Edit: 24/12/2021

#include <SPI.h> // Came with Wifi libraries
#include <WiFiNINA.h> // used for wifi
#include <rBase64.h> // used to decode base64 (not in use)
#include <FlashStorage.h> // used to store user database and passwords
#include <Arduino.h> // used for hardware serial (communicate with touchpad arduino)
#include "wiring_private.h" // used for hardware serial (communicate with touchpad arduino)
#include <Adafruit_Fingerprint.h>

// struct of the http request
struct httpRequest{
  String httpmethod= ""; // returns line of GET or POST
  String httpredirect = ""; // returns direction of GET or POST
  String data = ""; // returns data if POST
  String username = ""; // from authorization
  String password = ""; // from authorization
};

// struct of the database
struct dataBase{
  bool not_first_run;
  bool lockdown_mode;
  int door_password;
  char web_password[25];
  char user_name[52][25]; 
};

//// used for the hardware serial
// used to assign pin 5,6 as hardware serial
Uart keypadSerial (&sercom0, 5, 6, SERCOM_RX_PAD_1, UART_TX_PAD_0);
// Attach the interrupt handler to the SERCOM
void SERCOM0_Handler()
{
    keypadSerial.IrqHandler();
}


/////// Connecting wifi information (keyIndex is not used for WPA)
char ssid[] = "";    // your network SSID (name)
char pass[] = "";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                // your network key index number (needed only for WEP)

int status = WL_IDLE_STATUS;
WiFiServer server(80);

// define Serial1 (Rx,Tx pins) to fingerSerial (easier for work)
#define fingerSerial Serial1
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// create storage location for the user database and create user_base out of struct
FlashStorage(userBase_storage, dataBase);
dataBase user_base; // initialize structure of user database

void setup() {
  delay(1000);
  // Reassign pins 5 and 6 to SERCOM alt
  pinPeripheral(5, PIO_SERCOM_ALT);
  pinPeripheral(6, PIO_SERCOM_ALT);
  // Start assigned hardware serial
  keypadSerial.begin(115200);
  
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  }
  else{
    Serial.println("Did not find fingerprint sensor!");  
  }
  
  
  // set the digital pin to control opening the door
  pinMode(8, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(8, HIGH);

  // assign pin for the physical button responsible for opening the door
  pinMode(7, INPUT_PULLUP);
  
  user_base = userBase_storage.read(); // assign to user_base the data from flash storage
  
  // check if value was already assigned to flash
  if(user_base.not_first_run == false){
    Serial.print("First run after flashing, assigning default passwords - ");
    Serial.println("door: 321321, web:123123");
    // assign values for first setup (after flash)
    user_base.lockdown_mode = false;
    user_base.not_first_run = true;
    user_base.door_password = 321321;
    String default_web_password = "123123";
    default_web_password.toCharArray(user_base.web_password, 25);

    Serial.println("Asigning default User: Admin - ID: 51");
    // assigning default first user: (Protected)
    String default_user_name = "Admin";
    default_user_name.toCharArray(user_base.user_name[51], 25);

    // write the data to flash
    userBase_storage.write(user_base);
  }
  Serial.print("Current assigned passwords: web: ");
  Serial.print(user_base.web_password);
  Serial.print(", door: ");
  Serial.println(user_base.door_password);
  Serial.println("RouterLogin Webserver");

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Wifi fimrware update available. \nPlease upgrade the firmware");
  }

  // Configure device IP addres
   WiFi.config(IPAddress(10, 0, 40, 250));

  // print the network name (SSID);
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);

  // Create open network. Change this line if you want to create an WEP network:
  for(int i = 0; i < 3; i++){
    status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED) {
      Serial.println("Failed connecting to wifi");
      Serial.println("Retrying in 2 seconds:");
      delay(2000);
      Serial.println("Retrying");
    }
    else{
      // wait 5 seconds for connection:
      delay(5000);
    
      // start the web server on port 80
      server.begin();
    
      // you're connected now, so print out the status
      printWiFiStatus();
      break;
    }
  }
  if(status != WL_CONNECTED) {
    Serial.println("Failed connecting to wifi 3/3, going offline mode");
  }
  
}

int open_door_counter = 0; // close the door when value is 0 or smaller
int lockdown_engaged = false;
int inner_unlock_button_value = 0;
void loop() {
  // compare the previous status to the current status
  delay(50); // having a fixed delay is good

  inner_unlock_button_value = digitalRead(7);

  if(inner_unlock_button_value == LOW){
    open_door_counter = 100;
  }

  // if / else statement used to check how much is left for the door to stay open
  // if door counter reaches 0, the door will lock again
  if(open_door_counter > 0){
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(8, LOW);
    open_door_counter -= 1;
    // handle receiving fingerprint
    
  }
  else{
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(8, HIGH);
    handleFingerPrintReading(finger, user_base);
  }
  
  if (status != WiFi.status()) {
    // it has changed update the variable
    status = WiFi.status();
  }

  // handle receiving password input
  handleTouchScreenPassInput(user_base);
  
  if(status == WL_CONNECTED){
    WiFiClient client = server.available();   // listen for incoming clients
    if (client) {                             // if you get a client,
      Serial.println("new client");           // print a message out the serial port
      httpRequest client_request;
      String currentLine = "";
      while (client.connected()) {            // loop while the client's connected
        delay(10);                // This is required for the Arduino Nano RP2040 Connect - otherwise it will loop so fast that SPI will never be served.
        if (client.available()) {             // if there's bytes to read from the client,
          client_request = ParseHttpRequest(client);
          Serial.println("METHOD: " + client_request.httpmethod);
          Serial.println("DIRECTION: " + client_request.httpredirect);
          Serial.println("DATA: " + client_request.data);
          if(client_request.httpmethod == "GET"){
            if(client_request.httpredirect.startsWith("/")){
              printLoginPage(client, "");
              break;
            }
          }
          else if(client_request.httpmethod == "POST"){
            if(client_request.data.startsWith("pw_input=")){
              String check_password_concatenated = "pw_input=";
              check_password_concatenated.concat(user_base.web_password);
              Serial.print("Expected password: ");
              Serial.println(check_password_concatenated);
              if(client_request.data == check_password_concatenated){
                printMainScreen(client);
                break;
              }
              else{
                printLoginPage(client, "Invalid Password");
                break;
              }
            }
            else if(client_request.data == "goto=enrolluser"){
              printEnrollFinger(client, user_base, "");
              break;
            }
            else if(client_request.data == "goto=deleteuser"){
              printDeleteFinger(client, user_base, "");
              break;
            }
            else if(client_request.data == "goto=mainscreen"){
              printMainScreen(client);
              break;
            }
            else if(client_request.data == "goto=changepass"){
              printChangePass(client, "");
              break;
            }

            else if(client_request.data == "goto=lockdown"){
              if(lockdown_engaged == true){
                lockdown_engaged = false;
              }
              else{
                lockdown_engaged = true;
              }
              printMainScreen(client);
              break;
            }
            
            else if(client_request.data == "goto=opendoor"){
              open_door_counter = 100;
              digitalWrite(8, LOW); // do it before printing the client back to speed up the process
              keypadSerial.print("PSWD:");
              keypadSerial.println("GOOD");
              printMainScreen(client);
              break;
            }
  
            else if(client_request.httpredirect.startsWith("/enrolluser")){
              if(client_request.data.startsWith("user_id")){
                int user_id = extractUserID(client_request.data);
                String user_name = extractUserName(client_request.data);
                if((user_id > 0 and user_id < 51) and (user_name.length() < 21 and user_name.length() > 0)){
                  //given valid ID and username assign to database
                  int status_good = 1;
                  status_good = assignFingerPrint(finger, user_id);
                  
                  Serial.print("Status of finger: " + status_good);
                  if(status_good){
                    user_name.toCharArray(user_base.user_name[user_id], 25);
                    userBase_storage.write(user_base);
                    printEnrollFinger(client, user_base, "Fingerprint enrollment successful");
                    keypadSerial.print("SMSG:");
                    keypadSerial.println("SETFNGR-FINISH");
                    delay(100);
                    break;
                  }
                  printEnrollFinger(client, user_base, "Fingerprint enrollment failed, try again");
                  keypadSerial.print("SMSG:");
                  keypadSerial.println("SETFNGR-FAILED");
                  delay(100);
                  break;
                  
                }
                // invalid input received, return to with error
                printEnrollFinger(client, user_base, "Fingerprint enrollment failed, try again");
                keypadSerial.print("SMSG:");
                keypadSerial.println("SETFNGR-FAILED");
                delay(100);
                break;
              }
            }
            else if(client_request.httpredirect.startsWith("/deleteuser")){
              if(client_request.data.startsWith("user_id")){
                String user_id_str = client_request.data.substring(8);
                int user_id = user_id_str.toInt();
                
                if(user_id > 0 and user_id < 51){
                  Serial.print("ok.. deleting fp");
                  // perform deletion of fingerprint
                  int status_good = 0;
                  status_good = deleteFingerprint(finger, user_id);
                  if(status_good == 1){
                    // empty the name at user_id index
                    String empty_string = "";
                    empty_string.toCharArray(user_base.user_name[user_id], 25);
                    userBase_storage.write(user_base);
                    printDeleteFinger(client, user_base, "Fingerprint deleted successfully");
                    break;
                  }
                  else{
                    printDeleteFinger(client, user_base, "Fingerprint deletion failed, try again");
                    break;
                  }
                }
                printDeleteFinger(client, user_base, "Fingerprint deletion failed, try again");
                break;
              }
            }
            
            else if(client_request.data.startsWith("webpass")){
              String web_password = client_request.data.substring(8);
              if(web_password.length() > 0 and web_password.length() < 21){
                web_password.toCharArray(user_base.web_password, 25);
                userBase_storage.write(user_base);
                printChangePass(client, "Changed website password successfully");
                break;
              }
              else{
                printChangePass(client, "Failed to change website password");
                break;
              }
            }
            else if(client_request.data.startsWith("doorpass")){
              String door_password_str = client_request.data.substring(9);
              int door_password = door_password_str.toInt();
              if((door_password > 0 and door_password < 1000000) and (door_password_str.length() <= 6 and door_password_str.length() > 0)){
                user_base.door_password = door_password;
                userBase_storage.write(user_base);
                printChangePass(client, "Changed door password successfully");
                break;
              }
              else{
                printChangePass(client, "Failed to change door password");
                break;
              }
            }
            
            else{
             
            }
          }
          else{
            break;
          }
        }
        else{
          break;
        }
      }
      // close the connection:
      client.stop();
      Serial.println("client disconnected");
    }
  }
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
  
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);

}

String readWifiLine(WiFiClient &client){
  String output_string = "";
  while(client.available()){
    char c = client.read();
    if(c == '\n'){
      return output_string;
    }
    else if(c != '\r'){
      output_string += c;
    }
  }
  return output_string;
}

int extractUserID(String post_data){
  int index_of_ampersand = post_data.indexOf('&');
  if(index_of_ampersand > 0){
    int index_of_equal = post_data.indexOf('=');
    if(index_of_equal != -1){
      String user_id_str = post_data.substring(index_of_equal+1, index_of_ampersand);
      int user_id = user_id_str.toInt();
      return user_id;
    }
  }
  
  return -1;
}

String extractUserName(String post_data){
  int index_of_ampersand = post_data.indexOf('&');
  if(index_of_ampersand > 0){
    int index_of_equal = post_data.indexOf('=', index_of_ampersand);
    if(index_of_equal != -1){
      String user_name = post_data.substring(index_of_equal+1);
      return user_name;
    }
  }
  return "invalid";
}

httpRequest ParseHttpRequest(WiFiClient &client){
  httpRequest client_request;
  String start_key = "DATA=&"; // key to assign start of post data
  while(client.available()){
    String currentLine = readWifiLine(client);
    Serial.println(currentLine);
    if(currentLine.startsWith("GET /")){
      client_request.httpmethod = "GET";
      client_request.httpredirect = currentLine.substring(4, currentLine.length()-9);
      return client_request;
    }
    else if(currentLine.startsWith("POST /")){
      client_request.httpmethod = "POST";
      client_request.httpredirect = currentLine.substring(5, currentLine.length()-9);
    }
    if(client_request.httpmethod != ""){
      if(client_request.httpmethod == "POST"){
        if(currentLine.startsWith(start_key)){
          client_request.data = currentLine.substring(6);
        }
      }
    }
  }
}


int assignFingerPrint(Adafruit_Fingerprint &finger, int finger_id){
  //return what step you completed
  int finger_status = -1;
  int iter_counter = 0;
  // wait for finger press (first count);
  Serial.println("Place desired finger on the fingerprint scanner");
  keypadSerial.print("SMSG:");
  keypadSerial.println("SETFNGR-PLACE");
  delay(100);
  while(true){
    delay(20);
    finger_status = finger.getImage();
    if(finger_status == FINGERPRINT_OK){
      finger_status = finger.image2Tz(1);
    
      if(finger_status == FINGERPRINT_OK){
        Serial.println("Remove Finger");
        delay(1000);
        break;
      }
    }
    iter_counter += 1;
    if(iter_counter > 500){
      return 0;
    }
  }

  finger_status = 0;
  iter_counter = 0;
  // wait for fingerprint release
  keypadSerial.print("SMSG:");
  keypadSerial.println("SETFNGR-RELEASE");
  delay(100);
  while(finger_status != FINGERPRINT_NOFINGER){
    delay(20);
    finger_status = finger.getImage();
    iter_counter += 1;
    if(iter_counter > 500){
      return 0;
    }
  }

  iter_counter = 0;
  // wait for fingerprint press (second count)
  Serial.println("Place the same finger again");
  keypadSerial.print("SMSG:");
  keypadSerial.println("SETFNGR-PLACE");
  delay(100);
  while(true){
    delay(20);
    
    finger_status = finger.getImage();
    
    if(finger_status == FINGERPRINT_OK){
      finger_status = finger.image2Tz(2);
      if(finger_status != FINGERPRINT_OK){
        Serial.println("Failed to read second fingerprint");
        return 0;
      }
      finger_status = finger.createModel();
      if(finger_status != FINGERPRINT_OK){
        Serial.println("Failed to create fingerprint model");
        return 0;
      }
      finger_status = finger.storeModel(finger_id);
      if(finger_status != FINGERPRINT_OK){
        Serial.println("Failed to store fingerprint");
        return 0;
      }
      return 1;
    }
    iter_counter += 1;
    if(iter_counter > 500){
      return 0;
    }
  }
}


int deleteFingerprint(Adafruit_Fingerprint &finger, int finger_id){
  int finger_status = -1;
  finger_status = finger.deleteModel(finger_id);
  if(finger_status == FINGERPRINT_OK){
    return 1;
  }
  else{
    return -1;
  }
}


void printLoginPage(WiFiClient &client, String response){
  // refer tot he web src, html compressor was used for the source code

  String html ="<!DOCTYPE HTML><html> <head> <title>Door Lock WebServer</title> <meta name=viewport content=\"width=device-width, initial-scale=1\"/> <style type=text/css>*{font-family:Arial;font-weight:bold;font-size:16px;background-color:#292929;color:#007b94}p{color:palevioletred}form{display:inline-block}body{text-align:center;margin:15% auto}input[type=submit]{background-color:#007b94;color:white;border:double;border-radius:5px;padding:10px 20px;text-align:center;text-decoration:none;display:inline-block}input[type=submit]:hover{background-color:palevioletred}input[type=submit]:active{border-color:aqua}input[type='password']{background-color:#007b94;color:white;border:1;border-color:aqua;border-radius:5px;height:30px}input[type='password']:hover{border-color:palevioletred}</style> </head> <body> <h5>Input Password</h5> <form id=pw_insert action=/login method=post> <input type=hidden name=DATA> <input type=password name=pw_input> <br><br> <input type=submit value=Login>";

  client.println(html);

  client.println("<p>" + response + "</p>");

  String html_end ="</form><br> </body> </html>";

  client.println(html_end);
}


void printMainScreen(WiFiClient &client){
  String html ="<!DOCTYPE HTML><html> <head> <title>Door Lock WebServer</title> <meta name=viewport content=\"width=device-width, initial-scale=1\"/> <style type=text/css>*{font-family:Arial;font-weight:bold;font-size:16px;background-color:#292929;color:#007b94}form{display:inline-block}body{text-align:center;margin:15% auto}input[type=submit]{background-color:#007b94;color:white;border:double;border-radius:5px;padding:10px 20px;width:220px;height:50px;text-align:center;text-decoration:none;display:inline-block}input[type=submit]:hover{background-color:palevioletred}.title{font-size:25px;text-decoration:underline}</style> </head> <body> <p class=title>Door Lock</p> <form id=opendoor action=/opendoor method=post> <input type=hidden name=DATA> <input type=hidden name=goto value=opendoor> <input type=submit value=\"Open The Door\" style=border:dashed> </form> <br><br><br> <form id=enrolluser action=/enrolluser method=post> <input type=hidden name=DATA> <input type=hidden name=goto value=enrolluser> <input type=submit value=\"Enroll New User\"> </form> <br><br> <form id=deleteuser action=/deleteuser method=post> <input type=hidden name=DATA> <input type=hidden name=goto value=deleteuser> <input type=submit value=\"Delete Existing User\"> </form> <br><br> <form id=changepass action=/changepass method=post> <input type=hidden name=DATA> <input type=hidden name=goto value=changepass> <input type=submit value=\"Manage Passwords\"> </form> <br><br> <form id=lockdown action=/lockdown method=post> <input type=hidden name=DATA> <input type=hidden name=goto value=lockdown>";

  client.println(html);
  if(lockdown_engaged == true){
    String html_end ="<input type=\"submit\" value=\"Lockdown: Engaged\"> </form> <br><br> </body> </html>";
    client.println(html_end);
    return;
  }
  else{
    String html_end ="<input type=\"submit\" value=\"Lockdown: Lifted\"> </form> <br><br> </body> </html>";
    client.println(html_end);
    return;
  }
}

void printEnrollFinger(WiFiClient &client, dataBase &user_base, String response){
  // draw header + css
  String html ="<!DOCTYPE HTML><html> <head> <title>Door Lock WebServer</title> <meta name=viewport content=\"width=device-width, initial-scale=1\"/> <style type=text/css>*{font-family:Arial;font-weight:bold;font-size:16px;background-color:#292929;color:#007b94;box-sizing:border-box;margin:5px auto}form{display:inline-block}body{text-align:center;margin:15% auto}input[type=submit]{background-color:#007b94;color:white;border:double;border-radius:5px;padding:10px 20px;width:100%;height:40px;text-align:center;text-decoration:none;margin:10px auto}input[type=submit]:hover{background-color:palevioletred}.input_bar{background-color:#007b94;color:white;border-bottom:solid;border-color:aqua;border-radius:5px;height:40px;width:250px;padding:10px 20px;text-align:center}.input_bar:hover{border-color:palevioletred}.title{font-size:25px;text-decoration:underline}table,th,td{border:2px solid}.table{width:100%;text-align:left}::placeholder{color:white;text-align:center}</style> </head> <body> <p style=width:90%>Enroll a new user fingerprint</p> <form id=submit_form action=/enrolluser method=post style=justify-content:center> <input type=hidden name=DATA> <input class=input_bar type=number name=user_id min=1 max=50 step=1 required placeholder=\"User ID\"> <br> <input class=input_bar type=text name=user_name minlength=1 maxlength=20 required placeholder=\"User Name\"> <br> <input type=submit value=\"Start Enrollment\"> <br> <p style=text-decoration:underline>After pressing the button</p> <p>1. Press your finger on the scanner</p> <p>and hold for 3 seconds</p> <p>2. Remove your finger</p> <p>3. Press your finger again</p> <p style=text-decoration:underline> System will wait 10 seconds </p> <p> before timing out</p> <br> <table class=table> <tr> <th>User ID</th> <th>User Name</th> </tr>";

  client.println(html);
  //print row of user data assigned
  for(int i=0; i <= 50; i++){
    size_t name_length = strlen(user_base.user_name[i]);
    if(name_length > 0){
      client.print("<tr><th>");
      client.print(i);
      client.print("</th><th>");
      client.print(user_base.user_name[i]);
      client.println("</th></tr>");
    }
  }

  String html_end ="</table> </form> <br> <form action=\"/usermanager\" method=\"post\" style=\"margin-right:3%\"> <input type=\"hidden\" name=\"DATA\"> <input type=\"hidden\" name=\"goto\" value=\"mainscreen\"> <input type=\"submit\" value=\"Home\" style=\"margin:10px\"> </form>";

  client.println(html_end);
  
  client.println("<p>" + response + "</p></body></html>");

  
}

void printDeleteFinger(WiFiClient &client, dataBase &user_base, String response){
  // draw header + css

  String html ="<!DOCTYPE HTML><html> <head> <title>Door Lock WebServer</title> <meta name=viewport content=\"width=device-width, initial-scale=1\"/> <style type=text/css>*{font-family:Arial;font-weight:bold;font-size:16px;background-color:#292929;color:#007b94;box-sizing:border-box;margin:5px auto}form{display:inline-block}body{text-align:center;margin:15% auto}input[type=submit]{background-color:#007b94;color:white;border:double;border-radius:5px;padding:10px 20px;width:100%;height:40px;text-align:center;text-decoration:none;margin:10px auto}input[type=submit]:hover{background-color:palevioletred}.input_bar{background-color:#007b94;color:white;border-bottom:solid;border-color:aqua;border-radius:5px;height:40px;width:250px;padding:10px 20px;text-align:center}.input_bar:hover{border-color:palevioletred}.title{font-size:25px;text-decoration:underline}table,th,td{border:2px solid}.table{width:100%;text-align:left}::placeholder{color:white;text-align:center}</style> </head> <body> <p>Delete a fingerprint</p> <form action=/deleteuser method=post style=justify-content:center> <input type=hidden name=DATA> <input class=input_bar type=number name=user_id min=1 max=50 step=1 required placeholder=\"User ID\"> <br> <input type=submit value=\"Delete Finger Print\"> <br><br><br> <table class=table> <tr> <th>User ID</th> <th>User Name</th> </tr>";

  client.println(html);
  //print row of user data assigned
  for(int i=0; i <= 50; i++){
    size_t name_length = strlen(user_base.user_name[i]);
    if(name_length > 0){
      client.print("<tr><th>");
      client.print(i);
      client.print("</th><th>");
      client.print(user_base.user_name[i]);
      client.println("</th></tr>");
    }
  }
  
  //finish html
  String html_end ="</table> </form> <br> <form action=\"/usermanager\" method=\"post\" style=\"margin-right:2%\"> <input type=\"hidden\" name=\"DATA\"> <input type=\"hidden\" name=\"goto\" value=\"mainscreen\"> <input type=\"submit\" value=\"Home\" style=\"margin:10px\"> </form>";

  client.println(html_end);
  client.println("<p>" + response + "</p></body></html>");
}


void printChangePass(WiFiClient &client, String response){
  // draw header + css

  String html ="<!DOCTYPE HTML><html> <head> <title>Door Lock WebServer</title> <meta name=viewport content=\"width=device-width, initial-scale=1\"/> <style type=text/css>*{font-family:Arial;font-weight:bold;font-size:16px;background-color:#292929;color:#007b94;box-sizing:border-box;margin:5px auto}form{display:inline-block}body{text-align:center;margin:15% auto}input[type=submit]{background-color:#007b94;color:white;border:double;border-radius:5px;padding:10px 20px;width:100%;height:40px;text-align:center;text-decoration:none;margin:10px auto}input[type=submit]:hover{background-color:palevioletred}.input_bar{background-color:#007b94;color:white;border-bottom:solid;border-color:aqua;border-radius:5px;height:40px;width:100%;padding:10px 20px;text-align:center}.input_bar:hover{border-color:palevioletred}::placeholder{color:white;text-align:center}</style> </head> <body> <p>Assign New Passwords</p> <form class=manageuser action=/changepass method=post> <input type=hidden name=DATA> <input class=input_bar type=text name=webpass maxlength=20 required placeholder=\"Insert Web Password\"> <br> <input type=submit value=\"Change Password\"> </form> <br> <br> <form class=manageuser action=/changepass method=post> <input type=hidden name=DATA> <input class=input_bar type=text name=doorpass maxlength=6 minlength=4 required placeholder=\"Insert Door Password\" pattern=[0-9]{1,6}> <br> <input type=submit value=\"Change Password\"> </form> <br>";

  client.print(html);
  // print response
  client.println("<p>" + response + "</p>");

  //finish html
  String html_end ="<form action=\"/changepass\" method=\"post\"> <input type=\"hidden\" name=\"DATA\"> <input type=\"hidden\" name=\"goto\" value=\"mainscreen\"> <input type=\"submit\" value=\"Home\" style=\"margin:10px\"> </form> </body> </html>";

  client.print(html_end);
}

void handleFingerPrintReading(Adafruit_Fingerprint &finger, dataBase &user_base){
  int finger_status = finger.getImage();
  if(finger_status == FINGERPRINT_OK){
    
    finger_status = finger.image2Tz();
    if(finger_status != FINGERPRINT_OK){
      return;
    }
    finger_status = finger.fingerSearch();
    if(finger_status != FINGERPRINT_OK){
      return;
    }
    int finger_id = finger.fingerID;
    if(finger.confidence < 50){
      return;
    }
    Serial.print("Debug: finger_id: ");
    Serial.println(finger_id);
    if(finger_id > 0 and finger_id <= 51){
      size_t name_length = strlen(user_base.user_name[finger_id]);
      if(name_length > 0){
        // found a match and a fingerprint..
        Serial.print("Found User: ");
        Serial.println(user_base.user_name[finger_id]);
        open_door_counter = 100;
        keypadSerial.print("FNGR:");
        keypadSerial.println(user_base.user_name[finger_id]);
        delay(100);
        return;
      }
    }
    
  }
}

void handleTouchScreenPassInput(dataBase &user_base){
  if(keypadSerial.available() > 0){
    String received_input = keypadSerial.readStringUntil('\n');
    Serial.println(received_input);
    if(received_input.startsWith("PSWD:")){
      String input_password_str = received_input.substring(5);
      int input_password = input_password_str.toInt();
      if(lockdown_engaged == true){
        Serial.println("PSWD:LOCK");
        keypadSerial.print("PSWD:");
        keypadSerial.println("LOCK");
        delay(100);
        return;
      }
      else if(input_password == user_base.door_password){
        // send to door "access granted, blabla"
        open_door_counter = 100;
        Serial.println("PSWD:GOOD");
        keypadSerial.print("PSWD:");
        keypadSerial.println("GOOD");
        delay(100);
        return;
      }
      else{
        // tell user wrong password
        Serial.println("PSWD:BAD");
        keypadSerial.print("PSWD:");
        keypadSerial.println("BAD");
        delay(100);
        return;
      }
    }
  }
}

/*
void printInvalid(WiFiClient &client){
  client.println("HTTP/1.1 400 Bad Request");
  client.println();
}


void requestAuthenticate(WiFiClient &client){
  // request authentication
  client.println("HTTP/1.1 401 Unauthorized");
  client.println("WWW-Authenticate: Digest realm=\"Website1123\"");
}

*/
