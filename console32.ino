#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// Display Setup (ESP32-C3: SDA=6, SCL=7)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Pins
const int PIN_HOCH    = 1; 
const int PIN_RUNTER  = 2; 
const int PIN_SPRUNG  = 3; 
const int PIN_AKTION  = 4; 
const int PIN_LVLF    = 5; 

enum State { MENU, GAME_PLATFORMER, GAME_SPACE, GAMEOVER_SPACE, WIN };
State currentState = MENU;

unsigned long nextFrame = 0;
int menuIdx = 0;
int animFrame = 0;

// --- STARFIELD & EFFEKTE ---
struct Star { float x, y, speed; };
const int MAX_STARS = 15;
Star stars[MAX_STARS];              

void initStars() {
  for(int i = 0; i < MAX_STARS; i++) {
    stars[i].x = random(0, 128);
    stars[i].y = random(0, 64);
    stars[i].speed = (random(2, 10) / 10.0);
  }
}

void drawStarfield() {
  for(int i = 0; i < MAX_STARS; i++) {
    stars[i].y += stars[i].speed;
    if(stars[i].y > 64) { stars[i].y = 0; stars[i].x = random(0, 128); }
    u8g2.drawPixel(stars[i].x, stars[i].y);
    if(stars[i].speed > 0.7) u8g2.drawPixel(stars[i].x + 1, stars[i].y);
  }
}

void drawPlatformerBG() {
  u8g2.drawLine(0, 50, 20, 35);
  u8g2.drawLine(20, 35, 45, 55);
  u8g2.drawLine(40, 55, 70, 25);
  u8g2.drawLine(70, 25, 100, 58);
  u8g2.drawLine(90, 58, 128, 40);
}

// --- EXPLOSIONS & KONFETTI ---
float exX[12], exY[12], exVX[12], exVY[12];
int exTimer = 0;

void startExplosion(float x, float y) {
  exTimer = 25; 
  for(int i=0; i<12; i++) {
    exX[i] = x; exY[i] = y;
    exVX[i] = (random(-150, 150) / 40.0);
    exVY[i] = (random(-150, 150) / 40.0);
  }
}

// --- PLATFORMER DATEN ---
struct Platform { int x, y, w, h; };
struct Enemy { float x, y, speed, range, startX; };
struct Coin { int x, y; bool collected; };
struct Level {
  Platform plats[5]; int platCount;
  Enemy enemy;
  Coin coins[3]; int coinCount;
  int goalX, goalY;
};

