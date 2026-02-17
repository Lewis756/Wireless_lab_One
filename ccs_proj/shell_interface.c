// first file to work on !
// begin with shell an Uart
// shell to work on
//part sseven timer for wave  we need cases
//genral purpose timer0a
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
//streaming bits/ byes
// number f symnbols = total bits/ bits per Symbols different cases
//streaming
#define NUM_BYTES 90 //90bytes x 8Bits = 720/1bitpersymbol=bpsk
//qpsk 720/2 = 360 symbols
//8psk 720/3 240 symbols
//16Qam 720/4 = 180 symbols
//uint8_t dataBytes[NUM_BYTES];
// fror step 13 data values in memory
uint8_t StoredBpsk[8]; //BPSK array
uint16_t bpskSymbol = 0;
uint8_t mode = 0;

#define DAC_ZERO_OFFSET 2125 //2125 dac value for  zero volts
#define I_GAIN 1960


#define PI 3.14159265359f
#define SAMPLE_SINE_WAVE 4095 // samples for cycle
uint32_t frequency = 10000;
uint16_t sineDacTable[SAMPLE_SINE_WAVE];

#define FS 100000 //this number represents sample frequency, how many discrete points of the wave we calculate

uint32_t phase = 0;
uint32_t delta_phase = 0;
uint32_t freq_o = 5000; //this is the frequency of the actual wave

void setPhase(uint32_t fout)
{
    phase = (uint32_t) (((float) fout / FS) * 4294967295); //phase is a function of the desired wave freq/sampling freq * 2^32
}
void ldac_on()
{
    setPinValue(PORTF, 1, 1);
}

void ldac_off()
{
    setPinValue(PORTF, 1, 0);
}
//***************************************************//
//    TODO: WRITE ISR INIT, DOUBLE CHECK NCO LOGIC,
//    SIN/COS LUT, WRITE ENUM FOR "MODE"
//***************************************************//
uint32_t phaseSine = 0;
uint32_t phaseCosine = 0;
void writeDacAB(uint16_t rawI, uint16_t rawQ)
{
    //preserve 16 bit and and cleaer to write to correct channel
    uint16_t spitransferA = ((rawI & 0x0FFF) | 0x3000); //configured to A
    uint16_t spitransferB = ((rawQ & 0x0fff) | 0xB000); //configured to B
    // send to both now
    writeSpi1Data(spitransferA);
    writeSpi1Data(spitransferB);
    //latch them
    ldac_off();
    ldac_on();
}

void ISR() //pseudocode for frequency/NCO
{ //delatphase fixed point angleli
    switch(mode)
    {
        case 0: //sincos

            delta_phase += phase; //2^32delathase/2^20 to get 12 bits
            //Phase how much angle moves each smaple
            //deltaphase current angle
          //  phaseSine = (delta_phase >> 20);   // 12 bits 2^12 4096 matches sample
            //phase space 0-2^32-1
            //0-360degree represented
            //360 degree = 2^32
            //cosine 90 degree offset 90= 1/4 of the way through unit circle
            //90 = (1/4)*(2^32)/4 = 2^32/2^2 = 2^30 =1073741824
            phaseCosine = ((delta_phase + 1073741824) >> 20); //adds the offset and keeps 12 bits msb only
            phaseSine = ((delta_phase >> 20));
            rawI = sineDacTable[phaseCosine];
            rawQ = sineDacTable[phaseSine];

            writeDacAB(rawI, rawQ);
            //m,sb impoartant 1^12
            //2^12 4096 entries
            // theta = (delta_phase >> 22); //use first 10 bits
            // now channel A rawI cosine
            //channel B rawQ sine
            break;
        case 1: //bpsk
        if(StoredBpsk[bpskSymbol] == 0)
        {
            rawI = DAC_ZERO_OFFSET - I_GAIN;
            rawQ = DAC_ZERO_OFFSET;
            writeDacAB(rawI,rawQ);
        }
        else
        {
            rawI = DAC_ZERO_OFFSET + I_GAIN;
            rawQ = DAC_ZERO_OFFSET;
            writeDacAB(rawI,rawQ);
        }
            bpskSymbol++;
            bpskSymbol = bpskSymbol % 8;
            break;

        default:
              break;
    //  sin_val_i = LUT_sin[theta]; //LUT for sin
    //send the above value to the DAC
 }
}
//latest pairs
//uint16_t rawI = 2125; //0v
//uint16_t rawQ = 2125; //0v
//float dcI = 0.0f;
//float dcQ = 0.0f;
extern uint16_t rawI;
extern uint16_t rawQ;
extern float dcI;
extern float dcQ;

