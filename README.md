# Arduino Web Server Door Lock

---
## Introduction

### <ins>Behind the project</ins>:

This is a showcase project I've made as a request, while doing this project I learnt a lot new, this project will contain everything required to replicate the exact setup. but, can also be used to learn a lot about arduino, especially the new very powerful Arduino Nano 33 IoT.

### <ins>About the project</ins>:

This project is a door lock, commonly used in work offices. The doorlock uses the Arduino Nano 33 IoT as the main computer for the doorlock, it manages the database of allowed users, stores the door password, reads fingerprint sensor feedback and touchscreen feedback to determine when and whether to unlock the door. Additionally to these tasks, the main access point of the doorlock is a webserver that can be accessed by writing the arduino's IP address in any browser. Through the web gui, the user can change touchscreen door password, assign new fingerprints and corresponding owner name, and revoke fingerprint access if necessary, and to remotely unlock the door from a distance if required. The locking mechanism is a basic electromagnetic lock that magentises when the door is in "locked" state and demagnetises when the door is unlocked.

---
## Features

### <ins>Door Lock WebServer</ins>:

The main feature of this project, is the webserver, which is the Arduino Nano 33 IoT. The arduino holdes multiple features, being the central unit for handling all the operations and simultaniously acting as a webserver.

#### <ins>Main Page</ins>:

