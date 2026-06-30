/*
 * HAL.cpp - Hardware Abstraction Layer Implementation for M5Cardputer
 * 
 * This file implements hardware abstraction for MMBasic on M5Cardputer
 */

#include "HAL.h"

// Display state
static int cursorX = 0;
static int cursorY = 0;
static uint16_t textColor = WHITE;
static uint16_t bgColor = BLACK;
static float textSize = 1.0;

// Cursor state
static bool cursorVisible = false;
static bool cursorOn = true;
static unsigned long lastCursorToggle = 0;
static const unsigned long CURSOR_BLINK_INTERVAL = 500;
static char cursorChar = 0; // Character under cursor for blink restore

// SD card variables
static bool sdInitialized = false;

// Keyboard variables
static char keyboardBuffer[KEYBOARD_BUFFER_SIZE];
static int keyboardBufferIndex = 0;
static int keyboardBufferLength = 0;

// Internal: draw/erase the cursor block at current position
static void cursor_draw_block(int x, int y, uint16_t color) {
    M5Cardputer.Display.fillRect(x, y, CHAR_WIDTH, CHAR_HEIGHT, color);
}

static void cursor_erase(void) {
    M5Cardputer.Display.fillRect(cursorX, cursorY, CHAR_WIDTH, CHAR_HEIGHT, bgColor);
    if (cursorChar) {
        M5Cardputer.Display.setCursor(cursorX, cursorY);
        M5Cardputer.Display.setTextColor(textColor, bgColor);
        M5Cardputer.Display.print(cursorChar);
    }
}

static void cursor_draw(void) {
    M5Cardputer.Display.fillRect(cursorX, cursorY, CHAR_WIDTH, CHAR_HEIGHT, textColor);
}

// Internal: update cursorX/Y after printing text.
// Called after each print to track where the display cursor landed.
static void track_after_print(void) {
    int dx = M5Cardputer.Display.getCursorX();
    int dy = M5Cardputer.Display.getCursorY();
    if (dx != cursorX || dy != cursorY) {
        cursorX = dx;
        cursorY = dy;
    }
}

// Internal: move cursor to a new position, handling erase/redraw
static void cursor_reposition(int x, int y) {
    if (cursorVisible && (x != cursorX || y != cursorY)) {
        cursor_erase();
    }
    cursorX = x;
    cursorY = y;
    M5Cardputer.Display.setCursor(x, y);
    if (cursorVisible) {
        cursor_draw();
        cursorOn = true;
        lastCursorToggle = millis();
    }
}

void HAL_Display_Init(void) {
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(textSize);
    M5Cardputer.Display.setTextColor(textColor);
    M5Cardputer.Display.setTextScroll(true);
    M5Cardputer.Display.fillScreen(BLACK);
    cursorX = 0;
    cursorY = 0;
    M5Cardputer.Display.setCursor(0, 0);
}

void HAL_Display_Clear(void) {
    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setTextScroll(true);
    cursorX = 0;
    cursorY = 0;
    M5Cardputer.Display.setCursor(0, 0);
}

void HAL_Display_Print(const char* text) {
    if (cursorVisible) {
        cursor_erase();
    }
    M5Cardputer.Display.setCursor(cursorX, cursorY);
    M5Cardputer.Display.print(text);
    track_after_print();
    if (cursorVisible) {
        cursor_draw();
        cursorOn = true;
        lastCursorToggle = millis();
    }
}

void HAL_Display_Println(const char* text) {
    if (cursorVisible) {
        cursor_erase();
    }
    M5Cardputer.Display.setCursor(cursorX, cursorY);
    M5Cardputer.Display.println(text);
    track_after_print();
    
    if (cursorVisible) {
        cursor_draw();
        cursorOn = true;
        lastCursorToggle = millis();
    }
}

void HAL_Display_Newline(void) {
    M5Cardputer.Display.print("\n");
    track_after_print();
}

void HAL_Display_SetCursor(int x, int y) {
    cursor_reposition(x, y);
}

