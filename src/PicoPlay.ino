#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_TinyUSB.h>

// Display stuff
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// HID Gamepad descriptor
uint8_t const desc_hid_gamepad[] = {
  TUD_HID_REPORT_DESC_GAMEPAD()
};

Adafruit_USBD_HID hid;

// Button pins
#define BTN_UP 19
#define BTN_DOWN 28
#define BTN_LEFT 16
#define BTN_RIGHT 26
#define BTN_OK 6
#define BTN_BACK 15

bool screenInverted = false;
const char* menuItems[] = { 
  "Pong", "Snake", "Space Race", "Space Shooter", 
  "Pico Flap", "Dino Run", "USB Controller", "Settings", "About" 
};
int menuCount = 9;
int selected = 0;
const unsigned long EXIT_HOLD_TIME = 3000; // 3 seconds to exit games

int menuScrollOffset = 0;
const int maxVisibleItems = 5;

bool buttonPressed(int pin) {
  return digitalRead(pin) == LOW;
}

void waitAllButtonsReleased() {
  while (buttonPressed(BTN_UP) || buttonPressed(BTN_DOWN) ||
         buttonPressed(BTN_LEFT) || buttonPressed(BTN_RIGHT) ||
         buttonPressed(BTN_OK) || buttonPressed(BTN_BACK)) {
    delay(10);
  }
}

// Check if user is holding OK + BACK to exit
bool checkExitHold(unsigned long &exitStartTime) {
  if (buttonPressed(BTN_OK) && buttonPressed(BTN_BACK)) {
    if (exitStartTime == 0) {
      exitStartTime = millis();
    }
    if (millis() - exitStartTime > EXIT_HOLD_TIME) {
      return true;
    }
    // Show progress indicator
    display.fillRect(30, 24, 78, 16, SH110X_BLACK);
    display.drawRect(30, 24, 78, 16, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(40, 28);
    display.print("Exiting...");
    int progress = map(millis() - exitStartTime, 0, EXIT_HOLD_TIME, 0, 74);
    display.fillRect(32, 36, progress, 2, SH110X_WHITE);
  } else {
    exitStartTime = 0;
  }
  return false;
}

// Returns 1 for retry, 0 for exit
int gameOverScreen(int score) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 10);
  display.print("GAME OVER");

  display.setTextSize(1);
  display.setCursor(35, 30);
  display.print("Score: ");
  display.print(score);

  // Retry button
  display.drawRect(9, 45, 50, 16, SH110X_WHITE);
  display.setCursor(19, 49);
  display.print("Retry");

  // Exit button
  display.drawRect(69, 45, 50, 16, SH110X_WHITE);
  display.setCursor(82, 49);
  display.print("Exit");
  display.display();

  waitAllButtonsReleased();

  while(true) {
    if (buttonPressed(BTN_OK)) {
      waitAllButtonsReleased();
      return 1; // retry
    }
    if (buttonPressed(BTN_BACK)) {
      waitAllButtonsReleased();
      return 0; // exit
    }
    delay(20);
  }
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Header bar
  display.fillRect(0, 0, 128, 10, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(2, 1);
  display.print("PICO PLAY");

  // Menu items
  for (int i = 0; i < maxVisibleItems; i++) {
    int itemIndex = menuScrollOffset + i;
    if (itemIndex >= menuCount) break;
    
    int yPos = 14 + (i * 10);
    if (itemIndex == selected) {
      display.drawRect(0, yPos - 1, 128, 11, SH110X_WHITE);
      display.setCursor(10, yPos);
      display.setTextColor(SH110X_WHITE);
      display.print("> ");
    } else {
      display.setCursor(10, yPos);
      display.setTextColor(SH110X_WHITE);
    }
    display.print(menuItems[itemIndex]);
  }

  // Scroll indicators
  if (menuScrollOffset > 0) {
    display.fillTriangle(124, 14, 122, 18, 126, 18, SH110X_WHITE);
  }
  if (menuScrollOffset + maxVisibleItems < menuCount) {
    display.fillTriangle(124, 62, 122, 58, 126, 58, SH110X_WHITE);
  }
  display.display();
}