// Hier deine Level-Daten (gekürzt für Übersicht, nimm deine 40 Level!)
PROGMEM Level world[] = {
  {{{112,36,9,26},{-4,34,94,6},{0,4,88,7},{82,4,6,35}}, 4, {86,30,1.0,50,86}, {{124,6},{20,60},{69,30}}, 3, 122, 57},
  {{{-1,51,129,5},{49,6,4,45},{53,24,17,4},{80,18,16,3}}, 4, {21,59,1.0,50,21}, {{50,2},{62,20},{88,14}}, 3, 88, 57},
  {{{0,50,40,4}}, 1, {50,46,0.5,30,50}, {{20,35},{80,45}}, 2, 110,20},
  {{{10,55,30,4}, {60,45,30,4}}, 2, {60,41,1.0,20,60}, {{20,40},{75,30}}, 2, 100,10},
  {{{0,50,20,4}, {40,40,20,4}, {80,30,20,4}}, 3, {40,36,1.2,20,40}, {{10,30},{90,15}}, 2, 115,5},
  {{{5,55,110,2}}, 1, {10,51,2.0,90,10}, {{30,40},{60,40},{90,40}}, 3, 115,40},
  {{{0,45,20,4}, {40,45,20,4}, {80,45,20,4}}, 3, {40,41,1.5,15,40}, {{10,30},{90,30}}, 2, 115,30},
  {{{10,50,15,3}, {40,40,15,3}, {70,30,15,3}}, 3, {40,37,0.8,20,40}, {{15,35},{75,15}}, 2, 105,10},
  {{{0,58,128,2}, {40,30,40,3}}, 2, {45,26,2.5,30,45}, {{60,15}}, 1, 10,20},
  {{{10,30,30,4}, {80,30,30,4}}, 2, {80,26,1.3,30,80}, {{25,15},{95,15}}, 2, 55,10},
  {{{20,50,20,4}, {50,35,20,4}, {80,20,20,4}}, 3, {50,31,1.1,20,50}, {{25,40},{85,10}}, 2, 110,5},
  {{{10,55,20,4}, {100,55,20,4}, {55,30,20,4}}, 3, {55,26,2.0,20,55}, {{15,40},{105,40}}, 2, 60,15},
  {{{0,50,15,2}, {30,40,15,2}, {60,30,15,2}, {90,20,15,2}}, 4, {60,26,1.2,20,60}, {{35,25},{95,5}}, 2, 115,5},
  {{{10,55,100,2}, {40,30,40,2}}, 2, {20,51,2.8,80,20}, {{55,15}}, 1, 10,20},
  {{{5,25,30,2}, {45,40,30,2}, {85,25,30,2}}, 3, {45,36,1.5,25,45}, {{20,10},{95,10}}, 2, 60,50},
  {{{0,55,128,2}, {50,45,20,10}}, 2, {10,51,3.0,110,10}, {{60,30}}, 1, 115,10},
  {{{20,55,10,2}, {40,45,10,2}, {60,35,10,2}, {80,25,10,2}}, 4, {10,50,0.5,100,10}, {{25,45},{85,15}}, 2, 115,15},
  {{{0,40,20,2}, {100,40,20,2}, {50,55,28,2}}, 3, {50,51,2.2,25,50}, {{10,20},{110,20}}, 2, 60,35},
  {{{10,20,100,2}, {10,50,100,2}}, 2, {20,16,1.8,80,20}, {{20,40},{100,40}}, 2, 60,5},
  {{{0,50,30,2}, {90,50,30,2}, {45,30,38,2}}, 3, {45,26,1.5,35,45}, {{15,35},{110,35},{65,15}}, 3, 65,55},
  {{{5,55,118,4}}, 1, {10,51,3.5,100,10}, {{20,40},{60,40},{100,40}}, 3, 5,30},
  {{{10,55,10,2}, {40,45,10,2}, {70,35,10,2}, {100,25,10,2}, {50,15,30,2}}, 5, {10,51,0.8,110,10}, {{15,45},{105,15},{60,5}}, 3, 115,5},
    // Neue Level 21-25
  {{{0,55,30,2}, {40,45,30,2}, {80,35,30,2}, {40,25,30,2}, {0,15,30,2}}, 5, {40,21,1.5,30,40}, {{85,25},{10,5}}, 2, 5, 5},
  {{{0,50,20,2}, {100,50,28,2}}, 2, {20,60,3.5,80,20}, {{50,40},{60,40},{70,40}}, 3, 115, 40},
  {{{0,20,40,2}, {50,20,40,2}, {100,20,28,2}}, 3, {10,51,0.5,100,10}, {{20,10},{70,10},{115,10}}, 3, 115, 40},
  {{{10,50,10,2}, {30,35,10,2}, {55,20,15,2}, {80,35,10,2}, {100,50,10,2}}, 5, {55,16,1.0,15,55}, {{35,25},{85,25}}, 2, 110, 10},
  {{{0,45,128,2}, {0,25,100,2}}, 2, {10,21,4.0,80,10}, {{110,35},{10,15},{50,15}}, 3, 5, 5},
  // Level 26: Der "Sprung des Glaubens" (Lücke in der Mitte)
  {{{0,50,30,2}, {100,50,28,2}, {50,30,28,2}}, 3, {50,26,1.2,28,50}, {{15,40},{115,40}}, 2, 65, 15},

  // Level 27: Treppensteigen
  {{{10,55,20,2}, {35,45,20,2}, {60,35,20,2}, {85,25,20,2}, {110,15,18,2}}, 5, {10,51,0.5,100,10}, {{40,30},{90,10}}, 2, 115, 5},

  // Level 28: Die Falle (Gegner patrouilliert oben)
  {{{0,58,128,2}, {20,30,88,2}}, 2, {25,26,2.8,75,25}, {{50,15},{70,15},{90,15}}, 3, 5, 45},

  // Level 29: Slalom
  {{{10,15,10,40}, {40,0,10,45}, {70,15,10,45}, {100,0,10,45}}, 4, {10,51,0.8,100,10}, {{25,50},{55,50},{85,50}}, 3, 115, 50},

  // Level 30: Schwebende Inseln
  {{{10,50,15,2}, {40,50,15,2}, {70,50,15,2}, {100,50,15,2}, {55,30,20,2}}, 5, {55,26,1.4,20,55}, {{15,40},{110,40}}, 2, 65, 15},

  // Level 31: Enge Passage
  {{{0,40,50,2}, {70,40,58,2}, {55,55,10,2}}, 3, {0,36,2.2,120,20}, {{5,30},{120,30}}, 2, 60, 45},

  // Level 32: Der Gipfel
  {{{5,55,118,2}, {20,45,90,2}, {40,35,50,2}, {60,25,20,2}}, 4, {60,21,1.0,20,60}, {{25,35},{105,35}}, 2, 65, 10},

  // Level 33: Tunnel-Lauf
  {{{0,25,128,2}, {0,45,128,2}}, 2, {10,38,3.2,100,10}, {{30,35},{60,35},{90,35}}, 3, 115, 55},

  // Level 34: Die Säulen
  {{{20,33,5,30}, {50,20,5,30}, {80,20,5,43}, {110,20,5,43}}, 4, {10,15,1.5,100,10}, {{35,50},{65,50},{95,50}}, 3, 5, 10},

  // Level 35: Plattform-Chaos
  {{{5,20,20,2}, {100,20,20,2}, {40,35,50,2}, {10,50,20,2}, {100,50,20,2}}, 5, {45,31,2.0,40,45}, {{15,10},{110,10}}, 2, 65, 55},

  // Level 36: Der schnelle Flitzer
  {{{0,55,128,2}}, 1, {5,51,4.5,110,5}, {{20,40},{60,40},{100,40}}, 3, 120, 45},

  // Level 37: Höhlensprung
  {{{0,30,40,2}, {90,30,38,2}, {50,15,28,2}, {50,45,28,2}}, 4, {55,41,0.9,15,55}, {{15,20},{115,20}}, 2, 65, 5},

  // Level 38: Tanz auf dem Vulkan
  {{{10,50,10,2}, {30,50,10,2}, {50,50,10,2}, {70,50,10,2}, {90,50,10,2}}, 5, {10,46,2.5,90,10}, {{35,40},{75,40}}, 2, 115, 45},

  // Level 39: Präzision ist alles
  {{{5,55,10,2}, {115,55,10,2}, {60,40,10,2}, {30,25,10,2}, {90,25,10,2}}, 5, {10,10,1.2,100,10}, {{65,30}}, 1, 118, 45},

  // Level 40: DAS FINALE (Sehr schwer)
  {{{0,55,128,2}, {20,40,20,2}, {55,30,20,2}, {90,40,20,2}, {55,15,20,2}}, 5, {10,51,5.0,110,10}, {{25,30},{100,30},{65,5}}, 3, 5, 10},
  
};
const int MAX_LEVELS = sizeof(world) / sizeof(Level);

