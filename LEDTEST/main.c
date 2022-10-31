#include <stdint.h>
#include <stdbool.h>

#include "clock.h"
#include "wait.h"
#include "uart0.h"
#include "gpio.h"
#include "tm4c123gh6pm.h"

#define BLUE_LED PORTF,2
#define RED_LED PORTE, 0
#define GREEN_LED PORTA,4
#define YELLOW_LED PORTA,3
#define ORANGE_LED PORTA,2
#define PB0 PORTD,6
#define PB1 PORTD,7
#define PB2 PORTC,4
#define PB3 PORTC,5
#define PB4 PORTC,6
#define PB5 PORTC,7

void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTA);
    enablePort(PORTC);
    enablePort(PORTD);
    enablePort(PORTE);
    enablePort(PORTF);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(BLUE_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(YELLOW_LED);
    selectPinPushPullOutput(ORANGE_LED);

    setPinCommitControl(PB1);

    selectPinDigitalInput(PB0);
    selectPinDigitalInput(PB1);
    selectPinDigitalInput(PB2);
    selectPinDigitalInput(PB3);
    selectPinDigitalInput(PB4);
    selectPinDigitalInput(PB5);
    enablePinPullup(PB0);
    enablePinPullup(PB1);
    enablePinPullup(PB2);
    enablePinPullup(PB3);
    enablePinPullup(PB4);
    enablePinPullup(PB5);


}


int main(void)
{
    // Initialize hardware
    initHw();
    initUart0();
    setUart0BaudRate(115200, 40e6);
    GPIO_PORTD_LOCK_R = 0x4C4F434B;


    setPinValue(BLUE_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(RED_LED, 1);
    waitMicrosecond(100000);
    setPinValue(YELLOW_LED, 1);
    waitMicrosecond(100000);
    setPinValue(ORANGE_LED, 1);

    uint8_t prev = 0;
    while(true)
    {
        if(!getPinValue(PB0) & prev != 1)
        {
            putsUart0("PB0 Pushed\n");
            setPinValue(BLUE_LED, 0);

            prev = 1;
        }
        if(!getPinValue(PB1) & prev != 2)
        {
            putsUart0("PB1 Pushed\n");
            setPinValue(GREEN_LED, 0);

            prev = 2;
        }
        if(!getPinValue(PB2) & prev != 3)
        {
            putsUart0("PB2 Pushed\n");
            setPinValue(BLUE_LED, 1);
            setPinValue(GREEN_LED, 1);
            setPinValue(RED_LED, 0);
            setPinValue(YELLOW_LED, 1);
            setPinValue(ORANGE_LED, 0);

            prev = 3;
        }
        if(!getPinValue(PB3) & prev != 4)
        {
            putsUart0("PB3 Pushed\n");
            setPinValue(BLUE_LED, 0);
            setPinValue(GREEN_LED, 1);
            setPinValue(RED_LED, 0);
            setPinValue(YELLOW_LED, 1);
            setPinValue(ORANGE_LED, 0);

            prev = 4;
        }
        if(!getPinValue(PB4) & prev != 5)
        {
            putsUart0("PB4 Pushed\n");
            setPinValue(BLUE_LED, 1);
            setPinValue(GREEN_LED, 0);
            setPinValue(RED_LED, 1);
            setPinValue(YELLOW_LED, 0);
            setPinValue(ORANGE_LED, 1);
            prev = 5;
        }
        if(!getPinValue(PB5) & prev != 6)
        {
            putsUart0("PB5 Pushed\n");
            setPinValue(BLUE_LED, 1);
            setPinValue(GREEN_LED, 1);
            setPinValue(RED_LED, 1);
            setPinValue(YELLOW_LED, 1);
            setPinValue(ORANGE_LED, 1);
            prev = 6;
        }
    }

}
