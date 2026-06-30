/*
 * HAL.h - Hardware Abstraction Layer for M5Cardputer
 * 
 * This file provides hardware abstraction for MMBasic on M5Cardputer
 */

#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

// Display configuration
#define LCD_WIDTH       240
#define LCD_HEIGHT      135
#define CHAR_WIDTH      6
#define CHAR_HEIGHT     8
#define TEXT_SIZE        1
#define MAX_COLUMNS     (LCD_WIDTH / CHAR_WIDTH)
#define MAX_ROWS        (LCD_HEIGHT / CHAR_HEIGHT)

// SD Card pins (Cardputer-Adv: CS=12, MOSI=14, SCK=40, MISO=39)
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

// Keyboard buffer size
#define KEYBOARD_BUFFER_SIZE 256

// Special key codes for FN combinations
#define KEY_UP_ARROW    0x80
#define KEY_DOWN_ARROW  0x81
#define KEY_LEFT_ARROW  0x82
#define KEY_RIGHT_ARROW 0x83
#define KEY_DELETE      0x7F
#define KEY_ESCAPE      0x1B

// Function prototypes - Display
void HAL_Display_Init(void);
void HAL_Display_Clear(void);
void HAL_Display_Print(const char* text);
void HAL_Display_Println(const char* text);
void HAL_Display_Newline(void);
void HAL_Display_SetCursor(int x, int y);
void HAL_Display_SetTextColor(uint16_t color);
void HAL_Display_SetBgColor(uint16_t color);
uint16_t HAL_Display_GetTextColor(void);
void HAL_Display_SetTextSize(float size);
void HAL_Display_DrawPixel(int x, int y, uint16_t color);
void HAL_Display_DrawLine(int x1, int y1, int x2, int y2, uint16_t color);
void HAL_Display_DrawRect(int x, int y, int w, int h, uint16_t color);
void HAL_Display_FillRect(int x, int y, int w, int h, uint16_t color);
void HAL_Display_DrawCircle(int x, int y, int r, uint16_t color);
void HAL_Display_FillCircle(int x, int y, int r, uint16_t color);
int HAL_Display_GetWidth(void);
int HAL_Display_GetHeight(void);
void HAL_Display_Scroll(int dx, int dy);

// Function prototypes - Cursor
void HAL_Cursor_Show(void);
void HAL_Cursor_Hide(void);
void HAL_Cursor_Draw(void);
void HAL_Cursor_Erase(void);
void HAL_Cursor_Update(void);
void HAL_Cursor_MoveTo(int x, int y);

// Function prototypes - Keyboard
void HAL_Keyboard_Init(void);
bool HAL_Keyboard_Available(void);
char HAL_Keyboard_Read(void);
void HAL_Keyboard_GetLine(char* buffer, int maxLength);

// Function prototypes - SD Card
bool HAL_SD_Init(void);
bool HAL_SD_IsReady(void);
File HAL_SD_Open(const char* path, const char* mode);
void HAL_SD_Close(File file);
bool HAL_SD_Exists(const char* path);
bool HAL_SD_Remove(const char* path);
bool HAL_SD_Mkdir(const char* path);
bool HAL_SD_Rmdir(const char* path);

// Function prototypes - System
void HAL_System_Init(void);
void HAL_System_Update(void);
unsigned long HAL_Millis(void);
void HAL_Delay(unsigned long ms);

#endif // HAL_H
