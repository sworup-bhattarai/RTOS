#include <stdint.h>
#include <stdbool.h>
#include "uart0.h"
#include "clock.h"
#include "wait.h"
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
extern setASPbit();
extern setPSP();
extern getMSP();
extern getPSP();
extern putcUart0();
extern putsUart0();
extern getcUart0();
extern uint32_t __STACK_TOP;
uint32_t stack[256];




//-----------------------------------------------------------------------------
// swap
//-----------------------------------------------------------------------------
void swap(char *x, char *y)
{
    char t = *x;
    *x = *y;
    *y = t;
}
//-----------------------------------------------------------------------------
// reverse
//-----------------------------------------------------------------------------
char* reverse(char *buf, uint16_t i, uint16_t j)
{
    while (i < j)
    {
        swap(&buf[i++], &buf[j--]);
    }
    return buf;
}
//-----------------------------------------------------------------------------
// selfIToA ---- code implementation based on code from geeksforgeeks, https://www.geeksforgeeks.org/implement-itoa/
//-----------------------------------------------------------------------------
char* selfIToA(uint32_t value, char* buffer, uint16_t base)
{

    if (base < 2 || base > 32) {
        return buffer;
    }
    uint32_t n = value;
    uint32_t i = 0;
    while (n)
    {
        uint32_t r = n % base;
        if (r >= 10) {
            buffer[i++] = 65 + (r - 10);
        }
        else {
            buffer[i++] = 48 + r;
        }
        n = n / base;
    }

    // if the number is 0
    if (i == 0) {
        buffer[i++] = '0';
    }

    if (value < 0 && base == 10) {
        buffer[i++] = '-';
    }
    buffer[i] = '\0'; // null terminate string
    // reverse the string and return it
    return reverse(buffer, 0, i - 1);
}

//-----------------------------------------------------------------------------
// selfAtoi
//-----------------------------------------------------------------------------
int selfAtoi(char* str)//code implementation based on code from geeksforgeeks, https://www.geeksforgeeks.org/write-your-own-atoi/
{
    uint16_t result = 0;
    uint16_t i;

    for (i = 0; str[i] != '\0'; ++i)
    {
        result = result * 10 + str[i] - '0';
    }
    return result;

}
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
//-----------------------------------------------------------------------------
// stackDump
//-----------------------------------------------------------------------------
void stackDump(void)
{
    char str[50];
    uint32_t * row = (uint32_t*)__STACK_TOP;
    uint8_t i = 0;


    for(i = 0; i < 256; i++)
    {
        selfIToA(row[i], str, 16);
        putsUart0("0x");
        putsUart0(str);
        putsUart0("\n ");
    }
    return 0;

}


