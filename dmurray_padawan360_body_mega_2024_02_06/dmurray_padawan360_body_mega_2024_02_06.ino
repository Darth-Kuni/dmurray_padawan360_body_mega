// =======================================================================================
// /////////////////////////Padawan360 Body Code - Mega I2C v2.0 ////////////////////////////////////
// =======================================================================================


/*
by Dan Kraus
dskraus@gmail.com
Astromech: danomite4047
Project Site: https://github.com/dankraus/padawan360/

Heavily influenced by DanF's Padwan code which was built for Arduino+Wireless PS2
controller leveraging Bill Porter's PS2X Library. I was running into frequent disconnect
issues with 4 different controllers working in various capacities or not at all. I decided
that PS2 Controllers were going to be more difficult to come by every day, so I explored
some existing libraries out there to leverage and came across the USB Host Shield and it's
support for PS3 and Xbox 360 controllers. Bluetooth dongles were inconsistent as well
so I wanted to be able to have something with parts that other builder's could easily track
down and buy parts even at your local big box store.

v2.0 Changes:
- Makes left analog stick default drive control stick. Configurable between left or right stick via isLeftStickDrive 

Personalizations by Dale Murray to original Sketch:
- Included Pololu Library for Maestro control
- Added home sensor for dome
- Added 3rd autonomy mode which uses a person (face) detection sensor
- incorporated two physical buttons for hardware control from the dome buttons
- included xbox 360 chatpad capabilities


Hardware:
***Arduino Mega 2560***
USB Host Shield from circuits@home
Microsoft Xbox 360 Controller
Microsoft Xbox 360 Chatpad accessory
Xbox 360 USB Wireless Reciver
Sabertooth Motor Controller
Syren Motor Controller
Sparkfun MP3 Trigger

This sketch supports I2C and calls events on many sound effect actions to control lights and sounds.
It is NOT set up for Dan's method of using the serial packet to transfer data up to the dome
to trigger some light effects.It uses Hardware Serial pins on the Mega to control Sabertooth and Syren

Set Sabertooth 2x25/2x12 Dip Switches 1 and 2 Down, All Others Up
For SyRen Simple Serial Set Switches 1 and 2 Down, All Others Up
For SyRen Simple Serial Set Switchs 2 & 4 Down, All Others Up
Placed a 10K ohm resistor between S1 & GND on the SyRen 10 itself

*/

// ************************** Options, Configurations, and Settings ***********************************


// SPEED AND TURN SPEEDS
//set these 3 to whatever speeds work for you. 0-stop, 127-full speed.
const byte DRIVESPEED1 = 50;
// Recommend beginner: 50 to 75, experienced: 100 to 127, I like 100. 
// These may vary based on your drive system and power system
const byte DRIVESPEED2 = 100;
//Set to 0 if you only want 2 speeds.
const byte DRIVESPEED3 = 127;

// Default drive speed at startup
byte drivespeed = DRIVESPEED1;

// the higher this number the faster the droid will spin in place, lower - easier to control.
// Recommend beginner: 40 to 50, experienced: 50 $ up, I like 70
// This may vary based on your drive system and power system
const byte TURNSPEED = 70;

// Set isLeftStickDrive to true for driving  with the left stick
// Set isLeftStickDrive to false for driving with the right stick (legacy and original configuration)
boolean isLeftStickDrive = true; 

// If using a speed controller for the dome, sets the top speed. You'll want to vary it potenitally
// depending on your motor. My Pittman is really fast so I dial this down a ways from top speed.
// Use a number up to 127 for serial
const byte DOMESPEED = 110;

// Ramping- the lower this number the longer R2 will take to speedup or slow down,
// change this by incriments of 1
const byte RAMPING = 5;

// Compensation is for deadband/deadzone checking. There's a little play in the neutral zone
// which gets a reading of a value of something other than 0 when you're not moving the stick.
// It may vary a bit across controllers and how broken in they are, sometimex 360 controllers
// develop a little bit of play in the stick at the center position. You can do this with the
// direct method calls against the Syren/Sabertooth library itself but it's not supported in all
// serial modes so just manage and check it in software here
// use the lowest number with no drift
// DOMEDEADZONERANGE for the left stick, DRIVEDEADZONERANGE for the right stick
const byte DOMEDEADZONERANGE = 20;
const byte DRIVEDEADZONERANGE = 20;

// Set the baude rate for the Sabertooth motor controller (feet)
// 9600 is the default baud rate for Sabertooth packet serial.
// for packetized options are: 2400, 9600, 19200 and 38400. I think you need to pick one that works
// and I think it varies across different firmware versions.
const int SABERTOOTHBAUDRATE = 9600;

// Set the baude rate for the Syren motor controller (dome)
// for packetized options are: 2400, 9600, 19200 and 38400. I think you need to pick one that works
// and I think it varies across different firmware versions.
//const int DOMEBAUDRATE = 2400;
const int DOMEBAUDRATE = 9600;

// Default sound volume at startup
// 0 = full volume, 255 off
byte vol = 20;


// Automation Delays
// set automateDelay to min and max seconds between sounds
byte automateDelay = random(5, 20); 
//How much the dome may turn during automation.
int turnDirection = 20;

// Pin number to pull a relay high/low to trigger my upside down compressed air like R2's extinguisher
#define EXTINGUISHERPIN 3

#include <Sabertooth.h>
#include <MP3Trigger.h>
#include <Wire.h>
#include <XBOXRECV.h>
#include <PololuMaestro.h> //dmurray addition - Include Maestro libarary 
#define CHATPAD  // enable xbox 360 chatpad accessory
#include "person_sensor.h"


/////////////////////////////////////////////////////////////////
Sabertooth Sabertooth2x(128, Serial1);
Sabertooth Syren10(128, Serial2);
MiniMaestro maestro(Serial3);  //dmurray addition - Define maestro 

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

// Set some defaults for start up
// false = drive motors off ( right stick disabled ) at start
boolean isDriveEnabled = false;

// Automated functionality
// Used as a boolean to turn on/off automated functions like periodic random sounds and periodic dome turns
unsigned long automateMillis = 0;
// Action number used to randomly choose a sound effect or a dome turn
byte automateAction = 0;
// Action number used to randomly choose a body animation with servos
byte automateBody = 0;