![Main Web Page](https://i.imgur.com/mx1m2Vp.png)

The main page, consists of choosing options, designed to be comfortable to use both 
on computers and on mobile.

#### <ins>Enroll New User</ins>:

![Enroll New User](https://i.imgur.com/64ImHBf.png)

Enrollment of new users is done by assigning an integer ID from 1 to 50
and a name of the fingerprint owner.
The ID can be used later to remove the fingerprint from the system. The name will be displayed
with a welcoming sign on the touchscreen when a user enters the using a fingerprint
The table under the instructions will contain the list of already assigned ID's and matching names. the data is fetched from the arduino, and is kept in the device after shutting down the arduino as emulated EEPROM using flash.

#### <ins>Remove user</ins>:

![Delete User](https://i.imgur.com/I3NS1Vf.png)

Assigning ID will remove the user from the system, the table will contain all the users and their ID's.

#### <ins>Changing Passwords</ins>:
![Change Password](https://i.imgur.com/og014PN.png)

This window allows changing passwords, one for the web gui login page, another for the doorlock touch screen pin.
The web password can characters and numbers, while the door password will hold only integers.

>All the data handling is done using custom structs built to hold the data, and then stored in the arduino as emulated ROM.

---
## The server structure

### <ins>The Setup</ins>:
The arduino nano 33 iot, which acts as the webserver works in the following order:
1. <ins>Setup</ins>: The arduino will initialize by checking the flash memory if a database exists
    with the "not_first_run" flag raised or not.
    If the flag is False, the server will create a database struct and write it to the flash.
    The server will assign default web and door password which can be changed on the web gui.

    After the database setup, the arduino will proceed to attempt wifi connection. on successful connection the arduino will continue to the loop. The arduino will have 3 connection attempts before switching to offline mode, which hold the same function as the online mode but without the server listeing to wireless connections. The fingerprint and touch keypad will continue to function as usual.

2. <ins>Lock Counter</ins>:
    The arduino cycles at fixed delay. Because having the door open exactly for 5 seconds is not very important and we can allow error margin of half a second, having a fixed delay allows us to count time by cycles rather than accessing the internal clock and running delta's. the door lock couter will enable the magnetic lock (thus, locking the door) when it reaches 0, if the decides to unlock the door, it'll set the counter to 100, therefore allowing 5 seconds of unlocked time, with a slight error depending on the loop.

3. <ins>Fingerprint handler</ins>:
   The next step will be to give a window to the fingerprint serial connection to see if we got a finger placement on the fingerprint sensor. If we indeed, do have fingerprint placement, the function will proceed to checking if such a fingerprint was assigned and is in the system. The hash of the fingeprint is done internally in the fingerprit sensor, the adafruit library will return only the ID of the hash if it is positive. the number of prints possible in the sensor is dependent ont he sensor, the one in use allows up to 80, some allow 300 and more.
   Furthermore, if the fingerprint sensor does have a positive ID, the server will compare its ID with the database ID to find a matching name, if it exists it will send the name to the touchscreen arduino for a welcoming message. Otherwise, nothing will happen.

4. <ins>Touchscreen handler</ins>:
   This time, the touchscreen receives time window to read if we have password input from the touchscreen. On positive password match, the server will send a welcoming message for a guest.

5. <ins>Web server handling</ins>:
   The main part of the loop, most of the time it is not used, but is of the most importance beacuse of how fragile it is. The webserver will read for available bytes from the wireless module buffer, if available will will parse the data by lines using hand made http parser. The parser will check the message and assign the data to a http struct that simplifies the construction of the data, if GET is received the method is assinged as GET, for POST, assigned POST. if POST http method is given, the parser will assign to the data varialbe as well.
   
   After successful parsing, the webserver will check if POST or GET methods were given and act accordingly, this allows moving between pages. As a small security measure, the movement between pages is done as a POST method, any GET method will give only the login page.

><ins> about security: </ins>
The webserver was not designed for maximum security (maybe not even decent) because of the difficulty of using HTTPS with arduino and the limited time I had (The whole project was done in 2-3 months). 

---
## The Touchscreen Structure

The arduino uno with adafruit touch and LCD screen shield works as follows:
1. <ins>The Setup:</ins> The setup is very simple, create serial connection with the server     arduino, and connect to the touch screen and lcd. the "reset_screen()" function will draw the default keypad.

2. <ins>Screen Backlight Timer:</ins> Just like with the webserver arduino, number of loop iterations is used to know when to turn off the backlight, when the counter reaches 0, the backlight will turn off.
   
   `Note: the backlight requires soldering two points on the shield, refer to the adafruit manual for more information`
3. <ins>Arduino Server Commands Handler</ins>: This function will check the serial, if data is available it will parse it and choose what to do, from showing welcoming message to a positive fingerprint ID, to showing steps for new fingerprint assignments.
4. <ins>Touch handling</ins>: The last part is to check the touchscreen for touched X,Y positions, only the latest X,Y is taken. The arduino will loop for all 3 by 4 keys and check their bounds, if touched they will change the color of the button touched. The process will continue until the button is released, once released on the X,Y position, the pressed button is processed and if 0-9 digits is pressed, the number is appended to the password input. on DEL, the last digit is removed, on ENT, the password digit is sent to the arduino server.
if no input is given in 5 seconds, the password is reset and the LCD turns the backlight off.

> Note: When the backlight is off, the touch will continue to function to create "wake up" effect.

---
## Bill of materials

The following table lists all the equipment I used for the project.
The magnetic lock can be anything that works as On/Off switch and works with 12V.


|Equipment                                 |Quantity|   LINK         |
|------------------------------------------|:------:|:--------------:|
|Arduino Uno R3                            |1|[Official Arduino](https://store.arduino.cc/products/arduino-uno-rev3)|
|Arduino Nano 33 IOT                       |1|[Official Arduino](https://store.arduino.cc/products/arduino-nano-33-iot)|
|3.3V Switching Relay. **Any 3.3V relay will work**|1|[Aliexpress](aliexpress.com/item/1005001567474787.html)|
|5V/5.5A Reducing Voltage Regulator Model: D36V50F5|1 |[Pololu](https://www.pololu.com/product/4091)|
|3.3V/3.6A Reducing Voltage Regulator Model: D36V28F3|1 |[Pololu](https://www.pololu.com/product/3781)|
|12V/3A AC to DC Power Supply              | 1 | *Bought locally, may differ for every country*|
|Ultra-Slim Round Fingerprint Sensor and 6-pin Cable| 1| [Adafruit](https://www.adafruit.com/product/4750)|
|2.8" TFT Touch Shield for Arduino with Capacitive Touch| 1 | [Adafruit](https://www.adafruit.com/product/1947)|
|Magnetic Door Lock, Inter/Exter, 1200 lbs. **Any 12V lock will work**| 1| [Octopart](https://octopart.com/del12003-storm+interface-50419236)|
| 2mm zipties `The 3d printed cases use zipties.`|100| *Bought locally*|
> Does not include 3d Print filament, 3D printer, Electrical Wires, and connectors.
> The choice is up to you, any connectors will work, but Ethernet cable will be the best for the power and data transmission from the arduino inner side box to the outer side box.

---
## CAD and Wiring

The following images show the structure of the equipment. The project consits of two main parts.

<ins>**The inner encasement**</ins>:
The inner encasement holds the arduino server, voltage regulators, door opening button, and the switching relay. It should be placed inside the locked room to avoid tampering.

<ins>The inner encasement</ins>

![Inner encasement](https://i.imgur.com/tcLxjiq.png)

<ins>Closed inner encasement</ins>

![Closed Inner encasement](https://i.imgur.com/8XsNUeO.png)

<ins>Wiring Diagram</ins>

![Inner encasement wiring diagram](https://i.imgur.com/B0Q2k41.png)

> The repository encludes `dxf` files which allow seeing the wiring in a variable zoom.

<ins>**The outer encasement**</ins>:
The outer encasement holds the arduino uno with the touch screen and the fingerprint sensor.
A cable should be used to connect the power and communication wirings. Personally, I used a basic ethernet cat6 cable both for data and power, and it works flawlessly.

<ins>The outer encasement</ins>

![Outer encasmenet](https://i.imgur.com/CviMNN9.png)

> The repository includes `dxf` file with the wiring diagram.

> All the CAD files are available in the CAD directory, additional electrical components are available for download as solidworks files, due to their size. Some of the components are 3rd party from `Grabcad.com`. The following solidworks components can be downloaded from [here](https://mega.nz/file/0AtWTJyQ#VIRockYbuOe-4gDbsIgEDWmeqSzSNvUkM9Bv7lscPkU).

---
## Small PC Script
The repository includes a small python script (up to 10 lines of code) that can be used to send an http request to the webserver to open the door from computers on the same network. It's used to allow people from inside to continue with their laziness and avoid standing up to open the door when someone is knocking.

---
## Showcase Videos
<ins>Password keypad demo</ins>:

![Showcase password video](https://i.imgur.com/eeBrbVF.gif)

<ins>Fingerprint demo</ins>:

![Showcase fingerprint video](https://i.imgur.com/A8ghgwJ.gif)

---
## Licensing
The project is open for use as followed by the GNU General Public License V3.
The liecnse file is included in the repository.