// PONG - classic paddle game
void pongGame() {
  while(true) {
    int paddleY = 22;
    int paddleH = 20;
    int ballX = 64, ballY = 32;
    int velX = 2, velY = 1;
    int score = 0;
    static unsigned long exitStartTime = 0;
    exitStartTime = 0;
    waitAllButtonsReleased();

    while (true) {
      // Paddle movement
      if (buttonPressed(BTN_UP) && paddleY > 0) paddleY -= 3;
      if (buttonPressed(BTN_DOWN) && paddleY < (64 - paddleH)) paddleY += 3;

      // Ball physics
      ballX += velX;
      ballY += velY;

      // Wall bounces
      if (ballY <= 0 || ballY >= 63) velY = -velY;
      if (ballX >= 127) velX = -velX;

      // Paddle collision
      if (ballX <= 6 && ballY >= paddleY && ballY <= paddleY + paddleH) {
        velX = abs(velX) + 1;
        if(velX > 6) velX = 6; // speed cap
        score++;
      }

      // Miss!
      if (ballX < 0) {
        int choice = gameOverScreen(score);
        if (choice == 1) {
          break; // retry
        } else {
          return; // back to menu
        }
      }

      display.clearDisplay();
      display.fillRect(2, paddleY, 3, paddleH, SH110X_WHITE);
      display.fillCircle(ballX, ballY, 2, SH110X_WHITE);
      display.drawLine(127, 0, 127, 64, SH110X_WHITE);

      display.setTextSize(1);
      display.setCursor(2, 2);
      display.print(score);

      if (checkExitHold(exitStartTime)) return;

      display.display();
      delay(20);
    }
  }
}

// SNAKE - eat food, don't hit yourself
void snakeGame() {
  const int blockSize = 4;
  const int maxLen = 100;
  int snakeX[maxLen], snakeY[maxLen];
  int len = 3;
  int dirX = 1, dirY = 0;

  while(true) {
    len = 3;
    dirX = 1; dirY = 0;
    bool gameOver = false;

    // Initialize snake
    for (int i = 0; i < len; i++) {
      snakeX[i] = 20 - (i * blockSize);
      snakeY[i] = 20;
    }

    // Spawn food
    int foodX = random(1, (SCREEN_WIDTH/blockSize) - 1) * blockSize;
    int foodY = random(1, (SCREEN_HEIGHT/blockSize) - 1) * blockSize;
    static unsigned long exitStartTime = 0;
    exitStartTime = 0;
    waitAllButtonsReleased();

    while (!gameOver) {
      // Controls - can't reverse direction
      if (buttonPressed(BTN_UP) && dirY == 0) { dirX = 0; dirY = -1; }
      if (buttonPressed(BTN_DOWN) && dirY == 0) { dirX = 0; dirY = 1; }
      if (buttonPressed(BTN_LEFT) && dirX == 0) { dirX = -1; dirY = 0; }
      if (buttonPressed(BTN_RIGHT) && dirX == 0) { dirX = 1; dirY = 0; }

      // Move snake body
      for (int i = len - 1; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
      }
      snakeX[0] += dirX * blockSize;
      snakeY[0] += dirY * blockSize;

      // Check if food eaten
      if (snakeX[0] == foodX && snakeY[0] == foodY) {
        len++;
        if (len >= maxLen) len = maxLen;
        foodX = random(1, (SCREEN_WIDTH/blockSize) - 1) * blockSize;
        foodY = random(1, (SCREEN_HEIGHT/blockSize) - 1) * blockSize;
      }

      // Wall collision
      if (snakeX[0] < 0 || snakeX[0] >= SCREEN_WIDTH || snakeY[0] < 0 || snakeY[0] >= SCREEN_HEIGHT) {
        gameOver = true;
      }
      // Self collision
      for (int i = 1; i < len; i++) {
        if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) gameOver = true;
      }

      display.clearDisplay();
      display.fillRect(foodX, foodY, blockSize, blockSize, SH110X_WHITE);
      for (int i = 0; i < len; i++) {
        display.fillRect(snakeX[i], snakeY[i], blockSize, blockSize, SH110X_WHITE);
      }
      display.setTextSize(1);
      display.setCursor(2, 2);
      display.print(len - 3);

      if (checkExitHold(exitStartTime)) return;

      display.display();
      delay(150);
    }

    int choice = gameOverScreen(len - 3);
    if (choice == 1) {
      continue;
    } else {
      return;
    }
  }
}

