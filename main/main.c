#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dirent.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "fontx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "kb.h"
#include "st7789.h"

// Game configuration
#define PADDLE_WIDTH 10
#define PADDLE_HEIGHT 60
#define PADDLE_MARGIN 10
#define BALL_SIZE 12
#define INITIAL_BALL_SPEED_X 2
#define INITIAL_BALL_SPEED_Y 5
#define PADDLE_SPEED 6
#define SCORE_TO_WIN 5
#define MAX_SPEED 6
#define SPEED_INCREASE 0.2f

typedef enum {
  GAME_STATE_PLAYING,
  GAME_STATE_GAME_OVER,
  GAME_STATE_PAUSED
} GameState;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  int16_t speed;
} Paddle;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t size;
  float speed_x;
  float speed_y;
} Ball;

typedef struct {
  Paddle paddle;
  uint8_t score;
} Player;

typedef struct {
  FontxFile default_font[2];
  FontxFile large_font[2];
  FontxFile small_font[2];

  uint16_t paddle_color;
  uint16_t ball_color;
  uint16_t background_color;
} GameResources;

typedef struct {
  uint16_t width;
  uint16_t height;
} GameFieldSize;

typedef struct {
  Player player1;
  Player player2;
  Ball ball;
  float ball_speed_multiplier;
  GameState state;
  GameResources resources;
  GameFieldSize field_size;
} PongGame;

const static char *TAG = "main";

static PongGame game;

