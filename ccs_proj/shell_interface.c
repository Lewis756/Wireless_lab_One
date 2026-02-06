// first file to work on !
// begin with shell an Uart
// shell to work on
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "wait.h"
#include <math.h>
#include "spi1.h"
#include "gpio.h"
#include "interface_functions.h"
#include "shell_interface.h"

// mcp dac accepts 12 bit (0-4095)
//uart shell top read mV ??300 mv = .300volts
// uart processing starts below
float mvToV(int16_t millivolts)
{
    //from uart interface
    float volts = 0.0f;
    //divide by 10^3 to get decimals
    volts = (float) millivolts / 1000.0f;
    //return volts after calculation
    return volts;
}

void sendDacI(uint16_t v)
{
    uint16_t data = 0;
    uint16_t voltage = 0;
    //insert equation here
    voltage = rawI + v; //(raw + slope * v)
    data |= voltage | 0x3000; //0011 A
    writeSpi1Data(data);
}

void sendDacQ(uint16_t v)
{
    uint16_t data = 0;
    uint16_t voltage = 0;
    //insert equation here
    voltage = rawQ + v; //(raw + slope * v)
    data |= voltage | 0xB000; //1011 B
    writeSpi1Data(data);
}

// now volts to dac code
// when dac gets code the op amp, the outputvoltage is measured
//when type a voltage compute the dac code
void shell(void)
{
    USER_DATA data;

    while (true)
    {
        getsUart0(&data);
        parseFields(&data);
        bool valid = false;

        if (isCommand(&data, "RAW", 2))
        {
            valid = true;

            char channel = getFieldChar(&data, 1);
            int32_t value = getFieldInteger(&data, 2);

            // Force uppercase (lower works too)
            if (channel >= 'a' && channel <= 'z')
                channel -= 32;

            if ((channel != 'I' && channel != 'Q')
                    || (value < 0 || value > 4095))
            {
                valid = false;
            }
            else
            {
                if (channel == 'I')
                {
                    rawI = value;
                    putsUart0("\r\n RAW I set \r\n");
                }
                else //Q channel
                {
                    rawQ = value;
                    putsUart0("\r\n RAW Q set \r\n");
                }
            }
        }

        if (isCommand(&data, "DC", 2))
        {
            valid = true;

            char channel = getFieldChar(&data, 1);
            int32_t value = getFieldInteger(&data, 2);

            // Force uppercase (lower works too)
            if (channel >= 'a' && channel <= 'z')
                channel -= 32;

            if ((channel != 'I' && channel != 'Q')
                    || (value < 0 || value > 4095))
            {
                valid = false;
            }
            else
            {
                if (channel == 'I')
                {
                    dcI = value;
                    putsUart0("\r\n DC I set \r\n");
                    sendDacI(dcI);
                }
                else //Q channel
                {
                    dcQ = value;
                    putsUart0("\r\n DC Q set \r\n");
                    sendDacQ(dcQ);
                }
            }
        }

        if (!valid)
        {
            putsUart0(": Invalid command \r\n");
        }
      //  putsUart0("\r\nOk\r\n");
    }
}
