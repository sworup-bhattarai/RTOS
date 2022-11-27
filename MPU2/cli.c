#include <stdint.h>
#include <stdio.h>
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

#define size(x) (1 << x)
#define APmode1 (0x1 << 24) //APmode Access from privileged software only
#define fullAccess (0x3 << 24) //   0111 -> 0011 0000 0000 APmode 3
#define fullSize (0x1f << 1)   // 1 1111 -> 0011 1110 31 shifted 1 bit

#define region0  0x00000000
#define region1  0x00000001
#define region2  0x00000002
#define region3  0x00000003
#define region4  0x00000004
#define region5  0x00000005
#define region6  0x00000006

#define REGION_3  0x00000003
#define REGION_4  0x00000004
#define REGION_5  0x00000005
#define REGION_6  0x00000006




extern setTMPLbit();
extern setASPbit();
extern setPSP();
extern getMSP();
extern getPSP();
extern putcUart0();
extern putsUart0();
extern getcUart0();

uint32_t  *heap = ( uint32_t *)0x20007000;
uint32_t  *psp = (uint32_t*) 0x20008000;


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

    if (base < 2 || base > 32)
    {
        return buffer;
    }
    uint32_t n = value;
    uint32_t i = 0;
    while (n)
    {
        uint32_t r = n % base;
        if (r >= 10)
        {
            buffer[i++] = 65 + (r - 10);
        }
        else
        {
            buffer[i++] = 48 + r;
        }
        n = n / base;
    }

    // if the number is 0
    if (i == 0)
    {
        buffer[i++] = '0';
    }

    if (value < 0 && base == 10)
    {
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
// processstackDump
//-----------------------------------------------------------------------------
void processStackDump(void)
{
    char str[50];
    uint32_t  PC, LR, R0, R1, R2, R3, R12, xPSR;
    uint32_t *  val;
    PC = 0;
    val = getPSP();

    R0   = * val;
    R1   = *(val + 1);
    R2   = *(val + 2);
    R3   = *(val + 3);
    R12  = *(val + 4);
    LR   = *(val + 5);
    PC   = *(val + 6);
    xPSR   = *(val + 7);


    putsUart0("PC Value: 0x");
    selfIToA(PC , str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("LR Value: 0x");
    selfIToA(LR, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("R0 Value: 0x");
    selfIToA(R0, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("R1 Value: 0x");
    selfIToA(R1, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("R2 Value: 0x");
    selfIToA(R2, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("R3 Value: 0x");
    selfIToA(R3, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("R12 Value: 0x");
    selfIToA(R12, str, 16);
    putsUart0(str);
    putsUart0("\n");

    putsUart0("xPSR Value: 0x");
    selfIToA(xPSR, str, 16);
    putsUart0(str);
    putsUart0("\n");

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
    selfIToA(pid, str, 10);
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
    putsUart0("OFFENDING INSTRUCTION:\n0x");
    selfIToA(NVIC_MM_ADDR_R, str, 16);
    putsUart0(str);
    putsUart0("\n");



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
    putsUart0("MPU fault in process: ");
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

    processStackDump();

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
// malloc_from_heap
//-----------------------------------------------------------------------------
void * malloc_from_heap(int size_in_bytes)
{
    char str[50];
    void * p = heap;
    if(!(size_in_bytes % 1024 == 0))
    {
        size_in_bytes = size_in_bytes / 1024;
        size_in_bytes ++;
        size_in_bytes = size_in_bytes * 1024;
    }
    putsUart0("Heap Alocated From:\n0x");
    selfIToA(heap, str, 16);
    putsUart0(str);
    putsUart0("\n");

    heap = heap + size_in_bytes/4;
    if(heap > (0x20007FFF))
         p = NULL;

    putsUart0("Heap Alocated To:\n0x");
    selfIToA(heap, str, 16);
    putsUart0(str);
    putsUart0("\n\n\n");
    return p;


}

void backgroundEnable(void)
{  //                                   0000 0010        0000 0000
    NVIC_MPU_BASE_R = 0x00000000 | NVIC_MPU_BASE_VALID | region0; //REGION1
    NVIC_MPU_ATTR_R = fullSize | fullAccess | NVIC_MPU_ATTR_ENABLE | NVIC_MPU_ATTR_XN;
}   //                0011 1110   3000 0000         0000 0001           1000 0000

void allowFlashAccess(void)
{  //                                   0000 0010         0001
    NVIC_MPU_BASE_R =  0x00000000 | NVIC_MPU_BASE_VALID | region1;
    NVIC_MPU_ATTR_R = fullAccess | (0x11 << 1) | NVIC_MPU_ATTR_CACHEABLE | NVIC_MPU_ATTR_ENABLE;
} //                  3000 0000     0000 0110         0002 0000               0000 0001

void allowPeripheralAccess(void)
{  //                                   0000 0010          0010
    NVIC_MPU_BASE_R =  0x40000000 | NVIC_MPU_BASE_VALID | region2;
    NVIC_MPU_ATTR_R = fullAccess | (0x25 << 1) | NVIC_MPU_ATTR_SHAREABLE | NVIC_MPU_ATTR_BUFFRABLE | NVIC_MPU_ATTR_ENABLE;
}

void setupSramAccess(void)
{//                                                        0011
    NVIC_MPU_BASE_R = NVIC_MPU_BASE_VALID | 0x20000000 | region3 ;
//    NVIC_MPU_NUMBER_R = 2;
    NVIC_MPU_ATTR_R = 0x00000000| APmode1 | (0xC << 1) | NVIC_MPU_ATTR_ENABLE ;

    NVIC_MPU_BASE_R = NVIC_MPU_BASE_VALID | 0x20002000 | region4 ;
//    NVIC_MPU_NUMBER_R = 3;
    NVIC_MPU_ATTR_R = 0x00000000| APmode1 | (0xC << 1) | NVIC_MPU_ATTR_ENABLE ;

    NVIC_MPU_BASE_R = NVIC_MPU_BASE_VALID | 0x20004000 | region5 ;
//    NVIC_MPU_NUMBER_R = 4;
    NVIC_MPU_ATTR_R = 0x00000000| APmode1 | (0xC << 1) | NVIC_MPU_ATTR_ENABLE ;

    NVIC_MPU_BASE_R =  NVIC_MPU_BASE_VALID | 0x20006000 | region6 ;
//    NVIC_MPU_NUMBER_R = 4;
    NVIC_MPU_ATTR_R = 0x00000000| APmode1 | (0xC << 1) | NVIC_MPU_ATTR_ENABLE ;

}

void setSramAccessWindow(uint32_t baseAdd, uint32_t size_in_bytes) {

    uint32_t regionInWhichSrdBitsShouldBeSet = (baseAdd - 0x20000000) / 1024;
//    uint32_t subRegionPos = local_ceil((float) (baseAdd - 0x20000000) / 1024);
    uint32_t srd = local_ceil((float)size_in_bytes/1024);
    uint8_t i = 0;

    uint32_t srdRegionMask = 0;
    for (i = 0; i < srd; i++)
    {
        srdRegionMask |= (1 << i);
    }

    // 00000000 00000000 00000000 00000011
    // Offset byte 8 times the region number where the region is 0 based
    srdRegionMask <<= (regionInWhichSrdBitsShouldBeSet * 8);
    srdRegionMask <<= regionInWhichSrdBitsShouldBeSet;
//    srdRegionMask <<= subRegionPos;69

    NVIC_MPU_NUMBER_R = REGION_3;
    NVIC_MPU_ATTR_R &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |= (srdRegionMask & 0xFF) << 8;

    NVIC_MPU_NUMBER_R = REGION_4;
    NVIC_MPU_ATTR_R &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |= ((srdRegionMask >> 8) & 0xFF) << 8;

    NVIC_MPU_NUMBER_R = REGION_5;
    NVIC_MPU_ATTR_R &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |= ((srdRegionMask >> 16) & 0xFF) << 8;

    NVIC_MPU_NUMBER_R = REGION_6;
    NVIC_MPU_ATTR_R &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |= ((srdRegionMask >> 24) & 0xFF) << 8;
}

void enableMPU()
{
    NVIC_MPU_CTRL_R = 1;
}

void disableMPU()
{
    NVIC_MPU_CTRL_R = 0;
}

void test()
{
    uint32_t * p = (uint32_t *) 0x20007000;
    //setSramAccessWindow((uint32_t)p, 0x1000);
    *p = 100;
}


int main2(void)
{
    backgroundEnable();
    allowFlashAccess();
    allowPeripheralAccess();
    setupSramAccess();
    setSramAccessWindow(0x20000000, 0x8000);
    enableMPU();
    setTMPLbit();
    test();

    while(1){}
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

    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE | NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_MEM;
    NVIC_CFG_CTRL_R = NVIC_CFG_CTRL_DIV0;

//    uint32_t * psp = (uint32_t*) 0x20008000;
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
    uint8_t t = 1;
    while(t)
    {
        if(!getPinValue(PB0) & prev != 1) // USAGE FAULT
        {
            putsUart0("PB0 Pushed\n");
            uint8_t z = 0;
            uint8_t o = 1;
            o = o/z;

            prev = 1;
        }
        if(!getPinValue(PB1) & prev != 2)// BUS  FAULT
        {
            putsUart0("PB1 Pushed\n");
            setPinValue(GREEN_LED, 0);
            uint8_t * bus = (uint8_t *) 0x40000000;
            *bus = 96;
            prev = 2;
        }
        if(!getPinValue(PB2) & prev != 3) //MPU/HARD FAULT
        {
            backgroundEnable();
            allowFlashAccess();
            allowPeripheralAccess();
            setupSramAccess();
            setSramAccessWindow(0x20000000, 0x8000);
            enableMPU();
            setTMPLbit();
            uint32_t *p = malloc_from_heap(1024);
            *p = 1024;
            // putsUart0("PB2 Pushed\n");

            uint8_t * bus = (uint8_t *) 0x20007000;
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
            t = 0;
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
    putsUart0("Broke out!\n");
    setPSP(psp);
    setASPbit();

    main2();
}
