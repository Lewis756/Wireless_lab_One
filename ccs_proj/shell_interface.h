// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_INTERFACE_H_
#define SHELL_INTERFACE_H_

#include <stdbool.h>

uint8_t rawI;
uint8_t rawQ;
uint8_t dcI;
uint8_t dcQ;
void shell(void);
void sendDacI(uint16_t v);
void sendDacQ(uint16_t v);

#endif
