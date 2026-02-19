#ifndef WIRELESS_H_
#define WIRELESS_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    sine = 0,
    bpsk = 1
} MODE;

uint16_t rawI;
uint16_t rawQ;
float dcI;
float dcQ;
uint8_t mode;
uint16_t data;
void sendDacI(float v);
void sendDacQ(float v);
void writeDacAB(uint16_t rawI, uint16_t rawQ);
void ISR();
void bitSymbol(uint8_t size);


#endif /* WIRELESS_H_ */
