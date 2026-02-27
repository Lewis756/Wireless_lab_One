#include "wireless.h"
#include "gpio.h"
#include "spi1.h"
#include <stdbool.h>

MODE mode = dc;

#define DAC_ZERO_OFFSET 2125 //2125 dac value for  zero volts
#define I_GAIN 1960
#define Q_GAIN 1960

#define PI 3.14159265359f
#define SAMPLE_SINE_WAVE 4095 // samples for cycle
uint32_t frequency = 10000;
uint16_t sineDacTable[SAMPLE_SINE_WAVE];
#define H_GAIN 65536
#define FS 100000
int32_t RRCfilter[33] = { 0.0106 * H_GAIN, 0.0058 * H_GAIN, -0.0097 * H_GAIN,
                          -0.0214 * H_GAIN, -0.0188 * H_GAIN, 0.0030 * H_GAIN,
                          0.0327 * H_GAIN, 0.0471 * H_GAIN, 0.0265 * H_GAIN,
                          -0.0275 * H_GAIN,
                          -0.0852 * H_GAIN,
                          -0.0994 * H_GAIN,
                          -0.0321 * H_GAIN,
                          0.1190 * H_GAIN,
                          0.3110 * H_GAIN,
                          0.4717 * H_GAIN,
                          0.5343 * H_GAIN, // Center Tap
                          0.4717 * H_GAIN, 0.3110 * H_GAIN, 0.1190 * H_GAIN,
                          -0.0321 * H_GAIN, -0.0994 * H_GAIN, -0.0852 * H_GAIN,
                          -0.0275 * H_GAIN, 0.0265 * H_GAIN, 0.0471 * H_GAIN,
                          0.0327 * H_GAIN, 0.0030 * H_GAIN, -0.0188 * H_GAIN,
                          -0.0214 * H_GAIN, -0.0097 * H_GAIN, 0.0058 * H_GAIN,
                          0.0106 * H_GAIN, };

int16_t Iqam[16] = {
I_GAIN,
                     I_GAIN, I_GAIN, I_GAIN,
                     I_GAIN / 3,
                     I_GAIN / 3, I_GAIN / 3, I_GAIN / 3, -I_GAIN / 3, -I_GAIN
                             / 3,
                     -I_GAIN / 3, -I_GAIN / 3, -I_GAIN, -I_GAIN, -I_GAIN,
                     -I_GAIN };

int16_t Qqam[16] = {
Q_GAIN,
                     Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN,
                     Q_GAIN,
                     Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN,
                     Q_GAIN,
                     Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN,
                     Q_GAIN,
                     Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN };

int16_t Iqpsk[4] = { I_GAIN, I_GAIN, -I_GAIN, -I_GAIN };
int16_t Qqpsk[4] = { Q_GAIN, -Q_GAIN, Q_GAIN, -Q_GAIN };

int16_t Iepsk[8] = {
I_GAIN,                 // 0
        I_GAIN * 0.7071f,       // 45
        0,                      // 90
        -I_GAIN * 0.7071f,       // 135
        -I_GAIN,                 // 180
        -I_GAIN * 0.7071f,       // 225
        0,                      // 270
        I_GAIN * 0.7071f        // 315
};

int16_t Qepsk[8] = { 0,          // 0
        Q_GAIN * 0.7071f,       // 45
        Q_GAIN,                 // 90
        Q_GAIN * 0.7071f,       // 135
        0,                      // 180
        -Q_GAIN * 0.7071f,       // 225
        -Q_GAIN,                 // 270
        -Q_GAIN * 0.7071f        // 315
};

uint32_t phase = 0;
uint32_t delta_phase = 0;
uint32_t freq_o = 5000; //this is the frequency of the actual wave

void ldac_on()
{
    setPinValue(PORTF, 1, 1);
}

void ldac_off()
{
    setPinValue(PORTF, 1, 0);
}

uint32_t phaseSine = 0;
uint32_t phaseCosine = 0;

bool filter;

void setFilterStatus()
{
    filter ^= 1;
}

