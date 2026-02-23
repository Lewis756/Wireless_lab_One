#include "clock.h"
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "interface_functions.h"
#include "gpio.h"
#include "wait.h"
#include "shell_interface.h"
#include "spi1.h"
#include "systick.h"
#include "wireless.h"

//initialize hardware
void intHW()
{
    initSystemClockTo80Mhz();
    initUart0();
    initSpi1(USE_SSI_FSS);
    setSpi1BaudRate(20e6, 40e6);
    setSpi1Mode(0, 0);
    enablePort(PORTF);
    selectPinPushPullOutput(PORTF, 1);
    setPinValue(PORTF, 1, 1); //set ldac on
    init_SysTick();
}

int main()
{   
    intHW();
    shell();
    putsUart0("\r\n READY \r\n");

}
