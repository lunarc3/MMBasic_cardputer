/*
 * MMBasic.h - MMBasic Core Header
 * 
 * This file defines the core structures and functions for MMBasic
 */

#ifndef MMBASIC_H
#define MMBASIC_H

#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>
#include "MMBasic_Config.h"

// Token definitions
#define C_BASETOKEN 0x80
#define C_CMD       0x80
#define C_FUNC      0xC0
#define C_OPERATOR  0xF0

// Command tokens (0x80-0xBF for commands, 0xC0-0xFF for functions in table)
#define C_INVALID   0x00
#define C_PRINT     0x80
#define C_INPUT     0x81
#define C_IF        0x82
#define C_THEN      0x83
#define C_ELSE      0x84
#define C_ENDIF     0x85
#define C_FOR       0x86
#define C_TO        0x87
#define C_STEP      0x88
#define C_NEXT      0x89
#define C_GOTO      0x8A
#define C_GOSUB     0x8B
#define C_RETURN    0x8C
#define C_END       0x8D
#define C_LET       0x8E
#define C_DIM       0x8F
#define C_REM       0x90
#define C_REM2      0x91
#define C_DATA      0x92
#define C_READ      0x93
#define C_RESTORE   0x94
#define C_SWAP      0x95
#define C_CLS       0x96
#define C_LOCATE    0x97
#define C_COLOR     0x98
#define C_LINE      0x99
#define C_CIRCLE    0x9A
#define C_RECT      0x9B
#define C_PIXEL     0x9C
#define C_DRAW      0x9D
#define C_SAVE      0x9E
#define C_LOAD      0x9F
#define C_FILES     0xA0
#define C_RUN       0xA1
#define C_LIST      0xA2
#define C_NEW       0xA3
#define C_OPTION    0xA4
#define C_SUB       0xA5
#define C_ENDSUB    0xA6
#define C_FUNCTION  0xA7
#define C_ENDFUN    0xA8
#define C_EXIT      0xA9
#define C_SELECT    0xAA
#define C_CASE      0xAB
#define C_ENDSELECT 0xAC
#define C_DO        0xAD
#define C_LOOP      0xAE
#define C_CONTINUE  0xAF
#define C_ERROR     0xB0
#define C_ON        0xB1
#define C_OPEN      0xB2
#define C_CLOSE     0xB3
#define C_SEEK      0xB4
#define C_PRINT_HASH 0xB5
#define C_INPUT_HASH 0xB6
#define C_INC       0xB7
#define C_PAUSE     0xB8
#define C_RANDOMIZE 0xB9
#define C_POKE      0xBA
#define C_CONST     0xBB
#define C_WHILE     0xBC
#define C_WEND      0xBD
#define C_KILL      0xBE
#define C_MKDIR     0xBF
#define C_RMDIR     0xC0
#define C_CHDIR     0xC1
#define C_COPY      0xC2
#define C_RENAME    0xC3
#define C_DRIVE     0xC4
#define C_SETPIN    0xC5
#define C_DIGOUT    0xC6
#define C_PWMCMD    0xC7
#define C_I2C       0xC8
#define C_SPI       0xC9
#define C_IR        0xCA
#define C_SERVO     0xCB
#define C_PORTCMD   0xCC
#define C_PULSE     0xCD
#define C_BOX       0xCE
#define C_TEXT      0xCF
#define C_TRIANGLE  0xD0
#define C_CLEAR     0xD1
#define C_ERASE     0xD2
#define C_REDIM     0xD3
#define C_TRACE     0xD4
#define C_SORTCMD   0xD5
#define C_CALL      0xD6
#define C_TURTLE    0xD7
#define C_SPRITE    0xD8
#define C_VAR       0xD9

// Function tokens (values are unique IDs, dispatched via C_FUNC type in table)
#define F_ABS       0x01
#define F_INT       0x02
#define F_SGN       0x03
#define F_SQR       0x04
#define F_SIN       0x05
#define F_COS       0x06
#define F_TAN       0x07
#define F_ATN       0x08
#define F_LOG       0x09
#define F_EXP       0x0A
#define F_RND       0x0B
#define F_PEEK      0x0C
#define F_LEN       0x0D
#define F_VAL       0x0E
#define F_ASC       0x0F
#define F_CHR       0x10
#define F_STR       0x11
#define F_LEFT      0x12
#define F_RIGHT     0x13
#define F_MID       0x14
#define F_INSTR     0x15
#define F_STRING    0x16
#define F_SPACE     0x17
#define F_UPPER     0x18
#define F_LOWER     0x19
#define F_TRIM      0x1A
#define F_HEX       0x1B
#define F_OCT       0x1C
#define F_BIN       0x1D
#define F_DATE      0x1E
#define F_TIME      0x1F
#define F_EOF       0x20
#define F_LOF       0x21
#define F_LOC       0x22
#define F_NOT       0x23
#define F_MIN       0x24
#define F_MAX       0x25
#define F_PI        0x26
#define F_TAB       0x27
#define F_SPC       0x28
#define F_POINT     0x29
#define F_RGB       0x2A
#define F_DEG       0x2B
#define F_RAD       0x2C
#define F_ACOS      0x2D
#define F_ASIN      0x2E
#define F_ATAN2     0x2F
#define F_FORMAT    0x30
#define F_SCHANGE   0x31
#define F_GPIO_PIN  0x32
#define F_ADC       0x33
#define F_PULSIN    0x34
#define F_TOUCH     0x35
#define F_EVAL      0x36
#define F_BASE      0x37

