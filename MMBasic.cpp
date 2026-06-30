/*
 * MMBasic.c - MMBasic Core Implementation
 * 
 * This file implements the core MMBasic interpreter for M5Cardputer
 */

#include "MMBasic.h"
#include "HAL.h"
#include <Wire.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

// Forward declarations for internal functions
char *MMBasic_GetTempString(void);
static int EvaluateExpression(char **expr, int *itype, int *ival, float *fval, char **sval);
static int EvaluateSubExpression(char **expr, int *itype, int *ival, float *fval, char **sval);
static int EvaluateTerm(char **expr, int *itype, int *ival, float *fval, char **sval);
static int EvaluateFactor(char **expr, int *itype, int *ival, float *fval, char **sval);
static int EvaluateFunction(char **expr, int funcToken, int *itype, int *ival, float *fval, char **sval);
void ScanSubFunDefs(void);

// Global variables
char *progmem = NULL;           // Program memory
int progsize = 0;               // Program size
char *progptr = NULL;           // Current program pointer
char inpbuf[STRINGSIZE];        // Input buffer
char tknbuf[STRINGSIZE];        // Token buffer
struct s_vartbl *vartbl = NULL; // Variable table
int varcnt = 0;                 // Variable count
struct s_subfun subfun[MAXSUBFUN]; // Subroutine table
int subfunct = 0;               // Subroutine count
jmp_buf mark;                   // Error recovery jump buffer
int MMCharPos = 0;              // Character position
volatile int MMAbort = 0;       // Abort flag
char BreakKey = 3;              // Break key (Ctrl-C)

// Current line being executed
char *currentLine = NULL;
int currentLineIndex = -1;
bool flowControlActive = false; // Set true when a command changes line flow
bool traceOn = false;            // TRACE ON/OFF

// FOR loop stack
struct s_forstack {
    int lineIdx;                // Index in lines[] after the FOR line (loop body start)
    char varname[MAXVARLEN+1];  // Loop variable name
    int varindex;               // Variable index
    int toval;                  // TO value
    int stepval;                // STEP value
};
struct s_forstack forstack[MAXFORLOOPS];
int forstackptr = 0;

// GOSUB stack
int gosubstack[MAXGOSUB];
int gosubstackptr = 0;

// WHILE stack
int whilestack[MAXFORLOOPS];
int whilestackptr = 0;

// DO loop stack
int dostack[MAXFORLOOPS];
int dostackptr = 0;

// SELECT CASE stack
struct s_select {
    int matchValue;
    bool matched;
};
s_select selectstack[8];
int selectptr = 0;

// SUB/FUNCTION registry
struct s_subfun_entry {
    char name[MAXVARLEN+1];
    int startLine;      // Index in lines[] of SUB/FUNCTION definition
    int endLine;        // Index in lines[] of END SUB/END FUNCTION
    int paramCount;
    char paramNames[8][MAXVARLEN+1];
    bool isFunction;
    int returnLine;     // Return address for CALL (saved from GOSUB stack)
};
s_subfun_entry subFunTable[MAXSUBFUN];
int subFunCount = 0;
int subFunCallReturnIdx = -1;  // Return line index from CALL

// Variable shadow stack for parameter scoping
struct s_shadow {
    int varIndex;
    char type;
    int ival;
    float fval;
    char *sval;
    int array;
    int *arr;
    bool wasConst;
};
#define MAX_SHADOW 128
s_shadow shadowStack[MAX_SHADOW];
int shadowPtr = 0;
int shadowBase[16];      // Shadow pointer at SUB entry
int shadowBaseSP = 0;
int subFunRetStack[16];    // Return address per SUB level
#define MAX_LINES 1000
char *lines[MAX_LINES];
int lineNumbers[MAX_LINES];
int linecnt = 0;

// Temporary string pool
char strpool[16][STRINGSIZE];
int strpoolindex = 0;

// File handle table (1-indexed, #1..#MAXOPENFILES)
struct s_filehandle {
    File file;
    bool inUse;
    bool isCom;        // true if COM port
    int comPort;       // COM port number (1-2)
};
s_filehandle fileTable[MAXOPENFILES + 1];

// Current drive (0 = A: flash, 1 = B: SD card)
// Cardputer only has B: (SD card), default to B:
int currentDrive = 1;

// DATA/READ/RESTORE state
int dataLineIdx = 0;
int dataOffset = 0;

// CONST tracking
bool *varIsConst = NULL;