// SPACE RACE - dodge asteroids
struct Obstacle {
  int x, y, w;
  bool active;
};
#define NUM_ASTEROIDS 8

void spaceRaceGame() {
  Obstacle asteroids[NUM_ASTEROIDS];
  int playerY = 32;
  int gameSpeed = 1;
  unsigned long score = 0;
  unsigned long startTime = millis();

  while(true) {
    playerY = 32;
    gameSpeed = 2;
    startTime = millis();
    score = 0;
    
    // Setup asteroids
    for(int i=0; i<NUM_ASTEROIDS; i++) {
      asteroids[i].active = true;
      asteroids[i].x = 128 + random(0, 200);
      asteroids[i].y = random(0, 60);
      asteroids[i].w = random(5, 15);
    }
    static unsigned long exitStartTime = 0;
    exitStartTime = 0;
    bool gameOver = false;
    waitAllButtonsReleased();

    while(!gameOver) {
      if (buttonPressed(BTN_UP) && playerY > 0) playerY -= 2;
      if (buttonPressed(BTN_DOWN) && playerY < 60) playerY += 2;

      score = (millis() - startTime) / 100;
      
      // Speed increases over time
      if (score > 0 && score % 100 == 0) {
        gameSpeed++;
        if (gameSpeed > 8) gameSpeed = 8;
      }
      
      display.clearDisplay();

      // Update and draw asteroids
      for(int i=0; i<NUM_ASTEROIDS; i++) {
        asteroids[i].x -= gameSpeed;
        if(asteroids[i].x < -asteroids[i].w) {
          asteroids[i].x = 128 + random(0, 50);
          asteroids[i].y = random(0, 60);
          asteroids[i].w = random(5, 15);
        }
        display.fillRect(asteroids[i].x, asteroids[i].y, asteroids[i].w, 4, SH110X_WHITE);

        // Collision check
        if (10 < asteroids[i].x + asteroids[i].w &&
            10 + 4 > asteroids[i].x &&
            playerY < asteroids[i].y + 4 &&
            playerY + 4 > asteroids[i].y) {
          gameOver = true;
        }
      }

      // Draw player ship
      display.fillTriangle(10, playerY, 10, playerY+4, 14, playerY+2, SH110X_WHITE);
      display.setTextSize(1);
      display.setCursor(2, 2);
      display.print(score);

      if (checkExitHold(exitStartTime)) return;

      display.display();
      delay(16);
    }

    int choice = gameOverScreen(score);
    if (choice == 1) {
      continue;
    } else {
      return;
    }
  }
}

// SPACE SHOOTER - shoot the enemies!
struct Bullet {
  int x, y;
  bool active;
};

struct Enemy {
  int x, y;
  bool active;
};

#define NUM_BULLETS 5
#define NUM_ENEMIES 8

