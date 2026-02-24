#include "wireless.h"
#include "gpio.h"
#include "spi1.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

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
    I_GAIN, I_GAIN, I_GAIN, I_GAIN,
    I_GAIN / 3, I_GAIN / 3, I_GAIN / 3, I_GAIN / 3,
    -I_GAIN / 3, -I_GAIN / 3, -I_GAIN / 3, -I_GAIN / 3,
    -I_GAIN, -I_GAIN, -I_GAIN, -I_GAIN
};

int16_t Qqam[16] = {
    Q_GAIN, Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN,
    Q_GAIN, Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN,
    Q_GAIN, Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN,
    Q_GAIN, Q_GAIN / 3, -Q_GAIN / 3, -Q_GAIN
};

int16_t Iqpsk[4] = { I_GAIN, I_GAIN, -I_GAIN, -I_GAIN };
int16_t Qqpsk[4] = { Q_GAIN, -Q_GAIN, Q_GAIN, -Q_GAIN };

int16_t Iepsk[8] = {
    I_GAIN,                  // 0
    (int16_t)(I_GAIN * 0.7071f),        // 45
    0,                       // 90
    (int16_t)(-I_GAIN * 0.7071f),       // 135
    -I_GAIN,                 // 180
    (int16_t)(-I_GAIN * 0.7071f),       // 225
    0,                       // 270
    (int16_t)(I_GAIN * 0.7071f)         // 315
};

