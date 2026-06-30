#include "MMBasic.h"
#include "HAL.h"
#include <Wire.h>
#include <math.h>
#include <ctype.h>
void ScanSubFunDefs(void);
// External globals from MMBasic.cpp
// Struct definitions
struct s_forstack {
    int lineIdx;
    char varname[33];
    int varindex, toval, stepval;
};
struct s_filehandle { File file; bool inUse; bool isCom; int comPort; };
struct s_shadow { int varIndex; char type; int ival; float fval; char *sval; int array; int *arr; bool wasConst; };
struct s_subfun_entry { char name[33]; int startLine, endLine, paramCount; char paramNames[8][33]; bool isFunction; int returnLine; };
struct s_select { int matchValue; bool matched; };

extern char *progmem;
extern int progsize;
extern char *progptr;
extern char inpbuf[];
extern char tknbuf[];
extern struct s_vartbl *vartbl;
extern int varcnt;
extern struct s_subfun subfun[];
extern int subfunct;
extern jmp_buf mark;
extern int MMCharPos;
extern volatile int MMAbort;
extern char BreakKey;
extern char *currentLine;
extern int currentLineIndex;
extern bool flowControlActive;
extern bool traceOn;
extern int linecnt;
extern char *lines[1000];
extern int lineNumbers[1000];
#define MAX_LINES 1000
#define MAX_SHADOW 128
extern int forstackptr;
extern s_forstack forstack[16];
extern int gosubstack[16];
extern int gosubstackptr;
extern int whilestack[16];
extern int whilestackptr;
extern int dostack[16];
extern int dostackptr;
extern s_filehandle fileTable[5];
extern int currentDrive;
extern int dataLineIdx;
extern int dataOffset;
extern bool *varIsConst;
extern s_subfun_entry subFunTable[128];
extern int subFunCount;
extern int subFunCallReturnIdx;
extern s_shadow shadowStack[128];
extern int shadowPtr;
extern int shadowBase[16];
extern int shadowBaseSP;
extern int subFunRetStack[16];
extern s_select selectstack[8];
extern int selectptr;
extern char strpool[16][256];
extern int strpoolindex;
// Command Implementations
// ============================================================================

// PRINT command
void MMBasic_CmdPrint(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Check for PRINT# (output to file)
    int fnbr = 0;
    if (*line == '#') {
        line++;
        while (*line == ' ') line++;
        int numType, numIval;
        float numFval;
        char *numSval;
        if (MMBasic_EvaluateExpression(&line, &numType, &numIval, &numFval, &numSval)) return;
        fnbr = numIval;
        if (fnbr < 1 || fnbr > MAXOPENFILES || !fileTable[fnbr].inUse) {
            MMBasic_Error(ERR_FILE_IO, "File not open");
            return;
        }
        // Skip comma after file number
        while (*line == ' ') line++;
        if (*line == ',') line++;
    }
    
    // Handle empty PRINT (just newline)
    while (*line == ' ') line++;
    if (*line == '\0' || *line == '\n' || *line == '\r') {
        if (fnbr) {
            if (fileTable[fnbr].isCom) {
                HardwareSerial *s = (fileTable[fnbr].comPort==1)?&Serial1:&Serial2;
                s->println("");
            } else { fileTable[fnbr].file.println(""); }
        } else {
            HAL_Display_Newline();
        }
        return;
    }
    
    while (*line != '\0' && *line != '\n' && *line != '\r') {
        // Skip whitespace
        while (*line == ' ') line++;
        
        if (*line == '\0' || *line == '\n' || *line == '\r') break;
        
        // Check for semicolon or comma
        if (*line == ';') {
            line++;
            continue;
        }
        if (*line == ',') {
            if (fnbr) {
                fileTable[fnbr].file.print('\t');
            } else {
                int pos = MMCharPos;
                int nextTab = ((pos / 10) + 1) * 10;
                while (pos < nextTab) {
                    HAL_Display_Print(" ");
                    pos++;
                }
            }
            line++;
            continue;
        }
        
        // Evaluate expression
        int itype, ival;
        float fval;
        char *sval;
        
        if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) {
            return; // Error
        }
        
        // Print the value
        if (fnbr) {
            HardwareSerial *ser = fileTable[fnbr].isCom ? 
                ((fileTable[fnbr].comPort==1)?&Serial1:&Serial2) : NULL;
            switch (itype) {
                case T_INT:
                    if (ser) ser->print(ival); else fileTable[fnbr].file.print(ival);
                    break;
                case T_FLOAT:
                    if (ser) ser->print(fval); else fileTable[fnbr].file.print(fval);
                    break;
                case T_STR:
                    if (ser) ser->print(sval); else if (sval) fileTable[fnbr].file.print(sval);
                    break;
            }
        } else {
            switch (itype) {
                case T_INT: {
                    char buf[16];
                    sprintf(buf, "%d", ival);
                    HAL_Display_Print(buf);
                    break;
                }
                case T_FLOAT: {
                    char buf[32];
                    sprintf(buf, "%g", fval);
                    HAL_Display_Print(buf);
                    break;
                }
                case T_STR:
                    if (sval != NULL) {
                        HAL_Display_Print(sval);
                    }
                    break;
            }
        }
    }
    
    // Check if line ends with semicolon (suppress newline)
    char *end = line;
    while (*end == ' ') end++;
    if (*end == ';') {
        // Don't print newline
    } else {
        if (fnbr) {
            if (fileTable[fnbr].isCom) {
                HardwareSerial *s = (fileTable[fnbr].comPort==1)?&Serial1:&Serial2;
                s->println("");
            } else { fileTable[fnbr].file.println(""); }
        } else {
            HAL_Display_Newline();
        }
    }
}

// INPUT command
void MMBasic_CmdInput(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Check for INPUT# (read from file)
    int fnbr = 0;
    char prompt[STRINGSIZE] = "? ";
    
    if (*line == '#') {
        line++;
        while (*line == ' ') line++;
        int numType, numIval;
        float numFval;
        char *numSval;
        if (MMBasic_EvaluateExpression(&line, &numType, &numIval, &numFval, &numSval)) return;
        fnbr = numIval;
        if (fnbr < 1 || fnbr > MAXOPENFILES || !fileTable[fnbr].inUse) {
            MMBasic_Error(ERR_FILE_IO, "File not open");
            return;
        }
        // Skip comma
        while (*line == ' ') line++;
        if (*line == ',') line++;
    } else {
        // Check for prompt string (console input only)
        if (*line == '"') {
            line++;
            int i = 0;
            while (*line != '"' && *line != '\0' && i < STRINGSIZE - 1) {
                prompt[i++] = *line++;
            }
            prompt[i] = '\0';
            if (*line == '"') line++;
            
            // Check for semicolon after prompt
            while (*line == ' ') line++;
            if (*line == ';') {
                line++;
            }
        }
    }
    
    // Get variable name(s)
    while (*line == ' ') line++;
    
    if (fnbr) {
        // INPUT# from file - read a line and parse into variables
        while (*line != '\0' && *line != '\n' && *line != '\r') {
            while (*line == ' ') line++;
            if (*line == '\0' || *line == '\n' || *line == '\r') break;
            
            char varName[MAXVARLEN + 2];
            int i = 0;
            while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
                   (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
                if (i < MAXVARLEN + 1) varName[i++] = *line;
                line++;
            }
            varName[i] = '\0';
            
            for (int j = 0; varName[j]; j++) {
                if (varName[j] >= 'a' && varName[j] <= 'z') varName[j] -= 32;
            }
            
            // Skip comma between variables
            while (*line == ' ') line++;
            if (*line == ',') line++;
            
            if (varName[0] == '\0') continue;
            
            // Read next value from file
            char fileBuf[STRINGSIZE];
            int bi = 0;
            char ch;
            // Skip leading whitespace
            while (fileTable[fnbr].file.available()) {
                ch = fileTable[fnbr].file.read();
                if (ch != ' ' && ch != '\t') {
                    fileBuf[bi++] = ch;
                    break;
                }
            }
            if (bi == 0 && !fileTable[fnbr].file.available()) break;
            
            // Read until comma, newline, or end
            while (fileTable[fnbr].file.available() && bi < STRINGSIZE - 1) {
                ch = fileTable[fnbr].file.peek();
                if (ch == ',' || ch == '\n' || ch == '\r') break;
                fileBuf[bi++] = fileTable[fnbr].file.read();
            }
            // Skip the comma separator if present
            if (fileTable[fnbr].file.available()) {
                ch = fileTable[fnbr].file.peek();
                if (ch == ',') fileTable[fnbr].file.read();
            }
            // Trim trailing whitespace
            while (bi > 0 && (fileBuf[bi-1] == ' ' || fileBuf[bi-1] == '\r')) bi--;
            fileBuf[bi] = '\0';
            
            // Find or create variable
            int varIdx = MMBasic_FindVariable(varName);
            if (varIdx < 0) {
                char type = T_INT;
                if (varName[strlen(varName) - 1] == '$') type = T_STR;
                varIdx = MMBasic_CreateVariable(varName, type);
                if (varIdx < 0) return;
            }
            
            switch (vartbl[varIdx].type) {
                case T_INT:
                    vartbl[varIdx].val.ival = atoi(fileBuf);
                    break;
                case T_FLOAT:
                    vartbl[varIdx].val.fval = atof(fileBuf);
                    break;
                case T_STR:
                    strncpy(vartbl[varIdx].val.sval, fileBuf, STRINGSIZE - 1);
                    break;
            }
        }
    } else {
        // Console INPUT - supports multiple variables: INPUT A, B, C$
        
        // Collect all variable names first
        char varNames[16][MAXVARLEN + 2]; // Max 16 variables
        int varCount = 0;
        
        while (*line != '\0' && *line != '\n' && *line != '\r') {
            while (*line == ' ') line++;
            if (*line == '\0' || *line == '\n' || *line == '\r') break;
            
            char *vn = varNames[varCount];
            int i = 0;
            while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
                   (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
                if (i < MAXVARLEN + 1) vn[i++] = *line;
                line++;
            }
            vn[i] = '\0';
            
            for (int j = 0; vn[j]; j++) {
                if (vn[j] >= 'a' && vn[j] <= 'z') vn[j] -= 32;
            }
            
            if (vn[0] != '\0') varCount++;
            if (varCount >= 16) break;
            
            while (*line == ' ') line++;
            if (*line == ',') line++;
        }
        
        if (varCount == 0) {
            MMBasic_Error(ERR_SYNTAX, "Expected variable");
            return;
        }
        
        // Display prompt once
        HAL_Display_Print(prompt);
        
        // Get one line of input
        char input[STRINGSIZE];
        HAL_Keyboard_GetLine(input, STRINGSIZE);
        
        // Parse input into values, split by commas
        char *inp = input;
        for (int v = 0; v < varCount; v++) {
            char value[STRINGSIZE];
            int vi = 0;
            
            // Skip leading spaces
            while (*inp == ' ') inp++;
            
            // If quoted string
            if (*inp == '"') {
                inp++;
                while (*inp != '"' && *inp != '\0' && vi < STRINGSIZE - 1) {
                    value[vi++] = *inp++;
                }
                if (*inp == '"') inp++;
            } else {
                // Read until comma or end
                while (*inp != '\0' && *inp != ',' && vi < STRINGSIZE - 1) {
                    value[vi++] = *inp++;
                }
                // Trim trailing spaces
                while (vi > 0 && value[vi-1] == ' ') vi--;
            }
            value[vi] = '\0';
            
            // Skip comma separator
            while (*inp == ' ') inp++;
            if (*inp == ',') inp++;
            
            // Find or create variable
            int varIdx = MMBasic_FindVariable(varNames[v]);
            if (varIdx < 0) {
                char type = T_INT;
                int nlen = strlen(varNames[v]);
                if (nlen > 0 && varNames[v][nlen - 1] == '$') type = T_STR;
                varIdx = MMBasic_CreateVariable(varNames[v], type);
                if (varIdx < 0) return;
            }
            
            // Assign value
            switch (vartbl[varIdx].type) {
                case T_INT:
                    vartbl[varIdx].val.ival = atoi(value);
                    break;
                case T_FLOAT:
                    vartbl[varIdx].val.fval = atof(value);
                    break;
                case T_STR:
                    strncpy(vartbl[varIdx].val.sval, value, STRINGSIZE - 1);
                    break;
            }
        }
    }
}

// LET command (explicit)
void MMBasic_CmdLet(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get variable name
    char varName[MAXVARLEN + 2];
    int i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) varName[i++] = *line;
        line++;
    }
    varName[i] = '\0';
    
    // Convert to uppercase
    for (int j = 0; varName[j]; j++) {
        if (varName[j] >= 'a' && varName[j] <= 'z') {
            varName[j] = varName[j] - 'a' + 'A';
        }
    }
    
    if (varName[0] == '\0') {
        MMBasic_Error(ERR_SYNTAX, "Expected variable");
        return;
    }
    
    // Skip whitespace and expect =
    while (*line == ' ') line++;
    if (*line != '=') {
        MMBasic_Error(ERR_SYNTAX, "Expected =");
        return;
    }
    line++;
    
    // Evaluate expression
    int itype, ival;
    float fval;
    char *sval;
    
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) {
        return; // Error
    }
    
    // Find or create variable
    int varIdx = MMBasic_FindVariable(varName);
    if (varIdx < 0) {
        char type = T_INT;
        if (varName[strlen(varName) - 1] == '$') type = T_STR;
        varIdx = MMBasic_CreateVariable(varName, type);
        if (varIdx < 0) return;
    }
    
    // Check const
    if (varIsConst[varIdx]) {
        MMBasic_Error(ERR_SYNTAX, "Cannot change constant");
        return;
    }
    
    // Set value
    MMBasic_SetVariable(varIdx, ival, fval, sval);
}

// Implicit LET (variable = expression without LET keyword)
void MMBasic_CmdImplicitLet(char *line) {
    // This is called when the command is not recognized
    // Try to parse as variable = expression
    
    // Save position
    char *start = line;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get variable name
    char varName[MAXVARLEN + 2];
    int i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) varName[i++] = *line;
        line++;
    }
    varName[i] = '\0';
    
    // Convert to uppercase
    for (int j = 0; varName[j]; j++) {
        if (varName[j] >= 'a' && varName[j] <= 'z') {
            varName[j] = varName[j] - 'a' + 'A';
        }
    }
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Check for = sign
    if (*line != '=') {
        MMBasic_Error(ERR_UNKNOWN_CMD, NULL);
        return;
    }
    line++;
    
    // Evaluate expression
    int itype, ival;
    float fval;
    char *sval;
    
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) {
        return; // Error
    }
    
    // Find or create variable
    int varIdx = MMBasic_FindVariable(varName);
    if (varIdx < 0) {
        char type = T_INT;
        if (varName[strlen(varName) - 1] == '$') type = T_STR;
        varIdx = MMBasic_CreateVariable(varName, type);
        if (varIdx < 0) return;
    }
    
    // Check const
    if (varIsConst[varIdx]) {
        MMBasic_Error(ERR_SYNTAX, "Cannot change constant");
        return;
    }
    
    // Set value
    MMBasic_SetVariable(varIdx, ival, fval, sval);
}

// IF command
void MMBasic_CmdIf(void) {
    char *line = currentLine;
    
    // Evaluate condition
    int itype, ival;
    float fval;
    char *sval;
    
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) {
        return; // Error
    }
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Check for THEN (case-insensitive)
    if ((line[0] == 'T' || line[0] == 't') && (line[1] == 'H' || line[1] == 'h') &&
        (line[2] == 'E' || line[2] == 'e') && (line[3] == 'N' || line[3] == 'n')) {
        line += 4;
        while (*line == ' ') line++;
        
        if (ival) {
            // Execute THEN part
            MMBasic_Execute(line);
        }
    } else {
        // Simple IF without THEN
        if (ival) {
            MMBasic_Execute(line);
        }
    }
}

// FOR command
void MMBasic_CmdFor(void) {
    char *line = currentLine;
    
    // Get variable name
    while (*line == ' ') line++;
    
    char varName[MAXVARLEN + 2];
    int i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) varName[i++] = *line;
        line++;
    }
    varName[i] = '\0';
    
    // Convert to uppercase
    for (int j = 0; varName[j]; j++) {
        if (varName[j] >= 'a' && varName[j] <= 'z') {
            varName[j] = varName[j] - 'a' + 'A';
        }
    }
    
    // Expect =
    while (*line == ' ') line++;
    if (*line != '=') {
        MMBasic_Error(ERR_SYNTAX, "Expected =");
        return;
    }
    line++;
    
    // Get start value
    int startType, startIval;
    float startFval;
    char *startSval;
    if (MMBasic_EvaluateExpression(&line, &startType, &startIval, &startFval, &startSval)) return;
    
    // Expect TO or to (case-insensitive)
    while (*line == ' ') line++;
    if ((line[0] != 'T' && line[0] != 't') || (line[1] != 'O' && line[1] != 'o')) {
        MMBasic_Error(ERR_SYNTAX, "Expected TO");
        return;
    }
    line += 2;
    
    // Get end value
    int endType, endIval;
    float endFval;
    char *endSval;
    if (MMBasic_EvaluateExpression(&line, &endType, &endIval, &endFval, &endSval)) return;
    
    // Get optional STEP
    int stepIval = 1;
    float stepFval = 1.0;
    while (*line == ' ') line++;
    if ((line[0] == 'S' || line[0] == 's') && (line[1] == 'T' || line[1] == 't') &&
        (line[2] == 'E' || line[2] == 'e') && (line[3] == 'P' || line[3] == 'p')) {
        line += 4;
        int stepType;
        char *stepSval;
        if (MMBasic_EvaluateExpression(&line, &stepType, &stepIval, &stepFval, &stepSval)) return;
    }
    
    // Find or create variable
    int varIdx = MMBasic_FindVariable(varName);
    if (varIdx < 0) {
        varIdx = MMBasic_CreateVariable(varName, T_INT);
        if (varIdx < 0) return;
    }
    
    // Set initial value
    MMBasic_SetVariable(varIdx, startIval, startFval, startSval);
    
    // Push to FOR stack
    if (forstackptr >= MAXFORLOOPS) {
        MMBasic_Error(ERR_STACK_FULL, "FOR stack full");
        return;
    }
    
    forstack[forstackptr].lineIdx = currentLineIndex + 1; // Loop body starts at next line
    strcpy(forstack[forstackptr].varname, varName);
    forstack[forstackptr].varindex = varIdx;
    forstack[forstackptr].toval = endIval;
    forstack[forstackptr].stepval = stepIval;
    forstackptr++;
}