void spaceShooterGame() {
  Bullet bullets[NUM_BULLETS];
  Enemy enemies[NUM_ENEMIES];
  int playerX = 60;

  while(true) {
    playerX = 60;
    int score = 0;
    int enemySpeed = 1;
    bool gameOver = false;

    for(int i=0; i<NUM_BULLETS; i++) {
      bullets[i].active = false;
    }
    for(int i=0; i<NUM_ENEMIES; i++) {
      enemies[i].active = true;
      enemies[i].x = random(0, 120);
      enemies[i].y = random(-60, -10);
    }
    static unsigned long exitStartTime = 0;
    exitStartTime = 0;
    waitAllButtonsReleased();

    while(!gameOver) {
      if (buttonPressed(BTN_LEFT) && playerX > 0) playerX -= 3;
      if (buttonPressed(BTN_RIGHT) && playerX < 120) playerX += 3;

      // Shoot bullet
      if (buttonPressed(BTN_OK)) {
        for(int i=0; i<NUM_BULLETS; i++) {
          if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].x = playerX + 3;
            bullets[i].y = 56;
            break;
          }
        }
        while(buttonPressed(BTN_OK)) delay(10);
      }

      display.clearDisplay();

      // Update bullets
      for(int i=0; i<NUM_BULLETS; i++) {
        if (bullets[i].active) {
          bullets[i].y -= 4;
          if (bullets[i].y < 0) {
            bullets[i].active = false;
          }
          display.fillRect(bullets[i].x, bullets[i].y, 2, 4, SH110X_WHITE);
        }
      }

      // Update enemies
      for(int i=0; i<NUM_ENEMIES; i++) {
        if (enemies[i].active) {
          enemies[i].y += enemySpeed;
          if (enemies[i].y > 64) {
            enemies[i].x = random(0, 120);
            enemies[i].y = random(-20, -10);
          }

          display.fillRect(enemies[i].x, enemies[i].y, 8, 8, SH110X_WHITE);

          // Check bullet hits
          for(int j=0; j<NUM_BULLETS; j++) {
            if (bullets[j].active) {
              if (bullets[j].x < enemies[i].x + 8 &&
                  bullets[j].x + 2 > enemies[i].x &&
                  bullets[j].y < enemies[i].y + 8 &&
                  bullets[j].y + 4 > enemies[i].y) {
                bullets[j].active = false;
                enemies[i].x = random(0, 120);
                enemies[i].y = random(-20, -10);
                score++;
                if (score > 0 && score % 20 == 0) enemySpeed++;
                if (enemySpeed > 3) enemySpeed = 3;
              }
            }
          }

          // Check player collision
          if (playerX < enemies[i].x + 8 &&
              playerX + 8 > enemies[i].x &&
              56 < enemies[i].y + 8 &&
              56 + 8 > enemies[i].y) {
            gameOver = true;
          }
        }
      }

      display.fillTriangle(playerX+4, 56, playerX, 64, playerX+8, 64, SH110X_WHITE);
      display.setTextSize(1);
      display.setCursor(2, 2);
      display.print(score);

      if (checkExitHold(exitStartTime)) return;

      display.display();
      delay(16);
    }

    int choice = gameOverScreen(score);
    if (choice == 1) {
      continue;
    } else {
      return;
    }
  }
}

// PICO FLAP - flappy bird clone
struct Pipe {
  int x;
  int gapY;
  bool passed;
};
#define NUM_PIPES 3
#define PIPE_WIDTH 8
#define PIPE_GAP 28