uint16_t voltageToDacCode(float v)
{
    // linear eqaution
    float dacCode = 2125.0f - 3940.0f * v;
    //clamp
    if (dacCode < 165.0f)
        dacCode = 165.0f;
    if (dacCode > 4095.0f)
        dacCode = 4095.0f;
    //clippings example .DC I 300 is v .300 and
    //2125 -3940*.300 gives 94s  so output is .3v DC\return value below
    return (uint16_t) roundf(dacCode);
}
//10khz full sine cycle below
// think of it as fancy array list of DAC codes
void sine_values() //table
{ //amplitude .5 to .5
    float amplitude = 0.5f;
    uint16_t i = 0;
    for (i = 0; i < SAMPLE_SINE_WAVE; i++)
    { // i = 0 to i =4095
      //sine periodic complete wave at 2Pi and divide evenly into 4095
      //i chooses what angle a small slice
        float angle = (2 * PI * i) / SAMPLE_SINE_WAVE;
        //A*SIN(angle) shrinkwwave .5 to -.5
        float wave_voltage = amplitude * sin(angle);
        //send voltage to go iunto made a dac value
        sineDacTable[i] = voltageToDacCode(wave_voltage);
    }
}

//github test
// 2125 = 0V
// 165 = +0.5V
// 4095 = -0.5V
//DAC A = i channel
//DAC B = q channel
//0.5 to dac?
//o
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
//random table to fill allray that holds data bytes
/*void fillDataBytes(void)        // 8 bit data bytes for transmission
{
    uint16_t i;
    for (i = 0; i < NUM_BYTES; i++)        // 8 bits is a byte
    {
        dataBytes[i] = (uint8_t) rand(); //keeps in range because type cast
    }
}*/
void sendDacI(float v)
{
    // uint16_t data = 0;
    //uint16_t voltage = 0;
    //insert equation here
    //voltage = 2125 + -3940 * v; //(raw + slope * v)

    uint16_t dacCode = voltageToDacCode(v);
    // data |= voltage | 0x3000; //0011 A
    uint16_t data = (dacCode & 0x0FFF) | 0x3000;
    writeSpi1Data(data);
}

void sendDacQ(float v)
{
    // uint16_t data = 0;
    //uint16_t voltage = 0;
    //insert equation here
    //voltage = rawQ; //(raw + slope * v)
    //we can fix now and use voltage ?? not raw
    //voltage = 2125 + -3940 * v; // no more raw?!

    uint16_t dacCode = voltageToDacCode(v);
    // data |= voltage | 0xB000; //1011 B
    uint16_t data = (dacCode & 0x0FFF) | 0xB000;
    writeSpi1Data(data);
}
void bitSymbol(uint8_t size)
{ //bpsk gets one bit
    uint8_t infoByte, BitIndex;
    if (size == 1)
    {   //85 is 01010101
        infoByte = 85;//hard coded nice change of vaues
        for(BitIndex =0; BitIndex < 8; BitIndex++)
        {
            StoredBpsk[BitIndex] = (infoByte >> BitIndex) & 1;
            //shift to read lsb to msb
        }
    }
}
// now volts to dac code
// when dac gets code the op amp, the outputvoltage is measured
//when type a voltage compute the dac code
// write to BOTH at the same time ! Raw?
/*
 void writeDacAB(uint16_t rawI, uint16_t rawQ)
 {
 //preserve 16 bit and and cleaer to write to correct channel
 uint16_t spitransferA = ((rawI & 0x0FFF) | 0x3000); //configured to A
 uint16_t spitransferB = ((rawQ & 0x0fff) | 0xB000); //configured to B
 // send to both now
 writeSpi1Data(spitransferA);
 writeSpi1Data(spitransferB);
 //latch them
 ldac_off();
 ldac_on();
 }
 */
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