// variables added by dmurray
int soundToPlay = 0; // used to store the result of a random sound to play
int lastSoundPlayed = 0; // used to minimize chances of random sounds playing twice in a row
int homeSensor = 9; // hall effect sensor for dome home position 
int homecheckval; // numeric variable for reading home position sensor response
int turnrate = 28;
int movecount = 0; // count how many times we've moved through automation so we can reset dome position occassionally 
boolean muteSounds = true; // since my R2 is usually sitting in the background while I'm on work meetings, I'll mute him by default (for automated actions only)
boolean debugmode = false;
boolean bodyautomation = false; // used to turn on physical random events with servo animations
boolean isInAutomationMode = true;
boolean isInSentryMode = false;
unsigned long now = millis(); // used for keeping track of delay-free timers
const int statusLED = 12; // the pin for the status LED
const int modeButtonPin = 7;  // the number of the pushbutton for changing modes (left one when look at dome)
const int soundButtonPin = 8;  // the number of the spare button
int modeButtonState = 0;  // variable for current reading from the input button pin
int soundButtonState = 0;  // variable for current reading from the input button pin



// variables used for new automated sentry mode
const long faceinterval = 200;  // how long to wait for face detection checks
unsigned long lastDebounceTime1 = 0;  // the last time the button was loggled
unsigned long lastDebounceTime2 = 0;  // the last time the button was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the input fluctuates
unsigned long movingduration = 0; // how long has the dome been moving continuously
unsigned long previousscan = 0;  // will store last time we ran face detection
unsigned long lastseen = 0;  // will store last time we saw a face
unsigned long seekingstart = 0; // this is when the dome started seeking a face
int lastdirection = 0; // used for recalling whether the last direction we turned was left or right (0 is left, 1 is right)
float relativemove = 0; // keep track of how much relative movement we've made from center in automation mode
int xdirection = 0; // used for recording the direction the face has moved on the x axis (we will use 1 for left, 2 for right, and 0 for center)
int ydirection = 0;  // used for recording the direction the face has moved on the y axis (we will use 1 for up, 2 for down, and 0 for center)
int xdelta = 0; // used to compute how far to right  of centre is the face
int ydelta = 0; // used to compute how far above centre is the face
int deadzone = 22; // how much movement delta needs to be detected since previous read to react (0 = any movement at all)
boolean presence = false; // is there a face we're trying to track
boolean motorson = false; // define whether we've currently instructed the motors to move or not
int currentxmidpoint = 0; // used to compute the current centre X axis of a face
int currentymidpoint = 0; // used to compute the current centre Y axis of a face
int lastModeButtonState = LOW;  // variable for debouncing the pushbutton status by referencing it's previous reading
int lastSoundButtonState = LOW;  // variable for debouncing the pushbutton status by referencing it's previous reading

int driveThrottle = 0;
int throttleStickValue = 0;
int domeThrottle = 0;
int turnThrottle = 0;

boolean firstLoadOnConnect = false;

AnalogHatEnum throttleAxis;
AnalogHatEnum turnAxis;
AnalogHatEnum domeAxis;
ButtonEnum speedSelectButton;
ButtonEnum hpLightToggleButton;


// this is legacy right now. The rest of the sketch isn't set to send any of this
// data to another arduino like the original Padawan sketch does
// right now just using it to track whether or not the HP light is on so we can
// fire the correct I2C event to turn on/off the HP light.
//struct SEND_DATA_STRUCTURE{
//  //put your variable definitions here for the data you want to send
//  //THIS MUST BE EXACTLY THE SAME ON THE OTHER ARDUINO
//  int hpl; // hp light
//  int dsp; // 0 = random, 1 = alarm, 5 = leia, 11 = alarm2, 100 = no change
//};
//SEND_DATA_STRUCTURE domeData;//give a name to the group of data

boolean isHPOn = false;



MP3Trigger mp3Trigger;
USB Usb;
XBOXRECV Xbox(&Usb);

void setup() {
  // Serial.begin(9600); // for debugging
  Serial1.begin(SABERTOOTHBAUDRATE);
  Serial2.begin(DOMEBAUDRATE);
  Serial3.begin(9600); //dmurray addition, define baud rate for maestro and begin

#if defined(SYRENSIMPLE)
  Syren10.motor(0);
#else
  Syren10.autobaud();
#endif

  // Send the autobaud command to the Sabertooth controller(s).
  /* NOTE: *Not all* Sabertooth controllers need this command.
  It doesn't hurt anything, but V2 controllers use an
  EEPROM setting (changeable with the function setBaudRate) to set
  the baud rate instead of detecting with autobaud.
  If you have a 2x12, 2x25 V2, 2x60 or SyRen 50, you can remove
  the autobaud line and save yourself two seconds of startup delay.
  */
  Sabertooth2x.autobaud();
  // The Sabertooth won't act on mixed mode packet serial commands until
  // it has received power levels for BOTH throttle and turning, since it
  // mixes the two together to get diff-drive power levels for both motors.
  Sabertooth2x.drive(0);
  Sabertooth2x.turn(0);


  Sabertooth2x.setTimeout(950);
  Syren10.setTimeout(950);

  // pinMode(EXTINGUISHERPIN, OUTPUT);
  // digitalWrite(EXTINGUISHERPIN, HIGH);

  mp3Trigger.setup();
  mp3Trigger.setVolume(vol);

// setup IOs
pinMode(modeButtonPin, INPUT);
pinMode(soundButtonPin, INPUT);
pinMode(homeSensor, INPUT);
pinMode(statusLED, OUTPUT);


  if(isLeftStickDrive) {
    throttleAxis = LeftHatY;
    turnAxis = LeftHatX;
    domeAxis = RightHatX;
    speedSelectButton = L3;
    hpLightToggleButton = R3;

  } else {
    throttleAxis = RightHatY;
    turnAxis = RightHatX;
    domeAxis = LeftHatX;
    speedSelectButton = R3;
    hpLightToggleButton = L3;
  }


 // Start I2C Bus. The body is the master.
  Wire.begin();


  // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
  while (!Serial);
  if (Usb.Init() == -1) {
    //Serial.print(F("\r\nOSC did not start"));
    while (1); //halt
  }
  //Serial.print(F("\r\nXbox Wireless Receiver Library Started"));

// rotate dome to face front
if (debugmode != true)
  {
  findhome();  // set dome position back to front
  }

// reset servos to home position
maestro.restartScript(21); // run maestro predefined script as numbered
} // END OF SETUP