// NEXT command
void MMBasic_CmdNext(void) {
    char *line = currentLine;
    
    // Get optional variable name
    while (*line == ' ') line++;
    
    char varName[MAXVARLEN + 2] = "";
    if (*line != '\0' && *line != '\n' && *line != '\r') {
        int i = 0;
        while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
               (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
            if (i < MAXVARLEN + 1) varName[i++] = *line;
            line++;
        }
        varName[i] = '\0';
        
        // Convert to uppercase
        for (int j = 0; varName[j]; j++) {
            if (varName[j] >= 'a' && varName[j] <= 'z') {
                varName[j] = varName[j] - 'a' + 'A';
            }
        }
    }
    
    // Find matching FOR
    if (forstackptr == 0) {
        MMBasic_Error(ERR_FOR_NEXT, "NEXT without FOR");
        return;
    }
    
    // If variable name specified, find matching FOR
    int stackIdx = forstackptr - 1;
    if (varName[0] != '\0') {
        // Search backwards for matching variable
        for (stackIdx = forstackptr - 1; stackIdx >= 0; stackIdx--) {
            if (strcmp(forstack[stackIdx].varname, varName) == 0) {
                break;
            }
        }
        if (stackIdx < 0) {
            MMBasic_Error(ERR_FOR_NEXT, "NEXT without matching FOR");
            return;
        }
    }
    
    // Increment variable
    int varIdx = forstack[stackIdx].varindex;
    int newVal = vartbl[varIdx].val.ival + forstack[stackIdx].stepval;
    vartbl[varIdx].val.ival = newVal;
    
    // Check if loop should continue
    int continueLoop = 0;
    if (forstack[stackIdx].stepval > 0) {
        continueLoop = (newVal <= forstack[stackIdx].toval);
    } else {
        continueLoop = (newVal >= forstack[stackIdx].toval);
    }
    
    if (continueLoop) {
        // Loop back to line after FOR
        currentLineIndex = forstack[stackIdx].lineIdx;
        currentLine = lines[currentLineIndex];
        flowControlActive = true;
    } else {
        // Loop finished, remove from stack
        forstackptr = stackIdx;
    }
}

// GOTO command
void MMBasic_CmdGoto(void) {
    char *line = currentLine;
    
    // Get line number
    while (*line == ' ') line++;
    
    if (*line < '0' || *line > '9') {
        MMBasic_Error(ERR_SYNTAX, "Expected line number");
        return;
    }
    
    int targetLine = atoi(line);
    
    // Find the line
    for (int i = 0; i < linecnt; i++) {
        if (lineNumbers[i] == targetLine) {
            currentLineIndex = i;
            currentLine = lines[i];
            flowControlActive = true;
            return;
        }
    }
    
    MMBasic_Error(ERR_SYNTAX, "Line not found");
}

// GOSUB command
void MMBasic_CmdGosub(void) {
    char *line = currentLine;
    
    // Get line number
    while (*line == ' ') line++;
    
    if (*line < '0' || *line > '9') {
        MMBasic_Error(ERR_SYNTAX, "Expected line number");
        return;
    }
    
    int targetLine = atoi(line);
    
    // Push return line index (line after GOSUB)
    if (gosubstackptr >= MAXGOSUB) {
        MMBasic_Error(ERR_STACK_FULL, "GOSUB stack full");
        return;
    }
    
    gosubstack[gosubstackptr++] = currentLineIndex + 1;
    
    // Find the target line
    for (int i = 0; i < linecnt; i++) {
        if (lineNumbers[i] == targetLine) {
            currentLineIndex = i;
            currentLine = lines[i];
            flowControlActive = true;
            return;
        }
    }
    
    MMBasic_Error(ERR_SYNTAX, "Line not found");
}

// ON GOTO/GOSUB command
void MMBasic_CmdOn(void) {
    char *line = currentLine;
    
    // Evaluate the expression
    int itype, ival;
    float fval;
    char *sval;
    
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) return;
    int index = (itype == T_INT) ? ival : (int)fval;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Determine GOTO or GOSUB
    int isGosub = 0;
    if (strncmp(line, "GOTO", 4) == 0 && !isalpha(line[4])) {
        line += 4;
    } else if (strncmp(line, "GOSUB", 5) == 0 && !isalpha(line[5])) {
        line += 5;
        isGosub = 1;
    } else {
        MMBasic_Error(ERR_SYNTAX, "Expected GOTO or GOSUB");
        return;
    }
    
    // Parse comma-separated line numbers
    int targets[32];
    int count = 0;
    
    while (*line && count < 32) {
        while (*line == ' ') line++;
        if (*line < '0' || *line > '9') break;
        targets[count++] = atoi(line);
        while (*line >= '0' && *line <= '9') line++;
        while (*line == ' ') line++;
        if (*line == ',') line++;
    }
    
    // If index is 0 or > count, fall through to next line
    if (index <= 0 || index > count) return;
    
    int targetLine = targets[index - 1];
    
    // For GOSUB, push return address
    if (isGosub) {
        if (gosubstackptr >= MAXGOSUB) {
            MMBasic_Error(ERR_STACK_FULL, "GOSUB stack full");
            return;
        }
        gosubstack[gosubstackptr++] = currentLineIndex + 1;
    }
    
    // Find the target line
    for (int i = 0; i < linecnt; i++) {
        if (lineNumbers[i] == targetLine) {
            currentLineIndex = i;
            currentLine = lines[i];
            flowControlActive = true;
            return;
        }
    }
    
    MMBasic_Error(ERR_SYNTAX, "Line not found");
}

// RETURN command
void MMBasic_CmdReturn(void) {
    // SUB/FUNCTION return: restore shadow stack
    if (shadowBaseSP > 0) {
        shadowBaseSP--;
        while (shadowPtr > shadowBase[shadowBaseSP]) {
            shadowPtr--;
            int vi = shadowStack[shadowPtr].varIndex;
            if (vi >= 0) {
                if (vartbl[vi].type==T_STR && vartbl[vi].val.sval) {
                    free(vartbl[vi].val.sval); vartbl[vi].val.sval=NULL;
                }
                vartbl[vi].type = shadowStack[shadowPtr].type;
                vartbl[vi].val.ival = shadowStack[shadowPtr].ival;
                vartbl[vi].val.fval = shadowStack[shadowPtr].fval;
                vartbl[vi].array = shadowStack[shadowPtr].array;
                vartbl[vi].arr = shadowStack[shadowPtr].arr;
                varIsConst[vi] = shadowStack[shadowPtr].wasConst;
                if (shadowStack[shadowPtr].sval) {
                    vartbl[vi].val.sval = shadowStack[shadowPtr].sval;
                }
            }
        }
        int ret = -1;
        if (shadowBaseSP < 16) {
            ret = subFunRetStack[shadowBaseSP];
            subFunRetStack[shadowBaseSP] = -1;
        }
        if (ret < 0) { currentLineIndex++; } // No return addr, advance
        else {
            currentLineIndex = ret + 1;
            currentLine = (currentLineIndex < linecnt) ? lines[currentLineIndex] : NULL;
        }
        flowControlActive = true;
        return;
    }
    // Standard GOSUB return
    if (gosubstackptr == 0) {
        MMBasic_Error(ERR_RETURN, "RETURN without GOSUB");
        return;
    }
    currentLineIndex = gosubstack[--gosubstackptr] + 1;
    currentLine = (currentLineIndex < linecnt) ? lines[currentLineIndex] : NULL;
    flowControlActive = true;
}

// END command
void MMBasic_CmdEnd(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    if ((line[0]=='S'||line[0]=='s')&&(line[1]=='U'||line[1]=='u')&&(line[2]=='B'||line[2]=='b')) {
        MMBasic_CmdEndSubFun(); return;
    }
    if ((line[0]=='F'||line[0]=='f')&&(line[1]=='U'||line[1]=='u')&&(line[2]=='N'||line[2]=='n')) {
        MMBasic_CmdEndSubFun(); return;
    }
    if ((line[0]=='S'||line[0]=='s')&&(line[1]=='E'||line[1]=='e')&&(line[2]=='L'||line[2]=='l')) {
        MMBasic_CmdEndSelect(); return;
    }
    // In a running program, exit cleanly
    if (currentLineIndex >= 0 && currentLineIndex < linecnt) {
        currentLineIndex = linecnt;
        flowControlActive = true;
        return;
    }
    // Direct mode: longjmp to command prompt
    longjmp(mark, 1);
}

// SAVE command
void MMBasic_CmdSave(void) {
    char *line = currentLine;
    
    // Get filename
    while (*line == ' ') line++;
    
    char filename[FILENAME_LENGTH];
    if (*line == '"') {
        line++;
        int i = 0;
        while (*line != '"' && *line != '\0' && i < FILENAME_LENGTH - 1) {
            filename[i++] = *line++;
        }
        filename[i] = '\0';
    } else {
        int i = 0;
        while (*line != ' ' && *line != '\0' && i < FILENAME_LENGTH - 1) {
            filename[i++] = *line++;
        }
        filename[i] = '\0';
    }
    
    MMBasic_SaveProgram(filename);
}

// LOAD command
void MMBasic_CmdLoad(void) {
    char *line = currentLine;
    
    // Get filename
    while (*line == ' ') line++;
    
    char filename[FILENAME_LENGTH];
    if (*line == '"') {
        line++;
        int i = 0;
        while (*line != '"' && *line != '\0' && i < FILENAME_LENGTH - 1) {
            filename[i++] = *line++;
        }
        filename[i] = '\0';
    } else {
        int i = 0;
        while (*line != ' ' && *line != '\0' && i < FILENAME_LENGTH - 1) {
            filename[i++] = *line++;
        }
        filename[i] = '\0';
    }
    
    MMBasic_LoadProgram(filename);
}

// FILES command
void MMBasic_CmdFiles(void) {
    if (!HAL_SD_Init()) {
        HAL_Display_Println("SD card not available");
        return;
    }
    
    HAL_Display_Println("Files on SD card:");
    
    File root = HAL_SD_Open("/", "r");
    if (!root) {
        HAL_Display_Println("Failed to open root directory");
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            HAL_Display_Print("  [DIR] ");
            HAL_Display_Println(file.name());
        } else {
            HAL_Display_Print("  ");
            HAL_Display_Print(file.name());
            HAL_Display_Print("  ");
            char sizeStr[16];
            sprintf(sizeStr, "%d bytes", file.size());
            HAL_Display_Println(sizeStr);
        }
        file = root.openNextFile();
    }
    
    root.close();
}

// RUN command
void MMBasic_RunProgram(void) {
    if (linecnt == 0) {
        HAL_Display_Println("No program");
        return;
    }
    
    // Reset stacks
    forstackptr = 0;
    gosubstackptr = 0;
    whilestackptr = 0;
    dostackptr = 0;
    shadowBaseSP = 0;
    shadowPtr = 0;
    subFunCallReturnIdx = -1;
    
    // Scan for SUB/FUNCTION definitions
    ScanSubFunDefs();
    
    // Execute lines with flow control
    currentLineIndex = 0;
    while (currentLineIndex >= 0 && currentLineIndex < linecnt && !MMAbort) {
        currentLine = lines[currentLineIndex];
        flowControlActive = false;
        
        // TRACE: print line number
        if (traceOn) {
            char tb[16]; sprintf(tb, "[%d] ", lineNumbers[currentLineIndex]); HAL_Display_Print(tb);
        }
        
        // Set up error recovery
        if (setjmp(mark) == 0) {
            MMBasic_Execute(currentLine);
        } else {
            // Error occurred
            HAL_Display_Print(" in line ");
            char buf[16];
            sprintf(buf, "%d", lineNumbers[currentLineIndex]);
            HAL_Display_Println(buf);
            return;
        }
        
        // Advance unless a command changed the flow (GOTO, NEXT loop, etc.)
        if (!flowControlActive) {
            currentLineIndex++;
        }
    }
    
    MMAbort = 0;
}

// LIST command
void MMBasic_ListProgram(void) {
    if (linecnt == 0) {
        HAL_Display_Println("No program");
        return;
    }
    
    for (int i = 0; i < linecnt; i++) {
        char buf[16];
        sprintf(buf, "%d ", lineNumbers[i]);
        HAL_Display_Print(buf);
        HAL_Display_Print(lines[i]);
        HAL_Display_Newline();
    }
}

// Save program to file
void MMBasic_SaveProgram(char *filename) {
    if (!HAL_SD_Init()) {
        HAL_Display_Println("SD card not available");
        return;
    }
    
    // Ensure filename has leading /
    char fullPath[FILENAME_LENGTH + 2];
    if (filename[0] != '/') {
        fullPath[0] = '/';
        strncpy(fullPath + 1, filename, FILENAME_LENGTH - 1);
        fullPath[FILENAME_LENGTH] = '\0';
    } else {
        strncpy(fullPath, filename, FILENAME_LENGTH);
        fullPath[FILENAME_LENGTH] = '\0';
    }
    
    File file = HAL_SD_Open(fullPath, "w");
    if (!file) {
        HAL_Display_Println("Cannot create file");
        return;
    }
    
    // Write each line
    for (int i = 0; i < linecnt; i++) {
        char buf[16];
        sprintf(buf, "%d ", lineNumbers[i]);
        file.print(buf);
        file.println(lines[i]);
    }
    
    file.close();
    HAL_Display_Println("Program saved");
}

// Load program from file
void MMBasic_LoadProgram(char *filename) {
    if (!HAL_SD_Init()) {
        HAL_Display_Println("SD card not available");
        return;
    }
    
    // Ensure filename has leading /
    char fullPath[FILENAME_LENGTH + 2];
    if (filename[0] != '/') {
        fullPath[0] = '/';
        strncpy(fullPath + 1, filename, FILENAME_LENGTH - 1);
        fullPath[FILENAME_LENGTH] = '\0';
    } else {
        strncpy(fullPath, filename, FILENAME_LENGTH);
        fullPath[FILENAME_LENGTH] = '\0';
    }
    
    File file = HAL_SD_Open(fullPath, "r");
    if (!file) {
        HAL_Display_Println("File not found");
        return;
    }
    
    // Clear current program
    MMBasic_Reset();
    
    // Read lines
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0) {
            // Parse line number
            int spaceIdx = line.indexOf(' ');
            if (spaceIdx > 0) {
                String numStr = line.substring(0, spaceIdx);
                int linenum = numStr.toInt();
                String code = line.substring(spaceIdx + 1);
                
                // Store line
                char codeBuf[STRINGSIZE];
                code.toCharArray(codeBuf, STRINGSIZE);
                MMBasic_StoreLine(linenum, codeBuf);
            }
        }
    }
    
    file.close();
    HAL_Display_Println("Program loaded");
}

// ============================================================================
// Graphics Commands
// ============================================================================

// COLOR command - set foreground and background color
void MMBasic_CmdColor(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get foreground color
    int fgType, fgIval;
    float fgFval;
    char *fgSval;
    if (MMBasic_EvaluateExpression(&line, &fgType, &fgIval, &fgFval, &fgSval)) return;
    
    uint16_t fgColor = (uint16_t)fgIval;
    uint16_t bg = BLACK;
    
    // Check for comma (background color)
    while (*line == ' ') line++;
    if (*line == ',') {
        line++;
        int bgType, bgIval;
        float bgFval;
        char *bgSval;
        if (MMBasic_EvaluateExpression(&line, &bgType, &bgIval, &bgFval, &bgSval)) return;
        bg = (uint16_t)bgIval;
    }
    
    HAL_Display_SetTextColor(fgColor);
    HAL_Display_SetBgColor(bg);
}

// LOCATE command - set cursor position
void MMBasic_CmdLocate(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get X position
    int xType, xIval;
    float xFval;
    char *xSval;
    if (MMBasic_EvaluateExpression(&line, &xType, &xIval, &xFval, &xSval)) return;
    
    // Expect comma
    while (*line == ' ') line++;
    if (*line != ',') {
        MMBasic_Error(ERR_SYNTAX, "Expected ,");
        return;
    }
    line++;
    
    // Get Y position
    int yType, yIval;
    float yFval;
    char *ySval;
    if (MMBasic_EvaluateExpression(&line, &yType, &yIval, &yFval, &ySval)) return;
    
    M5Cardputer.Display.setCursor(xIval, yIval);
}

