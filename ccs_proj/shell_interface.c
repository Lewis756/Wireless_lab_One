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

void shell(void)
{
    USER_DATA data;
    sine_values();
    //fillDataBytes();
    bitSymbol(1);

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

        }
        if (isCommand(&data, "SINCOS", 0))
        {
            valid = true;
            mode = 0;
            delta_phase = 0;
            putsUart0("\r\n DISPLAYING SIN COS \r\n");

        }
        if (isCommand(&data, "BPSK", 0))
        {
            valid = true;
            mode = 1;
            bpskSymbol = 0;
            putsUart0("\r\n DISPLAYING BPSK \r\n");

        }
        if (!valid)
        {
            putsUart0(": Invalid command \r\n");
        }
        //  putsUart0("\r\nOk\r\n");
    }
}