// Token table
const struct s_tokentbl commandtbl[] = {
    {"PRINT", C_CMD, C_PRINT, NULL},
    {"INPUT", C_CMD, C_INPUT, NULL},
    {"IF", C_CMD, C_IF, NULL},
    {"THEN", C_CMD, C_THEN, NULL},
    {"ELSE", C_CMD, C_ELSE, NULL},
    {"ENDIF", C_CMD, C_ENDIF, NULL},
    {"FOR", C_CMD, C_FOR, NULL},
    {"TO", C_CMD, C_TO, NULL},
    {"STEP", C_CMD, C_STEP, NULL},
    {"NEXT", C_CMD, C_NEXT, NULL},
    {"GOTO", C_CMD, C_GOTO, NULL},
    {"GOSUB", C_CMD, C_GOSUB, NULL},
    {"ON", C_CMD, C_ON, NULL},
    {"RETURN", C_CMD, C_RETURN, NULL},
    {"END", C_CMD, C_END, NULL},
    {"LET", C_CMD, C_LET, NULL},
    {"DIM", C_CMD, C_DIM, NULL},
    {"REM", C_CMD, C_REM, NULL},
    {"'", C_CMD, C_REM2, NULL},
    {"CLS", C_CMD, C_CLS, NULL},
    {"LOCATE", C_CMD, C_LOCATE, NULL},
    {"COLOR", C_CMD, C_COLOR, NULL},
    {"LINE", C_CMD, C_LINE, NULL},
    {"CIRCLE", C_CMD, C_CIRCLE, NULL},
    {"RECT", C_CMD, C_RECT, NULL},
    {"PIXEL", C_CMD, C_PIXEL, NULL},
    {"DATA", C_CMD, C_DATA, NULL},
    {"READ", C_CMD, C_READ, NULL},
    {"RESTORE", C_CMD, C_RESTORE, NULL},
    {"SAVE", C_CMD, C_SAVE, NULL},
    {"LOAD", C_CMD, C_LOAD, NULL},
    {"FILES", C_CMD, C_FILES, NULL},
    {"RUN", C_CMD, C_RUN, NULL},
    {"LIST", C_CMD, C_LIST, NULL},
    {"NEW", C_CMD, C_NEW, NULL},
    {"OPEN", C_CMD, C_OPEN, NULL},
    {"CLOSE", C_CMD, C_CLOSE, NULL},
    {"SEEK", C_CMD, C_SEEK, NULL},
    {"SWAP", C_CMD, C_SWAP, NULL},
    {"INC", C_CMD, C_INC, NULL},
    {"PAUSE", C_CMD, C_PAUSE, NULL},
    {"RANDOMIZE", C_CMD, C_RANDOMIZE, NULL},
    {"POKE", C_CMD, C_POKE, NULL},
    {"CONST", C_CMD, C_CONST, NULL},
    {"WHILE", C_CMD, C_WHILE, NULL},
    {"WEND", C_CMD, C_WEND, NULL},
    {"SELECT", C_CMD, C_SELECT, NULL},
    {"CASE", C_CMD, C_CASE, NULL},
    {"ENDSELECT", C_CMD, C_ENDSELECT, NULL},
    {"DO", C_CMD, C_DO, NULL},
    {"LOOP", C_CMD, C_LOOP, NULL},
    {"CONTINUE", C_CMD, C_CONTINUE, NULL},
    {"EXIT", C_CMD, C_EXIT, NULL},
    {"OPTION", C_CMD, C_OPTION, NULL},
    {"KILL", C_CMD, C_KILL, NULL},
    {"MKDIR", C_CMD, C_MKDIR, NULL},
    {"RMDIR", C_CMD, C_RMDIR, NULL},
    {"CHDIR", C_CMD, C_CHDIR, NULL},
    {"COPY", C_CMD, C_COPY, NULL},
    {"RENAME", C_CMD, C_RENAME, NULL},
    {"DRIVE", C_CMD, C_DRIVE, NULL},
    {"SETPIN", C_CMD, C_SETPIN, NULL},
    {"DIGOUT", C_CMD, C_DIGOUT, NULL},
    {"PWM", C_CMD, C_PWMCMD, NULL},
    {"I2C", C_CMD, C_I2C, NULL},
    {"SPI", C_CMD, C_SPI, NULL},
    {"IR", C_CMD, C_IR, NULL},
    {"SERVO", C_CMD, C_SERVO, NULL},
    {"PORT", C_CMD, C_PORTCMD, NULL},
    {"PULSE", C_CMD, C_PULSE, NULL},
    {"BOX", C_CMD, C_BOX, NULL},
    {"TEXT", C_CMD, C_TEXT, NULL},
    {"TRIANGLE", C_CMD, C_TRIANGLE, NULL},
    {"CLEAR", C_CMD, C_CLEAR, NULL},
    {"ERASE", C_CMD, C_ERASE, NULL},
    {"REDIM", C_CMD, C_REDIM, NULL},
    {"ERROR", C_CMD, C_ERROR, NULL},
    {"TRACE", C_CMD, C_TRACE, NULL},
    {"SORT", C_CMD, C_SORTCMD, NULL},
    {"CALL", C_CMD, C_CALL, NULL},
    {"TURTLE", C_CMD, C_TURTLE, NULL},
    {"SPRITE", C_CMD, C_SPRITE, NULL},
    {"VAR", C_CMD, C_VAR, NULL},
    {"SUB", C_CMD, C_SUB, NULL},
    {"ENDSUB", C_CMD, C_ENDSUB, NULL},
    {"FUNCTION", C_CMD, C_FUNCTION, NULL},
    {"ENDFUNCTION", C_CMD, C_ENDFUN, NULL},
    {"ABS", C_FUNC, F_ABS, NULL},
    {"INT", C_FUNC, F_INT, NULL},
    {"SGN", C_FUNC, F_SGN, NULL},
    {"SQR", C_FUNC, F_SQR, NULL},
    {"SIN", C_FUNC, F_SIN, NULL},
    {"COS", C_FUNC, F_COS, NULL},
    {"TAN", C_FUNC, F_TAN, NULL},
    {"ATN", C_FUNC, F_ATN, NULL},
    {"LOG", C_FUNC, F_LOG, NULL},
    {"EXP", C_FUNC, F_EXP, NULL},
    {"RND", C_FUNC, F_RND, NULL},
    {"LEN", C_FUNC, F_LEN, NULL},
    {"VAL", C_FUNC, F_VAL, NULL},
    {"ASC", C_FUNC, F_ASC, NULL},
    {"CHR$", C_FUNC, F_CHR, NULL},
    {"STR$", C_FUNC, F_STR, NULL},
    {"LEFT$", C_FUNC, F_LEFT, NULL},
    {"RIGHT$", C_FUNC, F_RIGHT, NULL},
    {"MID$", C_FUNC, F_MID, NULL},
    {"INSTR", C_FUNC, F_INSTR, NULL},
    {"STRING$", C_FUNC, F_STRING, NULL},
    {"SPACE$", C_FUNC, F_SPACE, NULL},
    {"UPPER$", C_FUNC, F_UPPER, NULL},
    {"LOWER$", C_FUNC, F_LOWER, NULL},
    {"TRIM$", C_FUNC, F_TRIM, NULL},
    {"HEX$", C_FUNC, F_HEX, NULL},
    {"OCT$", C_FUNC, F_OCT, NULL},
    {"BIN$", C_FUNC, F_BIN, NULL},
    {"NOT", C_FUNC, F_NOT, NULL},
    {"EOF", C_FUNC, F_EOF, NULL},
    {"LOF", C_FUNC, F_LOF, NULL},
    {"LOC", C_FUNC, F_LOC, NULL},
    {"PI", C_FUNC, F_PI, NULL},
    {"PEEK", C_FUNC, F_PEEK, NULL},
    {"MAX", C_FUNC, F_MAX, NULL},
    {"MIN", C_FUNC, F_MIN, NULL},
    {"DEG", C_FUNC, F_DEG, NULL},
    {"RAD", C_FUNC, F_RAD, NULL},
    {"ACOS", C_FUNC, F_ACOS, NULL},
    {"ASIN", C_FUNC, F_ASIN, NULL},
    {"ATAN2", C_FUNC, F_ATAN2, NULL},
    {"DATE$", C_FUNC, F_DATE, NULL},
    {"TIME$", C_FUNC, F_TIME, NULL},
    {"TAB", C_FUNC, F_TAB, NULL},
    {"SPC", C_FUNC, F_SPC, NULL},
    {"FORMAT$", C_FUNC, F_FORMAT, NULL},
    {"SCHANGE$", C_FUNC, F_SCHANGE, NULL},
    {"PIN", C_FUNC, F_GPIO_PIN, NULL},
    {"ADC", C_FUNC, F_ADC, NULL},
    {"PULSIN", C_FUNC, F_PULSIN, NULL},
    {"TOUCH", C_FUNC, F_TOUCH, NULL},
    {"RGB", C_FUNC, F_RGB, NULL},
    {"PIXEL", C_FUNC, F_POINT, NULL},
    {"EVAL$", C_FUNC, F_EVAL, NULL},
    {"BASE$", C_FUNC, F_BASE, NULL},
    {"", 0, 0, NULL}
};

// Initialize MMBasic
void MMBasic_Init(void) {
    // Allocate program memory
    progmem = (char *)malloc(HEAP_MEMORY_SIZE);
    if (progmem == NULL) {
        HAL_Display_Println("Error: Cannot allocate memory");
        while(1);
    }
    
    // Allocate variable table
    vartbl = (struct s_vartbl *)malloc(MAXVARS * sizeof(struct s_vartbl));
    if (vartbl == NULL) {
        HAL_Display_Println("Error: Cannot allocate variable table");
        while(1);
    }
    
    // Allocate const flags
    varIsConst = (bool *)malloc(MAXVARS * sizeof(bool));
    if (varIsConst == NULL) {
        HAL_Display_Println("Error: Cannot allocate const flags");
        while(1);
    }
    
    // Reset MMBasic
    MMBasic_Reset();
    
    // Display sign-on message
    HAL_Display_Print(MES_SIGNON);
    HAL_Display_Println("Ready.");
}

// Reset MMBasic
void MMBasic_Reset(void) {
    progsize = 0;
    progptr = progmem;
    linecnt = 0;
    varcnt = 0;
    subfunct = 0;
    forstackptr = 0;
    gosubstackptr = 0;
    whilestackptr = 0;
    dostackptr = 0;
    strpoolindex = 0;
    MMAbort = 0;
    
    // Close any open files
    for (int i = 1; i <= MAXOPENFILES; i++) {
        if (fileTable[i].inUse) {
            fileTable[i].file.close();
            fileTable[i].inUse = false;
        }
    }
    
    // Clear program memory
    memset(progmem, 0, HEAP_MEMORY_SIZE);
    
    // Clear variable table
    memset(vartbl, 0, MAXVARS * sizeof(struct s_vartbl));
    
    // Clear subroutine table
    memset(subfun, 0, MAXSUBFUN * sizeof(struct s_subfun));
    
    // Clear const flags
    memset(varIsConst, 0, MAXVARS * sizeof(bool));
    
    // Reset DATA pointer
    dataLineIdx = 0;
    dataOffset = 0;
    
    // Clear line pointers
    memset(lines, 0, sizeof(lines));
    memset(lineNumbers, 0, sizeof(lineNumbers));
}

// Get next token from line
char *MMBasic_GetToken(char *line, char *token) {
    int i = 0;
    
    // Skip whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    // Check for end of line
    if (*line == '\0' || *line == '\n' || *line == '\r') {
        token[0] = '\0';
        return line;
    }
    
    // Check for string literal
    if (*line == '"') {
        line++;
        while (*line != '"' && *line != '\0' && i < STRINGSIZE - 1) {
            token[i++] = *line++;
        }
        if (*line == '"') line++;
        token[i] = '\0';
        return line;
    }
    
    // Check for number
    if ((*line >= '0' && *line <= '9') || *line == '.') {
        while ((*line >= '0' && *line <= '9') || *line == '.' || *line == 'E' || *line == 'e') {
            if (i < STRINGSIZE - 1) token[i++] = *line++;
        }
        token[i] = '\0';
        return line;
    }
    
    // Check for operator
    if (strchr("+-*/=<>^&|~(),;#\\", *line)) {
        token[0] = *line++;
        token[1] = '\0';
        
        // Check for two-character operators
        if (*line != '\0') {
            if ((token[0] == '<' && *line == '=') || 
                (token[0] == '>' && *line == '=') ||
                (token[0] == '<' && *line == '>') ||
                (token[0] == '<' && *line == '<') ||
                (token[0] == '>' && *line == '>')) {
                token[1] = *line++;
                token[2] = '\0';
            }
        }
        
        return line;
    }
    
    // Check for variable or keyword (including $ for string variables)
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < STRINGSIZE - 1) token[i++] = *line++;
    }
    token[i] = '\0';
    
    // Convert to uppercase
    for (int j = 0; token[j]; j++) {
        if (token[j] >= 'a' && token[j] <= 'z') {
            token[j] = token[j] - 'a' + 'A';
        }
    }
    
    return line;
}