// Operator tokens (used internally in expression evaluator, not in token table)
#define OP_PLUS      0x01
#define OP_MINUS     0x02
#define OP_MULTIPLY  0x03
#define OP_DIVIDE    0x04
#define OP_MOD       0x05
#define OP_POWER     0x06
#define OP_AND       0x07
#define OP_OR        0x08
#define OP_XOR       0x09
#define OP_NOT       0x0A
#define OP_EQ        0x0B
#define OP_NE        0x0C
#define OP_LT        0x0D
#define OP_GT        0x0E
#define OP_LE        0x0F
#define OP_GE        0x10
#define OP_SHL       0x11
#define OP_SHR       0x12
#define OP_DIVINT    0x13

// Variable types
#define T_NOTYPE    0
#define T_INT       1
#define T_STR       2
#define T_FLOAT     3
#define T_ARRAY     4

// Error codes
#define ERR_NONE        0
#define ERR_SYNTAX      1
#define ERR_UNEXPECTED  2
#define ERR_UNKNOWN_CMD 3
#define ERR_ARGUMENT    4
#define ERR_DIV_ZERO    5
#define ERR_OVERFLOW    6
#define ERR_OUT_MEMORY  7
#define ERR_STACK_FULL  8
#define ERR_STACK_EMPTY 9
#define ERR_FOR_NEXT    10
#define ERR_GOSUB       11
#define ERR_RETURN      12
#define ERR_FILE_IO     13
#define ERR_TYPE        14
#define ERR_BOUNDS      15
#define ERR_INVALID     16

// Variable structure
struct s_vartbl {
    char name[MAXVARLEN + 1];   // Variable name
    char type;                  // Variable type
    union {
        int ival;               // Integer value
        float fval;             // Float value
        char *sval;             // String value
    } val;
    int array;                  // Array dimensions (0 = not array)
    int *arr;                   // Array data pointer
};

// Function/subroutine structure
struct s_subfun {
    char name[MAXVARLEN + 1];   // Function name
    char *addr;                 // Address in program memory
    char type;                  // Return type
    int nargs;                  // Number of arguments
};

// Token table structure
struct s_tokentbl {
    char *name;                 // Token name
    unsigned char type;         // Token type
    unsigned char token;        // Token value
    void (*func)(void);         // Function pointer
};

// Global variables
extern char *progmem;           // Program memory
extern int progsize;            // Program size
extern char *progptr;           // Current program pointer
extern char inpbuf[];           // Input buffer
extern char tknbuf[];           // Token buffer
extern struct s_vartbl *vartbl; // Variable table
extern int varcnt;              // Variable count
extern struct s_subfun subfun[];// Subroutine table
extern int subfunct;            // Subroutine count
extern jmp_buf mark;            // Error recovery jump buffer
extern int MMCharPos;           // Character position
extern volatile int MMAbort;    // Abort flag
extern char BreakKey;           // Break key

// Core functions
void MMBasic_Init(void);
void MMBasic_Reset(void);
void MMBasic_Execute(char *line);
void MMBasic_Error(int error, char *msg);
void MMBasic_SaveProgram(char *filename);
void MMBasic_LoadProgram(char *filename);
void MMBasic_ListProgram(void);
void MMBasic_RunProgram(void);
void MMBasic_StoreLine(int linenum, char *line);

// Parser functions
char *MMBasic_GetToken(char *line, char *token);
int MMBasic_Tokenise(char *source, char *dest);
char *MMBasic_Detokenise(char *source, char *dest);
int MMBasic_GetCommand(char *token);
int MMBasic_GetFunction(char *token);
int MMBasic_GetOperator(char *token);

// Variable functions
void MMBasic_ClearVariables(void);
int MMBasic_FindVariable(char *name);
int MMBasic_CreateVariable(char *name, char type);
void MMBasic_SetVariable(int index, int ival, float fval, char *sval);
int MMBasic_GetVariableInt(int index);
float MMBasic_GetVariableFloat(int index);
char *MMBasic_GetVariableString(int index);

// Expression evaluator
int MMBasic_EvaluateExpression(char **expr, int *itype, int *ival, float *fval, char **sval);