void flappyBirdGame() {
  Pipe pipes[NUM_PIPES];
  float playerY = 32;
  float playerVel = 0;
  float gravity = 0.2;
  float flapStrength = -3.0;
  int pipeSpeed = 2;

  while(true) {
    playerY = 32;
    playerVel = 0;
    int score = 0;
    bool gameOver = false;

    for(int i=0; i<NUM_PIPES; i++) {
      pipes[i].x = 128 + i * 60;
      pipes[i].gapY = random(10, 40);
      pipes[i].passed = false;
    }
    static unsigned long exitStartTime = 0;
    exitStartTime = 0;
    waitAllButtonsReleased();

    while(!gameOver) {
      if (buttonPressed(BTN_OK)) {
        playerVel = flapStrength;
        while(buttonPressed(BTN_OK)) delay(10);
      }

      playerVel += gravity;
      playerY += playerVel;

      if (playerY > 60) {
        playerY = 60;
        gameOver = true;
      }
      if (playerY < 0) {
        playerY = 0;
        playerVel = 0;
      }
      
      display.clearDisplay();

      for(int i=0; i<NUM_PIPES; i++) {
        pipes[i].x -= pipeSpeed;

        if (pipes[i].x < -PIPE_WIDTH) {
          pipes[i].x = 128 + PIPE_WIDTH;
          pipes[i].gapY = random(10, 40);
          pipes[i].passed = false;
        }

        display.fillRect(pipes[i].x, 0, PIPE_WIDTH, pipes[i].gapY, SH110X_WHITE);
        display.fillRect(pipes[i].x, pipes[i].gapY + PIPE_GAP, PIPE_WIDTH, 64 - (pipes[i].gapY + PIPE_GAP), SH110X_WHITE);

        // Check collision
        if (20 + 4 > pipes[i].x && 20 < pipes[i].x + PIPE_WIDTH) {
          if (playerY < pipes[i].gapY || playerY + 4 > pipes[i].gapY + PIPE_GAP) {
            gameOver = true;
          }
        }

        if (!pipes[i].passed && pipes[i].x < 20) {
          pipes[i].passed = true;
          score++;
        }
      }

      display.fillRect(20, (int)playerY, 4, 4, SH110X_WHITE);
      display.setTextSize(1);
      display.setCursor(2, 2);
      display.print(score);

      if (checkExitHold(exitStartTime)) return;

      display.display();
      delay(16);
    }

    int choice = gameOverScreen(score);
    if (choice == 1) {
      continue;
    } else {
      return;
    }
  }
}

// DINO RUN - chrome dinosaur game style
struct Cactus {
  int x;
  int w;
};
#define NUM_CACTI 3
#define GROUND_Y 60

void dinoRunGame() {
  Cactus cacti[NUM_CACTI];
  float playerY = GROUND_Y - 8;
  float playerVel = 0;
  float gravity = 0.4;
  float jumpStrength = -4.5;
  bool isOnGround = true;
  int gameSpeed = 3;
  unsigned long score = 0;
  unsigned long startTime = millis();

  while(true) {
    playerY = GROUND_Y - 8;
    playerVel = 0;
    isOnGround = true;
    gameSpeed = 3;
    startTime = millis();
    score = 0;

    for(int i=0; i<NUM_CACTI; i++) {
      cacti[i].x = 128 + i * 70;
      cacti[i].w = random(4, 10);
    }
    static unsigned long exitStartTime = 0;
    exitStartTime = 0;
    bool gameOver = false;
    waitAllButtonsReleased();

    while(!gameOver) {
      if (isOnGround && buttonPressed(BTN_OK)) {
        playerVel = jumpStrength;
        isOnGround = false;
        while(buttonPressed(BTN_OK)) delay(10);
      }

      if (!isOnGround) {
        playerVel += gravity;
        playerY += playerVel;
      }

      if (playerY >= GROUND_Y - 8) {
        playerY = GROUND_Y - 8;
        playerVel = 0;
        isOnGround = true;
      }

      score = (millis() - startTime) / 20;
      if (score > 0 && score % 100 == 0) {
        gameSpeed++;
        if (gameSpeed > 6) gameSpeed = 6;
      }

      display.clearDisplay();
      display.drawLine(0, GROUND_Y, 128, GROUND_Y, SH110X_WHITE);

      for(int i=0; i<NUM_CACTI; i++) {
        cacti[i].x -= gameSpeed;

        if (cacti[i].x < -cacti[i].w) {
          cacti[i].x = 128 + random(20, 50);
          cacti[i].w = random(4, 10);
        }

        display.fillRect(cacti[i].x, GROUND_Y - 12, cacti[i].w, 12, SH110X_WHITE);

        if (20 < cacti[i].x + cacti[i].w &&
            20 + 8 > cacti[i].x &&
            playerY < (GROUND_Y - 12) + 12 &&
            playerY + 8 > (GROUND_Y - 12)) {
          gameOver = true;
        }
      }

      display.fillRect(20, (int)playerY, 8, 8, SH110X_WHITE);
      display.setTextSize(1);
      display.setCursor(2, 2);
      display.print(score);

      if (checkExitHold(exitStartTime)) return;

      display.display();
      delay(16);
    }

    int choice = gameOverScreen(score);
    if (choice == 1) {
      continue;
    } else {
      return;
    }
  }
}