// Find variable by name, returns index or -1 if not found
int MMBasic_FindVariable(char *name) {
    for (int i = 0; i < varcnt; i++) {
        if (strcmp(vartbl[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Create a new variable, returns index
int MMBasic_CreateVariable(char *name, char type) {
    if (varcnt >= MAXVARS) {
        MMBasic_Error(ERR_OUT_MEMORY, "Too many variables");
        return -1;
    }
    
    // Check if variable already exists
    int idx = MMBasic_FindVariable(name);
    if (idx >= 0) {
        // Variable exists, update type if needed
        return idx;
    }
    
    // Create new variable
    idx = varcnt++;
    memset(&vartbl[idx], 0, sizeof(struct s_vartbl));
    strncpy(vartbl[idx].name, name, MAXVARLEN);
    vartbl[idx].type = type;
    
    // Allocate string memory if string type
    if (type == T_STR) {
        vartbl[idx].val.sval = (char *)malloc(STRINGSIZE);
        if (vartbl[idx].val.sval == NULL) {
            MMBasic_Error(ERR_OUT_MEMORY, "Cannot allocate string");
            return -1;
        }
        vartbl[idx].val.sval[0] = '\0';
    }
    
    return idx;
}

// Set variable value
void MMBasic_SetVariable(int index, int ival, float fval, char *sval) {
    if (index < 0 || index >= varcnt) {
        MMBasic_Error(ERR_BOUNDS, "Invalid variable index");
        return;
    }
    
    switch (vartbl[index].type) {
        case T_INT:
            vartbl[index].val.ival = ival;
            break;
        case T_FLOAT:
            vartbl[index].val.fval = fval;
            break;
        case T_STR:
            if (sval != NULL) {
                strncpy(vartbl[index].val.sval, sval, STRINGSIZE - 1);
                vartbl[index].val.sval[STRINGSIZE - 1] = '\0';
            }
            break;
    }
}

// Get variable integer value
int MMBasic_GetVariableInt(int index) {
    if (index < 0 || index >= varcnt) {
        MMBasic_Error(ERR_BOUNDS, "Invalid variable index");
        return 0;
    }
    
    switch (vartbl[index].type) {
        case T_INT:
            return vartbl[index].val.ival;
        case T_FLOAT:
            return (int)vartbl[index].val.fval;
        case T_STR:
            return atoi(vartbl[index].val.sval);
        default:
            return 0;
    }
}

// Get variable float value
float MMBasic_GetVariableFloat(int index) {
    if (index < 0 || index >= varcnt) {
        MMBasic_Error(ERR_BOUNDS, "Invalid variable index");
        return 0.0;
    }
    
    switch (vartbl[index].type) {
        case T_INT:
            return (float)vartbl[index].val.ival;
        case T_FLOAT:
            return vartbl[index].val.fval;
        case T_STR:
            return atof(vartbl[index].val.sval);
        default:
            return 0.0;
    }
}

// Get variable string value
char *MMBasic_GetVariableString(int index) {
    if (index < 0 || index >= varcnt) {
        MMBasic_Error(ERR_BOUNDS, "Invalid variable index");
        return "";
    }
    
    if (vartbl[index].type == T_STR) {
        return vartbl[index].val.sval;
    }
    
    // Convert numeric to string
    char *str = MMBasic_GetTempString();
    if (vartbl[index].type == T_INT) {
        sprintf(str, "%d", vartbl[index].val.ival);
    } else if (vartbl[index].type == T_FLOAT) {
        sprintf(str, "%g", vartbl[index].val.fval);
    } else {
        str[0] = '\0';
    }
    return str;
}

// Tokenise a line
int MMBasic_Tokenise(char *source, char *dest) {
    char token[STRINGSIZE];
    char *src = source;
    char *dst = dest;
    int len = 0;
    
    while (*src) {
        src = MMBasic_GetToken(src, token);
        
        if (token[0] == '\0') break;
        
        // Check if token is a keyword
        for (int i = 0; commandtbl[i].name[0] != '\0'; i++) {
            if (strcmp(token, commandtbl[i].name) == 0) {
                *dst++ = commandtbl[i].token;
                len++;
                goto next_token;
            }
        }
        
        // Copy token as-is
        strcpy(dst, token);
        dst += strlen(token);
        len += strlen(token);
        
        next_token:
        ;
    }
    
    *dst = '\0';
    return len;
}

// Store a program line
void MMBasic_StoreLine(int linenum, char *line) {
    // Find insertion point
    int i;
    for (i = 0; i < linecnt; i++) {
        if (lineNumbers[i] == linenum) {
            // Replace existing line
            // For now, just mark as deleted if empty
            if (line[0] == '\0') {
                // Shift lines up
                for (int j = i; j < linecnt - 1; j++) {
                    lines[j] = lines[j + 1];
                    lineNumbers[j] = lineNumbers[j + 1];
                }
                linecnt--;
                return;
            }
            // Replace line content
            lines[i] = progptr;
            strcpy(progptr, line);
            progptr += strlen(line) + 1;
            return;
        }
        if (lineNumbers[i] > linenum) {
            break;
        }
    }
    
    // Insert new line
    if (line[0] == '\0') return; // Don't insert empty lines
    
    if (linecnt >= MAX_LINES) {
        MMBasic_Error(ERR_OUT_MEMORY, "Program too large");
        return;
    }
    
    // Shift lines down
    for (int j = linecnt; j > i; j--) {
        lines[j] = lines[j - 1];
        lineNumbers[j] = lineNumbers[j - 1];
    }
    
    // Store new line
    lines[i] = progptr;
    lineNumbers[i] = linenum;
    strcpy(progptr, line);
    progptr += strlen(line) + 1;
    linecnt++;
}

// Execute a line
void MMBasic_Execute(char *line) {
    char token[STRINGSIZE];
    int cmd;
    
    currentLine = line;
    
    // Skip whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    // Check for empty line
    if (*line == '\0') return;
    
    // Handle : (colon) statement separator
    {
        char *scan = line;
        char *colonPos = NULL;
        while (*scan) {
            if (*scan == '"') {
                scan++;
                while (*scan && *scan != '"') scan++;
                if (*scan == '"') scan++;
            } else if (*scan == ':') {
                colonPos = scan;
                break;
            } else {
                scan++;
            }
        }
        if (colonPos) {
            char saved = *colonPos;
            *colonPos = '\0';
            MMBasic_Execute(line);
            *colonPos = saved;
            if (!flowControlActive) {
                MMBasic_Execute(colonPos + 1);
            }
            return;
        }
    }
    
    // Get command token
    char *nextPos = MMBasic_GetToken(line, token);
    
    if (token[0] == '\0') return;
    
    // Check for line number (program storage)
    if (token[0] >= '0' && token[0] <= '9') {
        int linenum = atoi(token);
        // Store the rest of the line
        MMBasic_StoreLine(linenum, nextPos);
        return;
    }
    
    // Find command
    cmd = MMBasic_GetCommand(token);
    
    // Save current position for commands that need it
    currentLine = nextPos;
    
    switch (cmd) {
        case C_PRINT:
            MMBasic_CmdPrint();
            break;
        case C_INPUT:
            MMBasic_CmdInput();
            break;
        case C_IF:
            MMBasic_CmdIf();
            break;
        case C_ELSE:
            MMBasic_CmdElse();
            break;
        case C_ENDIF:
            MMBasic_CmdEndif();
            break;
        case C_FOR:
            MMBasic_CmdFor();
            break;
        case C_NEXT:
            MMBasic_CmdNext();
            break;
        case C_GOTO:
            MMBasic_CmdGoto();
            break;
        case C_GOSUB:
            MMBasic_CmdGosub();
            break;
        case C_ON:
            MMBasic_CmdOn();
            break;
        case C_RETURN:
            MMBasic_CmdReturn();
            break;
        case C_END:
            MMBasic_CmdEnd();
            break;
        case C_LET:
            MMBasic_CmdLet();
            break;
        case C_DIM:
            MMBasic_CmdDim();
            break;
        case C_CLS:
            HAL_Display_Clear();
            break;
        case C_COLOR:
            MMBasic_CmdColor();
            break;
        case C_LINE:
            MMBasic_CmdLine();
            break;
        case C_CIRCLE:
            MMBasic_CmdCircle();
            break;
        case C_RECT:
            MMBasic_CmdRect();
            break;
        case C_PIXEL:
            MMBasic_CmdPixel();
            break;
        case C_REM:
        case C_REM2:
            // Comment - do nothing
            break;
        case C_LOCATE:
            MMBasic_CmdLocate();
            break;
        case C_RUN:
            MMBasic_RunProgram();
            break;
        case C_LIST:
            MMBasic_ListProgram();
            break;
        case C_NEW:
            MMBasic_Reset();
            HAL_Display_Println("Ready.");
            break;
        case C_SAVE:
            MMBasic_CmdSave();
            break;
        case C_LOAD:
            MMBasic_CmdLoad();
            break;
        case C_FILES:
            MMBasic_CmdFiles();
            break;
        case C_OPEN:
            MMBasic_CmdOpen();
            break;
        case C_CLOSE:
            MMBasic_CmdClose();
            break;
        case C_SEEK:
            MMBasic_CmdSeek();
            break;
        case C_SWAP:
            MMBasic_CmdSwap();
            break;
        case C_INC:
            MMBasic_CmdInc();
            break;
        case C_PAUSE:
            MMBasic_CmdPause();
            break;
        case C_RANDOMIZE:
            MMBasic_CmdRandomize();
            break;
        case C_POKE:
            MMBasic_CmdPoke();
            break;
        case C_CONST:
            MMBasic_CmdConst();
            break;
        case C_DATA:
            // DATA is passive - lines are already stored, READ parses them
            break;
        case C_READ:
            MMBasic_CmdRead();
            break;
        case C_RESTORE:
            MMBasic_CmdRestore();
            break;
        case C_WHILE:
            MMBasic_CmdWhile();
            break;
        case C_WEND:
            MMBasic_CmdWend();
            break;
        case C_SELECT:
            MMBasic_CmdSelect();
            break;
        case C_CASE:
            MMBasic_CmdCase();
            break;
        case C_ENDSELECT:
            MMBasic_CmdEndSelect();
            break;
        case C_DO:
            MMBasic_CmdDo();
            break;
        case C_LOOP:
            MMBasic_CmdLoop();
            break;
        case C_CONTINUE:
            MMBasic_CmdContinue();
            break;
        case C_EXIT:
            MMBasic_CmdExit();
            break;
        case C_OPTION:
            MMBasic_CmdOption();
            break;
        case C_KILL:
            MMBasic_CmdKill();
            break;
        case C_MKDIR:
            MMBasic_CmdMkdir();
            break;
        case C_RMDIR:
            MMBasic_CmdRmdir();
            break;
        case C_CHDIR:
            MMBasic_CmdChdir();
            break;
        case C_COPY:
            MMBasic_CmdCopy();
            break;
        case C_RENAME:
            MMBasic_CmdRename();
            break;
        case C_DRIVE:
            MMBasic_CmdDrive();
            break;
        case C_SETPIN:
            MMBasic_CmdSetpin();
            break;
        case C_DIGOUT:
            MMBasic_CmdDigout();
            break;
        case C_PWMCMD:
            MMBasic_CmdPwm();
            break;
        case C_I2C:
            MMBasic_CmdI2c();
            break;
        case C_SPI:
            MMBasic_CmdSpi();
            break;
        case C_IR:
            MMBasic_CmdIr();
            break;
        case C_SERVO:
            MMBasic_CmdServo();
            break;
        case C_PORTCMD:
            MMBasic_CmdPort();
            break;
        case C_PULSE:
            MMBasic_CmdPulse();
            break;
        case C_BOX:
            MMBasic_CmdBox();
            break;
        case C_TEXT:
            MMBasic_CmdText();
            break;
        case C_TRIANGLE:
            MMBasic_CmdTriangle();
            break;
        case C_CLEAR:
            MMBasic_CmdClear();
            break;
        case C_ERASE:
            MMBasic_CmdErase();
            break;
        case C_REDIM:
            MMBasic_CmdRedim();
            break;
        case C_ERROR:
            MMBasic_CmdError();
            break;
        case C_TRACE:
            MMBasic_CmdTrace();
            break;
        case C_SORTCMD:
            MMBasic_CmdSort();
            break;
        case C_CALL:
            MMBasic_CmdCall();
            break;
        case C_TURTLE:
            MMBasic_CmdTurtle();
            break;
        case C_SPRITE:
            MMBasic_CmdSprite();
            break;
        case C_VAR:
            MMBasic_CmdVar();
            break;
        case C_SUB:
        case C_FUNCTION:
            MMBasic_CmdSubFunDef();
            break;
        case C_ENDSUB:
        case C_ENDFUN:
            MMBasic_CmdEndSubFun();
            break;
        case C_PRINT_HASH:
        case C_INPUT_HASH:
            // Handled inside CmdPrint/CmdInput via # detection
            break;
        case C_INVALID:
            // Try as assignment (variable = expression)
            MMBasic_CmdImplicitLet(line);
            break;
        default:
            HAL_Display_Println("Command not implemented");
            break;
    }
}

// Get command from token
int MMBasic_GetCommand(char *token) {
    for (int i = 0; commandtbl[i].name[0] != '\0'; i++) {
        if (strcmp(token, commandtbl[i].name) == 0) {
            if (commandtbl[i].type == C_CMD) {
                return commandtbl[i].token;
            }
        }
    }
    return C_INVALID;
}

// Get function from token
int MMBasic_GetFunction(char *token) {
    for (int i = 0; commandtbl[i].name[0] != '\0'; i++) {
        if (strcmp(token, commandtbl[i].name) == 0) {
            if (commandtbl[i].type == C_FUNC) {
                return commandtbl[i].token;
            }
        }
    }
    return C_INVALID;
}

// Error handling
void MMBasic_Error(int error, char *msg) {
    HAL_Display_Print("?Error: ");
    if (msg != NULL) {
        HAL_Display_Println(msg);
    } else {
        switch (error) {
            case ERR_SYNTAX:
                HAL_Display_Println("Syntax error");
                break;
            case ERR_UNKNOWN_CMD:
                HAL_Display_Println("Unknown command");
                break;
            case ERR_ARGUMENT:
                HAL_Display_Println("Invalid argument");
                break;
            case ERR_DIV_ZERO:
                HAL_Display_Println("Division by zero");
                break;
            case ERR_OVERFLOW:
                HAL_Display_Println("Overflow");
                break;
            case ERR_OUT_MEMORY:
                HAL_Display_Println("Out of memory");
                break;
            case ERR_STACK_FULL:
                HAL_Display_Println("Stack full");
                break;
            case ERR_STACK_EMPTY:
                HAL_Display_Println("Stack empty");
                break;
            case ERR_FOR_NEXT:
                HAL_Display_Println("FOR/NEXT mismatch");
                break;
            case ERR_GOSUB:
                HAL_Display_Println("GOSUB stack full");
                break;
            case ERR_RETURN:
                HAL_Display_Println("RETURN without GOSUB");
                break;
            case ERR_FILE_IO:
                HAL_Display_Println("File I/O error");
                break;
            case ERR_TYPE:
                HAL_Display_Println("Type mismatch");
                break;
            case ERR_BOUNDS:
                HAL_Display_Println("Out of bounds");
                break;
            default:
                HAL_Display_Println("Unknown error");
                break;
        }
    }
    
    // Reset stacks
    forstackptr = 0;
    gosubstackptr = 0;
    
    longjmp(mark, 1);
}

// Get temporary string from pool
char *MMBasic_GetTempString(void) {
    char *str = strpool[strpoolindex];
    strpoolindex = (strpoolindex + 1) % 16;
    str[0] = '\0';
    return str;
}

// ============================================================================
// Expression Evaluator
// ============================================================================

// Main expression evaluator
static int EvaluateExpression(char **expr, int *itype, int *ival, float *fval, char **sval) {
    return EvaluateSubExpression(expr, itype, ival, fval, sval);
}

// Handle OR, XOR
static int EvaluateSubExpression(char **expr, int *itype, int *ival, float *fval, char **sval) {
    int leftType, leftIval;
    float leftFval;
    char *leftSval;
    
    // Get left operand
    if (EvaluateTerm(expr, &leftType, &leftIval, &leftFval, &leftSval)) return 1;
    
    while (**expr) {
        // Skip whitespace
        while (**expr == ' ') (*expr)++;
        
        // Check for OR/XOR
        int op = 0;
        if (strncmp(*expr, "OR", 2) == 0 && !isalpha((*expr)[2])) {
            op = OP_OR;
            *expr += 2;
        } else if (strncmp(*expr, "XOR", 3) == 0 && !isalpha((*expr)[3])) {
            op = OP_XOR;
            *expr += 3;
        } else {
            break;
        }
        
        int rightType, rightIval;
        float rightFval;
        char *rightSval;
        
        if (EvaluateTerm(expr, &rightType, &rightIval, &rightFval, &rightSval)) return 1;
        
        // Perform operation
        int result = 0;
        if (op == OP_OR) {
            result = (leftIval || rightIval);
        } else if (op == OP_XOR) {
            result = (leftIval != rightIval);
        }
        
        leftType = T_INT;
        leftIval = result;
    }
    
    *itype = leftType;
    *ival = leftIval;
    *fval = leftFval;
    *sval = leftSval;
    return 0;
}

// Handle AND
static int EvaluateTerm(char **expr, int *itype, int *ival, float *fval, char **sval) {
    int leftType, leftIval;
    float leftFval;
    char *leftSval;
    
    // Get left operand
    if (EvaluateFactor(expr, &leftType, &leftIval, &leftFval, &leftSval)) return 1;
    
    while (**expr) {
        // Skip whitespace
        while (**expr == ' ') (*expr)++;
        
        // Check for AND
        if (strncmp(*expr, "AND", 3) == 0 && !isalpha((*expr)[3])) {
            *expr += 3;
        } else {
            break;
        }
        
        int rightType, rightIval;
        float rightFval;
        char *rightSval;
        
        if (EvaluateFactor(expr, &rightType, &rightIval, &rightFval, &rightSval)) return 1;
        
        // Perform AND operation
        leftIval = (leftIval && rightIval);
        leftType = T_INT;
    }
    
    *itype = leftType;
    *ival = leftIval;
    *fval = leftFval;
    *sval = leftSval;
    return 0;
}

// Handle comparisons and basic arithmetic
static int EvaluateFactor(char **expr, int *itype, int *ival, float *fval, char **sval) {
    int leftType, leftIval;
    float leftFval;
    char *leftSval;
    
    // Handle NOT
    if (strncmp(*expr, "NOT", 3) == 0 && !isalpha((*expr)[3])) {
        *expr += 3;
        if (EvaluateFactor(expr, &leftType, &leftIval, &leftFval, &leftSval)) return 1;
        *itype = T_INT;
        *ival = !leftIval;
        return 0;
    }
    
    // Handle unary minus
    int negate = 0;
    while (**expr == ' ') (*expr)++;
    if (**expr == '-') {
        negate = 1;
        (*expr)++;
    } else if (**expr == '+') {
        (*expr)++;
    }
    
    // Parse the primary value
    while (**expr == ' ') (*expr)++;
    
    // Check for parentheses
    if (**expr == '(') {
        (*expr)++;
        if (EvaluateExpression(expr, &leftType, &leftIval, &leftFval, &leftSval)) return 1;
        if (**expr == ')') (*expr)++;
        else {
            MMBasic_Error(ERR_SYNTAX, "Missing )");
            return 1;
        }
    }
    // Check for file number (#fnbr)
    else if (**expr == '#') {
        (*expr)++;
        while (**expr == ' ') (*expr)++;
        char numStr[16];
        int i = 0;
        while (**expr >= '0' && **expr <= '9' && i < 15) {
            numStr[i++] = **expr;
            (*expr)++;
        }
        numStr[i] = '\0';
        leftType = T_INT;
        leftIval = atoi(numStr);
        leftFval = (float)leftIval;
    }
    // Check for string literal
    else if (**expr == '"') {
        (*expr)++;
        leftSval = MMBasic_GetTempString();
        int i = 0;
        while (**expr != '"' && **expr != '\0' && i < STRINGSIZE - 1) {
            leftSval[i++] = **expr;
            (*expr)++;
        }
        leftSval[i] = '\0';
        if (**expr == '"') (*expr)++;
        leftType = T_STR;
        leftIval = 0;
        leftFval = 0.0;
    }
    // Check for number
    else if ((**expr >= '0' && **expr <= '9') || **expr == '.') {
        char numStr[64];
        int i = 0;
        int hasDot = 0;
        while ((**expr >= '0' && **expr <= '9') || **expr == '.' || **expr == 'E' || **expr == 'e') {
            if (**expr == '.') hasDot = 1;
            if (i < 63) numStr[i++] = **expr;
            (*expr)++;
        }
        numStr[i] = '\0';
        
        if (hasDot) {
            leftType = T_FLOAT;
            leftFval = atof(numStr);
            leftIval = (int)leftFval;
        } else {
            leftType = T_INT;
            leftIval = atoi(numStr);
            leftFval = (float)leftIval;
        }
        leftSval = NULL;
    }
    // Check for function or variable
    else if ((*expr[0] >= 'A' && *expr[0] <= 'Z') || (*expr[0] >= 'a' && *expr[0] <= 'z')) {
        char name[MAXVARLEN + 2]; // +2 for $ and null
        int i = 0;
        while ((**expr >= 'A' && **expr <= 'Z') || (**expr >= 'a' && **expr <= 'z') || 
               (**expr >= '0' && **expr <= '9') || **expr == '_' || **expr == '$') {
            if (i < MAXVARLEN + 1) name[i++] = **expr;
            (*expr)++;
        }
        name[i] = '\0';
        
        // Convert to uppercase
        for (int j = 0; name[j]; j++) {
            if (name[j] >= 'a' && name[j] <= 'z') {
                name[j] = name[j] - 'a' + 'A';
            }
        }
        
        // Check if it's a built-in function
        int funcToken = MMBasic_GetFunction(name);
        if (funcToken != C_INVALID) {
            if (EvaluateFunction(expr, funcToken, &leftType, &leftIval, &leftFval, &leftSval)) return 1;
        } else {
            // Check if it's a user-defined FUNCTION
            int ufIdx = -1;
            for (int k=0; k<subFunCount; k++) {
                if (subFunTable[k].isFunction && strcmp(name, subFunTable[k].name)==0) {
                    ufIdx = k; break;
                }
            }
            if (ufIdx >= 0) {
                // Call user FUNCTION - parse arguments in parens
                if (**expr == '(') (*expr)++;
                int args[8]; float fargs[8]; char *sargs[8]; int atypes[8]; int argc=0;
                while (**expr && **expr!=')' && argc<8) {
                    while (**expr==' '||**expr==','||**expr=='\t') (*expr)++;
                    if (**expr==')') break;
                    atypes[argc]=T_INT;
                    if (EvaluateExpression(expr,&atypes[argc],&args[argc],&fargs[argc],&sargs[argc])) return 1;
                    argc++;
                }
                if (**expr==')') (*expr)++;
                // Call the function
                int savedShadowBase = shadowPtr;
                if (shadowBaseSP<16) shadowBase[shadowBaseSP++]=shadowPtr;
                for (int p=0; p<subFunTable[ufIdx].paramCount; p++) {
                    char *pn = subFunTable[ufIdx].paramNames[p];
                    int vi = MMBasic_FindVariable(pn);
                    if (shadowPtr>=MAX_SHADOW) break;
                    if (vi>=0) {
                        shadowStack[shadowPtr].varIndex=vi; shadowStack[shadowPtr].type=vartbl[vi].type;
                        shadowStack[shadowPtr].ival=vartbl[vi].val.ival;
                        shadowStack[shadowPtr].fval=vartbl[vi].val.fval;
                        shadowStack[shadowPtr].sval=NULL;
                        if (vartbl[vi].type==T_STR && vartbl[vi].val.sval) {
                            shadowStack[shadowPtr].sval=(char*)malloc(STRINGSIZE);
                            strcpy(shadowStack[shadowPtr].sval, vartbl[vi].val.sval);
                        }
                        shadowStack[shadowPtr].array=vartbl[vi].array;
                        shadowStack[shadowPtr].arr=vartbl[vi].arr;
                        shadowStack[shadowPtr].wasConst=varIsConst[vi];
                    } else {
                        shadowStack[shadowPtr].varIndex=-1;
                        vi = MMBasic_CreateVariable(pn, (p<argc && atypes[p]==T_STR)?T_STR:T_INT);
                    }
                    shadowPtr++;
                    if (vi>=0 && p<argc) {
                        varIsConst[vi]=false;
                        if (atypes[p]==T_STR) {
                            if (!vartbl[vi].val.sval) vartbl[vi].val.sval=(char*)malloc(STRINGSIZE);
                            strncpy(vartbl[vi].val.sval, sargs[p]?sargs[p]:"",STRINGSIZE-1);
                            vartbl[vi].type=T_STR;
                        } else {
                            vartbl[vi].val.ival=args[p]; vartbl[vi].type=atypes[p];
                        }
                    }
                }
                // Execute function body
                int savedIdx = currentLineIndex; char *savedLine = currentLine; int savedFlow = flowControlActive;
                int savedSCRI = subFunCallReturnIdx;
                currentLineIndex = subFunTable[ufIdx].startLine + 1;
                flowControlActive = false;
                char originalName[MAXVARLEN+1]; strcpy(originalName, name);
                subFunCallReturnIdx = -1;
                while (currentLineIndex >= 0 && currentLineIndex < linecnt && 
                       currentLineIndex <= subFunTable[ufIdx].endLine) {
                    currentLine = lines[currentLineIndex];
                    flowControlActive = false;
                    if (setjmp(mark)==0) { MMBasic_Execute(currentLine); }
                    else {
                        // Restore and rethrow
                        shadowBaseSP--; shadowPtr = savedShadowBase;
                        longjmp(mark, 1);
                    }
                    if (!flowControlActive) currentLineIndex++;
                }
                // Restore state
                currentLineIndex = savedIdx; currentLine = savedLine;
                flowControlActive = savedFlow;
                subFunCallReturnIdx = savedSCRI;
                // Get return value from variable with function name
                int rvi = MMBasic_FindVariable(originalName);
                leftType = (rvi>=0) ? vartbl[rvi].type : T_INT;
                leftIval = (rvi>=0) ? vartbl[rvi].val.ival : 0;
                leftFval = (rvi>=0) ? vartbl[rvi].val.fval : 0.0f;
                leftSval = (rvi>=0 && vartbl[rvi].type==T_STR) ? vartbl[rvi].val.sval : NULL;
                // Cleanup shadows
                shadowBaseSP--;
                while (shadowPtr > savedShadowBase) {
                    shadowPtr--;
                    int vi2 = shadowStack[shadowPtr].varIndex;
                    if (vi2>=0) {
                        if (vartbl[vi2].type==T_STR && vartbl[vi2].val.sval) {
                            free(vartbl[vi2].val.sval); vartbl[vi2].val.sval=NULL;
                        }
                        vartbl[vi2].type=shadowStack[shadowPtr].type;
                        vartbl[vi2].val.ival=shadowStack[shadowPtr].ival;
                        vartbl[vi2].val.fval=shadowStack[shadowPtr].fval;
                        vartbl[vi2].array=shadowStack[shadowPtr].array;
                        vartbl[vi2].arr=shadowStack[shadowPtr].arr;
                        varIsConst[vi2]=shadowStack[shadowPtr].wasConst;
                        if (shadowStack[shadowPtr].sval) vartbl[vi2].val.sval=shadowStack[shadowPtr].sval;
                    }
                }
                // Don't return from EvaluateFactor - continue to operator handling
            } else {
            // It's a variable
            int varIdx = MMBasic_FindVariable(name);
            if (varIdx < 0) {
                char type = T_INT;
                if (name[strlen(name) - 1] == '$') type = T_STR;
                varIdx = MMBasic_CreateVariable(name, type);
                if (varIdx < 0) return 1;
            }
            
            leftType = vartbl[varIdx].type;
            leftIval = vartbl[varIdx].val.ival;
            leftFval = vartbl[varIdx].val.fval;
            leftSval = vartbl[varIdx].val.sval;
            }
        }
    }
    else {
        MMBasic_Error(ERR_SYNTAX, "Expected value");
        return 1;
    }
    
    // Apply unary minus
    if (negate) {
        if (leftType == T_INT) leftIval = -leftIval;
        else if (leftType == T_FLOAT) leftFval = -leftFval;
    }
    
    // Check for comparison operators
    while (**expr == ' ') (*expr)++;
    
    int compOp = 0;
    if (**expr == '<' && (*expr)[1] == '<') {
        // shift left - not a comparison, skip
    } else if (**expr == '>' && (*expr)[1] == '>') {
        // shift right - not a comparison, skip
    } else if (**expr == '=' && (*expr)[1] != '=') {
        compOp = OP_EQ;
        (*expr)++;
    } else if (**expr == '<') {
        if ((*expr)[1] == '>') {
            compOp = OP_NE;
            *expr += 2;
        } else if ((*expr)[1] == '=') {
            compOp = OP_LE;
            *expr += 2;
        } else {
            compOp = OP_LT;
            (*expr)++;
        }
    } else if (**expr == '>') {
        if ((*expr)[1] == '=') {
            compOp = OP_GE;
            *expr += 2;
        } else {
            compOp = OP_GT;
            (*expr)++;
        }
    }
    
    if (compOp) {
        int rightType, rightIval;
        float rightFval;
        char *rightSval;
        
        if (EvaluateFactor(expr, &rightType, &rightIval, &rightFval, &rightSval)) return 1;
        
        // Perform comparison
        int result = 0;
        if (leftType == T_STR && rightType == T_STR) {
            int cmp = strcmp(leftSval, rightSval);
            switch (compOp) {
                case OP_EQ: result = (cmp == 0); break;
                case OP_NE: result = (cmp != 0); break;
                case OP_LT: result = (cmp < 0); break;
                case OP_GT: result = (cmp > 0); break;
                case OP_LE: result = (cmp <= 0); break;
                case OP_GE: result = (cmp >= 0); break;
            }
        } else {
            float lval = (leftType == T_INT) ? (float)leftIval : leftFval;
            float rval = (rightType == T_INT) ? (float)rightIval : rightFval;
            switch (compOp) {
                case OP_EQ: result = (lval == rval); break;
                case OP_NE: result = (lval != rval); break;
                case OP_LT: result = (lval < rval); break;
                case OP_GT: result = (lval > rval); break;
                case OP_LE: result = (lval <= rval); break;
                case OP_GE: result = (lval >= rval); break;
            }
        }
        
        leftType = T_INT;
        leftIval = result;
        leftFval = (float)result;
        leftSval = NULL;
    }
    
    // Handle arithmetic operators (+, -, *, /, MOD, ^, \, <<, >>)
    while (**expr == ' ') (*expr)++;
    
    while (**expr == '+' || **expr == '-' || **expr == '*' || **expr == '/' || 
           **expr == '^' || **expr == '\\' ||
           strncmp(*expr, "MOD", 3) == 0 ||
           strncmp(*expr, "<<", 2) == 0 ||
           strncmp(*expr, ">>", 2) == 0) {
        
        int op;
        if (strncmp(*expr, "<<", 2) == 0) { op = OP_SHL; *expr += 2; }
        else if (strncmp(*expr, ">>", 2) == 0) { op = OP_SHR; *expr += 2; }
        else if (**expr == '+') { op = OP_PLUS; (*expr)++; }
        else if (**expr == '-') { op = OP_MINUS; (*expr)++; }
        else if (**expr == '*') { op = OP_MULTIPLY; (*expr)++; }
        else if (**expr == '/') { op = OP_DIVIDE; (*expr)++; }
        else if (**expr == '^') { op = OP_POWER; (*expr)++; }
        else if (**expr == '\\') { op = OP_DIVINT; (*expr)++; }
        else if (strncmp(*expr, "MOD", 3) == 0 && !isalpha((*expr)[3])) { op = OP_MOD; *expr += 3; }
        else break;
        
        int rightType, rightIval;
        float rightFval;
        char *rightSval;
        
        // Handle unary minus for right operand
        while (**expr == ' ') (*expr)++;
        int rightNegate = 0;
        if (**expr == '-') {
            rightNegate = 1;
            (*expr)++;
        }
        
        if (EvaluateFactor(expr, &rightType, &rightIval, &rightFval, &rightSval)) return 1;
        
        if (rightNegate) {
            if (rightType == T_INT) rightIval = -rightIval;
            else if (rightType == T_FLOAT) rightFval = -rightFval;
        }
        
        // String concatenation with +
        if (op == OP_PLUS && leftType == T_STR && rightType == T_STR) {
            char *result = MMBasic_GetTempString();
            strncpy(result, leftSval, STRINGSIZE - 1);
            strncat(result, rightSval, STRINGSIZE - strlen(result) - 1);
            leftSval = result;
            continue;
        }
        
        // Numeric operations
        float lval = (leftType == T_INT) ? (float)leftIval : leftFval;
        float rval = (rightType == T_INT) ? (float)rightIval : rightFval;
        
        switch (op) {
            case OP_PLUS:
                lval += rval;
                break;
            case OP_MINUS:
                lval -= rval;
                break;
            case OP_MULTIPLY:
                lval *= rval;
                break;
            case OP_DIVIDE:
                if (rval == 0) {
                    MMBasic_Error(ERR_DIV_ZERO, NULL);
                    return 1;
                }
                lval /= rval;
                break;
            case OP_MOD:
                if (rval == 0) {
                    MMBasic_Error(ERR_DIV_ZERO, NULL);
                    return 1;
                }
                lval = fmod(lval, rval);
                break;
            case OP_POWER:
                lval = pow(lval, rval);
                break;
            case OP_SHL:
                lval = (float)((int)lval << (int)rval);
                break;
            case OP_SHR:
                lval = (float)((int)lval >> (int)rval);
                break;
            case OP_DIVINT:
                if (rval == 0) {
                    MMBasic_Error(ERR_DIV_ZERO, NULL);
                    return 1;
                }
                lval = (float)((int)lval / (int)rval);
                break;
        }
        
        // Determine result type
        if (leftType == T_INT && rightType == T_INT && op != OP_DIVIDE && op != OP_POWER) {
            leftType = T_INT;
            leftIval = (int)lval;
            leftFval = lval;
        } else if (op == OP_SHL || op == OP_SHR || op == OP_DIVINT) {
            leftType = T_INT;
            leftIval = (int)lval;
            leftFval = (float)leftIval;
        } else {
            leftType = T_FLOAT;
            leftFval = lval;
            leftIval = (int)lval;
        }
        leftSval = NULL;
        
        while (**expr == ' ') (*expr)++;
    }
    
    *itype = leftType;
    *ival = leftIval;
    *fval = leftFval;
    *sval = leftSval;
    return 0;
}

// Evaluate a function call
static int EvaluateFunction(char **expr, int funcToken, int *itype, int *ival, float *fval, char **sval) {
    // Functions that don't require parentheses
    int noArg = (funcToken == F_RND || funcToken == F_PI || funcToken == F_DATE || funcToken == F_TIME);
    
    // Skip optional opening parenthesis
    int hasParen = 0;
    if (**expr == '(') {
        hasParen = 1;
        (*expr)++;
    } else if (!noArg) {
        MMBasic_Error(ERR_SYNTAX, "Expected (");
        return 1;
    }
    
    // Get argument(s)
    int arg1Type = T_INT, arg1Ival = 0;
    float arg1Fval = 0;
    char *arg1Sval = NULL;
    int arg2Type = T_INT, arg2Ival = 0;
    float arg2Fval = 0;
    char *arg2Sval = NULL;
    int hasArg2 = 0;
    int arg3Type = T_INT, arg3Ival = 0;
    float arg3Fval = 0;
    char *arg3Sval = NULL;
    int hasArg3 = 0;
    
    if (!noArg || hasParen) {
        // Check for empty parens: RND() or PI()
        while (**expr == ' ') (*expr)++;
        if (**expr == ')') {
            (*expr)++;
            hasParen = 0; // Consumed, don't expect closing paren
        } else {
            if (EvaluateExpression(expr, &arg1Type, &arg1Ival, &arg1Fval, &arg1Sval)) return 1;
            
            if (**expr == ',') {
                (*expr)++;
                hasArg2 = 1;
                if (EvaluateExpression(expr, &arg2Type, &arg2Ival, &arg2Fval, &arg2Sval)) return 1;
            }
            
            if (**expr == ',') {
                (*expr)++;
                hasArg3 = 1;
                if (EvaluateExpression(expr, &arg3Type, &arg3Ival, &arg3Fval, &arg3Sval)) return 1;
            }
        }
    }
    
    // Expect closing parenthesis (if we opened one)
    if (hasParen) {
        if (**expr == ')') (*expr)++;
        else {
            MMBasic_Error(ERR_SYNTAX, "Expected )");
            return 1;
        }
    }
    
    // Get numeric values
    float numVal = (arg1Type == T_INT) ? (float)arg1Ival : arg1Fval;
    float numVal2 = (arg2Type == T_INT) ? (float)arg2Ival : arg2Fval;
    
    // Execute function
    switch (funcToken) {
        case F_ABS:
            *itype = (arg1Type == T_INT) ? T_INT : T_FLOAT;
            *ival = abs(arg1Ival);
            *fval = fabs(numVal);
            break;
        case F_INT:
            *itype = T_INT;
            *ival = (int)numVal;
            *fval = (float)*ival;
            break;
        case F_SGN:
            *itype = T_INT;
            if (numVal > 0) *ival = 1;
            else if (numVal < 0) *ival = -1;
            else *ival = 0;
            *fval = (float)*ival;
            break;
        case F_SQR:
            *itype = T_FLOAT;
            *fval = sqrt(numVal);
            *ival = (int)*fval;
            break;
        case F_SIN:
            *itype = T_FLOAT;
            *fval = sin(numVal);
            *ival = (int)*fval;
            break;
        case F_COS:
            *itype = T_FLOAT;
            *fval = cos(numVal);
            *ival = (int)*fval;
            break;
        case F_TAN:
            *itype = T_FLOAT;
            *fval = tan(numVal);
            *ival = (int)*fval;
            break;
        case F_ATN:
            *itype = T_FLOAT;
            *fval = atan(numVal);
            *ival = (int)*fval;
            break;
        case F_LOG:
            *itype = T_FLOAT;
            *fval = log(numVal);
            *ival = (int)*fval;
            break;
        case F_EXP:
            *itype = T_FLOAT;
            *fval = exp(numVal);
            *ival = (int)*fval;
            break;
        case F_RND:
            *itype = T_FLOAT;
            *fval = (float)rand() / ((float)RAND_MAX + 1.0f);
            *ival = (int)*fval;
            break;
        case F_LEN:
            *itype = T_INT;
            if (arg1Type == T_STR && arg1Sval != NULL) {
                *ival = strlen(arg1Sval);
            } else {
                *ival = 0;
            }
            *fval = (float)*ival;
            break;
        case F_VAL:
            *itype = T_FLOAT;
            if (arg1Type == T_STR && arg1Sval != NULL) {
                *fval = atof(arg1Sval);
            } else {
                *fval = 0.0;
            }
            *ival = (int)*fval;
            break;
        case F_ASC:
            *itype = T_INT;
            if (arg1Type == T_STR && arg1Sval != NULL && arg1Sval[0] != '\0') {
                *ival = arg1Sval[0];
            } else {
                *ival = 0;
            }
            *fval = (float)*ival;
            break;
        case F_CHR:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            (*sval)[0] = (char)arg1Ival;
            (*sval)[1] = '\0';
            break;
        case F_STR:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            sprintf(*sval, "%g", numVal);
            break;
        case F_LEFT:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            if (arg1Type == T_STR && arg1Sval != NULL) {
                int len = hasArg2 ? arg2Ival : 0;
                if (len > strlen(arg1Sval)) len = strlen(arg1Sval);
                strncpy(*sval, arg1Sval, len);
                (*sval)[len] = '\0';
            }
            break;
        case F_RIGHT:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            if (arg1Type == T_STR && arg1Sval != NULL) {
                int len = hasArg2 ? arg2Ival : 0;
                int slen = strlen(arg1Sval);
                if (len > slen) len = slen;
                strcpy(*sval, arg1Sval + slen - len);
            }
            break;
        case F_MID:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            if (arg1Type == T_STR && arg1Sval != NULL) {
                int start = arg2Ival - 1; // BASIC is 1-based
                int len = hasArg3 ? arg3Ival : strlen(arg1Sval);
                int slen = strlen(arg1Sval);
                if (start < 0) start = 0;
                if (start >= slen) {
                    (*sval)[0] = '\0';
                } else {
                    if (start + len > slen) len = slen - start;
                    strncpy(*sval, arg1Sval + start, len);
                    (*sval)[len] = '\0';
                }
            }
            break;
        case F_INSTR:
            *itype = T_INT;
            *ival = 0;
            if (arg1Type == T_STR && arg2Type == T_STR) {
                char *pos = strstr(arg1Sval, arg2Sval);
                if (pos != NULL) {
                    *ival = (int)(pos - arg1Sval) + 1; // 1-based
                }
            }
            *fval = (float)*ival;
            break;
        case F_UPPER:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            if (arg1Type == T_STR && arg1Sval != NULL) {
                strcpy(*sval, arg1Sval);
                for (int i = 0; (*sval)[i]; i++) {
                    if ((*sval)[i] >= 'a' && (*sval)[i] <= 'z') {
                        (*sval)[i] -= 32;
                    }
                }
            }
            break;
        case F_LOWER:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            if (arg1Type == T_STR && arg1Sval != NULL) {
                strcpy(*sval, arg1Sval);
                for (int i = 0; (*sval)[i]; i++) {
                    if ((*sval)[i] >= 'A' && (*sval)[i] <= 'Z') {
                        (*sval)[i] += 32;
                    }
                }
            }
            break;
        case F_TRIM:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            if (arg1Type == T_STR && arg1Sval != NULL) {
                // Trim leading spaces
                while (*arg1Sval == ' ') arg1Sval++;
                strcpy(*sval, arg1Sval);
                // Trim trailing spaces
                int len = strlen(*sval);
                while (len > 0 && (*sval)[len - 1] == ' ') {
                    (*sval)[--len] = '\0';
                }
            }
            break;
        case F_HEX:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            sprintf(*sval, "%X", arg1Ival);
            break;
        case F_OCT:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            sprintf(*sval, "%o", arg1Ival);
            break;
        case F_BIN:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            // Convert to binary string
            {
                int val = arg1Ival;
                int i = 0;
                if (val == 0) {
                    (*sval)[i++] = '0';
                } else {
                    while (val > 0 && i < 32) {
                        (*sval)[i++] = (val & 1) ? '1' : '0';
                        val >>= 1;
                    }
                }
                (*sval)[i] = '\0';
                // Reverse string
                for (int j = 0; j < i / 2; j++) {
                    char tmp = (*sval)[j];
                    (*sval)[j] = (*sval)[i - 1 - j];
                    (*sval)[i - 1 - j] = tmp;
                }
            }
            break;
        case F_STRING:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            {
                int len = arg1Ival;
                char ch = (arg2Type == T_STR && arg2Sval != NULL) ? arg2Sval[0] : ' ';
                if (len >= STRINGSIZE) len = STRINGSIZE - 1;
                memset(*sval, ch, len);
                (*sval)[len] = '\0';
            }
            break;
        case F_SPACE:
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            {
                int len = arg1Ival;
                if (len >= STRINGSIZE) len = STRINGSIZE - 1;
                memset(*sval, ' ', len);
                (*sval)[len] = '\0';
            }
            break;
        case F_EOF: {
            int f = arg1Ival;
            if (f < 1 || f > MAXOPENFILES || !fileTable[f].inUse) {
                MMBasic_Error(ERR_FILE_IO, "File not open");
                return 1;
            }
            *itype = T_INT;
            *ival = !fileTable[f].file.available();
            *fval = *ival;
            break;
        }
        case F_LOF: {
            int f = arg1Ival;
            if (f < 1 || f > MAXOPENFILES || !fileTable[f].inUse) {
                MMBasic_Error(ERR_FILE_IO, "File not open");
                return 1;
            }
            *itype = T_INT;
            *ival = fileTable[f].file.size();
            *fval = *ival;
            break;
        }
        case F_LOC: {
            int f = arg1Ival;
            if (f < 1 || f > MAXOPENFILES || !fileTable[f].inUse) {
                MMBasic_Error(ERR_FILE_IO, "File not open");
                return 1;
            }
            *itype = T_INT;
            *ival = fileTable[f].file.position();
            *fval = *ival;
            break;
        }
        case F_PI:
            *itype = T_FLOAT;
            *fval = 3.14159265358979323846;
            *ival = 3;
            break;
        case F_PEEK: {
            unsigned char *addr = (unsigned char *)arg1Ival;
            *itype = T_INT;
            *ival = *addr;
            *fval = *ival;
            break;
        }
        case F_MAX:
            *itype = T_FLOAT;
            *fval = (numVal > numVal2) ? numVal : numVal2;
            *ival = (int)*fval;
            break;
        case F_MIN:
            *itype = T_FLOAT;
            *fval = (numVal < numVal2) ? numVal : numVal2;
            *ival = (int)*fval;
            break;
        case F_DEG:
            *itype = T_FLOAT;
            *fval = numVal * 57.2957795131f; // 180/PI
            *ival = (int)*fval;
            break;
        case F_RAD:
            *itype = T_FLOAT;
            *fval = numVal * 0.01745329252f; // PI/180
            *ival = (int)*fval;
            break;
        case F_ACOS:
            *itype = T_FLOAT;
            *fval = acos(numVal);
            *ival = (int)*fval;
            break;
        case F_ASIN:
            *itype = T_FLOAT;
            *fval = asin(numVal);
            *ival = (int)*fval;
            break;
        case F_ATAN2:
            *itype = T_FLOAT;
            *fval = atan2(numVal, numVal2);
            *ival = (int)*fval;
            break;
        case F_DATE: {
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            unsigned long now = millis();
            // Simple date: days since epoch, not calendar date
            // Return system uptime-based representation
            int days = now / 86400000;
            int years = 1970 + days / 365;
            int rem = days % 365;
            int months = rem / 30 + 1;
            if (months > 12) months = 12;
            int mdays = rem % 30 + 1;
            sprintf(*sval, "%02d-%02d-%04d", mdays, months, years);
            break;
        }
        case F_TIME: {
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            unsigned long now = millis();
            int totalSec = (now / 1000) % 86400;
            int h = totalSec / 3600;
            int m = (totalSec % 3600) / 60;
            int s = totalSec % 60;
            sprintf(*sval, "%02d:%02d:%02d", h, m, s);
            break;
        }
        case F_TAB: {
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            int target = arg1Ival - 1;
            if (target < 0) target = 0;
            int spaces = target - MMCharPos + 1;
            if (spaces <= 0) spaces = 1;
            if (spaces > STRINGSIZE - 1) spaces = STRINGSIZE - 1;
            memset(*sval, ' ', spaces);
            (*sval)[spaces] = '\0';
            break;
        }
        case F_SPC: {
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            int n = arg1Ival;
            if (n < 0) n = 0;
            if (n > STRINGSIZE - 1) n = STRINGSIZE - 1;
            memset(*sval, ' ', n);
            (*sval)[n] = '\0';
            break;
        }
        case F_FORMAT: {
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            const char* fmt = hasArg2 ? arg2Sval : "%g";
            // Use integer or float based on format specifier
            if (strchr(fmt, 'd') || strchr(fmt, 'x') || strchr(fmt, 'X') ||
                strchr(fmt, 'u') || strchr(fmt, 'c') || strchr(fmt, 'o'))
                sprintf(*sval, fmt, arg1Ival);
            else
                sprintf(*sval, fmt, numVal);
            break;
        }
        case F_SCHANGE: {
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            if (arg1Type == T_STR && arg2Type == T_STR && arg3Type == T_STR) {
                char *src = arg1Sval;
                char *find = arg2Sval;
                char *repl = arg3Sval;
                int srcLen = strlen(src);
                int findLen = strlen(find);
                int replLen = strlen(repl);
                int di = 0;
                int si = 0;
                while (si < srcLen && di < STRINGSIZE - 1) {
                    if (findLen > 0 && strncmp(src + si, find, findLen) == 0 && 
                        (srcLen - si) >= findLen) {
                        for (int k = 0; k < replLen && di < STRINGSIZE - 1; k++)
                            (*sval)[di++] = repl[k];
                        si += findLen;
                    } else {
                        (*sval)[di++] = src[si++];
                    }
                }
                (*sval)[di] = '\0';
            } else {
                (*sval)[0] = '\0';
            }
            break;
        }
        case F_GPIO_PIN:
            *itype = T_INT;
            *ival = digitalRead(arg1Ival);
            *fval = *ival;
            break;
        case F_ADC:
            *itype = T_INT;
            *ival = analogRead(arg1Ival);
            *fval = *ival;
            break;
        case F_PULSIN: {
            int state = hasArg2 ? arg2Ival : HIGH;
            *itype = T_INT;
            *ival = pulseIn(arg1Ival, state ? HIGH : LOW);
            *fval = *ival;
            break;
        }
        case F_TOUCH:
            *itype = T_INT;
            *ival = touchRead(arg1Ival);
            *fval = *ival;
            break;
        case F_EVAL: {
            *itype = T_STR;
            if (arg1Type == T_STR && arg1Sval) {
                int t2, i2; float f2; char *s2;
                char *copy = (char*)malloc(strlen(arg1Sval)+1);
                strcpy(copy, arg1Sval);
                char *p = copy;
                if (MMBasic_EvaluateExpression(&p, &t2, &i2, &f2, &s2)==0) {
                    if (t2 == T_STR) *sval = s2;
                    else {
                        *sval = MMBasic_GetTempString();
                        if (t2 == T_FLOAT) sprintf(*sval, "%g", f2);
                        else sprintf(*sval, "%d", i2);
                    }
                }
                free(copy);
            }
            break;
        }
        case F_BASE: {
            *itype = T_STR;
            *sval = MMBasic_GetTempString();
            int b = arg1Ival; if (b<2) b=2; if (b>36) b=36;
            unsigned long long n = hasArg2 ? (unsigned long long)arg2Ival : 0;
            char tmp[65]; int di=0;
            do {
                int d = n % b;
                tmp[di++] = (d<10) ? ('0'+d) : ('A'+d-10);
                n /= b;
            } while (n>0 && di<64);
            // Reverse
            for (int i=0; i<di; i++) (*sval)[i] = tmp[di-1-i];
            (*sval)[di] = 0;
            break;
        }
        case F_RGB:
            *itype = T_INT;
            *ival = ((arg1Ival & 0xF8) << 8) | ((arg2Ival & 0xFC) << 3) | ((arg3Ival & 0xF8) >> 3);
            *fval = *ival;
            break;
        case F_POINT:
            *itype = T_INT;
            *ival = M5Cardputer.Display.readPixel(arg1Ival, arg2Ival) & 0xFFFF;
            *fval = *ival;
            break;
        default:
            MMBasic_Error(ERR_SYNTAX, "Unknown function");
            return 1;
    }
    
    return 0;
}

// Public expression evaluator wrapper
int MMBasic_EvaluateExpression(char **expr, int *itype, int *ival, float *fval, char **sval) {
    return EvaluateExpression(expr, itype, ival, fval, sval);
}

// ============================================================================