/*
START OF MAIN LOOP - START OF MAIN LOOP - START OF MAIN LOOP - START OF MAIN LOOP - START OF MAIN LOOP - START OF MAIN LOOP - START OF MAIN LOOP 
*/


void loop() {

  now = millis(); // set currentMillis to the current amount of time the sketch has been running 


  


  Usb.Task();
  // if we're not connected, return so we don't bother doing anything else.
  // set all movement to 0 so if we lose connection we don't have a runaway droid!
  // a restraining bolt and jawa droid caller won't save us here!

  if (isDriveEnabled == true)
  {
  isInSentryMode = false;
  isInAutomationMode = false;
  bodyautomation = false;
  }


  if (!Xbox.XboxReceiverConnected || !Xbox.Xbox360Connected[0]) 
  {
    Sabertooth2x.drive(0);
    Sabertooth2x.turn(0);
    isDriveEnabled = false;
    firstLoadOnConnect = false;

    if ((isInAutomationMode == false) && (isInSentryMode == false))
        {
        return;
        }
  }
  else if (firstLoadOnConnect == false)
        {
        firstLoadOnConnect = true;
        mp3Trigger.play(22);
        Xbox.setLedMode(ROTATING, 0);
        }



  
  if (Xbox.getButtonClick(XBOX, 0)) 
  {
    if(Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)){ 
      Xbox.disconnect(0);
    }
  }

  // enable / disable right stick (droid movement) & play a sound to signal motor state
  if (Xbox.getButtonClick(START, 0)) {
    if (isDriveEnabled) 
    {
      isDriveEnabled = false;
      killmovement();
      Xbox.setLedMode(ROTATING, 0);
      mp3Trigger.play(53);
    } else {
      isDriveEnabled = true;
      isInAutomationMode = false;
      isInSentryMode = false;
      findhome();
      mp3Trigger.play(52);
      // //When the drive is enabled, set our LED accordingly to indicate speed
      if (drivespeed == DRIVESPEED1) {
        Xbox.setLedOn(LED1, 0);
      } else if (drivespeed == DRIVESPEED2 && (DRIVESPEED3 != 0)) {
        Xbox.setLedOn(LED2, 0);
      } else {
        Xbox.setLedOn(LED3, 0);
      }
    }
  }

  //Toggle automation mode with the BACK button
  if (Xbox.getButtonClick(BACK, 0)) 
  {
  modechange();
  }


/*
  if (Xbox.getButtonClick(BACK, 0)) {
    if (isInAutomationMode) {
      isInAutomationMode = false;
      automateAction = 0;
      mp3Trigger.play(53);
    } else {
      isInAutomationMode = true;
      mp3Trigger.play(52);
    }
  }
*/ 



// read the state of the current button (HIGH means button is being pressed)
int reading = digitalRead(modeButtonPin);

// check to see if the button has been pressed

  // If the switch changed, due to noise or pressing:
  if (reading != lastModeButtonState) 
    {
    // reset the debouncing timer
    lastDebounceTime1 = now;
    }

  if ((now - lastDebounceTime1) > debounceDelay)
  {
    if (reading != modeButtonState)
    {
    
      modeButtonState = reading;

      if (modeButtonState == HIGH)
          {
          modechange();
          }
    }
  }
lastModeButtonState = reading; // set the last button state to the current reading of the input button



// read the state of the current button (HIGH means button is being pressed)
int reading2 = digitalRead(soundButtonPin);

// check to see if the button has been pressed

  // If the switch changed, due to noise or pressing:
  if (reading2 != lastSoundButtonState) 
    {
    // reset the debouncing timer
    lastDebounceTime2 = now;
    }

  if ((now - lastDebounceTime2) > debounceDelay)
  {
    if (reading2 != soundButtonState)
    {
      soundButtonState = reading2;
      if (soundButtonState == HIGH)
          {
          if (muteSounds == false)
            {
            muteSounds = true; 
            // Serial.println("Muting sound...");
            }
          else
            {
            muteSounds = false;  
            vol = 20;
            mp3Trigger.play(32);
            // Serial.println("UNMUTE");
            }
          }
    }
  }
lastSoundButtonState = reading2; // set the last button state to the current reading of the input button





/*
--------------------------------------------------------------------------------------------
SENTRY MODE IS USED TO DETECT HUMAN FACES AND FOLLOW THEM, MAKING NOISES WHEN SEEN OR LOST
--------------------------------------------------------------------------------------------
*/

if(isInSentryMode == true)  // begin sentry mode loop
  {
isDriveEnabled = false; // make sure we're not driving

    getfaces();

  //person_sensor_results_t results = {};   // Perform a read action on the I2C address of the sensor to get the
  
      if (motorson == true)
      {
      int idlesound = 0;
      randomSeed(now);
      idlesound = random(1,10);

      if (idlesound < 6)
      {
      playidle();
      }
    
      

          getfaces();
            if (xdirection == 1)
            {
            moveleft(); // continue moving without resetting timer
            delay (xdelta * turnrate);
            xdelta = 0;
            }
            if (xdirection == 2)
            {
            moveright(); // continue moving without resetting timer
            delay (xdelta * turnrate); 
            xdelta = 0;
            }

            if (xdelta < deadzone)
            {
            stopx(); 
            xdelta = 0;
            motorson = false;
            }


      delay (500);
      }


/*
  while (motorson == true)
        {
        now = millis();
        
        
        getfaces();

        

          if (xdelta < deadzone) // we're close enough to be facing the target
                  {
                  stopx();
                  delay(1000); // rest for a moment
                        if (debugmode == true)
                          {
                          Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                          Serial.println("STOP DUE TO FACING TARGET");
                          Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                          // mp3Trigger.play(1);
                          break;
                          }
                  }
        


            else if ((now - movingduration) >= 4500) // we have been turning the dome for 3 seconds which is too long
                  {
                  stopx();
                  delay(1000); // rest for a moment
                        if (debugmode == true)
                          {
                          Serial.println("*********************************");
                          Serial.print("Stopping due to timeout: ");
                          Serial.print(now);
                          Serial.print(" - ");
                          Serial.print(movingduration);
                          Serial.print(" = ");
                          Serial.println(now - movingduration);
                          Serial.println("*********************************");
                          mp3Trigger.play(4);
                          break;
                          }
                 }


        
        Serial.println("Now");
        Serial.println(now);
        Serial.println(seekingstart);
        Serial.println(seekingstart);
        Serial.println("xdelta");
        Serial.println(xdelta);
        





        delay(200);

         } // end while motors are on loop
*/


        if ((now - lastseen > 8000) && (presence == true) && (lastseen != 0)) // if no faces in 8 seconds, cancel offset deltas so that we stop seeking
        {
        xdelta = 0;
        // presence = false;
        stopx();
        }


      if ((now - lastseen > 1800000) && (lastseen != 0)) // what to do if we have seen a face for the first time in more than 30 minutes
        {
          if (debugmode == true)
            {
            Serial.println("Hello again for the first time in 30 minutes");
            Serial.println(now);
            Serial.println(" - ");
            Serial.println(lastseen);
            Serial.println(" - ");
            Serial.println(now - lastseen);
            }
          mp3Trigger.play(2); // play scream sound
        }
      else
      if ((now - lastseen > 300000) && (lastseen != 0)) // what to do if we have seen a face for the first time in more than 5 minutes
        {
             if (debugmode == true)
            {
            Serial.println("Hello again for the first time in 5 minutes");
            }
          playseen();
        }
      


                  // assume we have some faces
                  if (( xdelta > deadzone) && (xdirection == 1) && (motorson == false))
                    {
                      moveleft(); // start moving left
                      movingduration = now; // record when we told the motor to start moving
                    }
                  else if (( xdelta > deadzone) && (xdirection == 2) && (motorson == false))  
                    {
                      moveright(); // start moving right
                      movingduration = now; // record when we told the motor to start moving
                    }
                  else 
                  {
                  //stopx();
                  }



      




  }