// LINE command - draw line
// LINE x1, y1, x2, y2 [, color]
void MMBasic_CmdLine(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    // Check for LINE INPUT
    if ((line[0]=='I'||line[0]=='i') && (line[1]=='N'||line[1]=='n') &&
        (line[2]=='P'||line[2]=='p') && (line[3]=='U'||line[3]=='u') &&
        (line[4]=='T'||line[4]=='t')) {
        line += 5;
        // LINE INPUT handling
        int fnbr = 0;
        while (*line == ' ') line++;
        if (*line == '#') { line++; while (*line==' ') line++;
            int t2, n; float f2; char *s2;
            if (MMBasic_EvaluateExpression(&line,&t2,&n,&f2,&s2)) return;
            fnbr = n;
            while (*line==' ') line++; if (*line==',') line++;
        }
        char prompt[STRINGSIZE] = "";
        if (*line == '"') { line++; int i=0;
            while (*line!='"'&&*line&&i<STRINGSIZE-1) prompt[i++]=*line++; prompt[i]=0;
            if (*line=='"') line++;
            while (*line==' ') line++; if (*line==';') line++;
        }
        while (*line == ' ') line++;
        char varName[MAXVARLEN+2]; int i=0;
        while ((*line>='A'&&*line<='Z')||(*line>='a'&&*line<='z')||*line=='_'||*line=='$') {
            if (i<MAXVARLEN+1) varName[i++]=*line; line++;
        } varName[i]=0;
        for (int j=0;varName[j];j++) if (varName[j]>='a'&&varName[j]<='z') varName[j]-=32;
        if (varName[0]==0) { MMBasic_Error(ERR_SYNTAX,"Expected string variable"); return; }
        
        char input[STRINGSIZE];
        if (fnbr) {
            // LINE INPUT #fnbr - read full line from file
            int bi = 0; char ch;
            while (fileTable[fnbr].file.available() && bi < STRINGSIZE-1) {
                ch = fileTable[fnbr].file.read();
                if (ch == '\n' || ch == '\r') break;
                input[bi++] = ch;
            }
            input[bi] = 0;
        } else {
            if (prompt[0]) HAL_Display_Print(prompt);
            HAL_Keyboard_GetLine(input, STRINGSIZE);
        }
        int idx = MMBasic_FindVariable(varName);
        if (idx<0) idx = MMBasic_CreateVariable(varName, T_STR);
        if (idx>=0 && vartbl[idx].type==T_STR)
            strncpy(vartbl[idx].val.sval, input, STRINGSIZE-1);
        return;
    }
    
    // Normal LINE drawing
    // Get x1
    int x1Type, x1Ival;
    float x1Fval;
    char *x1Sval;
    if (MMBasic_EvaluateExpression(&line, &x1Type, &x1Ival, &x1Fval, &x1Sval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get y1
    int y1Type, y1Ival;
    float y1Fval;
    char *y1Sval;
    if (MMBasic_EvaluateExpression(&line, &y1Type, &y1Ival, &y1Fval, &y1Sval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get x2
    int x2Type, x2Ival;
    float x2Fval;
    char *x2Sval;
    if (MMBasic_EvaluateExpression(&line, &x2Type, &x2Ival, &x2Fval, &x2Sval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get y2
    int y2Type, y2Ival;
    float y2Fval;
    char *y2Sval;
    if (MMBasic_EvaluateExpression(&line, &y2Type, &y2Ival, &y2Fval, &y2Sval)) return;
    
    // Get optional color
    uint16_t color = WHITE;
    while (*line == ' ') line++;
    if (*line == ',') {
        line++;
        int cType, cIval;
        float cFval;
        char *cSval;
        if (MMBasic_EvaluateExpression(&line, &cType, &cIval, &cFval, &cSval)) return;
        color = (uint16_t)cIval;
    }
    
    M5Cardputer.Display.drawLine(x1Ival, y1Ival, x2Ival, y2Ival, color);
}

// CIRCLE command - draw circle
// CIRCLE x, y, r [, color]
void MMBasic_CmdCircle(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get x
    int xType, xIval;
    float xFval;
    char *xSval;
    if (MMBasic_EvaluateExpression(&line, &xType, &xIval, &xFval, &xSval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get y
    int yType, yIval;
    float yFval;
    char *ySval;
    if (MMBasic_EvaluateExpression(&line, &yType, &yIval, &yFval, &ySval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get radius
    int rType, rIval;
    float rFval;
    char *rSval;
    if (MMBasic_EvaluateExpression(&line, &rType, &rIval, &rFval, &rSval)) return;
    
    // Get optional color
    uint16_t color = WHITE;
    while (*line == ' ') line++;
    if (*line == ',') {
        line++;
        int cType, cIval;
        float cFval;
        char *cSval;
        if (MMBasic_EvaluateExpression(&line, &cType, &cIval, &cFval, &cSval)) return;
        color = (uint16_t)cIval;
    }
    
    M5Cardputer.Display.drawCircle(xIval, yIval, rIval, color);
}

// RECT command - draw rectangle
// RECT x, y, w, h [, color]
void MMBasic_CmdRect(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get x
    int xType, xIval;
    float xFval;
    char *xSval;
    if (MMBasic_EvaluateExpression(&line, &xType, &xIval, &xFval, &xSval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get y
    int yType, yIval;
    float yFval;
    char *ySval;
    if (MMBasic_EvaluateExpression(&line, &yType, &yIval, &yFval, &ySval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get width
    int wType, wIval;
    float wFval;
    char *wSval;
    if (MMBasic_EvaluateExpression(&line, &wType, &wIval, &wFval, &wSval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get height
    int hType, hIval;
    float hFval;
    char *hSval;
    if (MMBasic_EvaluateExpression(&line, &hType, &hIval, &hFval, &hSval)) return;
    
    // Get optional color
    uint16_t color = WHITE;
    while (*line == ' ') line++;
    if (*line == ',') {
        line++;
        int cType, cIval;
        float cFval;
        char *cSval;
        if (MMBasic_EvaluateExpression(&line, &cType, &cIval, &cFval, &cSval)) return;
        color = (uint16_t)cIval;
    }
    
    M5Cardputer.Display.drawRect(xIval, yIval, wIval, hIval, color);
}

// PIXEL command - draw pixel
// PIXEL x, y [, color]
void MMBasic_CmdPixel(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get x
    int xType, xIval;
    float xFval;
    char *xSval;
    if (MMBasic_EvaluateExpression(&line, &xType, &xIval, &xFval, &xSval)) return;
    
    while (*line == ' ') line++;
    if (*line != ',') { MMBasic_Error(ERR_SYNTAX, "Expected ,"); return; }
    line++;
    
    // Get y
    int yType, yIval;
    float yFval;
    char *ySval;
    if (MMBasic_EvaluateExpression(&line, &yType, &yIval, &yFval, &ySval)) return;
    
    // Get optional color
    uint16_t color = WHITE;
    while (*line == ' ') line++;
    if (*line == ',') {
        line++;
        int cType, cIval;
        float cFval;
        char *cSval;
        if (MMBasic_EvaluateExpression(&line, &cType, &cIval, &cFval, &cSval)) return;
        color = (uint16_t)cIval;
    }
    
    M5Cardputer.Display.drawPixel(xIval, yIval, color);
}

// ============================================================================
// Multi-line IF/ELSE/ENDIF
// ============================================================================

// ============================================================================
// Multi-line IF/ELSE/ENDIF flow control
// ============================================================================

// Check if a line starts with a given keyword (case-insensitive)
static bool LineStartsWith(const char* line, const char* keyword) {
    while (*line == ' ' || *line == '\t') line++;
    int klen = strlen(keyword);
    for (int i = 0; i < klen; i++) {
        char c = line[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c != keyword[i]) return false;
    }
    // Make sure it's a full keyword (followed by space, EOL, or nothing)
    char after = line[klen];
    return after == ' ' || after == '\t' || after == '\0' || after == '\n' || after == '\r';
}

// Skip forward to matching ELSE or ENDIF, handling nested IF blocks.
// Updates currentLineIndex. Call when IF condition was false (skip to ELSE)
// or when IF was true and we hit ELSE (skip to ENDIF).
void SkipToElseOrEndif(bool stopAtElse) {
    int depth = 1;
    currentLineIndex++;
    flowControlActive = true;
    while (currentLineIndex < linecnt) {
        const char* l = lines[currentLineIndex];
        if (LineStartsWith(l, "IF")) {
            depth++;
        } else if (depth == 1 && stopAtElse && LineStartsWith(l, "ELSE")) {
            return; // Found ELSE at our level, execute from here
        } else if (LineStartsWith(l, "ENDIF")) {
            depth--;
            if (depth == 0) {
                return; // Found matching ENDIF, continue after it
            }
        } else if (depth == 1 && LineStartsWith(l, "ELSE")) {
            // Hit ELSE while not looking for it (shouldn't happen in well-formed code)
            depth--;
            if (depth == 0) return;
        }
        currentLineIndex++;
    }
}

// Enhanced IF command with multiline support
void MMBasic_CmdIfEnhanced(void) {
    char *line = currentLine;
    
    // Evaluate condition
    int itype, ival;
    float fval;
    char *sval;
    
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) {
        return; // Error
    }
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Check for THEN (case-insensitive)
    if ((line[0] == 'T' || line[0] == 't') && (line[1] == 'H' || line[1] == 'h') &&
        (line[2] == 'E' || line[2] == 'e') && (line[3] == 'N' || line[3] == 'n')) {
        line += 4;
        while (*line == ' ') line++;
        
        // Check if there's code after THEN on the same line
        if (*line != '\0' && *line != '\n' && *line != '\r') {
            // Single line IF...THEN...ELSE statement
            if (ival) {
                MMBasic_Execute(line);
            }
        } else {
            // Multiline IF...THEN
            if (!ival) {
                // Condition is false, skip to ELSE or ENDIF
                SkipToElseOrEndif(true);
            }
            // If true, just continue to next line normally
        }
    } else {
        // Simple IF without THEN
        if (ival) {
            MMBasic_Execute(line);
        }
    }
}

// ELSE command
void MMBasic_CmdElse(void) {
    // We reached ELSE, which means the IF part was true (we didn't skip)
    // So we need to skip to ENDIF
    SkipToElseOrEndif(false);
}

// ENDIF command
void MMBasic_CmdEndif(void) {
    // Nothing to do, just continue to next line
}

// ============================================================================
// Array Support
// ============================================================================

// DIM command - declare array
void MMBasic_CmdDim(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get variable name
    char varName[MAXVARLEN + 2];
    int i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) varName[i++] = *line;
        line++;
    }
    varName[i] = '\0';
    
    // Convert to uppercase
    for (int j = 0; varName[j]; j++) {
        if (varName[j] >= 'a' && varName[j] <= 'z') {
            varName[j] = varName[j] - 'a' + 'A';
        }
    }
    
    // Expect opening parenthesis
    while (*line == ' ') line++;
    if (*line != '(') {
        MMBasic_Error(ERR_SYNTAX, "Expected (");
        return;
    }
    line++;
    
    // Get array size
    int sizeType, sizeIval;
    float sizeFval;
    char *sizeSval;
    if (MMBasic_EvaluateExpression(&line, &sizeType, &sizeIval, &sizeFval, &sizeSval)) return;
    
    // Expect closing parenthesis
    while (*line == ' ') line++;
    if (*line != ')') {
        MMBasic_Error(ERR_SYNTAX, "Expected )");
        return;
    }
    line++;
    
    // Create variable as array
    int varIdx = MMBasic_FindVariable(varName);
    if (varIdx < 0) {
        char type = T_INT;
        if (varName[strlen(varName) - 1] == '$') type = T_STR;
        varIdx = MMBasic_CreateVariable(varName, type);
        if (varIdx < 0) return;
    }
    
    // Allocate array memory
    vartbl[varIdx].array = sizeIval;
    vartbl[varIdx].arr = (int *)malloc(sizeIval * sizeof(int));
    if (vartbl[varIdx].arr == NULL) {
        MMBasic_Error(ERR_OUT_MEMORY, "Cannot allocate array");
        return;
    }
    
    // Initialize array to zero
    memset(vartbl[varIdx].arr, 0, sizeIval * sizeof(int));
}

// ============================================================================
// File I/O Commands
// ============================================================================

// OPEN command
// OPEN filename FOR mode AS #filenumber
void MMBasic_CmdOpen(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    // Get filename
    char filename[FILENAME_LENGTH];
    if (*line == '"') {
        line++; int i=0;
        while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) filename[i++]=*line++; filename[i]=0;
        if (*line=='"') line++;
    } else { int i=0; while (*line!=' '&&*line&&i<FILENAME_LENGTH-1) filename[i++]=*line++; filename[i]=0; }
    
    // Check for COM port (PicoMite: OPEN "COM1: speed" AS #fnbr)
    if ((filename[0]=='C'||filename[0]=='c')&&(filename[1]=='O'||filename[1]=='o')&&
        (filename[2]=='M'||filename[2]=='m')) {
        int cn = filename[3]-'0';
        if (cn<1||cn>2) { MMBasic_Error(ERR_FILE_IO,"Only COM1:/COM2: supported"); return; }
        while (*line==' ') line++;
        if (*line==',') line++;  // skip comma after port name
        // Get baud rate
        int t2, baud; float f2; char *s2;
        if (MMBasic_EvaluateExpression(&line,&t2,&baud,&f2,&s2)) return;
        // Get AS and file number
        while (*line==' ') line++;
        if ((line[0]=='A'||line[0]=='a')&&(line[1]=='S'||line[1]=='s')) line+=2;
        while (*line==' ') line++;
        if (*line=='#') line++;
        int t3, fnbr; float f3; char *s3;
        if (MMBasic_EvaluateExpression(&line,&t3,&fnbr,&f3,&s3)) return;
        if (fnbr<1||fnbr>MAXOPENFILES) { MMBasic_Error(ERR_FILE_IO,"Invalid file number"); return; }
        if (fileTable[fnbr].inUse) { MMBasic_Error(ERR_FILE_IO,"File number already open"); return; }
        HardwareSerial *ser = (cn==1) ? &Serial1 : &Serial2;
        ser->begin(baud);
        fileTable[fnbr].isCom = true;
        fileTable[fnbr].comPort = cn;
        fileTable[fnbr].inUse = true;
        return;
    }
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Expect FOR (optional, also accept AS directly) - case-insensitive
    if ((line[0] == 'F' || line[0] == 'f') && (line[1] == 'O' || line[1] == 'o') &&
        (line[2] == 'R' || line[2] == 'r')) {
        line += 3;
    }
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get mode
    char modeStr[16];
    {
        int i = 0;
        while (*line != ' ' && *line != '\0' && *line != ',' && i < 15) {
            modeStr[i++] = *line++;
        }
        modeStr[i] = '\0';
        
        // Convert to uppercase
        for (int j = 0; modeStr[j]; j++) {
            if (modeStr[j] >= 'a' && modeStr[j] <= 'z') {
                modeStr[j] -= 32;
            }
        }
    }
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Handle quoted mode like "INPUT"
    if (modeStr[0] == '"') {
        int j = 0;
        int i = 1;
        while (modeStr[i] != '"' && modeStr[i] != '\0' && j < 15) {
            modeStr[j++] = modeStr[i++];
        }
        modeStr[j] = '\0';
    }
    
    // Expect AS (case-insensitive)
    while (*line == ' ') line++;
    if ((line[0] == 'A' || line[0] == 'a') && (line[1] == 'S' || line[1] == 's')) {
        line += 2;
    }
    
    // Get file number
    while (*line == ' ') line++;
    if (*line == '#') line++;
    
    int numType, numIval;
    float numFval;
    char *numSval;
    if (MMBasic_EvaluateExpression(&line, &numType, &numIval, &numFval, &numSval)) return;
    
    int fnbr = numIval;
    if (fnbr < 1 || fnbr > MAXOPENFILES) {
        MMBasic_Error(ERR_FILE_IO, "Invalid file number");
        return;
    }
    
    if (fileTable[fnbr].inUse) {
        MMBasic_Error(ERR_FILE_IO, "File number already open");
        return;
    }
    
    // Map MMBasic mode to Arduino SD mode
    const char* arduinoMode;
    if (strcmp(modeStr, "INPUT") == 0) {
        arduinoMode = FILE_READ;
    } else if (strcmp(modeStr, "OUTPUT") == 0) {
        arduinoMode = FILE_WRITE;
    } else if (strcmp(modeStr, "APPEND") == 0) {
        arduinoMode = FILE_APPEND;
    } else if (strcmp(modeStr, "RANDOM") == 0) {
        arduinoMode = FILE_WRITE;  // Read+write
    } else {
        MMBasic_Error(ERR_FILE_IO, "Invalid file mode");
        return;
    }
    
    if (!HAL_SD_Init()) {
        MMBasic_Error(ERR_FILE_IO, "SD card not available");
        return;
    }
    
    // For OUTPUT mode, remove existing file to ensure clean truncation
    if (strcmp(modeStr, "OUTPUT") == 0 && SD.exists(filename)) {
        SD.remove(filename);
    }
    
    fileTable[fnbr].file = SD.open(filename, arduinoMode);
    if (!fileTable[fnbr].file) {
        MMBasic_Error(ERR_FILE_IO, "Cannot open file");
        return;
    }
    
    fileTable[fnbr].inUse = true;
}

// CLOSE command
void MMBasic_CmdClose(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get file number
    if (*line == '#') line++;
    
    int numType, numIval;
    float numFval;
    char *numSval;
    if (MMBasic_EvaluateExpression(&line, &numType, &numIval, &numFval, &numSval)) return;
    
    int fnbr = numIval;
    if (fnbr < 1 || fnbr > MAXOPENFILES) {
        MMBasic_Error(ERR_FILE_IO, "Invalid file number");
        return;
    }
    
    if (!fileTable[fnbr].inUse) {
        MMBasic_Error(ERR_FILE_IO, "File not open");
        return;
    }
    
    if (fileTable[fnbr].isCom) {
        HardwareSerial *s = (fileTable[fnbr].comPort==1)?&Serial1:&Serial2;
        s->end();
    } else {
        fileTable[fnbr].file.close();
    }
    fileTable[fnbr].inUse = false;
    fileTable[fnbr].isCom = false;
}

// SEEK command
// SEEK #fnbr, position
void MMBasic_CmdSeek(void) {
    char *line = currentLine;
    
    // Skip whitespace
    while (*line == ' ') line++;
    
    // Get file number
    if (*line == '#') line++;
    
    int numType, numIval;
    float numFval;
    char *numSval;
    if (MMBasic_EvaluateExpression(&line, &numType, &numIval, &numFval, &numSval)) return;
    
    int fnbr = numIval;
        if (fnbr < 1 || fnbr > MAXOPENFILES || !fileTable[fnbr].inUse) {
            MMBasic_Error(ERR_FILE_IO, "File not open");
            return;
        }
        if (fileTable[fnbr].isCom) {
            // Skip comma, then output via Serial
            while (*line == ' ') line++;
            if (*line == ',') line++;
        }
    
    // Expect comma
    while (*line == ' ') line++;
    if (*line == ',') line++;
    
    if (MMBasic_EvaluateExpression(&line, &numType, &numIval, &numFval, &numSval)) return;
    
    int position = numIval;
    if (position < 0) position = 0;
    
    fileTable[fnbr].file.seek(position);
}

// POKE command - write byte to memory
// POKE addr, value
void MMBasic_CmdPoke(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    int t1, addr;
    float f1; char *s1;
    if (MMBasic_EvaluateExpression(&line, &t1, &addr, &f1, &s1)) return;
    
    while (*line == ' ') line++;
    if (*line == ',') line++;
    
    int t2, val;
    float f2; char *s2;
    if (MMBasic_EvaluateExpression(&line, &t2, &val, &f2, &s2)) return;
    
    *((unsigned char *)addr) = (unsigned char)(val & 0xFF);
}
// SWAP var1, var2
void MMBasic_CmdSwap(void) {
    char *line = currentLine;
    
    // Get first variable
    while (*line == ' ') line++;
    char name1[MAXVARLEN + 2];
    int i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) name1[i++] = *line;
        line++;
    }
    name1[i] = '\0';
    for (int j = 0; name1[j]; j++) if (name1[j] >= 'a' && name1[j] <= 'z') name1[j] -= 32;
    
    // Skip comma
    while (*line == ' ') line++;
    if (*line == ',') line++;
    
    // Get second variable
    while (*line == ' ') line++;
    char name2[MAXVARLEN + 2];
    i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) name2[i++] = *line;
        line++;
    }
    name2[i] = '\0';
    for (int j = 0; name2[j]; j++) if (name2[j] >= 'a' && name2[j] <= 'z') name2[j] -= 32;
    
    // Ensure both variables exist
    int idx1 = MMBasic_FindVariable(name1);
    int idx2 = MMBasic_FindVariable(name2);
    if (idx1 < 0) idx1 = MMBasic_CreateVariable(name1, T_INT);
    if (idx2 < 0) idx2 = MMBasic_CreateVariable(name2, T_INT);
    if (idx1 < 0 || idx2 < 0) return;
    
    // Swap the entire variable struct
    struct s_vartbl temp = vartbl[idx1];
    vartbl[idx1] = vartbl[idx2];
    vartbl[idx2] = temp;
    // Swap back the names to keep array vars with correct names
    char tmpname[MAXVARLEN + 1];
    strcpy(tmpname, vartbl[idx2].name);
    strcpy(vartbl[idx2].name, vartbl[idx1].name);
    strcpy(vartbl[idx1].name, tmpname);
}

// INC command - increment variable
// INC varname [, amount]
void MMBasic_CmdInc(void) {
    char *line = currentLine;
    
    while (*line == ' ') line++;
    char varName[MAXVARLEN + 2];
    int i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) varName[i++] = *line;
        line++;
    }
    varName[i] = '\0';
    for (int j = 0; varName[j]; j++) if (varName[j] >= 'a' && varName[j] <= 'z') varName[j] -= 32;
    
    // Get optional amount
    int amount = 1;
    while (*line == ' ') line++;
    if (*line == ',') {
        line++;
        int t; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &t, &amount, &f, &s)) return;
    }
    
    int idx = MMBasic_FindVariable(varName);
    if (idx < 0) idx = MMBasic_CreateVariable(varName, T_INT);
    if (idx < 0) return;
    
    if (vartbl[idx].type == T_FLOAT)
        vartbl[idx].val.fval += (float)amount;
    else
        vartbl[idx].val.ival += amount;
}

// PAUSE command - pause execution
// PAUSE ms
void MMBasic_CmdPause(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    int itype, ms;
    float fval; char *sval;
    if (MMBasic_EvaluateExpression(&line, &itype, &ms, &fval, &sval)) return;
    HAL_Delay(ms > 0 ? ms : 0);
}

// RANDOMIZE command - seed random number generator
// RANDOMIZE [seed]
void MMBasic_CmdRandomize(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    if (*line == '\0' || *line == '\n' || *line == '\r') {
        srand(millis());
    } else {
        int itype, seed;
        float fval; char *sval;
        if (MMBasic_EvaluateExpression(&line, &itype, &seed, &fval, &sval)) return;
        srand(seed);
    }
}

// CONST command - define a named constant
// CONST name = value
void MMBasic_CmdConst(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    char name[MAXVARLEN + 2];
    int i = 0;
    while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
           (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
        if (i < MAXVARLEN + 1) name[i++] = *line;
        line++;
    }
    name[i] = '\0';
    for (int j = 0; name[j]; j++) if (name[j] >= 'a' && name[j] <= 'z') name[j] -= 32;
    
    while (*line == ' ') line++;
    if (*line != '=') { MMBasic_Error(ERR_SYNTAX, "Expected ="); return; }
    line++;
    
    int itype, ival; float fval; char *sval;
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) return;
    
    char vtype = T_INT;
    if (itype == T_FLOAT) vtype = T_FLOAT;
    if (itype == T_STR) vtype = T_STR;
    int idx = MMBasic_CreateVariable(name, vtype);
    if (idx < 0) return;
    MMBasic_SetVariable(idx, ival, fval, sval);
    varIsConst[idx] = true;
}

// READ command - read from DATA statements
// READ var1, var2$, ...
void MMBasic_CmdRead(void) {
    char *line = currentLine;
    
    while (*line != '\0' && *line != '\n' && *line != '\r') {
        while (*line == ' ') line++;
        if (*line == '\0' || *line == '\n' || *line == '\r') break;
        
        char varName[MAXVARLEN + 2];
        int i = 0;
        while ((*line >= 'A' && *line <= 'Z') || (*line >= 'a' && *line <= 'z') || 
               (*line >= '0' && *line <= '9') || *line == '_' || *line == '$') {
            if (i < MAXVARLEN + 1) varName[i++] = *line;
            line++;
        }
        varName[i] = '\0';
        for (int j = 0; varName[j]; j++) if (varName[j] >= 'a' && varName[j] <= 'z') varName[j] -= 32;
        
        while (*line == ' ') line++;
        if (*line == ',') line++;
        if (varName[0] == '\0') continue;
        
        // Find next DATA value
        while (dataLineIdx < linecnt) {
            const char *dl = lines[dataLineIdx];
            while (*dl == ' ' || *dl == '\t') dl++;
            if (dl[0] == 'D' && dl[1] == 'A' && dl[2] == 'T' && dl[3] == 'A' &&
                (dl[4] == ' ' || dl[4] == '\t' || dl[4] == '\0')) {
                dl += 4;
                while (*dl == ' ' || *dl == '\t') dl++;
                char val[STRINGSIZE];
                int vi = 0;
                const char *sv = dl + dataOffset;
                while (*sv == ' ' || *sv == '\t') sv++;
                if (*sv == '\0') { dataLineIdx++; dataOffset = 0; continue; }
                if (*sv == '"') {
                    sv++;
                    while (*sv != '"' && *sv != '\0' && vi < STRINGSIZE - 1) val[vi++] = *sv++;
                    if (*sv == '"') sv++;
                } else {
                    while (*sv != ',' && *sv != '\0' && vi < STRINGSIZE - 1)
                        val[vi++] = *sv++;
                }
                while (vi > 0 && val[vi-1] == ' ') vi--;
                val[vi] = '\0';
                dataOffset = (int)(sv - dl);
                if (*sv == ',') { dataOffset++; }
                else { dataLineIdx++; dataOffset = 0; }
                
                int iv = MMBasic_FindVariable(varName);
                if (iv < 0) {
                    char t = (varName[strlen(varName)-1] == '$') ? T_STR : T_INT;
                    iv = MMBasic_CreateVariable(varName, t);
                }
                if (iv >= 0 && !varIsConst[iv]) {
                    if (vartbl[iv].type == T_STR)
                        strncpy(vartbl[iv].val.sval, val, STRINGSIZE - 1);
                    else
                        vartbl[iv].val.ival = atoi(val);
                }
                goto next_var;
            }
            dataLineIdx++;
        }
        MMBasic_Error(ERR_SYNTAX, "No DATA to read");
        return;
        next_var: ;
    }
}

// DO command - DO [WHILE|UNTIL condition] ... LOOP [WHILE|UNTIL condition]
void MMBasic_CmdDo(void) {
    // Check if re-entry (LOOP jumped back)
    int isReentry = (dostackptr > 0 && dostack[dostackptr - 1] == currentLineIndex);
    
    char *line = currentLine;
    while (*line == ' ') line++;
    
    int preWhile = 0, preUntil = 0;
    if ((line[0]=='W'||line[0]=='w') && (line[1]=='H'||line[1]=='h') &&
        (line[2]=='I'||line[2]=='i') && (line[3]=='L'||line[3]=='l') &&
        (line[4]=='E'||line[4]=='e')) {
        preWhile = 1; line += 5;
    } else if ((line[0]=='U'||line[0]=='u') && (line[1]=='N'||line[1]=='n') &&
               (line[2]=='T'||line[2]=='t') && (line[3]=='I'||line[3]=='i') &&
               (line[4]=='L'||line[4]=='l')) {
        preUntil = 1; line += 5;
    }
    
    int itype, ival; float fval; char *sval;
    if (preWhile || preUntil) {
        if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) return;
        if ((preWhile && !ival) || (preUntil && ival)) {
            // Condition false - exit loop
            if (isReentry) dostackptr--;
            int depth = 1;
            currentLineIndex++;
            while (currentLineIndex < linecnt) {
                const char *l = lines[currentLineIndex];
                while (*l == ' ' || *l == '\t') l++;
                if ((l[0]=='D'||l[0]=='d') && (l[1]=='O'||l[1]=='o')) {
                    while (*l && *l != ' ' && *l != '\t') l++;
                    depth++;
                } else if ((l[0]=='L'||l[0]=='l') && (l[1]=='O'||l[1]=='o') &&
                           (l[2]=='O'||l[2]=='o') && (l[3]=='P'||l[3]=='p')) {
                    depth--;
                    if (depth == 0) { currentLineIndex++; break; }
                }
                currentLineIndex++;
            }
            flowControlActive = true;
            return;
        }
    }
    
    // Push DO position on first entry only
    if (!isReentry) {
        if (dostackptr >= MAXFORLOOPS) { MMBasic_Error(ERR_STACK_FULL, "DO stack full"); return; }
        dostack[dostackptr++] = currentLineIndex;
    }
}

// CONTINUE command - skip to next iteration of current loop
void MMBasic_CmdContinue(void) {
    if (dostackptr > 0) {
        // DO loop: jump back to DO
        currentLineIndex = dostack[dostackptr - 1];
        currentLine = lines[currentLineIndex];
        flowControlActive = true;
    } else if (whilestackptr > 0) {
        // WHILE loop: jump back to WHILE (condition will be re-evaluated)
        currentLineIndex = whilestack[whilestackptr - 1];
        currentLine = lines[currentLineIndex];
        flowControlActive = true;
    } else if (forstackptr > 0) {
        // FOR loop: scan to NEXT at same level, re-execute NEXT
        int idx = forstackptr - 1;
        currentLineIndex++;
        while (currentLineIndex < linecnt) {
            const char *l = lines[currentLineIndex];
            while (*l == ' ' || *l == '\t') l++;
            if ((l[0]=='F'||l[0]=='f') && (l[1]=='O'||l[1]=='o') && (l[2]=='R'||l[2]=='r')) {
                idx++;
            } else if ((l[0]=='N'||l[0]=='n') && (l[1]=='E'||l[1]=='e') &&
                       (l[2]=='X'||l[2]=='x') && (l[3]=='T'||l[3]=='t')) {
                if (idx == forstackptr - 1) break;
            }
            currentLineIndex++;
        }
        flowControlActive = true;
    }
}

// EXIT command - exit current loop
// EXIT [FOR|DO|WHILE]
void MMBasic_CmdExit(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    if ((line[0]=='F'||line[0]=='f') && (line[1]=='O'||line[1]=='o') && (line[2]=='R'||line[2]=='r')) {
        // EXIT FOR: skip to after NEXT
        if (forstackptr == 0) { MMBasic_Error(ERR_SYNTAX, "EXIT FOR without FOR"); return; }
        forstackptr--;
        int depth = 1;
        currentLineIndex++;
        while (currentLineIndex < linecnt) {
            const char *l = lines[currentLineIndex];
            while (*l == ' ' || *l == '\t') l++;
            if ((l[0]=='F'||l[0]=='f') && (l[1]=='O'||l[1]=='o') && (l[2]=='R'||l[2]=='r')) depth++;
            else if ((l[0]=='N'||l[0]=='n') && (l[1]=='E'||l[1]=='e') &&
                     (l[2]=='X'||l[2]=='x') && (l[3]=='T'||l[3]=='t')) {
                depth--;
                if (depth == 0) { currentLineIndex++; break; }
            }
            currentLineIndex++;
        }
        flowControlActive = true;
    } else if ((line[0]=='D'||line[0]=='d') && (line[1]=='O'||line[1]=='o')) {
        // EXIT DO: pop DO stack, skip to after LOOP
        if (dostackptr == 0) { MMBasic_Error(ERR_SYNTAX, "EXIT DO without DO"); return; }
        dostackptr--;
        int depth = 1;
        currentLineIndex++;
        while (currentLineIndex < linecnt) {
            const char *l = lines[currentLineIndex];
            while (*l == ' ' || *l == '\t') l++;
            if ((l[0]=='D'||l[0]=='d') && (l[1]=='O'||l[1]=='o')) depth++;
            else if ((l[0]=='L'||l[0]=='l') && (l[1]=='O'||l[1]=='o') &&
                     (l[2]=='O'||l[2]=='o') && (l[3]=='P'||l[3]=='p')) {
                depth--;
                if (depth == 0) { currentLineIndex++; break; }
            }
            currentLineIndex++;
        }
        flowControlActive = true;
    } else if ((line[0]=='W'||line[0]=='w') && (line[1]=='H'||line[1]=='h')) {
        // EXIT WHILE: pop WHILE stack, skip to after WEND
        if (whilestackptr == 0) { MMBasic_Error(ERR_SYNTAX, "EXIT WHILE without WHILE"); return; }
        whilestackptr--;
        int depth = 1;
        currentLineIndex++;
        while (currentLineIndex < linecnt) {
            const char *l = lines[currentLineIndex];
            while (*l == ' ' || *l == '\t') l++;
            if ((l[0]=='W'||l[0]=='w') && (l[1]=='H'||l[1]=='h') &&
                (l[2]=='I'||l[2]=='i') && (l[3]=='L'||l[3]=='l') &&
                (l[4]=='E'||l[4]=='e')) depth++;
            else if ((l[0]=='W'||l[0]=='w') && (l[1]=='E'||l[1]=='e') &&
                     (l[2]=='N'||l[2]=='n') && (l[3]=='D'||l[3]=='d')) {
                depth--;
                if (depth == 0) { currentLineIndex++; break; }
            }
            currentLineIndex++;
        }
        flowControlActive = true;
    } else {
        // Bare EXIT (no specifier): exit innermost loop
        if (dostackptr > 0) {
            dostackptr--;
            int depth = 1;
            currentLineIndex++;
            while (currentLineIndex < linecnt) {
                const char *l = lines[currentLineIndex];
                while (*l == ' ' || *l == '\t') l++;
                if ((l[0]=='D'||l[0]=='d') && (l[1]=='O'||l[1]=='o')) depth++;
                else if ((l[0]=='L'||l[0]=='l') && (l[1]=='O'||l[1]=='o') &&
                         (l[2]=='O'||l[2]=='o') && (l[3]=='P'||l[3]=='p')) {
                    depth--;
                    if (depth == 0) { currentLineIndex++; break; }
                }
                currentLineIndex++;
            }
            flowControlActive = true;
        } else if (whilestackptr > 0) {
            whilestackptr--;
            int depth = 1;
            currentLineIndex++;
            while (currentLineIndex < linecnt) {
                const char *l = lines[currentLineIndex];
                while (*l == ' ' || *l == '\t') l++;
                if ((l[0]=='W'||l[0]=='w') && (l[1]=='H'||l[1]=='h')) depth++;
                else if ((l[0]=='W'||l[0]=='w') && (l[1]=='E'||l[1]=='e')) {
                    depth--;
                    if (depth == 0) { currentLineIndex++; break; }
                }
                currentLineIndex++;
            }
            flowControlActive = true;
        } else if (forstackptr > 0) {
            forstackptr--;
            int depth = 1;
            currentLineIndex++;
            while (currentLineIndex < linecnt) {
                const char *l = lines[currentLineIndex];
                while (*l == ' ' || *l == '\t') l++;
                if ((l[0]=='F'||l[0]=='f') && (l[1]=='O'||l[1]=='o')) depth++;
                else if ((l[0]=='N'||l[0]=='n') && (l[1]=='E'||l[1]=='e')) {
                    depth--;
                    if (depth == 0) { currentLineIndex++; break; }
                }
                currentLineIndex++;
            }
            flowControlActive = true;
        }
    }
}

// KILL command - delete a file
void MMBasic_CmdKill(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    char fname[FILENAME_LENGTH];
    if (*line == '"') { line++; int i=0; while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) fname[i++]=*line++; fname[i]=0; if (*line=='"') line++; }
    else { int i=0; while (*line!=' '&&*line&&i<FILENAME_LENGTH-1) fname[i++]=*line++; fname[i]=0; }
    char full[FILENAME_LENGTH+2];
    if (fname[0]!='/') { full[0]='/'; strcpy(full+1,fname); } else strcpy(full,fname);
    if (!HAL_SD_Init()) { HAL_Display_Println("SD card not available"); return; }
    if (SD.remove(full)) HAL_Display_Println("File deleted");
    else HAL_Display_Println("Cannot delete file");
}

// MKDIR command
void MMBasic_CmdMkdir(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    char dname[FILENAME_LENGTH];
    if (*line == '"') { line++; int i=0; while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) dname[i++]=*line++; dname[i]=0; if (*line=='"') line++; }
    else { int i=0; while (*line!=' '&&*line&&i<FILENAME_LENGTH-1) dname[i++]=*line++; dname[i]=0; }
    char full[FILENAME_LENGTH+2];
    if (dname[0]!='/') { full[0]='/'; strcpy(full+1,dname); } else strcpy(full,dname);
    if (!HAL_SD_Init()) { HAL_Display_Println("SD card not available"); return; }
    if (SD.mkdir(full)) HAL_Display_Println("Directory created");
    else HAL_Display_Println("Cannot create directory");
}

