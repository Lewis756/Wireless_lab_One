#ifndef  INTERFACE_FUNCTIONS_H_
#define  INTERFACE_FUNCTIONS_H_
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "wait.h"


#define MAX_CHARS 80
#define MAX_FIELDS 5

typedef struct _USER_DATA
{
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;
//
extern void putcUart0( char c);
extern void putsUart0(char* str);
//
extern char getcUart0();
//when buffer not empty

void getsUart0(USER_DATA* data);
void parseFields(USER_DATA* data);
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber);
char getFieldChar(USER_DATA *data, uint8_t fieldNumber);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
size_t LenOfStr(const char* start);
bool Stringcmpr(const char fieldstrg[], const char command[]);
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
int stringToInt(char* str);

#endif
