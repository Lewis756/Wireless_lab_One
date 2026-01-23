// Funcitons to include in the shell interface
//From Embedded Lab PWM excluded to modify
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "uart0.h"
#include <stdint.h>
#include <stdbool.h>
#include "wait.h"
#include "uart0.h"
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "spi1.h"
#include "gpio.h"
#include <math.h>


//from uartpartone jimmie bud lab 
#define MAX_CHARS 80
#define MAX_FIELDS 5
//struct to fill the buffer

typedef struct _USER_DATA
{
char buffer[MAX_CHARS+1];
uint8_t fieldCount;
uint8_t fieldPosition[MAX_FIELDS];
char fieldType[MAX_FIELDS];
} USER_DATA;

 void getsUart0(USER_DATA* data)
{
   // initUart0();//initialize uart0
    int Count_ = 0;

    while(true)
    {
        char word;
        word = getcUart0();
        putcUart0(word);
      if((word == 8||word == 127) && Count_ > 0) //for backspace
      {
         Count_--;
      }
    //    char getcUart0();
      else if(word == 13 || word == 10) // checking for carriage return and now word ==10
    { // from lab 4 main
        data->buffer[Count_] = '\0';
        break;
    }
        //putcUart0(c);//equals zero

    else if( word >= 32) // space or character
    {
      //  char word = data->buffer[count];
        //store word in the data buffer
        data->buffer[Count_] = word;
        Count_ ++;
    }
    else if(Count_ == MAX_CHARS)
    {
        //putcUart0(c);
        data->buffer[MAX_CHARS] = '\0';
        break;
         //exits
    }
    }
}
 int stringToInt(char *str)
 {
     int value = 0;
     int i;
     for (i = 0; str[i] != '\0'; ++i )
     {
         value = value*10 +str[i]- '0';
     }
     return value;
 }

 int compString( char *str1, const char *str2)
 {
     //loop through both strings
     while(*str1 && *str1 == *str2)
     {
         //if(*str1 == *str2)
        // {
             str1++;
             str2++;
         }
         return (uint32_t)(*str1)-(uint32_t)(*str2); //when characters are not the same
    // }
 }

  //parse fields function
 void parseFields(USER_DATA *data)
 {
     //everyhting else will be a delimmter would be a comma
     bool addedFieldType = true;
     //assuming previous character is a delimmiter
     uint32_t i =0 ;
     data->fieldCount = 0;

     for(i = 0; (data->buffer[i] != '\0') && (data->fieldCount < MAX_FIELDS); i++)
     {
      if((data->buffer[i]  >= 'A' && data->buffer[i] <= 'Z') || (data->buffer[i] >= 'a' && data ->buffer[i] <= 'z'))
          //marks string alpha marks number n
      {
          if(addedFieldType)
          {
              data->fieldPosition[data->fieldCount] = i;
              data ->fieldType[data->fieldCount] = 'a';
              data ->fieldCount++;
              addedFieldType = false;
          }
      }
      else if(data ->buffer[i] >= '0' && data->buffer[i] <= '9')
      {
          if(addedFieldType)
          {
              data->fieldPosition[data->fieldCount] = i;
              data ->fieldType[data->fieldCount] = 'n';
              data ->fieldCount++;
              addedFieldType = false;
          }
      }
      else // for the delimeters
      {
              data->buffer[i] = '\0';
              addedFieldType = true;
      }
     }

 }
 char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
 {
     //return field requested
     if(fieldNumber <= (data->fieldCount)-1)
     {
         return &data->buffer[data->fieldPosition[fieldNumber]];
     }
     return '\0';
 }
 int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
 {
     //number in range
     //field type is zero
     if(fieldNumber >= data-> fieldCount || data ->fieldType[fieldNumber] !='n')
     {
         return  0;
     }
     else
     {
         return stringToInt(&data->buffer[data->fieldPosition[fieldNumber]]);
     }
 }

 bool isCommand(USER_DATA *data, const char strCommand[], uint8_t minArguments)
 {
     //
     if(minArguments >= (data->fieldCount -1))
     {
         if((compString(&data->buffer[0], strCommand)) == 0)
         {
             return true;
         }
         return false;
     }
     else{
         return false;
     }
}