// RMDIR command
void MMBasic_CmdRmdir(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    char dname[FILENAME_LENGTH];
    if (*line == '"') { line++; int i=0; while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) dname[i++]=*line++; dname[i]=0; if (*line=='"') line++; }
    else { int i=0; while (*line!=' '&&*line&&i<FILENAME_LENGTH-1) dname[i++]=*line++; dname[i]=0; }
    char full[FILENAME_LENGTH+2];
    if (dname[0]!='/') { full[0]='/'; strcpy(full+1,dname); } else strcpy(full,dname);
    if (!HAL_SD_Init()) { HAL_Display_Println("SD card not available"); return; }
    if (SD.rmdir(full)) HAL_Display_Println("Directory removed");
    else HAL_Display_Println("Cannot remove directory");
}

// COPY command - COPY "source" TO "dest"
void MMBasic_CmdCopy(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    char src[FILENAME_LENGTH], dst[FILENAME_LENGTH];
    if (*line == '"') { line++; int i=0; while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) src[i++]=*line++; src[i]=0; if (*line=='"') line++; }
    else { int i=0; while (*line!=' '&&*line&&i<FILENAME_LENGTH-1) src[i++]=*line++; src[i]=0; }
    while (*line == ' ') line++;
    // Expect TO keyword
    if ((line[0]=='T'||line[0]=='t') && (line[1]=='O'||line[1]=='o')) line += 2;
    else { HAL_Display_Println("Expected TO"); return; }
    while (*line == ' ') line++;
    if (*line == '"') { line++; int i=0; while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) dst[i++]=*line++; dst[i]=0; if (*line=='"') line++; }
    else { int i=0; while (*line!=' '&&*line&&i<FILENAME_LENGTH-1) dst[i++]=*line++; dst[i]=0; }
    char fsrc[FILENAME_LENGTH+2], fdst[FILENAME_LENGTH+2];
    if (src[0]!='/') { fsrc[0]='/'; strcpy(fsrc+1,src); } else strcpy(fsrc,src);
    if (dst[0]!='/') { fdst[0]='/'; strcpy(fdst+1,dst); } else strcpy(fdst,dst);
    if (!HAL_SD_Init()) { HAL_Display_Println("SD card not available"); return; }
    File f1 = SD.open(fsrc, FILE_READ);
    if (!f1) { HAL_Display_Println("Source file not found"); return; }
    File f2 = SD.open(fdst, FILE_WRITE);
    if (!f2) { f1.close(); HAL_Display_Println("Cannot create destination"); return; }
    char buf[256]; int n;
    while ((n = f1.read((uint8_t*)buf, 256)) > 0) f2.write((uint8_t*)buf, n);
    f1.close(); f2.close();
    HAL_Display_Println("File copied");
}

