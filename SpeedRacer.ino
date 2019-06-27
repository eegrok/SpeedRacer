/*
    Speed Racer
    by Move38, Inc. 2019
    Lead development by Dan King
    original game by Dan King, Jonathan Bobrow

    Rules: https://github.com/Move38/SpeedRacer/blob/master/README.md

    --------------------
    Blinks by Move38
    Brought to life via Kickstarter 2018

    @madewithblinks
    www.move38.com
    --------------------
*/

//#include "Serial.h"
//ServicePortSerial sp;

enum gameStates {SETUP, PLAY, CRASH};
byte gameState = SETUP;

//SETUP DATA
bool connectedFaces[6];

//PLAY DATA
enum playStates {LOOSE, THROUGH, ENDPOINT};
byte playState = LOOSE;

enum faceRoadStates {FREEAGENT, ENTRANCE, EXIT, SIDEWALK};
byte faceRoadInfo[6];

enum handshakeStates {NOCAR, HAVECAR, READY, CARSENT};
byte handshakeState = NOCAR;

bool hasEntrance = false;
byte entranceFace = 0;

bool hasExit = false;
byte exitFace = 0;

bool haveCar = false;
bool carPassed = false;
word carProgress = 0;//from 0-100 is the regular progress

byte currentSpeed = 1;
#define SPEED_INCREMENTS 100
word currentTransitTime;
#define MIN_TRANSIT_TIME 750
#define MAX_TRANSIT_TIME 1500
Timer transitTimer;

//CRASH DATA
bool crashHere = false;

#define CAR_FADE_IN_DIST   200   // kind of like headlights
long carFadeOutDistance = 40 * currentSpeed; // the tail should have a relationship with the speed being travelled

void setup() {
  gameState = SETUP;
  //sp.begin();
  randomize();
}

void loop() {

  //run loops
  switch (gameState) {
    case SETUP:
      setupLoop();
      break;
    case PLAY:
      gameLoop();
      break;
    case CRASH:
      crashLoop();
      break;
  }

  //run graphics
  switch (gameState) {
    case SETUP:
      setupGraphics();
      break;
    case PLAY:
      playGraphics();
      break;
    case CRASH:
      crashGraphics();
      break;
  }

  //update communication
  switch (gameState) {
    case SETUP://this one is simple
      setValueSentOnAllFaces(SETUP << 4);
      break;
    case PLAY:
      FOREACH_FACE(f) {
        byte sendData = (PLAY << 4) + (faceRoadInfo[f] << 2) + handshakeState;
        setValueSentOnFace(sendData, f);
      }
      break;
    case CRASH:
      setValueSentOnAllFaces(CRASH << 4);
      break;
  }

  // TODO: Remove this, it is just a tool for reseting the game while in development
  if (buttonLongPressed()) {
    gameReset();
  }
}

void setupLoop() {

  //update connected faces array
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //something here
      connectedFaces[f] = true;
    } else {
      connectedFaces[f] = false;
    }
  }

  //listen for transition to PLAY
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //something here
      if (getGameState(getLastValueReceivedOnFace(f)) == PLAY) {//transition to PLAY
        gameState = PLAY;
        playState = LOOSE;
        FOREACH_FACE(f) {
          faceRoadInfo[f] = FREEAGENT;
        }
      }
    }
  }

  //listen for double click, but only if not alone
  if (!isAlone()) {
    if (buttonDoubleClicked()) {
      gameState = PLAY;
      handshakeState = HAVECAR;
      playState = ENDPOINT;
      currentSpeed = 1;
      currentTransitTime = map(SPEED_INCREMENTS - currentSpeed, 0, SPEED_INCREMENTS, MIN_TRANSIT_TIME, MAX_TRANSIT_TIME);
      transitTimer.set(currentTransitTime);
      FOREACH_FACE(f) {
        faceRoadInfo[f] = SIDEWALK;
      }

      //assign entrance/exit semi-randomly
      FOREACH_FACE(f) {
        if (!hasExit) {//only keep looking if no entrance/exit assigned
          if (!isValueReceivedOnFaceExpired(f)) { //something here
            hasExit = true;
            exitFace = f;
            setRoadInfoOnFace(EXIT, exitFace);

            hasEntrance = true;
            entranceFace = (f + 3) % 6;
            setRoadInfoOnFace(ENTRANCE, entranceFace);
          }
        }
      }

      //start the car
      haveCar = true;
      carPassed = false;
    }
  }
}