void HAL_Display_SetTextColor(uint16_t color) {
    textColor = color;
    M5Cardputer.Display.setTextColor(color, bgColor);
}

void HAL_Display_SetBgColor(uint16_t color) {
    bgColor = color;
    M5Cardputer.Display.setTextColor(textColor, bgColor);
}

uint16_t HAL_Display_GetTextColor(void) {
    return textColor;
}

void HAL_Display_SetTextSize(float size) {
    textSize = size;
    M5Cardputer.Display.setTextSize(size);
}

void HAL_Cursor_Show(void) {
    cursorVisible = true;
    M5Cardputer.Display.setCursor(cursorX, cursorY);
    cursor_draw();
    cursorOn = true;
    lastCursorToggle = millis();
}

void HAL_Cursor_Hide(void) {
    if (cursorVisible) {
        cursor_erase();
    }
    cursorVisible = false;
    cursorChar = 0;
}

void HAL_Cursor_Draw(void) {
    if (!cursorVisible) return;
    cursor_draw();
}

void HAL_Cursor_Erase(void) {
    cursor_erase();
}

void HAL_Cursor_Update(void) {
    if (!cursorVisible) return;
    
    unsigned long now = millis();
    if (now - lastCursorToggle >= CURSOR_BLINK_INTERVAL) {
        lastCursorToggle = now;
        cursorOn = !cursorOn;
        if (cursorOn) {
            cursor_draw();
        } else {
            cursor_erase();
        }
    }
}

void HAL_Cursor_MoveTo(int x, int y) {
    cursor_reposition(x, y);
}

void HAL_Display_DrawPixel(int x, int y, uint16_t color) {
    M5Cardputer.Display.drawPixel(x, y, color);
}

void HAL_Display_DrawLine(int x1, int y1, int x2, int y2, uint16_t color) {
    M5Cardputer.Display.drawLine(x1, y1, x2, y2, color);
}

void HAL_Display_DrawRect(int x, int y, int w, int h, uint16_t color) {
    M5Cardputer.Display.drawRect(x, y, w, h, color);
}

void HAL_Display_FillRect(int x, int y, int w, int h, uint16_t color) {
    M5Cardputer.Display.fillRect(x, y, w, h, color);
}

void HAL_Display_DrawCircle(int x, int y, int r, uint16_t color) {
    M5Cardputer.Display.drawCircle(x, y, r, color);
}

void HAL_Display_FillCircle(int x, int y, int r, uint16_t color) {
    M5Cardputer.Display.fillCircle(x, y, r, color);
}

int HAL_Display_GetWidth(void) {
    return LCD_WIDTH;
}

int HAL_Display_GetHeight(void) {
    return LCD_HEIGHT;
}

void HAL_Display_Scroll(int dx, int dy) {
    M5Cardputer.Display.scroll(dx, dy);
}

void HAL_Keyboard_Init(void) {
    keyboardBufferIndex = 0;
    keyboardBufferLength = 0;
}

bool HAL_Keyboard_Available(void) {
    M5Cardputer.update();
    return M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
}

char HAL_Keyboard_Read(void) {
    M5Cardputer.update();
    
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return 0;
    }
    
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    
    // Handle FN key combinations
    if (status.fn) {
        // FN + ` = ESC
        if (M5Cardputer.Keyboard.isKeyPressed('`')) {
            return KEY_ESCAPE;
        }
        // FN + ; = Up arrow
        if (M5Cardputer.Keyboard.isKeyPressed(';')) {
            return KEY_UP_ARROW;
        }
        // FN + . = Down arrow
        if (M5Cardputer.Keyboard.isKeyPressed('.')) {
            return KEY_DOWN_ARROW;
        }
        // FN + , = Left arrow
        if (M5Cardputer.Keyboard.isKeyPressed(',')) {
            return KEY_LEFT_ARROW;
        }
        // FN + / = Right arrow
        if (M5Cardputer.Keyboard.isKeyPressed('/')) {
            return KEY_RIGHT_ARROW;
        }
        // FN + Backspace = Delete
        if (status.del) {
            return KEY_DELETE;
        }
    }
    
    // Handle normal keys
    if (status.word.size() > 0) {
        return status.word[0];
    }
    
    if (status.del) {
        return 8; // Backspace
    }
    
    if (status.enter) {
        return 13; // Enter
    }
    
    return 0;
}