/*
--------------------------------------------------------------------------------------------
END OF SENTRY MODE!!!!!!!!!! 
--------------------------------------------------------------------------------------------
*/




/*
AUTOMATION MODE IS USED TO PLAY RANDOM SOUNDS AND SMALL DOME MOVEMENTS WITHOUT ANY REAL INTELLIGENCE
*/

  // Plays random sounds or dome movements for automations when in automation mode
  if (isInAutomationMode) {
    unsigned long currentMillis = millis();
    

    if (currentMillis - automateMillis > (automateDelay * 1000))
    {
      automateMillis = millis();
      automateAction = random(1, 5);
      automateBody = random (1,40);
      
      if ((automateBody > 1) && (bodyautomation == true))
      {
        if (automateBody == 15)
            {
            maestro.restartScript(1); // run maestro script: animation of gripper 
            }
        if (automateBody == 25)
            {
            maestro.restartScript(2); // run maestro script: animation of interface
            }
        if (automateBody == 35)
            {
            maestro.restartScript(3); // run maestro script: animation of utility arms
            }
        
        
      }


      if (automateAction > 1) 
      {
        soundToPlay = random(32,52); // decide random sound

        if (soundToPlay == lastSoundPlayed)
          {
          soundToPlay = random(32,52); // spin again to reduce chance of repetition 
                if (soundToPlay == lastSoundPlayed)
                {
                soundToPlay = random(32,52); // spin again to reduce chance of repetition 
                }
          }

        lastSoundPlayed = soundToPlay;
        if ((muteSounds == false) && (automateAction >= 3))
            {
            if (debugmode == true)
              {
              Serial.println("playing random sound");
              }
            mp3Trigger.play(soundToPlay);
            }
      }

      if (automateAction < 3) 
      {
      
      // keep track of how far we're moving
      randomSeed(now);
      // int turndelay = (random(200,800));
      int whichway = (random(1,100));
    


    // choose next direction randomly
    if (whichway < 50)
    {
    turnDirection = 45; // turn left
    lastdirection = 0;
    }
    else
    {
    turnDirection = -45; // then turn right
    lastdirection = 1;
    }

    /*
    // choose next direction based on previous direction we moved
    if (lastdirection == 0) // if we last turned left
    {
    turnDirection = -45; // then turn right
    lastdirection = 1;
    }
    else
    {
    turnDirection = 45; // else turn left
    lastdirection = 0;
    }
    */


    if (relativemove >= 200) // we've gone too far left
          {
          
          turnDirection = -45; // lets go right
          lastdirection = 1;
          }
    if (relativemove <= -200) // we've gone too far right
          {

          turnDirection = 45; // lets go left
          lastdirection = 0;
          }

  if (turnDirection > 0) // we decided to go left
  {
  relativemove = (relativemove + 100); // keep track of movement from center
  }
  else if (turnDirection < 0) // we decided to go right
  {
 relativemove = (relativemove - 100); // keep track of movement from center
  }
   
/*
Serial.print(" Last Direction: ");
Serial.print(lastdirection);
Serial.print(" turnDirection : ");
Serial.print(turnDirection);
Serial.print(" Relative Movement : ");
Serial.print(relativemove);
Serial.print(" MoveCount : ");
Serial.print(movecount);
Serial.println("****");
*/

movecount++;
if (movecount > 20) // if we've moved a lot, lets reset the dome position
      {
          if (relativemove >= 0)
            {
            lastdirection = 0;  
            }
          else if (relativemove < 0)
            {
            lastdirection = 1;
            }
      findhome(); 
      delay(2000);
      }

          #if defined(SYRENSIMPLE)
                  Syren10.motor(turnDirection);
          #else
                  Syren10.motor(1, turnDirection);
          #endif

          // delay to allow  motors to turn
          delay(750);


          // stop motors turning
          #if defined(SYRENSIMPLE)
                  Syren10.motor(0);
          #else
                  Syren10.motor(1, 0);
          #endif




      }

      // sets the mix, max seconds between automation actions - sounds and dome movement
      automateDelay = random(3,12);
    }
  }


  // dmurray modification: initialize/check for chatpad input
        #if defined(CHATPAD)      
        Check_Chatpad();
        #endif

  // Volume Control of MP3 Trigger
  // Hold R1 and Press Up/down on D-pad to increase/decrease volume
  if (Xbox.getButtonClick(UP, 0)) {
    // volume up
    if (Xbox.getButtonPress(R1, 0)) {
      if (vol > 0) {
        vol--;
        mp3Trigger.setVolume(vol);
      }
    }
  }
  if (Xbox.getButtonClick(DOWN, 0)) {
    //volume down
    if (Xbox.getButtonPress(R1, 0)) {
      if (vol < 255) {
        vol++;
        mp3Trigger.setVolume(vol);
      }
    }
  }




