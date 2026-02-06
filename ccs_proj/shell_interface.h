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

uint16_t rawI;
uint16_t rawQ;
float dcI;
float dcQ;
void shell(void);
void sendDacI(float v);
void sendDacQ(float v);

#endif