void HAL_Keyboard_GetLine(char* buffer, int maxLength) {
    int index = 0;
    int cursorPos = 0;
    bool lineComplete = false;
    
    // Remember starting Y position for this input line
    int lineY = cursorY;
    
    while (!lineComplete && index < maxLength - 1) {
        M5Cardputer.update();
        
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            
            // Handle FN key combinations
            if (status.fn) {
                if (M5Cardputer.Keyboard.isKeyPressed('`')) {
                    buffer[0] = '\0';
                    lineComplete = true;
                    continue;
                }
                if (M5Cardputer.Keyboard.isKeyPressed(';')) {
                    continue;
                }
                if (M5Cardputer.Keyboard.isKeyPressed('.')) {
                    continue;
                }
                if (M5Cardputer.Keyboard.isKeyPressed(',')) {
                    if (cursorPos > 0) {
                        if (cursorVisible) cursor_erase();
                        if (cursorPos < index) {
                            M5Cardputer.Display.setCursor(cursorPos * CHAR_WIDTH + CHAR_WIDTH * 2, lineY);
                            M5Cardputer.Display.print(buffer[cursorPos]);
                        }
                        cursorPos--;
                        cursorX = cursorPos * CHAR_WIDTH + CHAR_WIDTH * 2;
                        cursorY = lineY;
                        cursorChar = (cursorPos < index) ? buffer[cursorPos] : 0;
                        M5Cardputer.Display.setCursor(cursorX, cursorY);
                        if (cursorVisible) { cursor_draw(); cursorOn=true; lastCursorToggle=millis(); }
                    }
                    continue;
                }
                if (M5Cardputer.Keyboard.isKeyPressed('/')) {
                    if (cursorPos < index) {
                        if (cursorVisible) cursor_erase();
                        M5Cardputer.Display.setCursor(cursorPos * CHAR_WIDTH + CHAR_WIDTH * 2, lineY);
                        M5Cardputer.Display.print(buffer[cursorPos]);
                        cursorPos++;
                        cursorX = cursorPos * CHAR_WIDTH + CHAR_WIDTH * 2;
                        cursorY = lineY;
                        cursorChar = (cursorPos < index) ? buffer[cursorPos] : 0;
                        M5Cardputer.Display.setCursor(cursorX, cursorY);
                        if (cursorVisible) { cursor_draw(); cursorOn=true; lastCursorToggle=millis(); }
                    }
                    continue;
                }
                if (status.del && cursorPos < index) {
                    for (int i = cursorPos; i < index - 1; i++) {
                        buffer[i] = buffer[i + 1];
                    }
                    index--;
                    
                    M5Cardputer.Display.setCursor(CHAR_WIDTH * 2, lineY);
                    M5Cardputer.Display.fillRect(CHAR_WIDTH * 2, lineY, LCD_WIDTH - CHAR_WIDTH * 2, CHAR_HEIGHT, BLACK);
                    for (int i = 0; i < index; i++) {
                        M5Cardputer.Display.print(buffer[i]);
                    }
                    track_after_print();
                    HAL_Cursor_MoveTo(cursorPos * CHAR_WIDTH + CHAR_WIDTH * 2, lineY);
                    cursorChar = (cursorPos < index) ? buffer[cursorPos] : 0;
                    continue;
                }
            }
            
            // Handle normal characters
            for (auto i : status.word) {
                if (index < maxLength - 1) {
                    if (cursorPos < index) {
                        for (int j = index; j > cursorPos; j--) {
                            buffer[j] = buffer[j - 1];
                        }
                    }
                    buffer[cursorPos++] = i;
                    index++;
                    
                    M5Cardputer.Display.setCursor(CHAR_WIDTH * 2, lineY);
                    M5Cardputer.Display.fillRect(CHAR_WIDTH * 2, lineY, LCD_WIDTH - CHAR_WIDTH * 2, CHAR_HEIGHT, BLACK);
                    for (int j = 0; j < index; j++) {
                        M5Cardputer.Display.print(buffer[j]);
                    }
                    track_after_print();
                    HAL_Cursor_MoveTo(cursorPos * CHAR_WIDTH + CHAR_WIDTH * 2, lineY);
                    cursorChar = (cursorPos < index) ? buffer[cursorPos] : 0;
                }
            }
            
            // Handle backspace
            if (status.del && !status.fn && cursorPos > 0) {
                for (int i = cursorPos - 1; i < index - 1; i++) {
                    buffer[i] = buffer[i + 1];
                }
                index--;
                cursorPos--;
                
                M5Cardputer.Display.setCursor(CHAR_WIDTH * 2, lineY);
                M5Cardputer.Display.fillRect(CHAR_WIDTH * 2, lineY, LCD_WIDTH - CHAR_WIDTH * 2, CHAR_HEIGHT, BLACK);
                for (int i = 0; i < index; i++) {
                    M5Cardputer.Display.print(buffer[i]);
                }
                track_after_print();
                HAL_Cursor_MoveTo(cursorPos * CHAR_WIDTH + CHAR_WIDTH * 2, lineY);
                cursorChar = (cursorPos < index) ? buffer[cursorPos] : 0;
            }
            
            if (status.enter) {
                if (cursorVisible) {
                    cursor_erase();
                    cursorVisible = false;
                }
                cursorChar = 0;
                lineComplete = true;
            }
        }
        
        delay(10);
        HAL_Cursor_Update();
    }
    
    buffer[index] = '\0';
    HAL_Display_Newline();
}