//-----------------------------------------------------------------------------
// HARD_fault
//-----------------------------------------------------------------------------
void HARD_fault(void)
{
    uint8_t pid;
    char str[50];
    uint32_t MSPval = 45;
    uint32_t PSPval = 75;
    MSPval = getMSP();
    PSPval = getPSP();


    pid = 0;
    selfIToA(pid, str, 16);
    putsUart0("Hard fault in process: \nPID: ");
    putsUart0(str);
    putsUart0("\n");



    putsUart0("MSP Value: 0x");
    selfIToA(MSPval, str, 16);
    putsUart0(str);
    putsUart0("\n");


    putsUart0("PSP Value: 0x");
    selfIToA(PSPval, str, 16);
    putsUart0(str);
    putsUart0("\n");
    putsUart0("HFAULT STAT FORCED FLAG:\n");
    if(0x40000000 & NVIC_HFAULT_STAT_FORCED)
    {
        putsUart0("Force fault TRUE\n0x");
        selfIToA(NVIC_HFAULT_STAT_FORCED, str, 16);
        putsUart0(str);
        putsUart0("\n");
    }
    else if(0x00000002 & NVIC_HFAULT_STAT_VECT)
    {
        putsUart0("Vector fault TRUE\n0x");
        selfIToA(NVIC_HFAULT_STAT_VECT, str, 16);
        putsUart0(str);
        putsUart0("\n");
    }


    while(true){}
}
//-----------------------------------------------------------------------------
// MPU_fault
//-----------------------------------------------------------------------------
void MPU_fault(void)
{
    uint8_t pid;
    pid = 0;
    char str[10];
    selfIToA(pid, str, 10);
    putsUart0("Bus fault in process: ");
    putsUart0(str);
    putsUart0("\n");
    uint32_t MSPval = 45;
    uint32_t PSPval = 75;
    MSPval = getMSP();
    PSPval = getPSP();
    uint32_t * var;
    var = getPSP();
    putsUart0("MSP Value: 0x");
    selfIToA(MSPval, str, 16);
    putsUart0(str);
    putsUart0("\n");


    putsUart0("PSP Value: 0x");
    selfIToA(PSPval, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("PC Value: 0x");
    selfIToA(*var, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("MFAULT FLAG:\n");
    selfIToA(NVIC_FAULT_STAT_R, str, 16);
    putsUart0(str);
    putsUart0("\n");

    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_MEMP;  //MPU Fault clear
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;   // pendSV trigger

    while(true){}
}
//-----------------------------------------------------------------------------
// BUS_fault
//-----------------------------------------------------------------------------
void BUS_fault(void)
{
    uint8_t pid , i;
    pid = 6;
    char str[10];
    selfIToA(pid, str, 10);
    putsUart0("Bus fault in process: ");
    putsUart0(str);
    putsUart0("\n");


    uint32_t PSPval = 75;
    PSPval = getPSP();
    uint32_t * var;
    var = getPSP();

    putsUart0("PSP Value: 0x");
    selfIToA(PSPval, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("PC Value: 0x\n");
    var++;
    for(i = 25; i >=1; i--)
    {
        selfIToA(stack[i-1], str, 16);
        putsUart0("0x");
        putsUart0(str);
        putsUart0("\n");
    }
    putsUart0("\n");

    uint8_t z = 0;
    uint8_t o = 1;
    o = o/z;

    while(true){}
}
//-----------------------------------------------------------------------------
// USAGE_fault
//-----------------------------------------------------------------------------
void USAGE_fault(void)
{
    uint8_t pid;
    pid = 0;
    char str[10];
    putsUart0("USAGE FAULT!\n");
    putsUart0("PID:");
    selfIToA(pid, str, 10);
    putsUart0(str);
    putsUart0("\n");
    while(true){}

}


//-----------------------------------------------------------------------------
// PENDSV_fault
//-----------------------------------------------------------------------------
void PENDSV_fault(void)
{
    uint8_t pid;
    pid = 0;
    char str[10];
    putsUart0("PENDSV FAULT!\n");
    putsUart0("PID:");
    putsUart0("PID:");
    selfIToA(pid, str, 10);
    putsUart0(str);
    putsUart0("\n");


    if((NVIC_FAULT_STAT_DERR & NVIC_FAULT_STAT_R) == NVIC_FAULT_STAT_DERR)
    {
        NVIC_FAULT_STAT_R  &= ~NVIC_FAULT_STAT_DERR;
    }
    if((NVIC_FAULT_STAT_IERR & NVIC_FAULT_STAT_R) == NVIC_FAULT_STAT_IERR)
    {
        NVIC_FAULT_STAT_R  &= ~NVIC_FAULT_STAT_IERR;
    }


    while(true){}
}



//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------
int main(void)
{

    // Initialize hardware
    initHw();
    initUart0();
    setUart0BaudRate(115200, 40e6);
    //GPIO_PORTD_LOCK_R = 0x4C4F434B;

    NVIC_SYS_HND_CTRL_R = NVIC_SYS_HND_CTRL_USAGE| NVIC_SYS_HND_CTRL_BUS| NVIC_SYS_HND_CTRL_MEM;
    NVIC_CFG_CTRL_R = NVIC_CFG_CTRL_DIV0;

    setPSP((uint32_t)&stack);
    setASPbit();


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
        if(!getPinValue(PB0) & prev != 1) // USAGE FAULT
        {
            putsUart0("PB0 Pushed\n");
            uint8_t z = 0;
            uint8_t o = 1;
            o = o/z;

            prev = 1;
        }
        if(!getPinValue(PB1) & prev != 2)// BUS AND HARD FAULT
        {
            putsUart0("PB1 Pushed\n");
            setPinValue(GREEN_LED, 0);
            uint8_t * bus = (uint8_t *) 0x690000000;
            *bus = 96;
            prev = 2;
        }
        if(!getPinValue(PB2) & prev != 3)
        {
            putsUart0("PB2 Pushed\n");

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