// RENAME command - copy then delete (ESP32 SD rename unreliable)
void MMBasic_CmdRename(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    char oldn[FILENAME_LENGTH], newn[FILENAME_LENGTH];
    if (*line == '"') { line++; int i=0; while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) oldn[i++]=*line++; oldn[i]=0; if (*line=='"') line++; }
    else { int i=0; while (*line!=','&&*line!=' '&&*line&&i<FILENAME_LENGTH-1) oldn[i++]=*line++; oldn[i]=0; }
    while (*line == ' ' || *line == ',') line++;
    if (*line == '"') { line++; int i=0; while (*line!='"'&&*line&&i<FILENAME_LENGTH-1) newn[i++]=*line++; newn[i]=0; if (*line=='"') line++; }
    else { int i=0; while (*line!=' '&&*line&&i<FILENAME_LENGTH-1) newn[i++]=*line++; newn[i]=0; }
    char gold[FILENAME_LENGTH+2], gnew[FILENAME_LENGTH+2];
    if (oldn[0]!='/') { gold[0]='/'; strcpy(gold+1,oldn); } else strcpy(gold,oldn);
    if (newn[0]!='/') { gnew[0]='/'; strcpy(gnew+1,newn); } else strcpy(gnew,newn);
    if (!HAL_SD_Init()) { HAL_Display_Println("SD card not available"); return; }
    File f1 = SD.open(gold, FILE_READ);
    if (!f1) { HAL_Display_Println("Source file not found"); return; }
    File f2 = SD.open(gnew, FILE_WRITE);
    if (!f2) { f1.close(); HAL_Display_Println("Cannot create destination"); return; }
    char buf[256]; int n;
    while ((n = f1.read((uint8_t*)buf, 256)) > 0) f2.write((uint8_t*)buf, n);
    f1.close(); f2.close();
    SD.remove(gold);
    HAL_Display_Println("File renamed");
}

// CHDIR command
void MMBasic_CmdChdir(void) {
    HAL_Display_Println("CHDIR - stub (SD card uses flat root)");
}

