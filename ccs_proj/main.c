#include "clock.h"
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "interface_functions.h"
#include "gpio.h"
#include "wait.h"
#include "shell_interface.h"
#include "spi1.h"
//initialize hardware
void intHW()
{
    initSystemClockTo40Mhz();
    initUart0();
    initSpi1(1);
    setSpi1BaudRate(20e6, 40e6);
    setSpi1Mode(0, 0);
}

int main()
{   
    intHW();
    shell();
}
