#include "clock.h"
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "interface_fucntions.h"
#include "gpio.h"
#include "wait.h"
//initialize hardware

void intHW()
{
    initSystemClockTo40Mhz();
    initUart0();
}

int main()
{   
    intHW();
    shell();
}
