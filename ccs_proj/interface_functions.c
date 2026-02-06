// Funcitons to include in the shell interface
//From Embedded Lab PWM excluded to modify]
// add more funcitons for modulation part 7
#include "clock.h"
#include "uart0.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "uart0.h"
#include "tm4c123gh6pm.h"
#include <math.h>
//from uartpartone jimmie bud lab
#define MAX_CHARS 80
#define MAX_FIELDS 5
//struct to fill the buffer
int stringToInt(char *str);
extern void putcUart0(char c);
extern void putsUart0(char *str);
extern char getcUart0();

typedef struct _USER_DATA
{
    char buffer[MAX_CHARS + 1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;

void getsUart0(USER_DATA *data)
{
    // initUart0();//initialize uart0
    int Count_ = 0;

    while (true)
    {
        char word;
        word = getcUart0();
        putcUart0(word);
        if ((word == 8 || word == 127) && Count_ > 0) //for backspace
        {
            Count_--;
        }
        //    char getcUart0();
        else if (word == 13 || word == 10) // checking for carriage return and now word ==10
        { // from lab 4 main
            data->buffer[Count_] = '\0';
            break;
        }
        //putcUart0(c);//equals zero

        else if (word >= 32) // space or character
        {
            //  char word = data->buffer[count];
            //store word in the data buffer
            data->buffer[Count_] = word;
            Count_++;
        }
        else if (Count_ == MAX_CHARS)
        {
            //putcUart0(c);
            data->buffer[MAX_CHARS] = '\0';
            break;
            //exits
        }
    }
}
// modified parse fields to meet voltage to I and Q requirements
//parse fields function and  to Walk T      hrough The Buffer
void parseFields(USER_DATA *data)
{
    int count = 0; // Varibale to move  through input buffer
    int countIndex = 0; // incrementing only when field starts
    int flagField = 0; // Seperate fields, not characters

    data->fieldCount = countIndex; //Continual Parsing , removes previous results

    while ((data->buffer[count] != '\0') && (countIndex < MAX_FIELDS))
    {
        // '\0'  stops at user input and Max so no overrun
        char tempVar = data->buffer[count]; //read and reuse
        //What can we accept and what becomes a delimmeter ??
        // spaces tab commas newline punctuation
        //valid token characters (A-Z)(a-z)(0-9) everything else seperates
        if ((tempVar >= 'A' && tempVar <= 'Z')
                || (tempVar >= 'a' && tempVar <= 'z')
                || (tempVar >= '0' && tempVar <= '9'))
        {

            if (flagField == 0) // position at first caracter matters not duplicates
            {
                data->fieldPosition[countIndex] = count;
                //this for not copieng strings not store tokens already exsisting

                if ((tempVar >= 'A' && tempVar <= 'Z')
                        || (tempVar >= 'a' && tempVar <= 'z'))
                    data->fieldType[countIndex] = 'a';
                // interface shell to seperate into correct field Alpha or numerical

                else
                    data->fieldType[countIndex] = 'n';
                //else going to numerical field
                countIndex++;
                //current token commited now
                flagField = 1;
                //to show we are inside token
            }
        }
        else //anyhthing ese becomes a \0 for parsing
        {
            data->buffer[count] = '\0';
            flagField = 0;
            //resetls fpr the next token
        }
        count++;
        //for no infine loops or skipped charachters
    }
    data->fieldCount = countIndex;
    //for command rejection and argument validiaitons
}
//Signed values to hold positive and negative values miliivolts and DAC code
int32_t getFieldInteger(USER_DATA *data, uint8_t fieldNumber) //struct buffer state based
{
    //number in range
    //field type is zero
    if (fieldNumber >= data->fieldCount || data->fieldType[fieldNumber] != 'n')
    //line above protects from input that is invalid making sure fields are correct
    //field number only numbers are converted
    {
        return 0;
        //fallback value
    }
    else
    {

        return stringToInt(&data->buffer[data->fieldPosition[fieldNumber]]);
        //parse fields buffer array of argument and turns into c strings
        //and where token starts at field positions
    }
}

char getFieldChar(USER_DATA *data, uint8_t fieldNumber)
{
    if (fieldNumber >= data->fieldCount || data->fieldType[fieldNumber] != 'a')
    {
        return '\0';   // fallback value
    }
    else
    {
        return data->buffer[data->fieldPosition[fieldNumber]];
    }
}

int stringToInt(char *str)
{
    int value = 0;
    int i;
    for (i = 0; str[i] != '\0'; ++i)
    {
        value = value * 10 + str[i] - '0';
    }
    return value;
}

char* getFieldString(USER_DATA *data, uint8_t fieldNumber)
{
    //return field requested
    if (fieldNumber <= (data->fieldCount) - 1)
    {
        return &data->buffer[data->fieldPosition[fieldNumber]];
    }
    return '\0';
}
//DAC string helpers
size_t LenOfStr(const char *start) // matches strlen return type
{
    //start of string, cursorr(pointer) postion scanning until null terminator
    const char *cursor = start;
    //not going to modify string at all btw
    while (*cursor)
    {
        cursor++;
    }
    return (cursor - start); //characters before the null terminator
    //daca gives length of 4

}
bool Stringcmpr(const char fieldstrg[], const char command[])
{
    //does token match the command??
    int index = 0;
    while (true)
    {
        if (fieldstrg[index] == '\0')
            break;
        //token has matched soo far
        if (fieldstrg[index] != command[index])
            //mismatch invalids command for neg =/ pos etc
            return false;
        index++;
    }
    return true; //true proceed with DAC command
    //checking if neg or positive etc
}
bool isCommand(USER_DATA *data, const char strCommand[], uint8_t minArguments)
{
    // ENOUGH ARGUEMENTS? CHECKING
    //checks the command and if it has enought arguements
    bool isValid = true;
    //checks command is valid
    int check = data->fieldCount;
    //how many tokens typed
    if (check < minArguments)
        isValid = false;

    else
    {
        if (!(LenOfStr(strCommand) == LenOfStr(data->buffer)))
            return false;
        //user doesnt type command right will be rejected has to be exact length
        int charIndex = 0;
        //comparing charcters of both strings

        while (true)
        {
            char tempVar = data->buffer[charIndex];
            //reads the character from buffer input
            if (tempVar == '\0')
                break;
            //above once  the characters match the command
            //completiuon of the comparision

            if (tempVar != strCommand[charIndex])
            //first mismatch invalids the command
            {
                isValid = false;
                break;
            }
            //moving to the nect character prevents infinte lloop
            charIndex++;
        }
    }
    return isValid; //final retuen if it is valid
}