void setRoadInfoOnFace( byte info, byte face) {
  if ( face < 6 ) {
    faceRoadInfo[face] = info;
  }
  else {
    //sp.println("ERR-1"); // tried to write to out of bounds array
  }
}

void gameLoop() {

  if (playState == LOOSE) {
    gameLoopLoose();
  } else {
    gameLoopRoad();
  }

  //check for crash signal regardless of state
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getGameState(neighborData) == CRASH) {
        crashReset();
      }
    }
  }
}

void gameLoopLoose() {
  //I need to look for neighbors that make me not alone no more
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getGameState(neighborData) == PLAY) {//he's playing the game
        if (getRoadState(neighborData) == EXIT) {//he wants me to become a road piece!
          //become a road piece
          playState = ENDPOINT;
          entranceFace = f;
          hasEntrance = true;
        } else {
          //TODO: USE CONNECTED FACES ARRAY TO MAKE SOME OH NO SIGNALS
        }
      }
    }
  }
  //if I become a road piece, I need to get my info set up
  if (playState == ENDPOINT) {
    FOREACH_FACE(f) {
      setRoadInfoOnFace(SIDEWALK, f);
    }
    setRoadInfoOnFace(ENTRANCE, entranceFace);
    assignExit();
  }
}

void assignExit() {

  //check to see if a preferred exit face exists
  FOREACH_FACE(f) {
    if (!hasExit) {
      if (isValidExit(f)) {
        if (!isValueReceivedOnFaceExpired(f)) {
          byte neighborData = getLastValueReceivedOnFace(f);
          if (getRoadState(neighborData) == FREEAGENT || getRoadState(neighborData) == ENTRANCE) {
            hasExit = true;
            exitFace = f;
            setRoadInfoOnFace(EXIT, exitFace);
          }
        }
      }
    }
  }

  //so I've made it to the end of the preferred exit check. Do I have an exit?
  if (!hasExit) {
    hasExit = true;
    exitFace = (entranceFace + random(2) + 2) % 6;
    setRoadInfoOnFace(EXIT, exitFace);
  }
}

bool isValidExit(byte face) {
  if (face == (entranceFace + 2) & 6) {
    return true;
  } else if (face == (entranceFace + 3) & 6) {
    return true;
  } else if (face == (entranceFace + 4) & 6) {
    return true;
  } else {
    return false;
  }
}

void gameLoopRoad() {

  if (playState == ENDPOINT) {
    //search for a FREEAGENT on your exit face
    //if you find one, send a speed packet
    if (!hasExit) {
      //sp.println("ERR-3"); // out of bounds...
    }
    else if (!isValueReceivedOnFaceExpired(exitFace)) { //there is someone on my exit face
      byte neighborData = getLastValueReceivedOnFace(exitFace);
      if (getGameState(neighborData) == PLAY) {//this neighbor is able to accept a packet

        playState = THROUGH;

      }
    }
  }

  if (haveCar) {
    if (transitTimer.isExpired()) {
      //ok, so here is where shit gets tricky
      if ( !hasExit ) {
        //sp.println("ERR-4"); // out of bounds...
      } else if (!isValueReceivedOnFaceExpired(exitFace)) {
        byte neighborData = getLastValueReceivedOnFace(exitFace);
        if (getRoadState(neighborData) == ENTRANCE) {
          if (getHandshakeState(neighborData) == READY) {
            handshakeState = CARSENT;
            carPassed = true;
            haveCar = false;
            //TODO: DATAGRAM
          } else {
            //CRASH because not ready
            //sp.println("CRASH here");
            crashReset();
            crashHere = true;
          }
        } else {
          //CRASH crash because not entrance
          //sp.println("CRASH here");
          crashReset();
          crashHere = true;
        }
      } else {
        //CRASH because not there!
        crashReset();
        crashHere = true;
      }
    }
  } else {//I don't have the car
    //under any circumstances, you should go loose if you're alone
    if (isAlone()) {
      looseReset();
    }

    if (handshakeState == CARSENT) {//so I've passed the car
      if (getHandshakeState(getLastValueReceivedOnFace(exitFace)) == HAVECAR) {//the road I passed to has received it
        handshakeState = NOCAR;
      }
    }
    
    //check your entrance face for... things happening
    if (!isValueReceivedOnFaceExpired(entranceFace)) {//there's some on my entrance face
      byte neighborData = getLastValueReceivedOnFace(entranceFace);
      if (getGameState(neighborData) == PLAY) { //this guy is in PLAY state, so I can trust that this isn't the transition period
        if (getRoadState(neighborData) == EXIT) {//ok, so it could send me a car. Is it?
          if (handshakeState == NOCAR) {//check and see if they are in HAVECAR
            if (getHandshakeState(neighborData) == HAVECAR) {
              handshakeState = READY;
            }
          } else if (handshakeState == READY) {
            if (getHandshakeState(neighborData) == CARSENT) {
              //THEY HAVE SENT THE CAR. BECOME THE ACTIVE GUY
              handshakeState = HAVECAR;
              haveCar = true;
              currentSpeed = 1;
              currentTransitTime = map(SPEED_INCREMENTS - currentSpeed, 0, SPEED_INCREMENTS, MIN_TRANSIT_TIME, MAX_TRANSIT_TIME);
              transitTimer.set(currentTransitTime);
            }
          }
        }
      }
    } else {//there's nothing on my entrance face. Hmm...
      handshakeState = NOCAR;
    }
  }//end haveCar checks
}

