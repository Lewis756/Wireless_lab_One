#include <stdint.h>
#include "systick.h"
#include "tm4c123gh6pm.h"

extern volatile uint32_t us = 0;

void init_SysTick()
{
    NVIC_ST_CTRL_R = 0; //off
    NVIC_ST_RELOAD_R = 399; //value
    NVIC_ST_CURRENT_R = 0; //current value is 0
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_ENABLE | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_CLK_SRC; //reenable using 40MHz
}