float pX, pY, vY;
bool isJumping;
int currentLevel = 0, coinsCollected = 0;
float landingAnim = 0;
int collectX, collectY, collectAnim = 0;

void loadLevel(int lvl) {
  if (lvl >= MAX_LEVELS) { currentState = WIN; return; }
  pX = 5; pY = 40; vY = 0; isJumping = false; landingAnim = 0;
  coinsCollected = 0;
  for(int i=0; i<3; i++) world[lvl].coins[i].collected = false;
  world[lvl].enemy.x = world[lvl].enemy.startX;
  exTimer = 0;
}

// --- SPACE GAME ---
float shipX = 60;
struct Meteor { float x, y, speed; bool active; };
Meteor meteors[5];
int spaceScore = 0;

void initSpaceGame() {
  shipX = 60; spaceScore = 0; exTimer = 0;
  for(int i=0; i<5; i++) meteors[i].active = false;
}

void setup() {
  Wire.begin(6, 7);
  Wire.setClock(400000);
  u8g2.begin();
  initStars();
  pinMode(PIN_HOCH,    INPUT_PULLUP);
  pinMode(PIN_RUNTER,  INPUT_PULLUP);
  pinMode(PIN_SPRUNG,  INPUT_PULLUP);
  pinMode(PIN_AKTION,  INPUT_PULLUP);
  pinMode(PIN_LVLF,    INPUT_PULLUP);
}