void writeDacAB(uint16_t rawI, uint16_t rawQ)
{
    //preserve 16 bit and and clear to write to correct channel
    uint16_t spitransferA = ((rawI & 0x0FFF) | 0x3000); //configured to A
    uint16_t spitransferB = ((rawQ & 0x0fff) | 0xB000); //configured to B
    // send to both now
    writeSpi1Data(spitransferA);
    writeSpi1Data(spitransferB);
    //latch them
    ldac_off();
    ldac_on();
}

uint8_t *txBuffer = 0;      // pointer to user input data
uint32_t txLength = 0;      // length in bytes
uint32_t txByteIndex = 0;   // current byte
uint8_t txBitIndex = 0;    // current bit inside byte

void setTransmitBuffer(uint8_t *data, uint32_t length)
{
    txBuffer = data;
    txLength = length;
    txByteIndex = 0;
    txBitIndex = 0;
}

volatile uint32_t symbolRate = 1000;        // default 1 ksym/sec
volatile uint32_t samplesPerSymbol = 100;   // FS / symbolRate
volatile uint32_t sampleTick = 0;

void setPhase(uint32_t fout)
{
    phase = (uint32_t) (((float) fout / (FS) * 4294967296)); //phase is a function of the desired wave freq/sampling freq * 2^32
}

void setSymbolRate(uint32_t rate)
{
    if (rate == 0)
        return;

    symbolRate = rate;

    samplesPerSymbol = FS / symbolRate;

    if (samplesPerSymbol == 0)
        samplesPerSymbol = 1;   // prevent invalid case

    sampleTick = 0;
}
uint32_t bufferI[33];
uint32_t bufferQ[33];

void convolve(int16_t Iup, int16_t Qup) //convolution from signals
{
    int32_t sumI = 0;
    int32_t sumQ = 0;

    int i;
    for (i = 32; i > 0; i--) //shift
    {
        bufferI[i] = bufferI[i - 1];
        bufferQ[i] = bufferQ[i - 1];
    }

    bufferI[0] = Iup;
    bufferQ[0] = Qup;

    int j;
    for (j = 0; j < 33; j++) // sumnation
    {
        sumI += (int32_t) bufferI[j] * RRCfilter[j];
        sumQ += (int32_t) bufferQ[j] * RRCfilter[j];
    }

    sumI >>= 15;
    sumQ >>= 15;

    uint16_t rawI = sumI + DAC_ZERO_OFFSET;
    uint16_t rawQ = sumQ + DAC_ZERO_OFFSET;

    writeDacAB(rawI, rawQ);
}

