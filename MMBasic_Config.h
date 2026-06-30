/*
 * MMBasic_Config.h - MMBasic Configuration for M5Cardputer
 * 
 * This file defines the configuration parameters for MMBasic on M5Cardputer
 */

#ifndef MMBASIC_CONFIG_H
#define MMBASIC_CONFIG_H

// Version information
#define VERSION "1.0.0"
#define YEAR "2024"
#define MES_SIGNON "MMBasic for M5Cardputer v" VERSION "\r\n"

// Memory configuration (optimized for ESP32-S3 with 512KB SRAM)
#define HEAP_MEMORY_SIZE (128 * 1024)  // 128KB for BASIC programs
#define MAXVARS 256                     // Maximum number of variables
#define MAXSUBFUN 128                   // Maximum subroutines/functions
#define MAXFORLOOPS 16                  // Maximum nested FOR loops
#define MAXGOSUB 16                     // Maximum GOSUB nesting
#define MAX_MULTILINE_IF 16            // Maximum multiline IF nesting

// String configuration
#define STRINGSIZE 256                  // Maximum string size
#define MAXVARLEN 32                    // Maximum variable name length

// Display configuration
#define VCHARS 16                       // Number of visible character lines
#define HCHARS 40                       // Number of visible character columns

// File system configuration
#define MAXOPENFILES 4                  // Maximum open files
#define FILENAME_LENGTH 64              // Maximum filename length

// Keyboard configuration
#define MAXKEYLEN 64                    // Maximum keyboard input length

// Enable/disable features
#define ENABLE_FILE_IO                  // Enable file I/O commands
#define ENABLE_GRAPHICS                 // Enable graphics commands
#define ENABLE_STRING_FUNCTIONS         // Enable string functions
#define ENABLE_MATH_FUNCTIONS           // Enable math functions
#define DISABLE_I2C                     // Disable I2C for now
#define DISABLE_SPI                     // Disable SPI commands for now
#define DISABLE_SERIAL                  // Disable serial commands for now
#define DISABLE_PWM                     // Disable PWM for now
#define DISABLE_TOUCH                   // Disable touch for now

// Debug options
// #define DEBUGMODE                    // Enable debug mode

#endif // MMBASIC_CONFIG_H