void runPlatformer() {
  drawPlatformerBG();
  
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(110, 7);
  u8g2.print("L"); u8g2.print(currentLevel + 1);

  if (exTimer > 0) {
    for(int i=0; i<12; i++) {
      exX[i] += exVX[i]; exY[i] += exVY[i];
      u8g2.drawPixel(exX[i], exY[i]);
    }
    exTimer--;
    if (exTimer == 0) loadLevel(currentLevel); 
    return;
  }

  // Cheat/Level-Skip Button
  if (digitalRead(PIN_LVLF) == LOW){
    currentLevel++;
    if(currentLevel >= MAX_LEVELS) { currentState = WIN; return; }
    loadLevel(currentLevel);
    delay(200);
    return;
  }

  // WICHTIG: Erst prüfen, ob Level gültig, dann Daten holen
  Level &l = world[currentLevel];

  if (digitalRead(PIN_RUNTER) == LOW) pX -= 2.1;
  if (digitalRead(PIN_HOCH) == LOW) pX += 2.1;
  
  vY += 0.35;
  float nextY = pY + vY;

  if (nextY >= 59) { if(isJumping) landingAnim = 4; nextY = 59; vY = 0; isJumping = false; }
  for (int i=0; i<l.platCount; i++) {
    if (pX + 4 > l.plats[i].x && pX < l.plats[i].x + l.plats[i].w) {
      if (pY + 4 <= l.plats[i].y && nextY + 4 >= l.plats[i].y) {
        if(isJumping) landingAnim = 4;
        nextY = l.plats[i].y - 4; vY = 0; isJumping = false;
      }
    }
  }
  pY = nextY;
  if (digitalRead(PIN_SPRUNG) == LOW && !isJumping) { vY = -5.2; isJumping = true; }
  if (landingAnim > 0) landingAnim -= 0.5;

  for (int i=0; i<l.platCount; i++) u8g2.drawBox(l.plats[i].x, l.plats[i].y, l.plats[i].w, l.plats[i].h);
  u8g2.drawHLine(0, 63, 128);

  l.enemy.x += l.enemy.speed;
  if (l.enemy.x > l.enemy.startX + l.enemy.range || l.enemy.x < l.enemy.startX) l.enemy.speed *= -1;
  u8g2.drawTriangle(l.enemy.x-1, l.enemy.y, l.enemy.x+5, l.enemy.y, l.enemy.x+2, l.enemy.y-3);

  if (abs(pX - l.enemy.x) < 5 && abs(pY - l.enemy.y) < 5) startExplosion(pX + 2, pY + 2);

  float sW = 4 + landingAnim, sH = 4 - (landingAnim/2);
  u8g2.drawBox(pX - (sW-4)/2, pY + (4-sH), sW, sH);

for (int i=0; i<l.coinCount; i++) {
  if (!l.coins[i].collected) {
    // 1. Zeichnen: Radius auf 3 erhöht (statt 1)
    // Die Verschiebung (+1) wurde auf +3 angepasst, damit sie mittig bleibt
    u8g2.drawDisc(l.coins[i].x + 2, l.coins[i].y + 2 + (int)(sin(animFrame * 0.3) * 2), 2);

    // 2. Kollision: Abfrage-Distanz von <5 auf <8 erhöht
    if (abs(pX - l.coins[i].x) < 8 && abs(pY - l.coins[i].y) < 8) { 
      l.coins[i].collected = true; 
      coinsCollected++; 
      collectAnim = 8; 
      collectX = pX; 
      collectY = pY;
    }
  }
}

  if(collectAnim > 0) { u8g2.drawCircle(collectX, collectY, 10-collectAnim); collectAnim--; }

  if (coinsCollected >= l.coinCount) {
    u8g2.drawFrame(l.goalX, l.goalY, 6, 6);
    if (abs(pX - l.goalX) < 6 && abs(pY - l.goalY) < 6) {
      currentLevel++;
      if (currentLevel >= MAX_LEVELS) currentState = WIN;
      else loadLevel(currentLevel);
      delay(200);
    }
  }
}

