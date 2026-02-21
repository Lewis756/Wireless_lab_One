#include "wireless.h"
#include "gpio.h"
#include "spi1.h"
//.354 = 730 ish  maybe needed for mapping?
// number of symnbols = total bits/ bits per Symbols different cases
uint8_t StoredBpsk[32*2]; //BPSK array 32 symbols-ish
uint8_t StoredQpsk[16*2];//qpsk 2bits per symbol-ish
uint8_t StoredEpsk[10*2 + 1];//epsk 3 bits per symbol
uint8_t StoredQam[8*2];//qam 4 bits per symbol
//arrays to store them
uint16_t SymbolStored = 0; //isr index
uint16_t SymbolCount = 0;
uint8_t mode = 0;
uint16_t ReadConstellation = 0;
uint16_t bpskSymbol = 0;

#define DAC_ZERO_OFFSET 2125 //2125 dac value for  zero volts
#define I_GAIN 1960
#define Q_GAIN 1960

#define PI 3.14159265359f
#define SAMPLE_SINE_WAVE 4095 // samples for cycle
uint32_t frequency = 10000;
uint16_t sineDacTable[SAMPLE_SINE_WAVE];

int16_t Iqam[16] =
{
     I_GAIN,      I_GAIN,      I_GAIN,      I_GAIN,
     I_GAIN/3,    I_GAIN/3,    I_GAIN/3,    I_GAIN/3,
    -I_GAIN/3,   -I_GAIN/3,   -I_GAIN/3,   -I_GAIN/3,
    -I_GAIN,     -I_GAIN,     -I_GAIN,     -I_GAIN
};

int16_t Qqam[16] =
{
     Q_GAIN,      Q_GAIN/3,   -Q_GAIN/3,   -Q_GAIN,
     Q_GAIN,      Q_GAIN/3,   -Q_GAIN/3,   -Q_GAIN,
     Q_GAIN,      Q_GAIN/3,   -Q_GAIN/3,   -Q_GAIN,
     Q_GAIN,      Q_GAIN/3,   -Q_GAIN/3,   -Q_GAIN
};

int16_t Iqpsk[4] = {I_GAIN,I_GAIN,-I_GAIN,-I_GAIN};
int16_t Qqpsk[4] = {Q_GAIN,-Q_GAIN,Q_GAIN,-Q_GAIN};

int16_t Iepsk[8] = {
    I_GAIN,                 // 0°
    I_GAIN * 0.7071f,       // 45°
    0,                      // 90°
   -I_GAIN * 0.7071f,       // 135°
   -I_GAIN,                 // 180°
   -I_GAIN * 0.7071f,       // 225°
    0,                      // 270°
    I_GAIN * 0.7071f        // 315°
};

int16_t Qepsk[8] = {
    0,                      // 0°
    Q_GAIN * 0.7071f,       // 45°
    Q_GAIN,                 // 90°
    Q_GAIN * 0.7071f,       // 135°
    0,                      // 180°
   -Q_GAIN * 0.7071f,       // 225°
   -Q_GAIN,                 // 270°
   -Q_GAIN * 0.7071f        // 315°
};


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

uint32_t phaseSine = 0;
uint32_t phaseCosine = 0;

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

uint64_t r;
void ISR() //pseudocode for frequency/NCO
{ //delatphase fixed point angle
    r = rand();

    uint8_t iteration = 0;
    switch (mode)
    {

    case (sine): //sincos

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
    case (bpsk): //bpsk
        numberTransmitted(1, r);

        if (StoredBpsk[bpskSymbol] == 0)
        {
            rawI = DAC_ZERO_OFFSET - I_GAIN;
            rawQ = DAC_ZERO_OFFSET;
            writeDacAB(rawI, rawQ);
        }
        else
        {
            rawI = DAC_ZERO_OFFSET + I_GAIN;
            rawQ = DAC_ZERO_OFFSET;
            writeDacAB(rawI, rawQ);
        }
        bpskSymbol++;
        bpskSymbol = bpskSymbol % SymbolCount;
        break;
    case (qpsk):
    {
        numberTransmitted(2, r);
        static uint8_t sampleCounter = 0;

        if (sampleCounter == 0)
        {
            ReadConstellation = ReadConstellation % SymbolCount;
            iteration = StoredQpsk[ReadConstellation];
            rawI = DAC_ZERO_OFFSET + Iqpsk[iteration];
            rawQ = DAC_ZERO_OFFSET + Qqpsk[iteration];
            ReadConstellation++;
        }

        writeDacAB(rawI, rawQ);

        sampleCounter++;
        sampleCounter %= 2;

        break;
    }
    case (epsk):
    {
        numberTransmitted(3, r);

        static uint8_t sampleCounter = 0;

        if (sampleCounter == 0)
        {
            ReadConstellation = ReadConstellation % SymbolCount;
            iteration = StoredEpsk[ReadConstellation];
            rawI = DAC_ZERO_OFFSET + Iepsk[iteration];
            rawQ = DAC_ZERO_OFFSET + Qepsk[iteration];
            ReadConstellation++;
        }

        writeDacAB(rawI, rawQ);

        sampleCounter++;
        sampleCounter %= 2;

        break;
    }
    case (qam):
    {
        numberTransmitted(4, r);

        static uint8_t sampleCounter = 0;

        if (sampleCounter == 0)
        {
            ReadConstellation = ReadConstellation % SymbolCount;
            iteration = StoredQam[ReadConstellation];
            rawI = DAC_ZERO_OFFSET + Iqam[iteration];
            rawQ = DAC_ZERO_OFFSET + Qqam[iteration];
            ReadConstellation++;
        }

        writeDacAB(rawI, rawQ);

        sampleCounter++;
        sampleCounter %= 2;

        break;
    }
    default:
        break;
    }
}

//latest pairs
//uint16_t rawI = 2125; //0v
//uint16_t rawQ = 2125; //0v
//float dcI = 0.0f;
//float dcQ = 0.0f;
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
//hard coded values below

// used for validation of symbols creadted by number hex value passed
void numberTransmitted(uint8_t size, uint64_t number)
{
    uint8_t SymbolStored;// extracted symbol
    uint8_t BitIndex;//looping index

    if (size == 1)//bpsk
    {
        SymbolCount = 64; // 32 bits fromm number
        for (BitIndex = 0; BitIndex < 64; BitIndex++)
        {
            StoredBpsk[BitIndex] = (number >> BitIndex) & 0x1;//0001
        }
    }
    else if (size == 2)//qpsk
    {
        SymbolCount = 32;//
        for(BitIndex = 0; BitIndex < 32; BitIndex++)
        {
            SymbolStored = (number >> (BitIndex * 2)) & 0x03; //0011
            StoredQpsk[BitIndex] = SymbolStored;
        }
    }
    else if (size == 3) // 8psk
    {
        SymbolCount = 21;
        for(BitIndex = 0; BitIndex < 21; BitIndex++)
        {
            SymbolStored = (number >> (BitIndex * 3)) & 0x07; //0111
            StoredEpsk[BitIndex] = SymbolStored;
        }
    }
    else if (size == 4) // qam
    {
        SymbolCount = 16;
        for(BitIndex = 0; BitIndex < 16; BitIndex++)
        {
            SymbolStored = (number >> (BitIndex * 4)) & 0x0F; //1111
            StoredQam   [BitIndex] = SymbolStored;
        }
    }
}

