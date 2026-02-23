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
#include "wireless.h"

extern uint32_t delta_phase;
extern uint16_t bpskSymbol;
extern uint16_t ReadConstellation;

uint8_t input[256];

void shell(void)
{
    USER_DATA data;
    sine_values();
    int i;
    for(i = 0; i < 256; i++)
    {
        input[i] = rand();
    }
    setTransmitBuffer(input, 256);
    while (true)
    {
        putcUart0('>');

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
                    //sendDacI(dcI); to debug
                    //ldac_off();
                    //ldac_on();
                    writeDacAB(rawI, rawQ); //lathced lar in funciton
                }
                else //Q channel
                {
                    rawQ = value;
                    putsUart0("\r\n RAW Q set \r\n");
                    // sendDacQ(dcQ);
                    //ldac_off();
                    //ldac_on();
                    writeDacAB(rawI, rawQ);            //lathced in funciton alr
                }
            }
        }
        //dc to take in miliivoolts
        if (isCommand(&data, "DC", 2))
        {
            valid = true;
            float temp = 0;
            char channel = getFieldChar(&data, 1);
            int32_t value = getFieldInteger(&data, 2);

            // Force uppercase (lower works too)
            if (channel >= 'a' && channel <= 'z')
                channel -= 32;

            if ((channel != 'I' && channel != 'Q')
            //millivolts
                    || (value < -500 || value > 500))
            //(value < 0 || value > 4095))
            {
                valid = false;
            }
            else
            {
                temp = mvToV(value);
                if (channel == 'I')
                {

                    dcI = temp;
                    rawI = voltageToDacCode(dcI);
                    putsUart0("\r\n DC I set \r\n");

                    //sendDacI(dcI);
                    //   ldac_off();
                    // ldac_on();
                }
                else //Q channel
                {
                    dcQ = temp;
                    rawQ = voltageToDacCode(dcQ);
                    putsUart0("\r\n DC Q set \r\n");
                    // sendDacQ(dcQ);
                    //  ldac_off();
                    // ldac_on();
                }
                writeDacAB(rawI, rawQ);
            }
        }

        if (isCommand(&data, "PHASE", 2))
        {
            valid = true;

            int32_t value = getFieldInteger(&data, 1);

            setPhase(value);
            putsUart0("\r\n PHASE SET \r\n");
            mode = sine;
        }
        if (isCommand(&data, "SINCOS", 0))
        {
            valid = true;
            mode = sine;
            setPhase(10000);
            putsUart0("\r\n DISPLAYING SIN COS \r\n");

        }
        if (isCommand(&data, "BPSK", 0))
        {
            valid = true;
            mode = bpsk;
            putsUart0("\r\n DISPLAYING BPSK \r\n");
        }
        if (isCommand(&data, "QPSK", 0))
        {
            valid = true;
            mode = qpsk;
            putsUart0("\r\n DISPLAYING QPSK \r\n");
        }
        if (isCommand(&data, "EPSK", 0))
        {
            valid = true;
            mode = epsk;
            putsUart0("\r\n DISPLAYING EPSK \r\n");
        }
        if (isCommand(&data, "QAM", 0))
        {
            valid = true;
            mode = qam;
            putsUart0("\r\n DISPLAYING QAM \r\n");
        }
        if (isCommand(&data, "OFF", 0))
        {
            valid = true;
            putsUart0("\r\n OFF \r\n");
            mode = dc;
        }
        if (isCommand(&data, "RATE", 2))
        {
            valid = true;

            int32_t value = getFieldInteger(&data, 1);

            setSymbolRate(value);
            putsUart0("\r\n TRANSMISSION RATE SET \r\n");
        }
        if (isCommand(&data, "FILTER", 0))
        {
            valid = true;
            putsUart0("\r\n FILTERING TURNED ON \r\n");
         //   setfilterStatus(1);
        }
        if (!valid)
        {
            putsUart0("\r\n ERROR: Invalid command \r\n");
        }
        //  putsUart0("\r\nOk\r\n");
    }
}
