#ifndef WIRELESS_H_
#define WIRELESS_H_

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

typedef enum
{
    dc = 100, sine = 0, bpsk = 1, qpsk = 2, epsk = 3, qam = 4, tone = 5
} MODE;

extern uint16_t rawI;
extern uint16_t rawQ;
extern float dcI;
extern float dcQ;
extern uint8_t mode;
extern uint16_t data;
void setTransmitBuffer(uint8_t *data, uint32_t length);
void sendDacI(float v);
void sendDacQ(float v);
void writeDacAB(uint16_t rawI, uint16_t rawQ);
void ISR();
float mvToV(int16_t millivolts);
void setPhase(uint32_t fout);
void sine_values();
uint16_t voltageToDacCode(float v);
void setSymbolRate(uint32_t rate);
void setFilterStatus(void);
void convolve(int16_t Iup, int16_t Qup); //convolution from signals

#endif /* WIRELESS_H_ */