/* 
  // Logic display brightness.
  // Hold L1 and press up/down on dpad to increase/decrease brightness
  if (Xbox.getButtonClick(UP, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
    //triggerI2C(10, 24);
    }
  }
  if (Xbox.getButtonClick(DOWN, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
    //triggerI2C(10, 25);
    }
  }
*/


/*
  //FIRE EXTINGUISHER
  // When holding L2-UP, extinguisher is spraying. WHen released, stop spraying

  // TODO: ADD SERVO DOOR OPEN FIRST. ONLY ALLOW EXTINGUISHER ONCE IT'S SET TO 'OPENED'
  // THEN CLOSE THE SERVO DOOR
  if (Xbox.getButtonPress(L1, 0)) {
    if (Xbox.getButtonPress(UP, 0)) {
      digitalWrite(EXTINGUISHERPIN, LOW);
    } else {
      digitalWrite(EXTINGUISHERPIN, HIGH);
    }
  }
*/


  // GENERAL SOUND PLAYBACK AND DISPLAY CHANGING

  // Y Button and Y combo buttons
  if (Xbox.getButtonClick(Y, 0)) {
    if (Xbox.getButtonPress(L1, 0)) 
    {
    mp3Trigger.play(8);
    } 
    else if (Xbox.getButtonPress(L2, 0)) 
    {
    mp3Trigger.play(2);
    } 
    else if (Xbox.getButtonPress(R1, 0)) 
    {
    mp3Trigger.play(13);
    // maestro.restartScript(25); // run maestro script: dance
    } else 
    {

        soundToPlay = random(14,18); // decide random sound

        if (soundToPlay == lastSoundPlayed)
          {
          soundToPlay = random(14,18); // spin again to reduce chance of repetition 
            if (soundToPlay == lastSoundPlayed)
            {
            soundToPlay = random(14,18); // spin again to reduce chance of repetition 
            }
          }

          
        lastSoundPlayed = soundToPlay;
        mp3Trigger.play(soundToPlay);



      //logic lights, random
      //triggerI2C(10, 0);
    }
  }

  // A Button and A combo Buttons
  if (Xbox.getButtonClick(A, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
      mp3Trigger.play(6);
      //logic lights
      //triggerI2C(10, 6);
      // HPEvent 11 - SystemFailure - I2C
      //triggerI2C(25, 11);
      //triggerI2C(26, 11);
      //triggerI2C(27, 11);
    } else if (Xbox.getButtonPress(L2, 0)) {
      mp3Trigger.play(1);
      //logic lights, alarm
      //triggerI2C(10, 1);
      //  HPEvent 3 - alarm - I2C
      //triggerI2C(25, 3);
      //triggerI2C(26, 3);
      //triggerI2C(27, 3);
    } else if (Xbox.getButtonPress(R1, 0)) {
      mp3Trigger.play(11);
      // maestro.restartScript(25); // run maestro script: dance
      //logic lights, alarm2Display
      //triggerI2C(10, 11);
    } else {

        soundToPlay = random(18,26); // decide random sound

        if (soundToPlay == lastSoundPlayed)
          {
          soundToPlay = random(18,26); // spin again to reduce chance of repetition 
              if (soundToPlay == lastSoundPlayed)
              {
              soundToPlay = random(18,26); // spin again to reduce chance of repetition 
              }
          }

          
        lastSoundPlayed = soundToPlay;
        mp3Trigger.play(soundToPlay);



      
      //logic lights, random
      //triggerI2C(10, 0);
    }
  }

  // B Button and B combo Buttons
  if (Xbox.getButtonClick(B, 0)) {
    if (Xbox.getButtonPress(L1, 0)) {
      mp3Trigger.play(7);
      //logic lights, random
      //triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(L2, 0)) {
      mp3Trigger.play(3);
      //logic lights, random
      //triggerI2C(10, 0);
    } else if (Xbox.getButtonPress(R1, 0)) {
      mp3Trigger.play(10);
      // maestro.restartScript(25); // run maestro script: dance
      //logic lights bargrap
      //triggerI2C(10, 10);
      // HPEvent 1 - Disco - I2C
      //triggerI2C(25, 10);
      //triggerI2C(26, 10);
      //triggerI2C(27, 10);
    } else {
      
        soundToPlay = random(33,53); // decide random sound

        if (soundToPlay == lastSoundPlayed)
          {
          soundToPlay = random(33,53); // spin again to reduce chance of repetition 
                if (soundToPlay == lastSoundPlayed)
                {
                soundToPlay = random(33,53); // spin again to reduce chance of repetition 
                }
          }

          
        lastSoundPlayed = soundToPlay;
        mp3Trigger.play(soundToPlay);



      //logic lights, random
      //triggerI2C(10, 0);
    }
  }

  // X Button and X combo Buttons
  if (Xbox.getButtonClick(X, 0)) {
    // leia message L1+X
    if (Xbox.getButtonPress(L1, 0)) {
      mp3Trigger.play(5);
      //logic lights, leia message
      //triggerI2C(10, 5);
      // Front HPEvent 1 - HoloMessage - I2C -leia message
      //triggerI2C(25, 9);
    } else if (Xbox.getButtonPress(L2, 0)) {
      mp3Trigger.play(4);
      //logic lights
      //triggerI2C(10, 4);
    } else if (Xbox.getButtonPress(R1, 0)) {
      mp3Trigger.play(12);
      // maestro.restartScript(25); // run maestro script: dance
      //logic lights, random
      //triggerI2C(10, 0);
    } else {
      
        soundToPlay = random(26,33); // decide random sound

        if (soundToPlay == lastSoundPlayed)
          {
          soundToPlay = random(26,33); // spin again to reduce chance of repetition 
                if (soundToPlay == lastSoundPlayed)
                {
                soundToPlay = random(26,33); // spin again to reduce chance of repetition 
                }  
          }

          
        lastSoundPlayed = soundToPlay;
        mp3Trigger.play(soundToPlay);


      //logic lights, random
      //triggerI2C(10, 0);
    }
  }

  // turn hp light on & off with Right Analog Stick Press (R3) for left stick drive mode
  // turn hp light on & off with Left Analog Stick Press (L3) for right stick drive mode
  if (Xbox.getButtonClick(hpLightToggleButton, 0))  {
    // if hp light is on, turn it off
    if (isHPOn) {
      isHPOn = false;
      // turn hp light off
      // Front HPEvent 2 - ledOFF - I2C
      //triggerI2C(25, 2);
    } else {
      isHPOn = true;
      // turn hp light on
      // Front HPEvent 4 - whiteOn - I2C
      //triggerI2C(25, 1);
    }
  }


  // Change drivespeed if drive is enabled
  // Press Left Analog Stick (L3) for left stick drive mode
  // Press Right Analog Stick (R3) for right stick drive mode
  // Set LEDs for speed - 1 LED, Low. 2 LED - Med. 3 LED High
  if (Xbox.getButtonClick(speedSelectButton, 0) && isDriveEnabled) {
    //if in lowest speed
    if (drivespeed == DRIVESPEED1) {
      //change to medium speed and play sound 3-tone
      drivespeed = DRIVESPEED2;
      Xbox.setLedOn(LED2, 0);
      mp3Trigger.play(53);
      //triggerI2C(10, 22);
    } else if (drivespeed == DRIVESPEED2 && (DRIVESPEED3 != 0)) {
      //change to high speed and play sound scream
      drivespeed = DRIVESPEED3;
      Xbox.setLedOn(LED3, 0);
      mp3Trigger.play(2);
      //triggerI2C(10, 23);
    } else {
      //we must be in high speed
      //change to low speed and play sound 2-tone
      drivespeed = DRIVESPEED1;
      Xbox.setLedOn(LED1, 0);
      mp3Trigger.play(52);
      //triggerI2C(10, 21);
    }
  }


 
  // FOOT DRIVES
  // Xbox 360 analog stick values are signed 16 bit integer value
  // Sabertooth runs at 8 bit signed. -127 to 127 for speed (full speed reverse and  full speed forward)
  // Map the 360 stick values to our min/max current drive speed
  throttleStickValue = (map(Xbox.getAnalogHat(throttleAxis, 0), -32768, 32767, -drivespeed, drivespeed));
  if (throttleStickValue > -DRIVEDEADZONERANGE && throttleStickValue < DRIVEDEADZONERANGE) {
    // stick is in dead zone - don't drive
    driveThrottle = 0;
  } else {
    if (driveThrottle < throttleStickValue) {
      if (throttleStickValue - driveThrottle < (RAMPING + 1) ) {
        driveThrottle += RAMPING;
      } else {
        driveThrottle = throttleStickValue;
      }
    } else if (driveThrottle > throttleStickValue) {
      if (driveThrottle - throttleStickValue < (RAMPING + 1) ) {
        driveThrottle -= RAMPING;
      } else {
        driveThrottle = throttleStickValue;
      }
    }
  }

  turnThrottle = map(Xbox.getAnalogHat(turnAxis, 0), -32768, 32767, -TURNSPEED, TURNSPEED);

  // DRIVE!
  // right stick (drive)
  if (isDriveEnabled) {
    // Only do deadzone check for turning here. Our Drive throttle speed has some math applied
    // for RAMPING and stuff, so just keep it separate here
    if (turnThrottle > -DRIVEDEADZONERANGE && turnThrottle < DRIVEDEADZONERANGE) {
      // stick is in dead zone - don't turn
      turnThrottle = 0;
    }
    Sabertooth2x.turn(-turnThrottle);
    Sabertooth2x.drive(driveThrottle);
  }

  // DOME DRIVE!
  domeThrottle = (map(Xbox.getAnalogHat(domeAxis, 0), -32768, 32767, DOMESPEED, -DOMESPEED));
  if (domeThrottle > -DOMEDEADZONERANGE && domeThrottle < DOMEDEADZONERANGE) {
    //stick in dead zone - don't spin dome
    domeThrottle = 0;
  }

  Syren10.motor(1, domeThrottle);