uint8_t count;
uint16_t bit_count;
void ISR()
{
    switch (mode)
    {
    case (sine):
    {
        delta_phase += phase;

        phaseCosine = ((delta_phase + 1073741824) >> 20); //+90 offset
        phaseSine = (delta_phase >> 20);

        rawI = sineDacTable[phaseCosine];
        rawQ = sineDacTable[phaseSine];

        writeDacAB(rawI, rawQ);
        break;
    }
    case (tone):
    {
        delta_phase += phase;

        theta = (delta_phase >> 20);

        rawI = sineDacTable[theta];
        rawQ = DAC_ZERO_OFFSET;

        writeDacAB(rawI, rawQ);
        break;
    }
    case (bpsk):
    {
        int16_t symbolI = 0;
        int16_t symbolQ = 0;

        if (filter)  //for filtering,
        {
            if ((count % 4) == 0) //Ensure there are 3 zeros between each convolution
            {
                if (txByteIndex >= txLength)
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t bit = (currentByte >> txBitIndex) & 0x01;

                symbolI = (bit == 0) ? -I_GAIN : I_GAIN;
                symbolQ = 0;

                txBitIndex++;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }
            else //send a zero between bits
            {
                symbolI = 0;
                symbolQ = 0;
            }
            convolve(symbolI, symbolQ);
            count++;
        }
        else //non filtering
        {
            if (sampleTick == 0) //Sample rate div
            {
                if (txByteIndex >= txLength) //wrap
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t bit = (currentByte >> txBitIndex) & 0x01;

                rawI = (bit == 0) ?
                        (DAC_ZERO_OFFSET - I_GAIN) : (DAC_ZERO_OFFSET + I_GAIN);
                rawQ = DAC_ZERO_OFFSET;

                txBitIndex++;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }

            writeDacAB(rawI, rawQ);

            sampleTick++;
            if (sampleTick >= samplesPerSymbol)
                sampleTick = 0;
        }
        break;
    }

    case (qpsk):
    {
        int16_t symbolI = 0;
        int16_t symbolQ = 0;

        if (filter)
        {
            if ((count % 4) == 0)
            {
                if (txByteIndex >= txLength)
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t sym = (currentByte >> txBitIndex) & 0x03;

                symbolI = Iqpsk[sym];
                symbolQ = Qqpsk[sym];

                txBitIndex += 2;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }
            else
            {
                symbolI = 0;
                symbolQ = 0;
            }

            convolve(symbolI, symbolQ);
            count++;
        }
        else
        {
            if (sampleTick == 0)
            {
                if (txByteIndex >= txLength)
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t sym = (currentByte >> txBitIndex) & 0x03;

                rawI = DAC_ZERO_OFFSET + Iqpsk[sym];
                rawQ = DAC_ZERO_OFFSET + Qqpsk[sym];

                txBitIndex += 2;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }

            writeDacAB(rawI, rawQ);

            sampleTick++;
            if (sampleTick >= samplesPerSymbol)
                sampleTick = 0;
            break;
        }
    }

    case (epsk):  // 8-PSK
    {
        int16_t symbolI = 0;
        int16_t symbolQ = 0;

        if (filter)
        {
            if ((count % 4) == 0)
            {
                if (txByteIndex >= txLength)
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t sym = (currentByte >> txBitIndex) & 0x07;

                symbolI = Iepsk[sym];
                symbolQ = Qepsk[sym];

                txBitIndex += 3;
                if (txBitIndex >= 8)
                {
                    txBitIndex -= 8;
                    txByteIndex++;
                }
            }
            else
            {
                symbolI = 0;
                symbolQ = 0;
            }

            convolve(symbolI, symbolQ);
            count++;
        }
        else
        {
            if (sampleTick == 0)
            {
                if (txByteIndex >= txLength)
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t sym = (currentByte >> txBitIndex) & 0x07;

                rawI = DAC_ZERO_OFFSET + Iepsk[sym];
                rawQ = DAC_ZERO_OFFSET + Qepsk[sym];

                txBitIndex += 3;
                if (txBitIndex >= 8)
                {
                    txBitIndex -= 8;
                    txByteIndex++;
                }
            }

            writeDacAB(rawI, rawQ);

            sampleTick++;
            if (sampleTick >= samplesPerSymbol)
                sampleTick = 0;
        }
        break;
    }

    case (qam):  // 16-QAM
    {
        int16_t symbolI = 0;
        int16_t symbolQ = 0;

        if (filter)
        {
            if ((count % 4) == 0)
            {
                if (txByteIndex >= txLength)
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t sym = (currentByte >> txBitIndex) & 0x0F;

                symbolI = Iqam[sym];
                symbolQ = Qqam[sym];

                txBitIndex += 4;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }
            else
            {
                symbolI = 0;
                symbolQ = 0;
            }

            convolve(symbolI, symbolQ);
            count++;
        }
        else
        {
            if (sampleTick == 0)
            {
                if (txByteIndex >= txLength)
                    txByteIndex = 0;

                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t symbol = (currentByte >> txBitIndex) & 0x0F;

                rawI = DAC_ZERO_OFFSET + Iqam[symbol];
                rawQ = DAC_ZERO_OFFSET + Qqam[symbol];

                txBitIndex += 4;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }

            writeDacAB(rawI, rawQ);
            sampleTick++;
            if (sampleTick >= samplesPerSymbol)
                sampleTick = 0;
            break;
        }
        break;
    }

    default:
        break;
    }
}

uint16_t rawI;
uint16_t rawQ;
float dcI;
float dcQ;

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

float mvToV(int16_t millivolts)
{
    //from uart interface
    float volts = 0.0f;
    //divide by 10^3 to get decimals
    volts = (float) millivolts / 1000.0f;
    //return volts after calculation
    return volts;
}

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
