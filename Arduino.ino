#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

// Include MMBasic headers
#include "HAL.h"
#include "MMBasic.h"

// Function prototypes
void ProcessCommand(char *line);
void HandleDirectCommand(char *line);

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    Serial.println("MMBasic for M5Cardputer starting...");
    
    // Initialize hardware
    HAL_System_Init();
    
    // Initialize MMBasic
    MMBasic_Init();
    
    Serial.println("MMBasic initialized.");
}

void loop() {
    char inputLine[256];
    
    // Display prompt
    HAL_Display_Print("> ");
    
    // Show cursor
    HAL_Cursor_Show();
    
    // Get input line
    HAL_Keyboard_GetLine(inputLine, sizeof(inputLine));
    
    // Hide cursor
    HAL_Cursor_Hide();
    
    // Process the input
    if (strlen(inputLine) > 0) {
        ProcessCommand(inputLine);
    }
}

void ProcessCommand(char *line) {
    // Skip whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    // Check for empty line
    if (*line == '\0') return;
    
    // Execute via MMBasic_Execute (handles line numbers, commands, etc.)
    HandleDirectCommand(line);
}

void HandleDirectCommand(char *line) {
    // Set up error recovery
    if (setjmp(mark) == 0) {
        // Execute the command
        MMBasic_Execute(line);
    } else {
        // Error occurred, display prompt
        HAL_Display_Println("Ready.");
    }
}