// store which direction we're turning for return-to-home features
    if ((domeThrottle < -DOMEDEADZONERANGE) || (domeThrottle > DOMEDEADZONERANGE))
        {
        homecheckval =  (digitalRead(homeSensor)); // read the home position sensor  to determine if we need to move  
            if ((domeThrottle < 0) && (homecheckval == HIGH))
            {
            lastdirection = 0;
            }
            else if ((domeThrottle > 0) && (homecheckval == HIGH))
            {
            lastdirection = 1;  
            }
        }




  
} // END OF MAIN loop()
/*
END OF MAIN LOOP - END OF MAIN LOOP - END OF MAIN LOOP - END OF MAIN LOOP - END OF MAIN LOOP - END OF MAIN LOOP - END OF MAIN LOOP - 
*/






void triggerI2C(byte deviceID, byte eventID) {
  Wire.beginTransmission(deviceID);
  Wire.write(eventID);
  Wire.endTransmission();
}


void Check_Chatpad() 
{

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D1, 0)) 
  {
  maestro.restartScript(2); // // run maestro script: interface animation
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(17); // run maestro script: close interface door 
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(9); // run maestro script: close interface door 
      }
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D2, 0)) 
  {
  maestro.restartScript(1); // run maestro script:  gripper animation 
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(16); // run maestro script: close interface door 
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(8); // run maestro script: close interface door 
      }
  }


  if (Xbox.getChatpadClick(XBOX_CHATPAD_D3, 0)) 
  {
  maestro.restartScript(14); // run maestro script: open charge door 
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(6); // run maestro script: close charge door 
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(6); // run maestro script: close charge door 
      }

  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D4, 0)) 
  {
   maestro.restartScript(15); // run maestro script: open data door
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(7); // run maestro script: close data door 
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(7); // run maestro script: close data door 
      }
  }



  if (Xbox.getChatpadClick(XBOX_CHATPAD_D5, 0)) 
  {
  maestro.restartScript(3); // run maestro script: animation of utility arms

      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(18); // run maestro script: animate claw
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(13); // run maestro script: lower gripper
      }

  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D6, 0)) 
  {
  maestro.restartScript(20); // run maestro script: raise interface
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(23); // run maestro script: animate interface
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(27); // run maestro script: lower interface
      }
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D7, 0)) 
  {
  maestro.restartScript(19); // run maestro script: raise gripper
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(22); // run maestro script: animate claw
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(26); // run maestro script: lower gripper
      }
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D8, 0)) 
  {
  maestro.restartScript(25); // run maestro script: dance routine
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D9, 0)) 
  {
  maestro.restartScript(4); // run maestro script: close all doors
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D0, 0)) 
  {
  maestro.restartScript(21); // run maestro script: reset all
  }


  if (Xbox.getChatpadClick(XBOX_CHATPAD_A, 0)) 
  {
  // spare
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_B, 0)) 
  {
  mp3Trigger.play(30); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_C, 0)) 
  {
  maestro.restartScript(14); // run maestro script: open charge door 
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(6); // run maestro script: close charge door 
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(6); // run maestro script: close charge door 
      }

  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_D, 0)) 
  {
  maestro.restartScript(15); // run maestro script: open data door
      if (Xbox.getButtonPress(L1, 0)) 
      {
      maestro.restartScript(7); // run maestro script: close data door 
      }
      if (Xbox.getButtonPress(R1, 0)) 
      {
      maestro.restartScript(7); // run maestro script: close data door 
      }
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_E, 0)) 
  {
  mp3Trigger.play(35); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_F, 0)) 
  {
  // spare
  //  Serial.println(F("F"));
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_G, 0)) 
  {
  maestro.restartScript(16); // run maestro script: open gripper door
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_H, 0)) 
  {
  findhome();
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_I, 0)) 
  {
maestro.restartScript(17); // run maestro script: open interface door
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_J, 0)) 
  {
  mp3Trigger.play(14); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_K, 0)) 
  {
  mp3Trigger.play(9); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_L, 0)) 
  {
  mp3Trigger.play(37); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_M, 0)) 
  {
    if (isInAutomationMode == true)
      {
        if (bodyautomation == true)
        {
          bodyautomation = false;
        }
        else 
        {
          bodyautomation = true;
          mp3Trigger.play(47); 
        }
      }
    if (isInSentryMode == true)
      {

        /* 
        if (turnrate == 14)
        {
          turnrate = 28;
          mp3Trigger.play(53); 
        }
        else 
        {
          turnrate = 14;
          mp3Trigger.play(53); 
        }
        */
      
      }
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_N, 0)) 
  {
  mp3Trigger.play(29); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_O, 0)) 
  {
  mp3Trigger.play(36); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_P, 0)) 
  {
  mp3Trigger.play(17); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_Q, 0)) 
  {
  mp3Trigger.play(23); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_R, 0)) 
  {
  mp3Trigger.play(5); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_S, 0)) 
  {
  mp3Trigger.play(2);
  maestro.restartScript(24); // run maestro script: scream (all move)
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_T, 0)) 
  {
  mp3Trigger.play(3); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_U, 0)) 
  {
  // mp3Trigger.play(5); 
  // Serial.println(F("U"));
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_V, 0)) 
  {
  mp3Trigger.play(32); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_W, 0)) 
  {
  mp3Trigger.play(42); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_X, 0)) 
  {
  mp3Trigger.play(49); 
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_Y, 0)) 
  {
  // spare
  // Serial.println(F("Y"));
  }

  if (Xbox.getChatpadClick(XBOX_CHATPAD_Z, 0)) 
  {
  mp3Trigger.play(44); 
  }

  

}