// DRIVE command
void MMBasic_CmdDrive(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    if ((line[0]=='B'||line[0]=='b') && (line[1]==':'||line[1]==0)) {
        currentDrive = 1;
        HAL_Display_Println("Drive B: selected");
    } else if ((line[0]=='A'||line[0]=='a') && (line[1]==':'||line[1]==0)) {
        HAL_Display_Println("Drive A: not available (flash FS)");
    } else {
        HAL_Display_Println("Invalid drive");
    }
}
void MMBasic_CmdSetpin(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    int t, pin; float f; char *s;
    if (MMBasic_EvaluateExpression(&line, &t, &pin, &f, &s)) return;
    while (*line == ' ') line++; if (*line == ',') line++;
    int t2, cfg;
    if (MMBasic_EvaluateExpression(&line, &t2, &cfg, &f, &s)) return;
    switch (cfg) {
        case 0: pinMode(pin, INPUT); break;
        case 1: pinMode(pin, OUTPUT); digitalWrite(pin, LOW); break;
        case 2: pinMode(pin, INPUT); break;
        case 3: pinMode(pin, INPUT_PULLUP); break;
        case 4: pinMode(pin, INPUT); break;
        case 5: break;
        default: HAL_Display_Println("Invalid mode"); break;
    }
}
void MMBasic_CmdDigout(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    int t, pin; float f; char *s;
    if (MMBasic_EvaluateExpression(&line, &t, &pin, &f, &s)) return;
    while (*line == ' ') line++; if (*line == ',') line++;
    int t2, val;
    if (MMBasic_EvaluateExpression(&line, &t2, &val, &f, &s)) return;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val ? HIGH : LOW);
}
void MMBasic_CmdPwm(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    int t, pin; float f; char *s;
    if (MMBasic_EvaluateExpression(&line, &t, &pin, &f, &s)) return;
    while (*line == ' ') line++; if (*line == ',') line++;
    int t2, freq;
    if (MMBasic_EvaluateExpression(&line, &t2, &freq, &f, &s)) return;
    while (*line == ' ') line++; if (*line == ',') line++;
    int t3, duty;
    if (MMBasic_EvaluateExpression(&line, &t3, &duty, &f, &s)) return;
    if (duty < 0) duty = 0; if (duty > 100) duty = 100;
    ledcAttach(pin, freq, 8);
    ledcWrite(pin, (duty * 255) / 100);
}
// I2C command: I2C OPEN speed | I2C CLOSE | I2C SEND addr, data... | I2C RECEIVE addr, count
static bool i2cOpen = false;
void MMBasic_CmdI2c(void) {
    char *line = currentLine; while (*line == ' ') line++;
    char sub[16]; int i = 0;
    while (*line!=' '&&*line!='\0'&&i<15) sub[i++]=*line++; sub[i]=0;
    for (int j=0;sub[j];j++) if (sub[j]>='a'&&sub[j]<='z') sub[j]-=32;
    while (*line==' ') line++;
    if (strcmp(sub,"OPEN")==0) {
        int t, speed; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&t,&speed,&f,&s)) return;
        Wire.begin(); Wire.setClock(speed*1000); i2cOpen = true;
    } else if (strcmp(sub,"CLOSE")==0) {
        Wire.end(); i2cOpen = false;
    } else if (strcmp(sub,"SEND")==0) {
        if (!i2cOpen) { HAL_Display_Println("I2C not open"); return; }
        int t, addr; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&t,&addr,&f,&s)) return;
        while (*line==','||*line==' ') line++;
        Wire.beginTransmission(addr);
        while (*line) {
            int v; float f2; char *s2;
            if (!((*line>='0'&&*line<='9')||*line=='-')) break;
            if (MMBasic_EvaluateExpression(&line,&t,&v,&f2,&s2)) return;
            Wire.write(v & 0xFF);
            while (*line==','||*line==' ') line++;
        }
        Wire.endTransmission();
    } else if (strcmp(sub,"RECEIVE")==0) {
        if (!i2cOpen) { HAL_Display_Println("I2C not open"); return; }
        int t, addr; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&t,&addr,&f,&s)) return;
        while (*line==','||*line==' ') line++;
        int t2, count;
        if (MMBasic_EvaluateExpression(&line,&t2,&count,&f,&s)) return;
        Wire.requestFrom((uint8_t)addr,(uint8_t)count);
        while (Wire.available()&&count>0) {
            char b[8]; sprintf(b,"%02X ",Wire.read()); HAL_Display_Print(b); count--;
        }
        HAL_Display_Newline();
    } else { HAL_Display_Println("I2C: unknown sub-command"); }
}
// SPI command: SPI OPEN speed, mode, cs | SPI CLOSE | SPI READ count | SPI WRITE data...
static SPIClass *spiDev = NULL; static int spiCsPin = -1;
void MMBasic_CmdSpi(void) {
    char *line = currentLine; while (*line == ' ') line++;
    char sub[16]; int i = 0;
    while (*line!=' '&&*line!='\0'&&i<15) sub[i++]=*line++; sub[i]=0;
    for (int j=0;sub[j];j++) if (sub[j]>='a'&&sub[j]<='z') sub[j]-=32;
    while (*line==' ') line++;
    if (strcmp(sub,"OPEN")==0) {
        int t, speed; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&t,&speed,&f,&s)) return;
        while (*line==','||*line==' ') line++;
        int t2, mode;
        if (MMBasic_EvaluateExpression(&line,&t2,&mode,&f,&s)) return;
        while (*line==','||*line==' ') line++;
        int t3, cs;
        if (MMBasic_EvaluateExpression(&line,&t3,&cs,&f,&s)) return;
        spiDev = &SPI; spiCsPin = cs;
        pinMode(cs,OUTPUT); digitalWrite(cs,HIGH);
        spiDev->begin(40,39,14,cs);
        spiDev->beginTransaction(SPISettings(speed*1000000,MSBFIRST,mode));
    } else if (strcmp(sub,"CLOSE")==0) {
        if (spiDev) { spiDev->endTransaction(); spiDev->end(); spiDev=NULL; }
    } else if (strcmp(sub,"WRITE")==0) {
        if (!spiDev) { HAL_Display_Println("SPI not open"); return; }
        int tt; float ff; char *ss;
        digitalWrite(spiCsPin,LOW);
        while (*line) { while (*line==','||*line==' ') line++; int v;
            if (!((*line>='0'&&*line<='9')||*line=='-')) break;
            if (MMBasic_EvaluateExpression(&line,&tt,&v,&ff,&ss)) { digitalWrite(spiCsPin,HIGH); return; }
            spiDev->transfer(v & 0xFF); }
        digitalWrite(spiCsPin,HIGH);
    } else if (strcmp(sub,"READ")==0) {
        if (!spiDev) { HAL_Display_Println("SPI not open"); return; }
        int t, count; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&t,&count,&f,&s)) return;
        digitalWrite(spiCsPin,LOW);
        for (int i=0;i<count;i++) { char b[8]; sprintf(b,"%02X ",spiDev->transfer(0xFF)); HAL_Display_Print(b); }
        digitalWrite(spiCsPin,HIGH); HAL_Display_Newline();
    } else { HAL_Display_Println("SPI: unknown sub-command"); }
}
// Serial via OPEN: OPEN "COM1: speed" AS #fnbr
void MMBasic_CmdIr(void) {
    char *line = currentLine; while (*line == ' ') line++;
    char sub[16]; int i=0;
    while (*line!=' '&&*line!='\0'&&i<15) sub[i++]=*line++; sub[i]=0;
    for (int j=0;sub[j];j++) if (sub[j]>='a'&&sub[j]<='z') sub[j]-=32;
    while (*line==' ') line++;
    if (strcmp(sub,"SEND")==0) {
        int t, data; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&t,&data,&f,&s)) return;
        int freq = 38000;
        while (*line==','||*line==' ') line++;
        if (*line>='0'&&*line<='9') {
            int t2; float f2; char *s2;
            if (MMBasic_EvaluateExpression(&line,&t2,&freq,&f2,&s2)) return;
        }
        // Bit-bang IR at carrier freq on GPIO 44 (Cardputer IR LED)
        uint8_t irPin = 44;
        pinMode(irPin, OUTPUT);
        for (int b=0; b<8; b++) {
            if (data & (1<<b)) {
                for (int c=0; c<22; c++) { // Mark
                    digitalWrite(irPin, HIGH); delayMicroseconds(26);
                    digitalWrite(irPin, LOW); delayMicroseconds(26);
                }
                delayMicroseconds(562); // Space
            } else {
                digitalWrite(irPin, LOW);
                delayMicroseconds(562);
            }
        }
        digitalWrite(irPin, LOW);
    }
}
void MMBasic_CmdServo(void) {
    char *line = currentLine; while (*line == ' ') line++;
    int t, pin; float f; char *s;
    if (MMBasic_EvaluateExpression(&line,&t,&pin,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t2, pos;
    if (MMBasic_EvaluateExpression(&line,&t2,&pos,&f,&s)) return;
    int duty = map(pos, 0, 1000, 26, 128); // 0-1000 map to ~0.5ms-2.5ms at 50Hz
    if (duty<26) duty=26; if (duty>128) duty=128;
    ledcAttach(pin, 50, 8);
    ledcWrite(pin, duty);
}
void MMBasic_CmdPort(void) {
    char *line = currentLine; while (*line == ' ') line++;
    int t, bits; float f; char *s;
    if (MMBasic_EvaluateExpression(&line,&t,&bits,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t2, val;
    if (MMBasic_EvaluateExpression(&line,&t2,&val,&f,&s)) return;
    // Write bits to GPIO outputs via ESP32 register
    uint32_t mask = bits;
    uint32_t out = val & mask;
    GPIO.out_w1ts = out;           // Set bits
    GPIO.out_w1tc = (~out) & mask; // Clear bits
    // Set direction for used bits
    for (int i=0; i<32; i++) if (mask & (1<<i)) pinMode(i, OUTPUT);
}
void MMBasic_CmdPulse(void) {
    char *line = currentLine; while (*line == ' ') line++;
    int t, pin; float f; char *s;
    if (MMBasic_EvaluateExpression(&line,&t,&pin,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t2, width;
    if (MMBasic_EvaluateExpression(&line,&t2,&width,&f,&s)) return;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH); delayMicroseconds(width);
    digitalWrite(pin, LOW);
}
// BOX command: BOX x,y,w,h [,fillColor]
void MMBasic_CmdBox(void) {
    char *line = currentLine; while (*line == ' ') line++;
    int t, x; float f; char *s;
    if (MMBasic_EvaluateExpression(&line,&t,&x,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t2, y; if (MMBasic_EvaluateExpression(&line,&t2,&y,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t3, w; if (MMBasic_EvaluateExpression(&line,&t3,&w,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t4, h; if (MMBasic_EvaluateExpression(&line,&t4,&h,&f,&s)) return;
    while (*line==' ') line++;
    if (*line==',') {
        line++;
        int tc, c; if (MMBasic_EvaluateExpression(&line,&tc,&c,&f,&s)) return;
        M5Cardputer.Display.fillRect(x,y,w,h,(uint16_t)c);
    } else {
        M5Cardputer.Display.drawRect(x,y,w,h,HAL_Display_GetTextColor());
    }
}
// TEXT command: TEXT x,y,"string" [,color]
void MMBasic_CmdText(void) {
    char *line = currentLine; while (*line == ' ') line++;
    int t, x; float f; char *s;
    if (MMBasic_EvaluateExpression(&line,&t,&x,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t2, y; if (MMBasic_EvaluateExpression(&line,&t2,&y,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    char txt[256]; int i=0;
    if (*line=='"') { line++; while (*line!='"'&&*line&&i<255) txt[i++]=*line++; txt[i]=0; if (*line=='"') line++; }
    else { while (*line!=','&&*line!=' '&&*line&&i<255) txt[i++]=*line++; txt[i]=0; }
    uint16_t col = HAL_Display_GetTextColor();
    while (*line==' ') line++;
    if (*line==',') { line++; int tc,c; float f2; char *s2;
        if (MMBasic_EvaluateExpression(&line,&tc,&c,&f2,&s2)) return; col=(uint16_t)c; }
    M5Cardputer.Display.setTextColor(col);
    M5Cardputer.Display.setCursor(x,y);
    M5Cardputer.Display.print(txt);
    M5Cardputer.Display.setTextColor(HAL_Display_GetTextColor());
}
// TRIANGLE command: TRIANGLE x1,y1,x2,y2,x3,y3 [,color]
void MMBasic_CmdTriangle(void) {
    char *line = currentLine; while (*line == ' ') line++;
    int t, x1; float f; char *s;
    if (MMBasic_EvaluateExpression(&line,&t,&x1,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t2, y1; if (MMBasic_EvaluateExpression(&line,&t2,&y1,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t3, x2; if (MMBasic_EvaluateExpression(&line,&t3,&x2,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t4, y2; if (MMBasic_EvaluateExpression(&line,&t4,&y2,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t5, x3; if (MMBasic_EvaluateExpression(&line,&t5,&x3,&f,&s)) return;
    while (*line==' ') line++; if (*line==',') line++;
    int t6, y3; if (MMBasic_EvaluateExpression(&line,&t6,&y3,&f,&s)) return;
    uint16_t col = HAL_Display_GetTextColor();
    while (*line==' ') line++;
    if (*line==',') { line++; int tc,c; float f2; char *s2;
        if (MMBasic_EvaluateExpression(&line,&tc,&c,&f2,&s2)) return; col=(uint16_t)c; }
    M5Cardputer.Display.fillTriangle(x1,y1,x2,y2,x3,y3,col);
}
// CLEAR - clear all variables, keep program
void MMBasic_CmdClear(void) {
    varcnt = 0;
    memset(vartbl, 0, MAXVARS * sizeof(struct s_vartbl));
    memset(varIsConst, 0, MAXVARS * sizeof(bool));
    forstackptr = gosubstackptr = 0;
    whilestackptr = dostackptr = 0;
    selectptr = 0;
}
// ERASE array - free array memory
void MMBasic_CmdErase(void) {
    char *line = currentLine; while (*line == ' ') line++;
    char name[MAXVARLEN+2]; int i=0;
    while ((*line>='A'&&*line<='Z')||(*line>='a'&&*line<='z')||(*line>='0'&&*line<='9')||*line=='_'||*line=='$') {
        if (i<MAXVARLEN+1) name[i++]=*line; line++;
    } name[i]=0;
    for (int j=0;name[j];j++) if (name[j]>='a'&&name[j]<='z') name[j]-=32;
    int idx = MMBasic_FindVariable(name);
    if (idx<0||vartbl[idx].array==0) { HAL_Display_Println("Not an array"); return; }
    free(vartbl[idx].arr); vartbl[idx].arr=NULL; vartbl[idx].array=0;
}
// REDIM array(newsize)
void MMBasic_CmdRedim(void) {
    char *line = currentLine; while (*line == ' ') line++;
    char name[MAXVARLEN+2]; int i=0;
    while ((*line>='A'&&*line<='Z')||(*line>='a'&&*line<='z')||(*line>='0'&&*line<='9')||*line=='_'||*line=='$') {
        if (i<MAXVARLEN+1) name[i++]=*line; line++;
    } name[i]=0;
    for (int j=0;name[j];j++) if (name[j]>='a'&&name[j]<='z') name[j]-=32;
    while (*line==' ') line++;
    if (*line!='(') { MMBasic_Error(ERR_SYNTAX,"Expected ("); return; } line++;
    int t,size; float f; char *s;
    if (MMBasic_EvaluateExpression(&line,&t,&size,&f,&s)) return;
    int idx = MMBasic_FindVariable(name);
    if (idx<0) idx = MMBasic_CreateVariable(name, T_INT);
    if (vartbl[idx].arr) free(vartbl[idx].arr);
    vartbl[idx].array = size;
    vartbl[idx].arr = (int*)malloc(size * sizeof(int));
    memset(vartbl[idx].arr, 0, size * sizeof(int));
}
// LINE INPUT handling is inside CmdLine (checks for LINE INPUT prefix)
// ERROR command - raise custom error
void MMBasic_CmdError(void) {
    char *line = currentLine; while (*line == ' ') line++;
    if (*line == '"') { line++; char msg[STRINGSIZE]; int i=0;
        while (*line!='"'&&*line&&i<STRINGSIZE-1) msg[i++]=*line++; msg[i]=0;
        MMBasic_Error(ERR_SYNTAX, msg);
    } else {
        char msg[STRINGSIZE]; int i=0;
        while (*line&&i<STRINGSIZE-1) msg[i++]=*line++; msg[i]=0;
        MMBasic_Error(ERR_SYNTAX, msg);
    }
}
// TRACE command - TROFF/TRON
void MMBasic_CmdTrace(void) {
    char *line = currentLine; while (*line == ' ') line++;
    char sub[16]; int i=0;
    while ((*line>='A'&&*line<='Z')||(*line>='a'&&*line<='z')&&i<15) sub[i++]=*line++; sub[i]=0;
    for (int j=0;sub[j];j++) if (sub[j]>='a'&&sub[j]<='z') sub[j]-=32;
    if (strcmp(sub,"ON")==0 || strcmp(sub,"TRON")==0) {
        traceOn = true; HAL_Display_Println("Trace ON");
    } else {
        traceOn = false; HAL_Display_Println("Trace OFF");
    }
}
// SUB/FUNCTION definition handler - skip to END SUB/END FUNCTION at runtime
void MMBasic_CmdSubFunDef(void) {
    int idx = -1;
    const char *line = lines[currentLineIndex];
    while (*line==' '||*line=='\t') line++;
    if ((line[0]=='S'||line[0]=='s')&&(line[1]=='U'||line[1]=='u')) line+=3;
    else line+=8;
    while (*line==' '||*line=='\t') line++;
    char name[MAXVARLEN+1]; int ni=0;
    while ((*line>='A'&&*line<='Z')||(*line>='a'&&*line<='z')||(*line>='0'&&*line<='9')||*line=='_'||*line=='$'||*line=='%'||*line=='!') {
        if (ni<MAXVARLEN) name[ni++]=*line; line++;
    } name[ni]=0;
    for (int j=0;name[j];j++) if (name[j]>='a'&&name[j]<='z') name[j]-=32;
    for (int k=0; k<subFunCount; k++) {
        if (strcmp(name, subFunTable[k].name)==0) { idx=k; break; }
    }
    if (idx>=0 && subFunTable[idx].endLine > currentLineIndex) {
        currentLineIndex = subFunTable[idx].endLine;
        flowControlActive = true;
    }
}
void MMBasic_CmdEndSubFun(void) {
    if (shadowBaseSP > 0) MMBasic_CmdReturn();
    // Otherwise just continue (definition skip)
}
// CALL command: CALL name(arg1, arg2, ...)
void MMBasic_CmdCall(void) {
    char *line = currentLine; while (*line==' ') line++;
    char name[MAXVARLEN+1]; int ni=0;
    while ((*line>='A'&&*line<='Z')||(*line>='a'&&*line<='z')||(*line>='0'&&*line<='9')||*line=='_'||*line=='$'||*line=='%'||*line=='!') {
        if (ni<MAXVARLEN) name[ni++]=*line; line++;
    } name[ni]=0;
    for (int j=0;name[j];j++) if (name[j]>='a'&&name[j]<='z') name[j]-=32;
    int idx=-1;
    for (int k=0; k<subFunCount; k++) {
        if (strcmp(name, subFunTable[k].name)==0 && !subFunTable[k].isFunction) {
            idx=k; break;
        }
    }
    if (idx<0) { MMBasic_Error(ERR_SYNTAX,"Unknown subroutine"); return; }
    
    // Save return position
    subFunTable[idx].returnLine = currentLineIndex;
    subFunCallReturnIdx = currentLineIndex;
    if (shadowBaseSP < 16) subFunRetStack[shadowBaseSP] = subFunCallReturnIdx;
    
    // Push shadow base
    if (shadowBaseSP>=16) { MMBasic_Error(ERR_STACK_FULL,"SUB nesting too deep"); return; }
    shadowBase[shadowBaseSP++] = shadowPtr;
    
    // Parse arguments
    int args[8]; float fargs[8]; char *sargs[8]; int atypes[8];
    int argc=0;
    while (*line==' '||*line=='\t') line++;
    if (*line=='(') {
        line++;
        while (*line && *line!=')' && argc<8) {
            while (*line==' '||*line==','||*line=='\t') line++;
            if (*line==')') break;
            atypes[argc]=T_INT;
            if (MMBasic_EvaluateExpression(&line,&atypes[argc],&args[argc],&fargs[argc],&sargs[argc])) return;
            argc++;
        }
    }
    
    // Push shadows for parameters
    for (int p=0; p<subFunTable[idx].paramCount; p++) {
        char *pn = subFunTable[idx].paramNames[p];
        int vi = MMBasic_FindVariable(pn);
        // Save old value
        if (shadowPtr >= MAX_SHADOW) break;
        if (vi>=0) {
            shadowStack[shadowPtr].varIndex = vi;
            shadowStack[shadowPtr].type = vartbl[vi].type;
            shadowStack[shadowPtr].ival = vartbl[vi].val.ival;
            shadowStack[shadowPtr].fval = vartbl[vi].val.fval;
            if (vartbl[vi].type==T_STR && vartbl[vi].val.sval) {
                shadowStack[shadowPtr].sval = (char*)malloc(STRINGSIZE);
                strcpy(shadowStack[shadowPtr].sval, vartbl[vi].val.sval);
            } else shadowStack[shadowPtr].sval = NULL;
            shadowStack[shadowPtr].array = vartbl[vi].array;
            shadowStack[shadowPtr].arr = vartbl[vi].arr;
            shadowStack[shadowPtr].wasConst = varIsConst[vi];
        } else {
            shadowStack[shadowPtr].varIndex = -1;
            vi = MMBasic_CreateVariable(pn, (p<argc && atypes[p]==T_STR) ? T_STR : T_INT);
        }
        shadowPtr++;
        
        // Set new value
        if (vi>=0 && p<argc) {
            varIsConst[vi]=false;
            if (atypes[p]==T_STR) {
                if (!vartbl[vi].val.sval) vartbl[vi].val.sval=(char*)malloc(STRINGSIZE);
                strncpy(vartbl[vi].val.sval, sargs[p]?sargs[p]:"", STRINGSIZE-1);
                vartbl[vi].type=T_STR;
            } else {
                vartbl[vi].val.ival=args[p];
                vartbl[vi].type=atypes[p];
            }
        }
    }
    
    // Jump to SUB body (line after definition)
    currentLineIndex = subFunTable[idx].startLine + 1;
    currentLine = lines[currentLineIndex];
    flowControlActive = true;
}
void MMBasic_CmdSort(void) {
    char *line = currentLine; while (*line == ' ') line++;
    char name[MAXVARLEN+2]; int i=0;
    while ((*line>='A'&&*line<='Z')||(*line>='a'&&*line<='z')||(*line>='0'&&*line<='9')||*line=='_'||*line=='$') {
        if (i<MAXVARLEN+1) name[i++]=*line; line++;
    } name[i]=0;
    for (int j=0;name[j];j++) if (name[j]>='a'&&name[j]<='z') name[j]-=32;
    while (*line==' ') line++;
    if (*line!='(') { MMBasic_Error(ERR_SYNTAX,"Expected ("); return; } line++;
    if (*line==')') line++;
    int idx = MMBasic_FindVariable(name);
    if (idx<0||vartbl[idx].array==0) { HAL_Display_Println("Not an array"); return; }
    int n = vartbl[idx].array;
    if (vartbl[idx].type == T_INT) {
        int *a = vartbl[idx].arr;
        for (int i=0; i<n-1; i++)
            for (int j=i+1; j<n; j++)
                if (a[i] > a[j]) { int t=a[i]; a[i]=a[j]; a[j]=t; }
    }
}
// PRINT# and INPUT# already work with file numbers
static HardwareSerial *serDev = NULL;
void MMBasic_CmdOption(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    // For now, options are stubs - just parse and ignore
    // Common MMBasic options: BASE, EXPLICIT, ANGLE, etc.
    HAL_Display_Println("OPTION - stub");
}
void MMBasic_CmdLoop(void) {
    char *line = currentLine;
    while (*line == ' ') line++;
    
    int postWhile = 0, postUntil = 0;
    if ((line[0]=='W'||line[0]=='w') && (line[1]=='H'||line[1]=='h') &&
        (line[2]=='I'||line[2]=='i') && (line[3]=='L'||line[3]=='l') &&
        (line[4]=='E'||line[4]=='e')) {
        postWhile = 1; line += 5;
    } else if ((line[0]=='U'||line[0]=='u') && (line[1]=='N'||line[1]=='n') &&
               (line[2]=='T'||line[2]=='t') && (line[3]=='I'||line[3]=='i') &&
               (line[4]=='L'||line[4]=='l')) {
        postUntil = 1; line += 5;
    }
    
    if (dostackptr == 0) { MMBasic_Error(ERR_SYNTAX, "LOOP without DO"); return; }
    
    int keepLooping = 1;
    if (postWhile || postUntil) {
        int itype, ival; float fval; char *sval;
        if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) return;
        if ((postWhile && !ival) || (postUntil && ival)) keepLooping = 0;
    }
    
    if (keepLooping) {
        currentLineIndex = dostack[dostackptr - 1];
        currentLine = lines[currentLineIndex];
        flowControlActive = true;
    } else {
        dostackptr--; // Exit loop
    }
}
void MMBasic_CmdRestore(void) {
    dataLineIdx = 0;
    dataOffset = 0;
}

// WHILE command - pre-test loop
void MMBasic_CmdWhile(void) {
    // Check if this is a re-entry (WEND jumped back) or first entry
    int isReentry = (whilestackptr > 0 && whilestack[whilestackptr - 1] == currentLineIndex);
    
    char *line = currentLine;
    while (*line == ' ') line++;
    
    int itype, ival;
    float fval; char *sval;
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) return;
    
    if (ival) {
        // Condition true - push on first entry, continue on re-entry
        if (!isReentry) {
            if (whilestackptr >= MAXFORLOOPS) {
                MMBasic_Error(ERR_STACK_FULL, "WHILE stack full");
                return;
            }
            whilestack[whilestackptr++] = currentLineIndex;
        }
    } else {
        // Condition false - pop stack and skip to WEND
        if (isReentry) whilestackptr--;
        int depth = 1;
        currentLineIndex++;
        while (currentLineIndex < linecnt) {
            const char *l = lines[currentLineIndex];
            while (*l == ' ' || *l == '\t') l++;
            if ((l[0] == 'W' || l[0] == 'w') && (l[1] == 'H' || l[1] == 'h') &&
                (l[2] == 'I' || l[2] == 'i') && (l[3] == 'L' || l[3] == 'l') &&
                (l[4] == 'E' || l[4] == 'e')) depth++;
            else if ((l[0] == 'W' || l[0] == 'w') && (l[1] == 'E' || l[1] == 'e') &&
                     (l[2] == 'N' || l[2] == 'n') && (l[3] == 'D' || l[3] == 'd')) {
                depth--;
                if (depth == 0) { currentLineIndex++; break; }
            }
            currentLineIndex++;
        }
        flowControlActive = true;
    }
}

// SELECT CASE command
// SELECT CASE expression
void MMBasic_CmdSelect(void) {
    char *line = currentLine;
    // Skip "CASE" keyword if present: SELECT CASE expr
    while (*line == ' ') line++;
    if ((line[0]=='C'||line[0]=='c') && (line[1]=='A'||line[1]=='a') &&
        (line[2]=='S'||line[2]=='s') && (line[3]=='E'||line[3]=='e')) line += 4;
    
    int itype, ival; float fval; char *sval;
    if (MMBasic_EvaluateExpression(&line, &itype, &ival, &fval, &sval)) return;
    
    if (selectptr >= 8) { MMBasic_Error(ERR_STACK_FULL, "SELECT stack full"); return; }
    selectstack[selectptr].matchValue = ival;
    selectstack[selectptr].matched = false;
    selectptr++;
}

// CASE command
void MMBasic_CmdCase(void) {
    if (selectptr == 0) { MMBasic_Error(ERR_SYNTAX, "CASE without SELECT"); return; }
    s_select *sel = &selectstack[selectptr - 1];
    
    char *line = currentLine;
    while (*line == ' ') line++;
    
    // Check for CASE ELSE
    if ((line[0]=='E'||line[0]=='e') && (line[1]=='L'||line[1]=='l') &&
        (line[2]=='S'||line[2]=='s') && (line[3]=='E'||line[3]=='e')) {
        if (!sel->matched) {
            sel->matched = true;
        } else {
            MMBasic_SkipToEndSelect();
        }
        return;
    }
    
    // Parse comma-separated values
    bool match = false;
    while (*line != '\0' && *line != '\n' && *line != '\r') {
        while (*line == ' ') line++;
        int t, v; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &t, &v, &f, &s)) return;
        if (v == sel->matchValue) match = true;
        while (*line == ' ') line++;
        if (*line == ',') line++;
    }
    
    if (match && !sel->matched) {
        sel->matched = true;
    } else if (!match && sel->matched) {
        MMBasic_SkipToEndSelect();
    } else if (!match && !sel->matched) {
        MMBasic_SkipToNextCase();
    }
}

// Skip to next CASE or ENDSELECT at same nesting level
void MMBasic_SkipToNextCase(void) {
    int depth = 0;
    currentLineIndex++;
    while (currentLineIndex < linecnt) {
        const char *l = lines[currentLineIndex];
        while (*l == ' ' || *l == '\t') l++;
        if ((l[0]=='S'||l[0]=='s') && (l[1]=='E'||l[1]=='e') &&
            (l[2]=='L'||l[2]=='l') && (l[3]=='E'||l[3]=='e') &&
            (l[4]=='C'||l[4]=='c') && (l[5]=='T'||l[5]=='t')) {
            depth++;
        } else if (depth == 0 && (l[0]=='C'||l[0]=='c') && (l[1]=='A'||l[1]=='a') &&
                   (l[2]=='S'||l[2]=='s') && (l[3]=='E'||l[3]=='e')) {
            break; // Stop at next CASE, don't skip it
        } else if ((l[0]=='E'||l[0]=='e') && (l[1]=='N'||l[1]=='n') &&
                   (l[2]=='D'||l[2]=='d') && (l[3]=='S'||l[3]=='s') &&
                   (l[4]=='E'||l[4]=='e') && (l[5]=='L'||l[5]=='l') &&
                   (l[6]=='E'||l[6]=='e') && (l[7]=='C'||l[7]=='c') &&
                   (l[8]=='T'||l[8]=='t')) {
            if (depth == 0) { currentLineIndex++; break; }
            depth--;
        }
        currentLineIndex++;
    }
    flowControlActive = true;
}

// Skip to END SELECT at same nesting level
void MMBasic_SkipToEndSelect(void) {
    int depth = 0;
    currentLineIndex++;
    while (currentLineIndex < linecnt) {
        const char *l = lines[currentLineIndex];
        while (*l == ' ' || *l == '\t') l++;
        if ((l[0]=='S'||l[0]=='s') && (l[1]=='E'||l[1]=='e') &&
            (l[2]=='L'||l[2]=='l') && (l[3]=='E'||l[3]=='e') &&
            (l[4]=='C'||l[4]=='c') && (l[5]=='T'||l[5]=='t')) {
            depth++;
        } else if ((l[0]=='E'||l[0]=='e') && (l[1]=='N'||l[1]=='n') &&
                   (l[2]=='D'||l[2]=='d') && (l[3]=='S'||l[3]=='s') &&
                   (l[4]=='E'||l[4]=='e') && (l[5]=='L'||l[5]=='l') &&
                   (l[6]=='E'||l[6]=='e') && (l[7]=='C'||l[7]=='c') &&
                   (l[8]=='T'||l[8]=='t')) {
            if (depth == 0) { currentLineIndex++; break; }
            depth--;
        }
        currentLineIndex++;
    }
    flowControlActive = true;
}

// END SELECT command
void MMBasic_CmdEndSelect(void) {
    if (selectptr > 0) selectptr--;
}
void MMBasic_CmdWend(void) {
    if (whilestackptr == 0) {
        MMBasic_Error(ERR_SYNTAX, "WEND without WHILE");
        return;
    }
    currentLineIndex = whilestack[whilestackptr - 1];
    currentLine = lines[currentLineIndex];
    flowControlActive = true;
}

// Scan program for SUB/FUNCTION definitions
void ScanSubFunDefs(void) {
    subFunCount = 0;
    for (int i = 0; i < linecnt; i++) {
        const char *l = lines[i];
        while (*l == ' ' || *l == '\t') l++;
        int isSub = 0, isFun = 0;
        if ((l[0]=='S'||l[0]=='s')&&(l[1]=='U'||l[1]=='u')&&(l[2]=='B'||l[2]=='b')) isSub=1;
        else if ((l[0]=='F'||l[0]=='f')&&(l[1]=='U'||l[1]=='u')&&(l[2]=='N'||l[2]=='n')) isFun=1;
        if (!isSub && !isFun) continue;
        if (subFunCount >= MAXSUBFUN) break;
        l += (isSub ? 3 : 8); while (*l==' '||*l=='\t') l++;
        char *name = subFunTable[subFunCount].name; int ni=0;
        while ((*l>='A'&&*l<='Z')||(*l>='a'&&*l<='z')||(*l>='0'&&*l<='9')||*l=='_'||*l=='$'||*l=='%'||*l=='!') {
            if (ni<MAXVARLEN) name[ni++]=*l; l++;
        } name[ni]=0;
        for (int j=0;name[j];j++) if (name[j]>='a'&&name[j]<='z') name[j]-=32;
        if (name[0]==0) continue;
        subFunTable[subFunCount].startLine = i;
        subFunTable[subFunCount].isFunction = isFun;
        int pc=0;
        while (*l==' '||*l=='\t') l++;
        if (*l=='(') { l++;
            while (*l && *l!=')' && pc<8) {
                while (*l==' '||*l==','||*l=='\t') l++;
                char *pn = subFunTable[subFunCount].paramNames[pc]; int pni=0;
                while ((*l>='A'&&*l<='Z')||(*l>='a'&&*l<='z')||(*l>='0'&&*l<='9')||*l=='_'||*l=='$'||*l=='%'||*l=='!') {
                    if (pni<MAXVARLEN) pn[pni++]=*l; l++;
                } pn[pni]=0;
                for (int j=0;pn[j];j++) if (pn[j]>='a'&&pn[j]<='z') pn[j]-=32;
                if (pn[0]) pc++;
            }
            if (*l==')') l++;
        }
        subFunTable[subFunCount].paramCount = pc;
        int depth = 1, ei = i+1;
        while (ei < linecnt && depth > 0) {
            const char *el = lines[ei];
            while (*el==' '||*el=='\t') el++;
            if ((el[0]=='S'||el[0]=='s')&&(el[1]=='U'||el[1]=='u')&&(el[2]=='B'||el[2]=='b')) depth++;
            else if ((el[0]=='F'||el[0]=='f')&&(el[1]=='U'||el[1]=='u')&&(el[2]=='N'||el[2]=='n')) depth++;
            else if ((el[0]=='E'||el[0]=='e')&&(el[1]=='N'||el[1]=='n')&&(el[2]=='D'||el[2]=='d')) {
                el+=3; while (*el==' '||*el=='\t') el++;
                if ((el[0]=='S'||el[0]=='s')&&(el[1]=='U'||el[1]=='u')&&(el[2]=='B'||el[2]=='b')) depth--;
                else if ((el[0]=='F'||el[0]=='f')&&(el[1]=='U'||el[1]=='u')&&(el[2]=='N'||el[2]=='n')) depth--;
            }
            if (depth > 0) ei++;
        }
        subFunTable[subFunCount].endLine = (depth==0) ? ei : i;
        subFunCount++;
    }
}

// ============ TURTLE Graphics ============
struct TurtleState {
    float x, y, heading;
    int penDown, penColor, penWidth;
    int visible, cursorSize, cursorColor;
    float oldX, oldY, oldHeading;
    int oldCursorSize, oldCursorColor;
    int fillColor, fillEnabled;
};
static TurtleState t = {120, 67, 0, 1, 0xFFFF, 1, 0, 5, 0x07E0, 120, 67, 0, 5, 0x07E0, 0xFFFF, 0};
static float tStackX[16], tStackY[16], tStackH[16]; static int tStackPtr = 0;

static void tDrawCursor(void) {
    if (!t.visible) return;
    float rad = t.heading * 3.14159f / 180.0f;
    float s6 = t.cursorSize * 0.6f;
    int tx = (int)t.x, ty = (int)t.y;
    int fx = tx + (int)(t.cursorSize * sin(rad));
    int fy = ty - (int)(t.cursorSize * cos(rad));
    int lx = tx + (int)(s6 * sin(rad + 2.618f));   // +150 deg
    int ly = ty - (int)(s6 * cos(rad + 2.618f));
    int rx = tx + (int)(s6 * sin(rad - 2.618f));   // -150 deg
    int ry = ty - (int)(s6 * cos(rad - 2.618f));
    M5Cardputer.Display.drawLine(fx, fy, lx, ly, t.cursorColor);
    M5Cardputer.Display.drawLine(lx, ly, rx, ry, t.cursorColor);
    M5Cardputer.Display.drawLine(rx, ry, fx, fy, t.cursorColor);
    t.oldX = t.x; t.oldY = t.y; t.oldHeading = t.heading;
    t.oldCursorSize = t.cursorSize; t.oldCursorColor = t.cursorColor;
}
static void tEraseCursor(void) {
    if (!t.visible) return;
    float rad = t.oldHeading * 3.14159f / 180.0f;
    int tx = (int)t.oldX, ty = (int)t.oldY, sz = t.oldCursorSize;
    int fx = tx + (int)(sz * sin(rad));
    int fy = ty - (int)(sz * cos(rad));
    float s6 = sz * 0.6f;
    int lx = tx + (int)(s6 * sin(rad + 2.618f));
    int ly = ty - (int)(s6 * cos(rad + 2.618f));
    int rx = tx + (int)(s6 * sin(rad - 2.618f));
    int ry = ty - (int)(s6 * cos(rad - 2.618f));
    int x1 = min(min(fx, lx), rx); int x2 = max(max(fx, lx), rx);
    int y1 = min(min(fy, ly), ry); int y2 = max(max(fy, ly), ry);
    M5Cardputer.Display.fillRect(x1, y1, x2-x1+1, y2-y1+1, 0);
}
static void tMove(float dist) {
    if (fabs(dist) < 0.01f) return;
    float ox = t.x, oy = t.y;
    float rad = t.heading * 3.14159f / 180.0f;
    float s = sin(rad), c = cos(rad);
    if (fabs(fmod(t.heading + 0.001f, 90.0f)) < 0.002f) {
        int q = (int)((t.heading + 0.001f) / 90.0f) % 4;
        if (q == 0) { s = 0; c = 1; }
        else if (q == 1) { s = 1; c = 0; }
        else if (q == 2) { s = 0; c = -1; }
        else { s = -1; c = 0; }
    }
    t.x += dist * s;
    t.y -= dist * c;
    if (t.penDown && t.penWidth > 0) {
        for (int i = 0; i < t.penWidth; i++)
            M5Cardputer.Display.drawLine((int)ox + i, (int)oy, (int)t.x + i, (int)t.y, t.penColor);
    }
    tDrawCursor();
}
static void tLineTo(float nx, float ny) {
    if (t.penDown && t.penWidth > 0) {
        for (int i = 0; i < t.penWidth; i++)
            M5Cardputer.Display.drawLine((int)t.x + i, (int)t.y, (int)nx + i, (int)ny, t.penColor);
    }
    t.x = nx; t.y = ny;
    tDrawCursor();
}

static bool tMatch(char **p, const char *word) {
    char *s = *p;
    while (*s == ' ') s++;
    int len = strlen(word);
    for (int i = 0; i < len; i++) {
        char c = s[i]; if (c >= 'a' && c <= 'z') c -= 32;
        if (c != word[i]) return false;
    }
    if (s[len] != ' ' && s[len] != '\0' && s[len] != '\n' && s[len] != '\r') return false;
    *p = s + len;
    return true;
}

void MMBasic_CmdTurtle(void) {
    char *line = (char*)currentLine;
    while (*line == ' ') line++;
    if (tMatch(&line, "INIT") || tMatch(&line, "RESET")) {
        M5Cardputer.Display.fillScreen(0);
        t.x = 120; t.y = 67; t.heading = 0;
        t.penDown = 1; t.penColor = 0xFFFF; t.penWidth = 1;
        t.visible = 0; t.cursorSize = 5; t.cursorColor = 0x07E0;
        t.fillColor = 0xFFFF; t.fillEnabled = 0;
        tDrawCursor();
    } else if (tMatch(&line, "FORWARD") || tMatch(&line, "FD")) {
        int tt, d; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &d, &f, &s)) return;
        tMove((float)d);
    } else if (tMatch(&line, "BACK") || tMatch(&line, "BK") || tMatch(&line, "BACKWARD")) {
        int tt, d; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &d, &f, &s)) return;
        tMove(-(float)d);
    } else if (tMatch(&line, "RIGHT") || tMatch(&line, "RT")) {
        int tt, a = 90; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &a, &f, &s)) ; // optional
        tEraseCursor();
        t.heading += (float)a;
        while (t.heading < 0) t.heading += 360;
        while (t.heading >= 360) t.heading -= 360;
        tDrawCursor();
    } else if (tMatch(&line, "LEFT") || tMatch(&line, "LT")) {
        int tt, a = 90; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &a, &f, &s)) ;
        tEraseCursor();
        t.heading -= (float)a;
        while (t.heading < 0) t.heading += 360;
        while (t.heading >= 360) t.heading -= 360;
        tDrawCursor();
    } else if (tMatch(&line, "PEN") && (tMatch(&line, "UP") || tMatch(&line, "PU"))) {
        t.penDown = 0;
    } else if (tMatch(&line, "PEN") && (tMatch(&line, "DOWN") || tMatch(&line, "PD"))) {
        t.penDown = 1;
    } else if (tMatch(&line, "PEN") && (tMatch(&line, "COLOUR") || tMatch(&line, "COLOR") || tMatch(&line, "PC"))) {
        int tt, c; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &c, &f, &s)) return;
        t.penColor = (uint16_t)c;
    } else if (tMatch(&line, "PEN") && (tMatch(&line, "WIDTH") || tMatch(&line, "PW"))) {
        int tt, w; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &w, &f, &s)) return;
        if (w < 1) w = 1; if (w > 50) w = 50;
        t.penWidth = w;
    } else if (tMatch(&line, "HOME")) {
        tEraseCursor();
        tLineTo(120, 67);
        t.heading = 0;
        tDrawCursor();
    } else if (tMatch(&line, "SET") && (tMatch(&line, "XY") || tMatch(&line, "MOVE"))) {
        int tt, nx, ny; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &nx, &f, &s)) return;
        while (*line == ' ' || *line == ',') line++;
        if (MMBasic_EvaluateExpression(&line, &tt, &ny, &f, &s)) return;
        tLineTo((float)nx, (float)ny);
    } else if (tMatch(&line, "SET") && (tMatch(&line, "HEADING") || tMatch(&line, "SETH"))) {
        int tt, h; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &h, &f, &s)) return;
        tEraseCursor();
        t.heading = (float)h;
        while (t.heading < 0) t.heading += 360;
        while (t.heading >= 360) t.heading -= 360;
        tDrawCursor();
    } else if (tMatch(&line, "SHOW") || tMatch(&line, "ST")) {
        t.visible = 1;
        tDrawCursor();
    } else if (tMatch(&line, "HIDE") || tMatch(&line, "HT")) {
        tEraseCursor();
        t.visible = 0;
    } else if (tMatch(&line, "CURSOR") && (tMatch(&line, "SIZE") || tMatch(&line, "CS"))) {
        int tt, sz; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &sz, &f, &s)) return;
        if (sz < 5) sz = 5; if (sz > 50) sz = 50;
        tEraseCursor();
        t.cursorSize = sz;
        tDrawCursor();
    } else if (tMatch(&line, "CURSOR") && (tMatch(&line, "COLOUR") || tMatch(&line, "COLOR") || tMatch(&line, "CC"))) {
        int tt, c; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &c, &f, &s)) return;
        tEraseCursor();
        t.cursorColor = (uint16_t)c;
        tDrawCursor();
    } else if (tMatch(&line, "CIRCLE")) {
        int tt, r; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &r, &f, &s)) return;
        M5Cardputer.Display.drawCircle((int)t.x, (int)t.y, r, t.penColor);
    } else if (tMatch(&line, "FCIRCLE")) {
        int tt, r; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &r, &f, &s)) return;
        M5Cardputer.Display.fillCircle((int)t.x, (int)t.y, r, t.fillEnabled ? t.fillColor : t.penColor);
    } else if (tMatch(&line, "DOT")) {
        int tt, sz = 5; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &sz, &f, &s)) ;
        M5Cardputer.Display.fillCircle((int)t.x, (int)t.y, sz, t.penColor);
    } else if (tMatch(&line, "FRECT") || tMatch(&line, "FRECTANGLE")) {
        int tt, w, h; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &w, &f, &s)) return;
        while (*line == ' ' || *line == ',') line++;
        if (MMBasic_EvaluateExpression(&line, &tt, &h, &f, &s)) return;
        int fc = t.fillEnabled ? t.fillColor : t.penColor;
        M5Cardputer.Display.fillRect((int)t.x - w/2, (int)t.y - h/2, w, h, fc);
    } else if (tMatch(&line, "FILL") && (tMatch(&line, "COLOUR") || tMatch(&line, "COLOR") || tMatch(&line, "FC"))) {
        int tt, c; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &c, &f, &s)) return;
        t.fillColor = (uint16_t)c; t.fillEnabled = 1;
    } else if (tMatch(&line, "NO") && tMatch(&line, "FILL")) {
        t.fillEnabled = 0;
    } else if (tMatch(&line, "BEZIER")) {
        int tt, cp1, a1, cp2, a2, ep, ea; float f; char *s;
        if (MMBasic_EvaluateExpression(&line, &tt, &cp1, &f, &s)) return;
        while (*line==' '||*line==',') line++;
        if (MMBasic_EvaluateExpression(&line, &tt, &a1, &f, &s)) return;
        while (*line==' '||*line==',') line++;
        if (MMBasic_EvaluateExpression(&line, &tt, &cp2, &f, &s)) return;
        while (*line==' '||*line==',') line++;
        if (MMBasic_EvaluateExpression(&line, &tt, &a2, &f, &s)) return;
        while (*line==' '||*line==',') line++;
        if (MMBasic_EvaluateExpression(&line, &tt, &ep, &f, &s)) return;
        while (*line==' '||*line==',') line++;
        if (MMBasic_EvaluateExpression(&line, &tt, &ea, &f, &s)) return;
        tEraseCursor();
        float h = t.heading;
        float r1 = h + a1, r2 = h + a1 + a2, re = h + a1 + a2 + ea;
        float p1x = t.x + cp1 * sin(r1 * 3.14159f / 180.0f);
        float p1y = t.y - cp1 * cos(r1 * 3.14159f / 180.0f);
        float p2x = p1x + cp2 * sin(r2 * 3.14159f / 180.0f);
        float p2y = p1y - cp2 * cos(r2 * 3.14159f / 180.0f);
        float ex = p2x + ep * sin(re * 3.14159f / 180.0f);
        float ey = p2y - ep * cos(re * 3.14159f / 180.0f);
        float ox = t.x, oy = t.y;
        for (int seg = 1; seg <= 20; seg++) {
            float tt = seg / 20.0f;
            float u = 1.0f - tt;
            float cx = u*u*u*ox + 3*u*u*tt*p1x + 3*u*tt*tt*p2x + tt*tt*tt*ex;
            float cy = u*u*u*oy + 3*u*u*tt*p1y + 3*u*tt*tt*p2y + tt*tt*tt*ey;
            if (t.penDown) M5Cardputer.Display.drawLine((int)t.x, (int)t.y, (int)cx, (int)cy, t.penColor);
            t.x = cx; t.y = cy;
        }
        t.heading = re;
        while (t.heading < 0) t.heading += 360;
        while (t.heading >= 360) t.heading -= 360;
        tDrawCursor();
    } else if (tMatch(&line, "STAMP")) {
        int cx = (int)t.x, cy = (int)t.y;
        M5Cardputer.Display.fillCircle(cx, cy - 3, 5, 0x07E0);
        M5Cardputer.Display.fillCircle(cx, cy + 2, 7, 0x07E0);
        M5Cardputer.Display.fillRect(cx - 3, cy - 8, 6, 4, 0x07E0);
        M5Cardputer.Display.fillRect(cx - 6, cy - 1, 4, 3, 0x07E0);
        M5Cardputer.Display.fillRect(cx + 2, cy - 1, 4, 3, 0x07E0);
        M5Cardputer.Display.fillRect(cx - 5, cy + 6, 3, 3, 0x07E0);
        M5Cardputer.Display.fillRect(cx + 2, cy + 6, 3, 3, 0x07E0);
    } else if (tMatch(&line, "PUSH")) {
        if (tStackPtr < 16) {
            tStackX[tStackPtr] = t.x; tStackY[tStackPtr] = t.y;
            tStackH[tStackPtr] = t.heading; tStackPtr++;
        }
    } else if (tMatch(&line, "POP")) {
        if (tStackPtr > 0) {
            tStackPtr--;
            tLineTo(tStackX[tStackPtr], tStackY[tStackPtr]);
            tEraseCursor();
            t.heading = tStackH[tStackPtr];
            tDrawCursor();
        }
    }
}
// ============ SPRITE System (pixel-based, PicoMite-compatible) ============
#define MAX_SPRITES 16
#define SPR_MAX_W 32
// RGB121 4-bit → 16-bit RGB565 conversion table
static const uint16_t pal16[16] = {
    0x0000, 0x001F, 0x0200, 0x021F, 0x0400, 0x041F, 0x07E0, 0x07FF,
    0xF800, 0xF81F, 0xF900, 0xF91F, 0xFC00, 0xFC1F, 0xFFE0, 0xFFFF
};
// Character '0'-'9','A'-'F' to palette index (space=transparent, -1)
static int sprCharToIdx(char c) {
    if (c==' ') return -1;
    if (c>='0'&&c<='9') return c-'0';
    if (c>='A'&&c<='F') return c-'A'+10;
    if (c>='a'&&c<='f') return c-'a'+10;
    return -1;
}
struct SpritePixel {
    uint8_t *pixels;    // 4-bit packed: 2 pixels/byte, low nibble = even col
    int w, h, x, y, nx, ny, layer;
    bool active;
};
static SpritePixel spr[MAX_SPRITES];
static uint16_t spr_transparent = 0; // Palette index 0 is transparent