// SD Card functions
bool HAL_SD_Init(void) {
    if (sdInitialized) return true;
    
    if (SD.begin(SD_SPI_CS_PIN)) {
        sdInitialized = true;
        return true;
    }
    return false;
}

bool HAL_SD_IsReady(void) {
    return sdInitialized;
}

File HAL_SD_Open(const char* path, const char* mode) {
    return SD.open(path, mode);
}

void HAL_SD_Close(File file) {
    file.close();
}

bool HAL_SD_Exists(const char* path) {
    return SD.exists(path);
}

bool HAL_SD_Remove(const char* path) {
    return SD.remove(path);
}

bool HAL_SD_Mkdir(const char* path) {
    return SD.mkdir(path);
}

bool HAL_SD_Rmdir(const char* path) {
    return SD.rmdir(path);
}

void HAL_System_Init(void) {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    
    HAL_Display_Init();
    HAL_Keyboard_Init();
    
    // Initialize SD card
    HAL_SD_Init();
    
    // Load color scheme from mmbasic.ini, create default if missing
    if (sdInitialized) {
        File ini = SD.open("/mmbasic.ini", FILE_READ);
        if (ini) {
            while (ini.available()) {
                String line = ini.readStringUntil('\n');
                line.trim();
                if (line.indexOf('=') > 0) {
                    String key = line.substring(0, line.indexOf('='));
                    String val = line.substring(line.indexOf('=')+1);
                    key.trim(); val.trim();
                    int v = val.toInt();
                    if (key == "fgcolor") {
                        textColor = v;
                    } else if (key == "bgcolor") {
                        bgColor = v;
                    }
                }
            }
            ini.close();
            M5Cardputer.Display.setTextColor(textColor, bgColor);
        } else {
            // Create default green-on-black INI
            ini = SD.open("/mmbasic.ini", FILE_WRITE);
            if (ini) {
                ini.println("fgcolor=2016");
                ini.println("bgcolor=0");
                ini.close();
            }
        }
    }
}

void HAL_System_Update(void) {
    M5Cardputer.update();
}

unsigned long HAL_Millis(void) {
    return millis();
}

void HAL_Delay(unsigned long ms) {
    delay(ms);
}