// ====== START of buttons bindings ======
void btn_left_handler(void *handler_args, esp_event_base_t base, int32_t id,
                      void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    game.player1.paddle.speed = PADDLE_SPEED;
  } else {
    game.player1.paddle.speed = 0;
  }

  ESP_LOGI("LEFT", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_up_handler(void *handler_args, esp_event_base_t base, int32_t id,
                    void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    game.player1.paddle.speed = -PADDLE_SPEED;
  } else {
    game.player1.paddle.speed = 0;
  }

  ESP_LOGI("UP", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_down_handler(void *handler_args, esp_event_base_t base, int32_t id,
                      void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    game.player2.paddle.speed = PADDLE_SPEED;
  } else {
    game.player2.paddle.speed = 0;
  }

  ESP_LOGI("DOWN", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_right_handler(void *handler_args, esp_event_base_t base, int32_t id,
                       void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    game.player2.paddle.speed = -PADDLE_SPEED;
  } else {
    game.player2.paddle.speed = 0;
  }

  ESP_LOGI("RIGHT", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_confirm_handler(void *handler_args, esp_event_base_t base, int32_t id,
                         void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state && game.state == GAME_STATE_GAME_OVER) {
    game.state = GAME_STATE_PLAYING;
    game.player1.score = 0;
    game.player2.score = 0;
  }

  ESP_LOGI("CONFIRM", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_cancel_handler(void *handler_args, esp_event_base_t base, int32_t id,
                        void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state && game.state == GAME_STATE_PLAYING) {
    game.state = GAME_STATE_PAUSED;
  } else if (state_info->state && game.state == GAME_STATE_PAUSED) {
    game.state = GAME_STATE_PLAYING;
  }

  ESP_LOGI("CANCEL", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}
// ====== END of buttons bindings ======

void init_display(TFT_t *dev) {
  // Change SPI Clock Frequency
  spi_clock_speed(40000000);  // 40MHz
  // spi_clock_speed(60000000);  // 60MHz

  spi_master_init(dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO,
                  CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);
  lcdInit(dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);
}

void init_game(TFT_t *dev) {
  // Init field size
  game.field_size.width = dev->_width;
  game.field_size.height = dev->_height;

  // Init fonts
  const char *fonts[] = {
      "/fonts/ILMH16XB.FNT",  // 8x16Dot Mincyo
      "/fonts/ILMH24XB.FNT",  // 12x24Dot Mincyo
      "/fonts/ILMH32XB.FNT"   // 16x32Dot Mincyo
  };
  InitFontx(game.resources.small_font, fonts[0], "");    // 8x16Dot Mincyo
  InitFontx(game.resources.default_font, fonts[1], "");  // 12x24Dot Mincyo
  InitFontx(game.resources.large_font, fonts[2], "");    // 16x32Dot Mincyo

  game.resources.paddle_color = WHITE;
  game.resources.ball_color = GREEN;
  game.resources.background_color = WHITE;

  // Padles
  Paddle *p1 = &game.player1.paddle;
  Paddle *p2 = &game.player2.paddle;

  // Player1 paddle
  p1->x = PADDLE_MARGIN;
  p1->y = game.field_size.height / 2 - PADDLE_HEIGHT / 2;
  p1->width = PADDLE_WIDTH;
  p1->height = PADDLE_HEIGHT;
  p1->speed = 0;

  // Player2 paddle
  p2->x = game.field_size.width - PADDLE_MARGIN - PADDLE_WIDTH;
  p2->y = game.field_size.height / 2 - PADDLE_HEIGHT / 2;
  p2->width = PADDLE_WIDTH;
  p2->height = PADDLE_HEIGHT;
  p2->speed = 0;

  // Ball
  Ball *ball = &game.ball;
  ball->x = game.field_size.width / 2;
  ball->y = game.field_size.height / 2;
  ball->size = BALL_SIZE;
  ball->speed_x = INITIAL_BALL_SPEED_X * (rand() % 2 ? 1 : -1);
  ball->speed_y = INITIAL_BALL_SPEED_Y * (rand() % 2 ? 1 : -1);

  game.ball_speed_multiplier = 1.0f;

  // Scores
  game.player1.score = 0;
  game.player2.score = 0;
}

void reset_ball(int direction) {
  Ball *ball = &game.ball;

  ball->x = game.field_size.width / 2 - BALL_SIZE / 2;
  ball->y = game.field_size.height / 2 - BALL_SIZE / 2;
  ball->speed_x = INITIAL_BALL_SPEED_X * direction;
  ball->speed_y = INITIAL_BALL_SPEED_Y * (rand() % 2 ? 1 : -1);
}

void update_game() {
  if (game.state != GAME_STATE_PLAYING) return;

  const GameFieldSize f_size = game.field_size;
  Paddle *p1 = &game.player1.paddle;
  Paddle *p2 = &game.player2.paddle;
  Ball *ball = &game.ball;

  // Move paddles
  p1->y += p1->speed;
  p2->y += p2->speed;

  // Keep paddles on screen
  if (game.player1.paddle.y < 0) game.player1.paddle.y = 0;
  if (game.player1.paddle.y > f_size.height - p1->height)
    p1->y = f_size.height - p1->height;

  if (game.player2.paddle.y < 0) game.player2.paddle.y = 0;
  if (game.player2.paddle.y > f_size.height - p2->height)
    p2->y = f_size.height - p2->height;

  // Move ball
  ball->x += ball->speed_x * game.ball_speed_multiplier;
  ball->y += ball->speed_y * game.ball_speed_multiplier;

  // Ball collision with top and bottom
  int ball_top = ball->y - ball->size / 2;
  int ball_bottom = ball->y + ball->size / 2;
  int ball_left = ball->x - ball->size / 2;
  int ball_right = ball->x + ball->size / 2;
  if (ball_top <= 0) {
    ball->y = ball->size / 2;
    ball->speed_y *= -1;
  }

  if (ball_bottom >= f_size.height) {
    ball->y = f_size.height - ball->size;
    ball->speed_y *= -1;
  }

  // Ball collision with paddles
  int p1_right = p1->x + p1->width;
  int p1_top = p1->y;
  int p1_bottom = p1->y + p1->height;
  if ((ball_left <= p1_right) && (p1_top <= ball_top) &&
      (ball_bottom <= p1_bottom)) {
    ball->x = p1_right + ball->size / 2;
    ball->speed_x *= -1;
    game.ball_speed_multiplier += SPEED_INCREASE;
  }

  int p2_left = p2->x;
  int p2_top = p2->y;
  int p2_bottom = p2->y + p2->height;
  if ((ball_right >= p2_left) && (p2_top <= ball_top) &&
      (ball_bottom <= p2_bottom)) {
    ball->x = p2_left - ball->size / 2;
    ball->speed_x *= -1;
    game.ball_speed_multiplier += SPEED_INCREASE;
  }

  // Limit maximum speed
  if (game.ball_speed_multiplier > MAX_SPEED) {
    game.ball_speed_multiplier = MAX_SPEED;
  }

  // Ball out of bounds - score points
  if (game.ball.x < 0) {
    game.player2.score++;
    reset_ball(1);  // Reset ball towards AI
  }
  if (game.ball.x > f_size.width) {
    game.player1.score++;
    reset_ball(-1);  // Reset ball towards player
  }

  // Check for winner
  if (game.player1.score >= SCORE_TO_WIN ||
      game.player2.score >= SCORE_TO_WIN) {
    game.state = GAME_STATE_GAME_OVER;
  }
}

void draw_midcourt_line(TFT_t *dev, uint16_t color) {
  for (int y = 0; y < dev->_height; y += 10) {
    lcdDrawFillRect(dev, dev->_width / 2 - 1, y, dev->_width / 2 + 1, y + 5,
                    color);
  }
}

void draw_paddle(TFT_t *dev, Paddle paddle, uint16_t color) {
  lcdDrawFillRect(dev, paddle.x, paddle.y, paddle.x + paddle.width,
                  paddle.y + paddle.height, color);
}

void draw_ball(TFT_t *dev, Ball ball, uint16_t color) {
  lcdDrawFillCircle(dev, ball.x, ball.y, ball.size / 2, color);
}

void draw_scores(TFT_t *dev, uint16_t p1_score, uint16_t p2_score,
                 uint16_t color) {
  char score_str[16];
  snprintf(score_str, sizeof(score_str), "%d - %d", p1_score, p2_score);
  lcdDrawString(dev, game.resources.default_font, dev->_width / 2 - 30, 30,
                (uint8_t *)score_str, color);
}

void draw_playing(TFT_t *dev) {
  // Draw center line
  draw_midcourt_line(dev, GRAY);

  // Draw paddles
  draw_paddle(dev, game.player1.paddle, WHITE);
  draw_paddle(dev, game.player2.paddle, WHITE);

  // Draw ball
  draw_ball(dev, game.ball, GREEN);

  // Draw scores
  draw_scores(dev, game.player1.score, game.player2.score, WHITE);

  // Draw pause state
  if (game.state == GAME_STATE_PAUSED)
    lcdDrawString(dev, game.resources.large_font, dev->_width / 2 - 48,
                  dev->_height / 2, (uint8_t *)"PAUSED", RED);
}

void draw_game_over(TFT_t *dev) {
  uint16_t pl1_score = game.player1.score;
  uint16_t pl2_score = game.player2.score;
  FontxFile *def_font = game.resources.default_font;

  // Draw winner
  const char *winner =
      pl1_score > pl2_score ? "Player1 Wins!" : "Player2 Wins!";
  lcdDrawString(dev, def_font, dev->_width / 2 - 80, dev->_height / 2 - 30,
                (uint8_t *)winner, GREEN);

  // Draw final score
  char score_str[32];
  snprintf(score_str, sizeof(score_str), "Final Score: %d - %d", pl1_score,
           pl2_score);
  lcdDrawString(dev, def_font, dev->_width / 2 - 105, dev->_height / 2,
                (uint8_t *)score_str, WHITE);

  // Draw instructions
  lcdDrawString(dev, def_font, dev->_width / 2 - 90, dev->_height / 2 + 30,
                (uint8_t *)"CONFIRM: Restart", WHITE);
}

void draw_game(TFT_t *dev) {
  lcdFillScreen(dev, BLACK);

  switch (game.state) {
    case GAME_STATE_PLAYING:
    case GAME_STATE_PAUSED:
      draw_playing(dev);
      break;
    case GAME_STATE_GAME_OVER:
      draw_game_over(dev);
      break;
  }

  lcdDrawFinish(dev);
}

void game_task(void *pvParameters) {
#ifdef FRAME_RATE
  int64_t time_start, delta_time;
  float fps;
  char fps_s[16];
#endif

  TFT_t dev;
  init_display(&dev);
  init_game(&dev);

  while (1) {
#ifdef FRAME_RATE
    time_start = esp_timer_get_time();
#endif

    update_game();
    draw_game(&dev);

#ifdef FRAME_RATE
    delta_time = esp_timer_get_time() - time_start;
    fps = 1000000.0 / delta_time;
    snprintf(fps_s, sizeof(fps_s), "FPS: %.2f", fps);
    lcdDrawString(&dev, game.resources.small_font, 10, 220, (uint8_t *)fps_s,
                  BLUE);
    lcdDrawFinish(&dev);
#endif

    // Delay for watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
  }  // end while

  // never reach here
  vTaskDelete(NULL);
}

// Mounting files (fonts, imgs, etc.)
static void listSPIFFS(char *path) {
  DIR *dir = opendir(path);
  assert(dir != NULL);
  while (true) {
    struct dirent *pe = readdir(dir);
    if (!pe) break;
    ESP_LOGI(__FUNCTION__, "d_name=%s d_ino=%d d_type=%x", pe->d_name,
             pe->d_ino, pe->d_type);
  }
  closedir(dir);
}

esp_err_t mountSPIFFS(char *path, char *label, int max_files) {
  esp_vfs_spiffs_conf_t conf = {.base_path = path,
                                .partition_label = label,
                                .max_files = max_files,
                                .format_if_mount_failed = true};

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return ret;
  }

#if 0
	ESP_LOGI(TAG, "Performing SPIFFS_check().");
	ret = esp_spiffs_check(conf.partition_label);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
		return ret;
	} else {
			ESP_LOGI(TAG, "SPIFFS_check() successful");
	}
#endif

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Mount %s to %s success", path, label);
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

  return ret;
}

void app_main(void) {
  ESP_LOGI(TAG, "Hello, User!!:)");

  ESP_LOGI(TAG, "Initializing SPIFFS");
  ESP_ERROR_CHECK(mountSPIFFS("/fonts", "storage1", 3));
  listSPIFFS("/fonts/");

  esp_event_loop_create_default();

  xTaskCreate(keyboard_task, "keyboard_task", 1024 * 3, NULL, 2, NULL);
  xTaskCreate(game_task, "game_task", 1024 * 6, NULL, 3, NULL);
}