static void sprDraw(int n) {
    if (!spr[n].active || !spr[n].pixels) return;
    uint8_t *p = spr[n].pixels;
    int w = spr[n].w, h = spr[n].h, x0 = spr[n].x, y0 = spr[n].y;
    for (int row=0; row<h; row++) {
        for (int col=0; col<w; col++) {
            int byteIdx = (row*w + col) >> 1;
            int nib = (col & 1) ? (p[byteIdx] >> 4) : (p[byteIdx] & 0xF);
            if (nib == spr_transparent) continue;
            M5Cardputer.Display.drawPixel(x0+col, y0+row, pal16[nib]);
        }
    }
}
static void sprErase(int n) {
    if (!spr[n].active) return;
    M5Cardputer.Display.fillRect(spr[n].x, spr[n].y, spr[n].w, spr[n].h, 0);
}

void MMBasic_CmdSprite(void) {
    char *line = (char*)currentLine;
    while (*line == ' ') line++;
    if (tMatch(&line, "SHOW")) {
        int tt, n, x=0, y=0, layer=1; float f; char *s;
        while (*line==' ') line++; if (*line=='#') line++;
        if (MMBasic_EvaluateExpression(&line,&tt,&n,&f,&s)) return;
        while (*line==' '||*line==',') line++; if (MMBasic_EvaluateExpression(&line,&tt,&x,&f,&s)) return;
        while (*line==' '||*line==',') line++; if (MMBasic_EvaluateExpression(&line,&tt,&y,&f,&s)) return;
        while (*line==' '||*line==',') line++; if (MMBasic_EvaluateExpression(&line,&tt,&layer,&f,&s)) ;
        if (n<0||n>=MAX_SPRITES||!spr[n].pixels) return;
        spr[n].active=1; spr[n].x=x; spr[n].y=y; spr[n].layer=layer;
        sprDraw(n);
    } else if (tMatch(&line, "HIDE")) {
        while (*line==' ') line++; if (*line=='#') line++;
        int tt, n; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&tt,&n,&f,&s)) return;
        if (n<0||n>=MAX_SPRITES) return;
        sprErase(n); spr[n].active=0;
    } else if (tMatch(&line, "NEXT")) {
        while (*line==' ') line++; if (*line=='#') line++;
        int tt, n, nx, ny; float f; char *s;
        if (MMBasic_EvaluateExpression(&line,&tt,&n,&f,&s)) return;
        while (*line==' '||*line==',') line++; if (MMBasic_EvaluateExpression(&line,&tt,&nx,&f,&s)) return;
        while (*line==' '||*line==',') line++; if (MMBasic_EvaluateExpression(&line,&tt,&ny,&f,&s)) return;
        if (n<0||n>=MAX_SPRITES) return;
        spr[n].nx=nx; spr[n].ny=ny;
    } else if (tMatch(&line, "MOVE")) {
        for (int i=0; i<MAX_SPRITES; i++) if (spr[i].active) sprErase(i);
        for (int i=0; i<MAX_SPRITES; i++) if (spr[i].active) {
            spr[i].x=spr[i].nx; spr[i].y=spr[i].ny;
            sprDraw(i);
        }
    } else if (tMatch(&line, "LOAD")) {
        int tt, n=1; float f; char *s;
        char fname[64]; int fi=0;
        while (*line==' ') line++;
        if (*line=='"') { line++; while (*line!='"'&&*line&&fi<63) fname[fi++]=*line++; fname[fi]=0; if (*line=='"') line++; }
        else { while (*line!=' '&&*line&&fi<63) fname[fi++]=*line++; fname[fi]=0; }
        while (*line==' '||*line==',') line++;
        if (MMBasic_EvaluateExpression(&line,&tt,&n,&f,&s)) ;
        if (!HAL_SD_Init()) return;
        char path[68]; path[0]='/'; strcpy(path+1,fname);
        File file = SD.open(path, FILE_READ);
        if (!file) return;
        // Read header: w, count, h (comma separated)
        String hdr = file.readStringUntil('\n'); hdr.trim();
        while (hdr.length()>0 && hdr[0]=='\'') { hdr = file.readStringUntil('\n'); hdr.trim(); }
        int ws=0, cnt=0, hs=0, ci=0;
        char *hp = (char*)hdr.c_str();
        while (*hp==' ') hp++; ws=atoi(hp); while (*hp&&*hp!=',') hp++; if (*hp==',') hp++;
        while (*hp==' ') hp++; cnt=atoi(hp); while (*hp&&*hp!=',') hp++; if (*hp==',') hp++;
        while (*hp==' ') hp++; hs=atoi(hp); if (hs==0) hs=ws;
        if (ws<=0||ws>SPR_MAX_W||hs<=0||hs>SPR_MAX_W) { file.close(); return; }
        int bufSize = (ws*hs+1)>>1;
        for (int idx=n; idx<n+cnt && idx<MAX_SPRITES; idx++) {
            spr[idx].w=ws; spr[idx].h=hs; spr[idx].active=0;
            if (spr[idx].pixels) free(spr[idx].pixels);
            spr[idx].pixels = (uint8_t*)malloc(bufSize);
            if (!spr[idx].pixels) { file.close(); return; }
            memset(spr[idx].pixels,0,bufSize);
            for (int row=0; row<hs && file.available(); row++) {
                String line = file.readStringUntil('\n'); line.trim();
                while (line.length()>0 && line[0]=='\'') { line = file.readStringUntil('\n'); line.trim(); }
                for (int col=0; col<ws && col<line.length(); col++) {
                    int ci = sprCharToIdx(line[col]);
                    if (ci<0) ci=0;
                    int byteIdx = (row*ws+col)>>1;
                    if (col&1) spr[idx].pixels[byteIdx] |= (ci<<4);
                    else spr[idx].pixels[byteIdx] = ci;
                }
            }
        }
        file.close();
    } else if (tMatch(&line, "CLOSE")) {
        if (tMatch(&line, "ALL")) {
            for (int i=0; i<MAX_SPRITES; i++) {
                if (spr[i].active) sprErase(i);
                spr[i].active=0;
                if (spr[i].pixels) { free(spr[i].pixels); spr[i].pixels=0; }
            }
        } else {
            while (*line==' ') line++; if (*line=='#') line++;
            int tt, n; float f; char *s;
            if (MMBasic_EvaluateExpression(&line,&tt,&n,&f,&s)) return;
            if (n>=0&&n<MAX_SPRITES) {
                if (spr[n].active) sprErase(n);
                spr[n].active=0;
                if (spr[n].pixels) { free(spr[n].pixels); spr[n].pixels=0; }
            }
        }
    } else if (tMatch(&line, "COPY")) {
        int tt, src, dst, cnt=1; float f; char *s;
        while (*line==' ') line++; if (*line=='#') line++;
        if (MMBasic_EvaluateExpression(&line,&tt,&src,&f,&s)) return;
        while (*line==' '||*line==',') line++; if (*line=='#') line++;
        if (MMBasic_EvaluateExpression(&line,&tt,&dst,&f,&s)) return;
        while (*line==' '||*line==',') line++; if (MMBasic_EvaluateExpression(&line,&tt,&cnt,&f,&s)) ;
        if (src<0||src>=MAX_SPRITES||!spr[src].pixels) return;
        for (int i=0; i<cnt && dst+i<MAX_SPRITES; i++) {
            int sz = (spr[src].w*spr[src].h+1)>>1;
            if (spr[dst+i].pixels) free(spr[dst+i].pixels);
            spr[dst+i].pixels = (uint8_t*)malloc(sz);
            memcpy(spr[dst+i].pixels, spr[src].pixels, sz);
            spr[dst+i].w=spr[src].w; spr[dst+i].h=spr[src].h;
            spr[dst+i].x=spr[src].x; spr[dst+i].y=spr[src].y;
            spr[dst+i].active=0;
        }
    }
}
// VAR SAVE/RESTORE to SD card
void MMBasic_CmdVar(void) {
    char *line = (char*)currentLine; while (*line==' ') line++;
    if (tMatch(&line, "SAVE")) {
        char fname[64]; int fi=0;
        while (*line==' ') line++;
        if (*line=='"') { line++; while (*line!='"'&&*line&&fi<63) fname[fi++]=*line++; fname[fi]=0; if (*line=='"') line++; }
        else { while (*line!=' '&&*line&&fi<63) fname[fi++]=*line++; fname[fi]=0; }
        if (!HAL_SD_Init()) return;
        char path[68]; path[0]='/'; strcpy(path+1,fname);
        File f = SD.open(path, FILE_WRITE);
        if (!f) return;
        for (int i=0; i<varcnt; i++) {
            f.print(vartbl[i].name); f.print(",");
            f.print((int)vartbl[i].type); f.print(",");
            if (vartbl[i].type==T_STR) {
                if (vartbl[i].val.sval) f.print(vartbl[i].val.sval); else f.print("");
            } else if (vartbl[i].type==T_FLOAT) {
                f.print(vartbl[i].val.fval, 6);
            } else {
                f.print(vartbl[i].val.ival);
            }
            f.println("");
        }
        f.close();
    } else if (tMatch(&line, "RESTORE")) {
        char fname[64]; int fi=0;
        while (*line==' ') line++;
        if (*line=='"') { line++; while (*line!='"'&&*line&&fi<63) fname[fi++]=*line++; fname[fi]=0; if (*line=='"') line++; }
        else { while (*line!=' '&&*line&&fi<63) fname[fi++]=*line++; fname[fi]=0; }
        if (!HAL_SD_Init()) return;
        char path[68]; path[0]='/'; strcpy(path+1,fname);
        File f = SD.open(path, FILE_READ);
        if (!f) return;
        varcnt=0; memset(vartbl,0,MAXVARS*sizeof(s_vartbl));
        while (f.available()) {
            String ln = f.readStringUntil('\n'); ln.trim();
            int c1=ln.indexOf(','), c2=ln.indexOf(',',c1+1);
            if (c1<0||c2<0) continue;
            String nm = ln.substring(0,c1);
            int tp = ln.substring(c1+1,c2).toInt();
            String vl = ln.substring(c2+1);
            char nbuf[33]; nm.toCharArray(nbuf,33);
            int idx = MMBasic_CreateVariable(nbuf, tp);
            if (idx>=0) {
                if (tp==T_STR) {
                    if (!vartbl[idx].val.sval) vartbl[idx].val.sval=(char*)malloc(STRINGSIZE);
                    vl.toCharArray(vartbl[idx].val.sval, STRINGSIZE);
                } else if (tp==T_FLOAT) vartbl[idx].val.fval=vl.toFloat();
                else vartbl[idx].val.ival=vl.toInt();
                vartbl[idx].type=tp;
            }
        }
        f.close();
    } else if (tMatch(&line, "CLEAR")) {
        varcnt=0; memset(vartbl,0,MAXVARS*sizeof(s_vartbl));
    }
}
// RGB(r, g, b) - create 16-bit color from RGB values
uint16_t MMBasic_RGB(int r, int g, int b) {
    // Convert 8-bit RGB to 16-bit 565 format
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}