// USB CONTROLLER MODE
void controllerMode() {
  hid_gamepad_report_t gamepad;
  memset(&gamepad, 0, sizeof(gamepad));
  unsigned long exitStartTime = 0;
  unsigned long lastReportTime = 0;

  hid.sendReport(0, &gamepad, sizeof(gamepad));
  waitAllButtonsReleased();

  while (true) {
    bool up = buttonPressed(BTN_UP);
    bool down = buttonPressed(BTN_DOWN);
    bool left = buttonPressed(BTN_LEFT);
    bool right = buttonPressed(BTN_RIGHT);
    bool btnA = buttonPressed(BTN_OK);
    bool btnB = buttonPressed(BTN_BACK);

    // Exit check
    if (btnA && btnB) {
      if (exitStartTime == 0) {
        exitStartTime = millis();
      }
      if (millis() - exitStartTime > EXIT_HOLD_TIME) {
        memset(&gamepad, 0, sizeof(gamepad));
        gamepad.hat = GAMEPAD_HAT_CENTERED;
        hid.sendReport(0, &gamepad, sizeof(gamepad));
        waitAllButtonsReleased();
        return;
      }
    } else {
      exitStartTime = 0;
    }

    // D-pad mapping (HAT switch)
    if (up && right) gamepad.hat = GAMEPAD_HAT_UP_RIGHT;
    else if (down && right) gamepad.hat = GAMEPAD_HAT_DOWN_RIGHT;
    else if (down && left) gamepad.hat = GAMEPAD_HAT_DOWN_LEFT;
    else if (up && left) gamepad.hat = GAMEPAD_HAT_UP_LEFT;
    else if (up) gamepad.hat = GAMEPAD_HAT_UP;
    else if (right) gamepad.hat = GAMEPAD_HAT_RIGHT;
    else if (down) gamepad.hat = GAMEPAD_HAT_DOWN;
    else if (left) gamepad.hat = GAMEPAD_HAT_LEFT;
    else gamepad.hat = GAMEPAD_HAT_CENTERED;

    // Button mapping
    gamepad.buttons = 0;
    if (btnA) gamepad.buttons |= 0x01;
    if (btnB) gamepad.buttons |= 0x02;

    // Zero analog axes
    gamepad.x = 0;
    gamepad.y = 0;
    gamepad.z = 0;
    gamepad.rx = 0;
    gamepad.ry = 0;
    gamepad.rz = 0;

    // Send report at ~100Hz
    if (millis() - lastReportTime >= 10) {
      hid.sendReport(0, &gamepad, sizeof(gamepad));
      lastReportTime = millis();
    }

    // Display update
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 10);
    display.print("USB Controller Mode");
    display.drawLine(0, 20, 128, 20, SH110X_WHITE);
    display.setCursor(10, 25);
    display.print("DPad: ");
    
    if (gamepad.hat == GAMEPAD_HAT_CENTERED) display.print("Center");
    else if (gamepad.hat == GAMEPAD_HAT_UP) display.print("Up");
    else if (gamepad.hat == GAMEPAD_HAT_UP_RIGHT) display.print("Up-Right");
    else if (gamepad.hat == GAMEPAD_HAT_RIGHT) display.print("Right");
    else if (gamepad.hat == GAMEPAD_HAT_DOWN_RIGHT) display.print("Down-Right");
    else if (gamepad.hat == GAMEPAD_HAT_DOWN) display.print("Down");
    else if (gamepad.hat == GAMEPAD_HAT_DOWN_LEFT) display.print("Down-Left");
    else if (gamepad.hat == GAMEPAD_HAT_LEFT) display.print("Left");
    else if (gamepad.hat == GAMEPAD_HAT_UP_LEFT) display.print("Up-Left");
    
    display.setCursor(10, 35);
    display.print("A: "); display.print(btnA ? "ON " : "OFF");
    display.print(" B: "); display.print(btnB ? "ON" : "OFF");
    
    display.setCursor(10, 50);
    display.print("Hold A+B to Exit");

    if (exitStartTime > 0) {
      display.fillRect(10, 58, 108, 6, SH110X_BLACK);
      display.drawRect(10, 58, 108, 6, SH110X_WHITE);
      int progress = map(millis() - exitStartTime, 0, EXIT_HOLD_TIME, 0, 104);
      display.fillRect(12, 60, progress, 2, SH110X_WHITE);
    }
    
    display.display();
    delay(5);
  }
}