void runSpaceGame() {
  drawStarfield(); 
  
  if (exTimer > 0) {
    for(int i=0; i<12; i++) {
      exX[i] += exVX[i]; exY[i] += exVY[i];
      u8g2.drawPixel(exX[i], exY[i]);
    }
    exTimer--;
    if (exTimer == 0) currentState = GAMEOVER_SPACE;
    return;
  }

  // Schiff-Steuerung
  if(digitalRead(PIN_RUNTER) == LOW) shipX -= 3;
  if(digitalRead(PIN_HOCH) == LOW) shipX += 3;
  shipX = constrain(shipX, 4, 124);
  
  // Schiff zeichnen
  u8g2.drawTriangle(shipX, 52, shipX-4, 60, shipX+4, 60);
  if(animFrame % 2 == 0) u8g2.drawLine(shipX, 61, shipX, 63); // Triebwerks-Feuer

  for(int i=0; i<5; i++) {
    if(meteors[i].active) {
      meteors[i].y += meteors[i].speed;

      // --- NEU: DER SCHWEIF ---
      // Wir zeichnen 4 Punkte hinter dem Meteoriten
      u8g2.drawPixel(meteors[i].x, meteors[i].y - 6);
      u8g2.drawPixel(meteors[i].x, meteors[i].y - 9);
      
      // Flackernde Seiten-Partikel
      if(animFrame % 2 == 0) {
        u8g2.drawPixel(meteors[i].x - 2, meteors[i].y - 6);
        u8g2.drawPixel(meteors[i].x + 2, meteors[i].y - 10);
      } else {
        u8g2.drawPixel(meteors[i].x + 2, meteors[i].y - 6);
        u8g2.drawPixel(meteors[i].x - 2, meteors[i].y - 10);
      }
      // ------------------------

      // Meteoriten-Kopf
      u8g2.drawCircle(meteors[i].x, meteors[i].y, 3);
      u8g2.drawPixel(meteors[i].x, meteors[i].y); // Kern des Meteoriten

      // Kollision und Reset
      if(meteors[i].y > 70) { // Etwas weiter aus dem Bild laufen lassen
        meteors[i].active = false; 
        spaceScore++; 
      }
      
      if(abs(meteors[i].x - shipX) < 6 && abs(meteors[i].y - 56) < 6) {
        startExplosion(shipX, 56);
      }
    } else if(random(0,15) == 1) {
      meteors[i].x = random(5,123); 
      meteors[i].y = -10; 
      meteors[i].speed = random(2, 5); 
      meteors[i].active = true;
    }
  }

  // Score Anzeige
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 7);
  u8g2.print(spaceScore);
}

void loop() {
  if (millis() < nextFrame) return;
  nextFrame = millis() + 30;
  u8g2.clearBuffer();
  animFrame++;

  if(digitalRead(PIN_AKTION) == LOW && currentState != MENU) { currentState = MENU; delay(200); }

  switch (currentState) {
    case MENU:
      drawStarfield();
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(25, 15, "CONSOLE_32v2.1");
      u8g2.drawFrame(10, 25+(menuIdx*15), 108, 14);
      u8g2.drawStr(15, 35, "PLATFORMER");
      u8g2.drawStr(15, 50, "METEOR RUN");
      if(digitalRead(PIN_HOCH) == LOW)   menuIdx = 0;
      if(digitalRead(PIN_RUNTER) == LOW) menuIdx = 1;
      if(digitalRead(PIN_SPRUNG) == LOW) { 
        if(menuIdx==0) { currentLevel = 0; loadLevel(0); currentState = GAME_PLATFORMER; } 
        else { initSpaceGame(); currentState = GAME_SPACE; }
        delay(250); 
      }
      break;
    case GAME_PLATFORMER: runPlatformer(); break;
    case GAME_SPACE:      runSpaceGame(); break;
    case GAMEOVER_SPACE:
      drawStarfield();
      u8g2.drawStr(40, 30, "GAME OVER");
      u8g2.drawUTF8(40, 40,"score:");
      u8g2.setCursor(69, 40);
      u8g2.print(spaceScore);
      if(digitalRead(PIN_SPRUNG) == LOW) { currentState = MENU; delay(250); }
      break;
    case WIN:
      drawStarfield();
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(40, 30, "YOU WON!");
      u8g2.drawStr(20, 50, "JUMP FOR MENU");
      if(digitalRead(PIN_SPRUNG) == LOW) { currentState = MENU; delay(250); }
      break;
  }
  u8g2.sendBuffer();
}