void looseReset() {

  playState = LOOSE;
  handshakeState = NOCAR;
  haveCar = false;
  carPassed = false;
  currentSpeed = 1;
  entranceFace = 6;
  hasEntrance = false;
  exitFace = 6;
  hasExit = false;

  FOREACH_FACE(f) {
    faceRoadInfo[f] = FREEAGENT;
  }
}

void crashReset() {
  looseReset();
  gameState = CRASH;
}

void gameReset() {
  crashReset();
  crashHere = false;
  gameState = SETUP;
}

void crashLoop() {
  //listen for transition to SETUP
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //something here
      if (getGameState(getLastValueReceivedOnFace(f)) == SETUP) {//transition to PATHFIND
        gameState = SETUP;
      }
    }
  }

  //listen for double click
  if (buttonDoubleClicked()) {
    gameState = SETUP;
    //sp.println("I'm sending us back to SETUP");
  }
}

byte getGameState(byte neighborData) {
  return (neighborData >> 4);//1st and 2nd bits
}

byte getRoadState(byte neighborData) {
  return ((neighborData >> 2) & 3);//3rd and 4th bits
}

byte getHandshakeState(byte neighborData) {
  return (neighborData & 3);//5th and 6th bits
}

void setupGraphics () {
  setColor(CYAN);
  FOREACH_FACE(f) {
    if (connectedFaces[f] == true) {
      setColorOnFace(YELLOW, f);
    }
  }
}

void playGraphics() {
  FOREACH_FACE(f) {
    switch (faceRoadInfo[f]) {
      case FREEAGENT:
        setColorOnFace(MAGENTA, f);
        break;
      case ENTRANCE:
        setColorOnFace(YELLOW, f);
        break;
      case EXIT:
        setColorOnFace(YELLOW, f);
        break;
      case SIDEWALK:

        //NOCAR, HAVECAR, READY, CARSENT
        switch (handshakeState) {
          case NOCAR:
            setColorOnFace(OFF, f);
            break;
          case HAVECAR:
            setColorOnFace(dim(WHITE, 100), f);
            break;
          case READY:
            setColorOnFace(dim(YELLOW, 100), f);
            break;
          case CARSENT:
            setColorOnFace(dim(RED, 100), f);
            break;
        }
        break;
    }
  }

  if (haveCar) {
    //    carProgress = (100 * (currentTransitTime - transitTimer.getRemaining())) / currentTransitTime;
    //    sp.print(F("car: "));
    //    sp.println(carProgress);
    //    //    sp.print(F("time: "));
    //    //    sp.println(transitTimer.getRemaining());
    //    //    sp.print(F("cur: "));
    //    //    sp.println(currentTransitTime);
    //    //    sp.println("-");
    //    if (hasEntrance && hasExit) {
    //      FOREACH_FACE(f) {
    //        setColorOnFace(getFaceColorBasedOnCarPosition(f, carProgress, entranceFace, exitFace), f);
    //      }
    //    }
  }
}