int16_t Qepsk[8] = {
    0,                       // 0
    (int16_t)(Q_GAIN * 0.7071f),        // 45
    Q_GAIN,                  // 90
    (int16_t)(Q_GAIN * 0.7071f),        // 135
    0,                       // 180
    (int16_t)(-Q_GAIN * 0.7071f),       // 225
    -Q_GAIN,                 // 270
    (int16_t)(-Q_GAIN * 0.7071f)        // 315
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

void writeDacAB(uint16_t rawI, uint16_t rawQ)
{
    uint16_t spitransferA = ((rawI & 0x0FFF) | 0x3000); // A
    uint16_t spitransferB = ((rawQ & 0x0fff) | 0xB000); // B
    writeSpi1Data(spitransferA);
    writeSpi1Data(spitransferB);
    ldac_off();
    ldac_on();
}

uint8_t *txBuffer = 0;      // pointer to user input
uint32_t txLength = 0;      // byte length
uint32_t txByteIndex = 0;   // current byte
uint8_t txBitIndex = 0;     // current bit of byte

void setTransmitBuffer(uint8_t *data, uint32_t length)
{
    txBuffer = data;
    txLength = length;
    txByteIndex = 0;
    txBitIndex = 0;
}

volatile uint32_t symbolRate = 1000;
volatile uint32_t samplesPerSymbol = 100;   // FS / symbolRate
volatile uint32_t sampleTick = 0;

void setPhase(uint32_t fout)
{
    phase = (uint32_t)(((float)fout / (FS) * 4294967296.0f)); // fo/fs * 2^32
}

void setSymbolRate(uint32_t rate) //number of symbols sent/sec
{
    if (rate == 0)
        return;

    symbolRate = rate;
    samplesPerSymbol = FS / symbolRate;

    if (samplesPerSymbol == 0)
        samplesPerSymbol = 1;

    sampleTick = 0;
}

bool filter = false; //filter is default off

uint16_t rawI = DAC_ZERO_OFFSET;
uint16_t rawQ = DAC_ZERO_OFFSET;
float dcI = 0.0f;
float dcQ = 0.0f;

uint16_t voltageToDacCode(float v)
{
    float dacCode = 2125.0f - 3940.0f * v;
    if (dacCode < 165.0f)
        dacCode = 165.0f;
    if (dacCode > 4095.0f)
        dacCode = 4095.0f;
    return (uint16_t)roundf(dacCode);
}

void sine_values()
{
    float amplitude = 0.5f;
    uint16_t i = 0;
    for (i = 0; i < SAMPLE_SINE_WAVE; i++)
    {
        float angle = (2.0f * PI * i) / SAMPLE_SINE_WAVE;
        float wave_voltage = amplitude * sinf(angle);
        sineDacTable[i] = voltageToDacCode(wave_voltage);
    }
}

float mvToV(int16_t millivolts)
{
    float volts = 0.0f;
    volts = (float)millivolts / 1000.0f;
    return volts;
}

void sendDacI(float v)
{
    uint16_t dacCode = voltageToDacCode(v);
    uint16_t data = (dacCode & 0x0FFF) | 0x3000;
    writeSpi1Data(data);
}

void sendDacQ(float v)
{
    uint16_t dacCode = voltageToDacCode(v);
    uint16_t data = (dacCode & 0x0FFF) | 0xB000;
    writeSpi1Data(data);
}

#define RRC_TAPS   33
#define UPSAMPLE   4

static int16_t iHist[RRC_TAPS];
static int16_t qHist[RRC_TAPS];

static uint8_t upPhase = 0;
static int16_t symI = 0;
static int16_t symQ = 0;

static void rrcInit(void)
{
    uint8_t i;
    for (i = 0; i < RRC_TAPS; i++)
    {
        iHist[i] = 0;
        qHist[i] = 0;
    }
}

static void rrcShiftIn(int16_t newI, int16_t newQ)
{
    int i;
    for (i = (RRC_TAPS - 1); i > 0; i--)
    {
        iHist[i] = iHist[i - 1];
        qHist[i] = qHist[i - 1];
    }
    iHist[0] = newI;
    qHist[0] = newQ;
}

static uint16_t clamp12(int32_t x)
{
    if (x < 0) return 0;
    if (x > 4095) return 4095;
    return (uint16_t)x;
}

static void rrcOutputToDac(void)
{
    int64_t accI = 0;
    int64_t accQ = 0;
    uint8_t i;

    for (i = 0; i < RRC_TAPS; i++)
    {
        accI += (int64_t)iHist[i] * (int64_t)RRCfilter[i];
        accQ += (int64_t)qHist[i] * (int64_t)RRCfilter[i];
    }

    // taps scaled by H_GAIN = 2^16
    {
        int32_t outI = (int32_t)(accI >> 16);
        int32_t outQ = (int32_t)(accQ >> 16);

        int32_t dacI = (int32_t)DAC_ZERO_OFFSET + outI;
        int32_t dacQ = (int32_t)DAC_ZERO_OFFSET + outQ;

        writeDacAB((clamp12(dacI)), (clamp12(dacQ)));
    }
}

static void getNextSymbolIQ(void)
{
    if (txBuffer == 0 || txLength == 0)
    {
        symI = 0;
        symQ = 0;
        return;
    }

    if (txByteIndex >= txLength)
        txByteIndex = 0;

    {
        uint8_t currentByte = txBuffer[txByteIndex];

        if (mode == bpsk)
        {
            uint8_t bit = (currentByte >> txBitIndex) & 0x01;
            symI = bit ? +I_GAIN : -I_GAIN;
            symQ = 0;

            txBitIndex++;
            if (txBitIndex >= 8)
            {
                txBitIndex = 0;
                txByteIndex++;
            }
        }
        else if (mode == qpsk)
        {
            uint8_t symbol = (currentByte >> txBitIndex) & 0x03;
            symI = Iqpsk[symbol];
            symQ = Qqpsk[symbol];

            txBitIndex += 2;
            if (txBitIndex >= 8)
            {
                txBitIndex = 0;
                txByteIndex++;
            }
        }
        else if (mode == epsk)
        {
            uint8_t symbol = (currentByte >> txBitIndex) & 0x07;
            symI = Iepsk[symbol];
            symQ = Qepsk[symbol];

            txBitIndex += 3;
            if (txBitIndex >= 8)
            {
                txBitIndex -= 8; // leftover bits
                txByteIndex++;
            }
        }
        else if (mode == qam)
        {
            uint8_t symbol = (currentByte >> txBitIndex) & 0x0F;
            symI = Iqam[symbol];
            symQ = Qqam[symbol];

            txBitIndex += 4;
            if (txBitIndex >= 8)
            {
                txBitIndex = 0;
                txByteIndex++;
            }
        }
        else
        {
            symI = 0;
            symQ = 0;
        }
    }
}

void setFilterStatus(void)
{
    filter = !filter;   // toggle on/off

    rrcInit();
    upPhase = 0;
    symI = 0;
    symQ = 0;

    sampleTick = 0;
}

void ISR(void)
{
    if (mode == sine) //sin/cos waves
    {
        delta_phase += phase;

        phaseCosine = ((delta_phase + 1073741824) >> 20);
        phaseSine   = (delta_phase >> 20);

        rawI = sineDacTable[phaseCosine];
        rawQ = sineDacTable[phaseSine];

        writeDacAB(rawI, rawQ);
        return;
    }

    if ((mode == bpsk) || (mode == qpsk) || (mode == epsk) || (mode == qam))
    {
        if (filter) //do filtering if its on
        {
            if (sampleTick == 0)
            {
                getNextSymbolIQ();
                upPhase = 0;
            }

            if (upPhase == 0)
                rrcShiftIn(symI,symQ);
            else
                rrcShiftIn(0, 0);

            upPhase++;
            if (upPhase >= UPSAMPLE)
                upPhase = 0;

            rrcOutputToDac();

            sampleTick++;
            if (sampleTick >= samplesPerSymbol)
                sampleTick = 0;

            return;
        }
    }

    if (mode == bpsk) //unfiltered
    {
        if (sampleTick == 0)
        {
            if (txByteIndex >= txLength)
                txByteIndex = 0;

            {
                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t bit = (currentByte >> txBitIndex) & 0x01;

                if (bit == 0)
                    rawI = DAC_ZERO_OFFSET - I_GAIN;
                else
                    rawI = DAC_ZERO_OFFSET + I_GAIN;

                rawQ = DAC_ZERO_OFFSET;

                txBitIndex++;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }
        }

        writeDacAB(rawI, rawQ);
        sampleTick++;
        if (sampleTick >= samplesPerSymbol)
            sampleTick = 0;
    }
    else if (mode == qpsk)
    {
        if (sampleTick == 0)
        {
            if (txByteIndex >= txLength)
                txByteIndex = 0;

            {
                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t symbol = (currentByte >> txBitIndex) & 0x03;

                rawI = DAC_ZERO_OFFSET + Iqpsk[symbol];
                rawQ = DAC_ZERO_OFFSET + Qqpsk[symbol];

                txBitIndex += 2;
                if (txBitIndex >= 8)
                {
                    txBitIndex = 0;
                    txByteIndex++;
                }
            }
        }

        writeDacAB(rawI, rawQ);
        sampleTick++;
        if (sampleTick >= samplesPerSymbol)
            sampleTick = 0;
    }
    else if (mode == epsk)
    {
        if (sampleTick == 0)
        {
            if (txByteIndex >= txLength)
                txByteIndex = 0;

            {
                uint8_t currentByte = txBuffer[txByteIndex];
                uint8_t symbol = (currentByte >> txBitIndex) & 0x07;

                rawI = DAC_ZERO_OFFSET + Iepsk[symbol];
                rawQ = DAC_ZERO_OFFSET + Qepsk[symbol];

                txBitIndex += 3;
                if (txBitIndex >= 8)
                {
                    txBitIndex -= 8;
                    txByteIndex++;
                }
            }
        }

        writeDacAB(rawI, rawQ);
        sampleTick++;
        if (sampleTick >= samplesPerSymbol)
            sampleTick = 0;
    }
    else if (mode == qam)
    {
        if (sampleTick == 0)
        {
            if (txByteIndex >= txLength)
                txByteIndex = 0;

            {
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
        }

        writeDacAB(rawI, rawQ);
        sampleTick++;
        if (sampleTick >= samplesPerSymbol)
            sampleTick = 0;
    }
    else
    {
        // dc / off
        writeDacAB(rawI, rawQ);
    }
}