// SETTINGS MENU
void settingsMenu() {
  while (true) {
    if (buttonPressed(BTN_BACK)) {
      waitAllButtonsReleased();
      return;
    }

    if (buttonPressed(BTN_LEFT) || buttonPressed(BTN_RIGHT)) {
      screenInverted = !screenInverted;
      display.invertDisplay(screenInverted);
      waitAllButtonsReleased();
    }

    display.clearDisplay();
    display.setCursor(30, 10);
    display.print("SETTINGS");
    display.drawLine(0, 20, 128, 20, SH110X_WHITE);
    display.setCursor(10, 35);
    display.print("Invert Screen:");
    display.setCursor(100, 35);
    display.print(screenInverted ? "ON" : "OFF");
    display.setCursor(10, 55);
    display.print("[L/R] Toggle [BACK] Exit");
    display.display();
    delay(50);
  }
}

// ABOUT SCREEN
void aboutScreen() {
  while (true) {
    if (buttonPressed(BTN_BACK)) {
      waitAllButtonsReleased();
      return;
    }

    display.clearDisplay();
    display.setCursor(40, 5);
    display.print("ABOUT");
    display.drawLine(20, 15, 108, 15, SH110X_WHITE);
    display.setCursor(10, 25);
    display.print("Pico Play");
    display.setCursor(10, 35);
    display.print("v1");
    display.setCursor(10, 45);
    display.print("Chip: RP2040");
    display.setCursor(25, 55);
    display.print("[BACK] to Exit");
    display.display();
    delay(50);
  }
}

void setup() {
  Serial.begin(115200);

  // Setup button pins with pullups
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // Setup USB HID
  hid.setPollInterval(2);
  hid.setReportDescriptor(desc_hid_gamepad, sizeof(desc_hid_gamepad));
  hid.begin();

  // Init display
  display.begin(0x3C, true);
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.display();

  // Wait for USB enumeration
  while(!TinyUSBDevice.mounted()) delay(1);

  randomSeed(analogRead(27));
}

void loop() {
  // Handle UP navigation
  if (buttonPressed(BTN_UP)) {
    selected--;
    if (selected < 0) {
      selected = menuCount - 1;
      menuScrollOffset = menuCount - maxVisibleItems;
      if (menuScrollOffset < 0) menuScrollOffset = 0;
    }
    else if (selected < menuScrollOffset) {
      menuScrollOffset = selected;
    }
    drawMenu();
    delay(150);
  }

  // Handle DOWN navigation
  if (buttonPressed(BTN_DOWN)) {
    selected++;
    if (selected >= menuCount) {
      selected = 0;
      menuScrollOffset = 0;
    }
    else if (selected >= menuScrollOffset + maxVisibleItems) {
      menuScrollOffset++;
    }
    drawMenu();
    delay(150);
  }

  // Handle selection
  if (buttonPressed(BTN_OK)) {
    delay(200);
    switch (selected) {
      case 0: pongGame(); break;
      case 1: snakeGame(); break;
      case 2: spaceRaceGame(); break;
      case 3: spaceShooterGame(); break;
      case 4: flappyBirdGame(); break;
      case 5: dinoRunGame(); break;
      case 6: controllerMode(); break;
      case 7: settingsMenu(); break;
      case 8: aboutScreen(); break;
    }
    drawMenu();
    waitAllButtonsReleased();
  }

  // Draw menu on first boot
  static bool firstRun = true;
  if (firstRun) {
    drawMenu();
    firstRun = false;
  }
}