void crashGraphics() {
  if (crashHere) {
    setColor(RED);
  } else {
    setColor(ORANGE);
  }
}

/*
   fade from the first side to the opposite side
   front of the fade should be faster than the fall off

*/

/*
  Color getFaceColorBasedOnCarPosition(byte face, byte pos, byte from, byte to) {
  byte hue, saturation, brightness;
  byte carFadeInDistance = 20;
  byte carFadeOutDistance = 50;

  byte loBound, hiBound;

  // are we going straight, turning left, or turning right
  if ( (from + 6 - to) % 6 == 3 ) {

    byte center;
    byte faceRotated = (6 + face - from) % 6;
    switch ( faceRotated ) { //... rotate to the correct direction
      case 0: center = 0;  break;
      case 1: center = 33; break;
      case 2: center = 67; break;
      case 3: center = 100;  break;
      case 4: center = 67; break;
      case 5: center = 33; break;
    }

    if (carFadeInDistance > center) {
      loBound = 0;
    }
    else {
      loBound = center - carFadeInDistance;
    }

    // we are traveling straight
    if ( pos < loBound || pos > carFadeOutDistance + center ) {
      // out of range for us...
      brightness = 0;
    }

    else if ( pos < center ) {
      // fade in
      brightness = (byte) map(pos, loBound, center, 0, 255);
    }

    else if ( pos == center ) {
      brightness = 255;
    }

    else if ( pos > center ) {
      // fade out
      if ( pos - center > carFadeOutDistance) {
        brightness = 0;
      }
      else {
        brightness = 255 - (byte) map(pos, center, carFadeOutDistance + center, 0, 255);
      }
    }

  }

  else if ( (from + 6 - to) % 6 == 2 ) {
    // we are turning right
    byte center;
    byte faceRotated = (6 + face - from) % 6;
    switch ( faceRotated ) { //... rotate to the correct direction
      case 0: center = 0;  break;
      case 1: center = 25; break;
      case 2: center = 50;  break;
      case 3: center = 75; break;
      case 4: center = 100;  break;
      case 5: center = 50;  break;
    }

    if (carFadeInDistance > center) {
      loBound = 0;
    }
    else {
      loBound = center - carFadeInDistance;
    }

    // inner side shouldn't light up on the turn
    if ( faceRotated == 5 || pos < loBound || pos > carFadeOutDistance + center ) {
      // out of range for us...
      brightness = 0;
    }

    else if ( pos < center ) {
      // fade in
      brightness = (byte) map(pos, loBound, center, 0, 255);
    }

    else if ( pos == center ) {
      brightness = 255;
    }

    else if ( pos > center ) {
      // fade out
      if ( pos - center > carFadeOutDistance) {
        brightness = 0;
      }
      else {
        brightness = 255 - (byte) map(pos, center, carFadeOutDistance + center, 0, 255);

        sp.print(brightness);
        sp.print(F(", pos: "));
        sp.println(pos);
      }
    }
  }

  else if ( (from + 6 - to) % 6 == 4 ) {
    // we are turning left
    byte center;
    byte faceRotated = (6 + face - from) % 6;
    switch ( faceRotated ) { //... rotate to the correct direction
      case 0: center = 0;  break;
      case 1: center = 50;  break;
      case 2: center = 100;  break;
      case 3: center = 75; break;
      case 4: center = 50;  break;
      case 5: center = 25; break;
    }

    if (carFadeInDistance > center) {
      loBound = 0;
    }
    else {
      loBound = center - carFadeInDistance;
    }

    // inner side shouldn't light up on the turn
    if ( faceRotated == 1 || pos < loBound || pos > carFadeOutDistance + center ) {
      // out of range for us...
      brightness = 0;
    }

    else if ( pos < center ) {
      // fade in
      //      brightness = 0;
      brightness = (byte) map(pos, loBound, center, 0, 255);
    }

    else if ( pos == center ) {
      brightness = 255;
    }

    else if ( pos > center ) {
      // fade out
      if ( pos - center > carFadeOutDistance) {
        brightness = 0;
      }
      else {
        brightness = 255 - (byte) map(pos, center, carFadeOutDistance + center, 0, 255);
      }
    }
  }

  return makeColorHSB(0, 0, brightness);
  }
*/