void moveleft()
{
  if (debugmode == true)
  {
  Serial.println("<<<<<<<<<<<<<<<<<<<<<<<<<<<< ");
  Serial.print("********Moving LEFT******* xdelta: ");
  Serial.println(xdelta);
  Serial.println("<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
  }
Syren10.motor(1,45);
motorson = true;
lastdirection = 0; // store that we last moved left
}

void moveright()
{
  if (debugmode == true)
  {
  Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>> ");
  Serial.print("********Moving RIGHT*******  xdelta: ");
  Serial.println(xdelta);
  Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>> ");
  }
Syren10.motor(1,-45);
motorson = true;
lastdirection = 1; // store that we last moved right
}


void stopx()
{
if (debugmode == true)
  {
    Serial.println("Stopping motor");
    mp3Trigger.play(20);
  }
Syren10.motor(1,0); // stop motor 
motorson = false;
}

void killmovement()
{
Sabertooth2x.drive(0);  // stop foot motors
Sabertooth2x.turn(0); // stop foot motors
Syren10.motor(1,0); // stop dome motor 
isInSentryMode = false;
isInAutomationMode = false;
isDriveEnabled = false; // turn off drives
bodyautomation = false; // stop servo animations
motorson = false;
Xbox.setLedMode(ROTATING, 0); // indicate drives are turned off
}




void modechange()
{
// immediately stop motors and disable drive mode
Sabertooth2x.drive(0);  // stop foot motors
Sabertooth2x.turn(0); // stop foot motors
Syren10.motor(1,0); // stop dome motor 
isDriveEnabled = false; // turn off drives
Xbox.setLedMode(ROTATING, 0); // indicate drives are turned off

if (debugmode == true)
{
Serial.print("Sentry Mode: ");
Serial.println(isInSentryMode);
Serial.print("Automation Mode: ");
Serial.println(isInAutomationMode);

}

if ((isInSentryMode == true) && (isInAutomationMode == false))
  {
  isInSentryMode = false;
  isInAutomationMode = true;
  mp3Trigger.play(31);
  delay(1000);
  }
else if ((isInSentryMode == false) && (isInAutomationMode == true))
  {
  isInSentryMode = false;
  isInAutomationMode = false;
  automateAction = 0;
  mp3Trigger.play(29);
  // digitalWrite(LED_BUILTIN, HIGH);
  // digitalWrite(statusLED, HIGH);
  delay(1000);
  }
else if ((isInSentryMode == false) && (isInAutomationMode == false))
  {
  isInSentryMode = true;
  isInAutomationMode = false;
  automateAction = 0;
  mp3Trigger.play(8);
  delay(1000);
  }
}



void getfaces()
{
person_sensor_results_t results = {};   // Perform a read action on the I2C address of the sensor to get the

      if (now - previousscan >= faceinterval) 
      {
          // update the last time we tried face detection
          previousscan = now;
        

        if (!person_sensor_read(&results)) 
        {
          // Serial.println("No person sensor results found on the i2c bus");
          return;
        }


        if ((now - lastseen > 20000) && (presence == true))  // what to do if we haven't seen anyone for 20 seconds since last sighting
        {
            if (debugmode == true)
            {
            Serial.println("Goodbye then");
            }
          presence = false;
          playgone();
          findhome();         
        }

        

      }

    if (results.num_faces > 0)
    {
              
    

      for (int i = 0; i < 1; ++i)  // I crudely modified this loop to focus only on the first result 
      { // found faces loop
        const person_sensor_face_t* face = &results.faces[i];



            // determine current midpoint if confident enough
            if ((face->box_confidence > 98) && (face->box_confidence < 101))
            presence = true; // we have seen someone (at least one face)
            now = millis(); // update current timer
            lastseen = millis(); // update when we last saw a face
            
            {
            currentxmidpoint = (face->box_left + ((face->box_right - face->box_left) / 2));
            currentymidpoint = (face->box_bottom + ((face->box_top - face->box_bottom) / 2));

            
            xdelta = (128 - currentxmidpoint); // 128 is the middle of the camera frame, so we will calculate how far offset the detected face is from center
            ydelta = (128 - currentymidpoint); // 128 is the middle of the camera frame, so we will calculate how far offset the detected face is from center


          // calculate how far off center the face is on the X axis
            if (currentxmidpoint > (128+deadzone))
                {
                xdelta = (currentxmidpoint - 128);
                xdirection = 2;
                }
            else if (currentxmidpoint < (128-deadzone))
                {
                xdelta = (128 - currentxmidpoint);
                xdirection = 1;
                }
            else 
                {
                xdelta = 0;
                xdirection = 0;
                }


          // calculate how far off center the face is on the Y axis
            if (currentymidpoint > (128+deadzone))
                {
                ydelta = (currentymidpoint - 128);
                ydirection = 1;
                }
            else if (currentymidpoint < (128-deadzone))
                {
                ydelta = (128 - currentymidpoint);
                ydirection = 2;
                }
            else 
                {
                ydelta = 0;
                ydirection = 0;
                }


              if (debugmode == true)
              {
              Serial.print("confidence: ");
              Serial.println(face->box_confidence);
              Serial.print("faces: ");
              Serial.println(results.num_faces);
              Serial.print("//////////// XDELTA //////////// :") ;
              Serial.println(xdelta);
              }
            }

        
                if (debugmode == true)
                {
                Serial.print("Motor state: ");
                Serial.println(motorson);
                Serial.print("Confidence: ");
                Serial.print(face->box_confidence);
                Serial.print("  -  Xdelta:");
                Serial.println(xdelta);
                Serial.print("face X direction: ");
                Serial.println(xdirection);
                }

          if ((motorson == true) && (debugmode == true))
          {
            Serial.print("moving... ");
          }

      } // end face iteration loop
  } // end results found loop

}


void playidle()
{
int playsound = 0;
randomSeed(now);
playsound = random(1,20);


  if ((playsound > 14) && (muteSounds == false))
    {
        if (playsound == 15)
        {
        mp3Trigger.play(4); 
        }
        else if (playsound == 15)
        {
        mp3Trigger.play(8); 
        }
        else if (playsound == 16)
        {
        mp3Trigger.play(19); 
        }
        else if (playsound == 17)
        {
        mp3Trigger.play(23); 
        }
        else if (playsound == 18)
        {
        mp3Trigger.play(26);   
        }
        else if (playsound == 19)
        {
        mp3Trigger.play(47);   
        }
        else if (playsound == 20)
        {
        mp3Trigger.play(53);  
        }
    }

} // end idle


void playseen()
{
int playsound = 0;
randomSeed(now);
playsound = random(10,24);
  if ((playsound > 14) && (muteSounds == false))
    {
        if (playsound == 15)
        {
        mp3Trigger.play(14); 
        }
        else if (playsound == 15)
        {
        mp3Trigger.play(15); 
        }
        else if (playsound == 16)
        {
        mp3Trigger.play(16); 
        }
        else if (playsound == 17)
        {
        mp3Trigger.play(20); 
        }
        else if (playsound == 18)
        {
        mp3Trigger.play(18); 
        }
        else if (playsound == 19)
        {
        mp3Trigger.play(24); 
        }
        else if (playsound == 20)
        {
        mp3Trigger.play(34); 
        }
        else if (playsound == 21)
        {
        mp3Trigger.play(38); 
        }
        else if (playsound == 22)
        {
        mp3Trigger.play(41); 
        }
        else if (playsound == 23)
        {
        mp3Trigger.play(43); 
        }


    }
} // end seen

void playgone()
{
int playsound = 0;
randomSeed(now);
playsound = random(14,24);
  if ((playsound > 14) && (muteSounds == false))
    {
        if (playsound == 15)
        {
        mp3Trigger.play(9); 
        }
        else if (playsound == 15)
        {
        mp3Trigger.play(23); 
        }
        else if (playsound == 16)
        {
        mp3Trigger.play(27); 
        }
        else if (playsound == 17)
        {
        mp3Trigger.play(28);  
        }
        else if (playsound == 18)
        {
        mp3Trigger.play(29);  
        }
        else if (playsound == 19)
        {
        mp3Trigger.play(39);  
        }
        else if (playsound == 20)
        {
        mp3Trigger.play(40);  
        }
        else if (playsound == 21)
        {
        mp3Trigger.play(42);  
        }
        else if (playsound == 22)
        {
        mp3Trigger.play(45);  
        }
        else if (playsound == 23)
        {
        mp3Trigger.play(51);  
        }
    }


} // end gone


void findhome()
{
now = millis();
unsigned long seekinghome = now;


    homecheckval =  (digitalRead(homeSensor)); // read the home position sensor  to determine if we need to move

if (debugmode == true )
    {
    Serial.print("Now:");
    Serial.print(now);
    Serial.print(" Home:");
    Serial.println(seekinghome);
    Serial.print("Home Sensor value: ");
    Serial.println(homecheckval);
    }


      while (homecheckval !=  HIGH)
        {
        now = millis();
        homecheckval =  (digitalRead(homeSensor)); // read the home position sensor
        if (lastdirection == 1)
            {
            Syren10.motor(1,60); // turn left to find home
            motorson = true;
            }
        else
            {
            Syren10.motor(1,-60); // turn right to find home
            motorson = true;
            }
        


        if ((now - seekinghome) >= 9000)
                  {
                  stopx();
                  if (debugmode == true)
                    {
                    Serial.println("hometimeout");
                    }
                  break;
                  }

        relativemove = 0;
        movecount = 0;// reset the count of how many times automation moved the dome
                  
        }

 
 stopx();
 
} // end find home routine