// Command implementations
void MMBasic_CmdPrint(void);
void MMBasic_CmdInput(void);
void MMBasic_CmdIf(void);
void MMBasic_CmdFor(void);
void MMBasic_CmdNext(void);
void MMBasic_CmdGoto(void);
void MMBasic_CmdGosub(void);
void MMBasic_CmdReturn(void);
void MMBasic_CmdEnd(void);
void MMBasic_CmdLet(void);
void MMBasic_CmdImplicitLet(char *line);
void MMBasic_CmdDim(void);
void MMBasic_CmdRem(void);
void MMBasic_CmdCls(void);
void MMBasic_CmdLocate(void);
void MMBasic_CmdColor(void);
void MMBasic_CmdLine(void);
void MMBasic_CmdCircle(void);
void MMBasic_CmdRect(void);
void MMBasic_CmdPixel(void);
void MMBasic_CmdSave(void);
void MMBasic_CmdLoad(void);
void MMBasic_CmdFiles(void);
void MMBasic_CmdRun(void);
void MMBasic_CmdList(void);
void MMBasic_CmdNew(void);
void MMBasic_CmdOpen(void);
void MMBasic_CmdClose(void);
void MMBasic_CmdSeek(void);
void MMBasic_CmdSwap(void);
void MMBasic_CmdInc(void);
void MMBasic_CmdPause(void);
void MMBasic_CmdRandomize(void);
void MMBasic_CmdPoke(void);
void MMBasic_CmdConst(void);
void MMBasic_CmdRead(void);
void MMBasic_CmdRestore(void);
void MMBasic_CmdWhile(void);
void MMBasic_CmdWend(void);
void MMBasic_CmdSelect(void);
void MMBasic_CmdCase(void);
void MMBasic_CmdEndSelect(void);
void MMBasic_SkipToNextCase(void);
void MMBasic_SkipToEndSelect(void);
void MMBasic_CmdDo(void);
void MMBasic_CmdLoop(void);
void MMBasic_CmdContinue(void);
void MMBasic_CmdExit(void);
void MMBasic_CmdOption(void);
void MMBasic_CmdKill(void);
void MMBasic_CmdMkdir(void);
void MMBasic_CmdRmdir(void);
void MMBasic_CmdChdir(void);
void MMBasic_CmdCopy(void);
void MMBasic_CmdRename(void);
void MMBasic_CmdDrive(void);
void MMBasic_CmdSetpin(void);
void MMBasic_CmdDigout(void);
void MMBasic_CmdPwm(void);
void MMBasic_CmdI2c(void);
void MMBasic_CmdSpi(void);
void MMBasic_CmdIr(void);
void MMBasic_CmdServo(void);
void MMBasic_CmdPort(void);
void MMBasic_CmdPulse(void);
void MMBasic_CmdBox(void);
void MMBasic_CmdText(void);
void MMBasic_CmdTriangle(void);
void MMBasic_CmdClear(void);
void MMBasic_CmdErase(void);
void MMBasic_CmdRedim(void);
void MMBasic_CmdError(void);
void MMBasic_CmdTrace(void);
void MMBasic_CmdSort(void);
void MMBasic_CmdCall(void);
void MMBasic_CmdTurtle(void);
void MMBasic_CmdSprite(void);
void MMBasic_CmdVar(void);
void MMBasic_CmdSubFunDef(void);
void MMBasic_CmdEndSubFun(void);
void MMBasic_CmdOn(void);

// File I/O functions
int MMBasic_FuncEOF(int fnbr);
int MMBasic_FuncLOF(int fnbr);
int MMBasic_FuncLOC(int fnbr);

// Multi-line IF commands
void MMBasic_CmdIfEnhanced(void);
void MMBasic_CmdElse(void);
void MMBasic_CmdEndif(void);

// RGB function
uint16_t MMBasic_RGB(int r, int g, int b);

// Function implementations
int MMBasic_FuncAbs(int val);
int MMBasic_FuncInt(float val);
int MMBasic_FuncSgn(int val);
float MMBasic_FuncSqr(float val);
float MMBasic_FuncSin(float val);
float MMBasic_FuncCos(float val);
float MMBasic_FuncTan(float val);
float MMBasic_FuncAtn(float val);
float MMBasic_FuncLog(float val);
float MMBasic_FuncExp(float val);
int MMBasic_FuncRnd(void);
int MMBasic_FuncLen(char *str);
float MMBasic_FuncVal(char *str);
int MMBasic_FuncAsc(char *str);
char *MMBasic_FuncChr(int val);
char *MMBasic_FuncStr(float val);
char *MMBasic_FuncLeft(char *str, int len);
char *MMBasic_FuncRight(char *str, int len);
char *MMBasic_FuncMid(char *str, int start, int len);
int MMBasic_FuncInstr(char *str, char *search);
char *MMBasic_FuncString(int len, char ch);
char *MMBasic_FuncSpace(int len);
char *MMBasic_FuncUpper(char *str);
char *MMBasic_FuncLower(char *str);
char *MMBasic_FuncTrim(char *str);
char *MMBasic_FuncHex(int val);
char *MMBasic_FuncOct(int val);
char *MMBasic_FuncBin(int val);

#endif // MMBASIC_H
