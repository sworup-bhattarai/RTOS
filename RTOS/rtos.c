// RTOS Framework - Fall 2022
// J Losh

// Student Name: Sworup Bhattarai

// TO DO: Add your name(s) on this line.
//        Do not include your ID number(s) in the file.

// Please do not change any function name in this code or the thread priorities

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// 6 Pushbuttons and 5 LEDs, UART
// LEDs on these pins:
// Blue:   PF2 (on-board)
// Red:    PE0 (lengthy and general)
// Orange: PA2 (idle)
// Yellow: PA3 (oneshot and general)
// Green:  PA4 (flash4hz)
// PBs on these pins
// PB0:    PD6 (set red, toggle yellow)
// PB1:    PD7 (clear red, post flash_request semaphore)
// PB2:    PC4 (restart flash4hz)
// PB3:    PC5 (stop flash4hz, uncoop)
// PB4:    PC6 (lengthy priority increase)
// PB5:    PC7 (errant)
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1
// Memory Protection Unit (MPU):
//   Region to allow peripheral access (RW) or a general all memory access (RW)
//   Region to allow flash access (XRW)
//   Regions to allow 32 1KiB SRAM access (RW or none)

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "parseString.h"
#include "clock.h"
#include "gpio.h"
#include "uart0.h"
#include "wait.h"
// TODO: Add header files here for your strings functions, ...

#define BLUE_LED    PORTF,2 // on-board blue LED
#define RED_LED     PORTE,0 // off-board red LED
#define ORANGE_LED  PORTA,2 // off-board orange LED
#define YELLOW_LED  PORTA,3 // off-board yellow LED
#define GREEN_LED   PORTA,4 // off-board green LED

#define PB0         PORTD,6 // off-board push button 0
#define PB1         PORTD,7 // off-board push button 1
#define PB2         PORTC,4 // off-board push button 2
#define PB3         PORTC,5 // off-board push button 3
#define PB4         PORTC,6 // off-board push button 4
#define PB5         PORTC,7 // off-board push button 5

#define size(x)     (1 << x)
#define APmode1     (0x1 << 24) //APmode Access from privileged software only
#define fullAccess  (0x3 << 24) //   0111 -> 0011 0000 0000 APmode 3
#define fullSize    (0x1f << 1)   // 1 1111 -> 0011 1110 31 shifted 1 bit

#define region0     0x00000000
#define region1     0x00000001
#define region2     0x00000002
#define region3     0x00000003
#define region4     0x00000004
#define region5     0x00000005
#define region6     0x00000006

//-----------------------------------------------------------------------------
// Extern Functions
//-----------------------------------------------------------------------------
extern setTMPLbit();
extern setASPbit();
extern setPSP();
extern getMSP();
extern getPSP();
extern putcUart0();
extern putsUart0();
extern getcUart0();
extern pushToPSP();
extern pushR4toR11();
extern popR11toR4();

//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// function pointer
typedef void (*_fn)();

// semaphore
#define MAX_SEMAPHORES 5
#define MAX_QUEUE_SIZE 5
typedef struct _semaphore
{
    uint16_t count;
    uint16_t queueSize;
    uint32_t processQueue[MAX_QUEUE_SIZE]; // store task index here
} semaphore;

semaphore semaphores[MAX_SEMAPHORES];
#define keyPressed 1
#define keyReleased 2
#define flashReq 3
#define resource 4

// task
#define STATE_INVALID    0 // no task
#define STATE_UNRUN      1 // task has never been run
#define STATE_READY      2 // has run, can resume at any time
#define STATE_DELAYED    3 // has run, but now awaiting timer
#define STATE_BLOCKED    4 // has run, but now blocked by semaphore

#define MAX_TASKS 12       // maximum number of valid tasks
uint8_t taskCurrent = 0;   // index of last dispatched task
uint8_t taskCount = 0;     // total number of valid tasks
uint32_t  *heap = ( uint32_t *)0x20001000;

// REQUIRED: add store and management for the memory used by the thread stacks
//           thread stacks must start on 1 kiB boundaries so mpu can work correctly

struct _tcb
{
    uint8_t state;                 // see STATE_ values above
    void *pid;                     // used to uniquely identify thread
    void *spInit;                  // original top of stack
    void *sp;                      // current stack pointer
    int8_t priority;               // 0=highest to 7=lowest
    uint32_t ticks;                // ticks until sleep complete
    uint32_t srd;                  // MPU subregion disable bits (one per 1 KiB)
    char name[16];                 // name of task used in ps command
    void *semaphore;               // pointer to the semaphore that is blocking the thread
} tcb[MAX_TASKS];


//-----------------------------------------------------------------------------
// Memory Manager and MPU Funcitons
//-----------------------------------------------------------------------------

// TODO: add your malloc code here and update the SRD bits for the current thread
void * mallocFromHeap(uint32_t size_in_bytes)
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

// REQUIRED: add your MPU functions here
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

void enableMPU()
{
    NVIC_MPU_CTRL_R = 1;
}

void disableMPU()
{
    NVIC_MPU_CTRL_R = 0;
}

// REQUIRED: initialize MPU here
void initMpu(void)
{
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE | NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_MEM;

    backgroundEnable();
    allowFlashAccess();
    allowPeripheralAccess();
    //setupSramAccess();
    enableMPU();

    // REQUIRED: call your MPU functions here
}

//-----------------------------------------------------------------------------
// RTOS Kernel Functions
//-----------------------------------------------------------------------------

// REQUIRED: initialize systick for 1ms system timer
void initRtos()
{
    uint8_t i;
    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++)
    {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
    }
}

// REQUIRED: Implement prioritization to 8 levels
int rtosScheduler()
{
    bool ok;
    static uint8_t task = 0xFF;
    ok = false;
    while (!ok)
    {
        task++;
        if (task >= MAX_TASKS)
            task = 0;
        ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
    }
    return task;
}

bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes)
{
    bool ok = false;
    uint8_t i = 0;
    bool found = false;
    // REQUIRED:



    if (taskCount < MAX_TASKS)
    {
        // make sure fn not already in list (prevent reentrancy)
        while (!found && (i < MAX_TASKS))
        {
            found = (tcb[i++].pid ==  fn);
        }
        if (!found) // add task if room in task list----DONE
        {
            // find first available tcb record
            i = 0;
            while (tcb[i].state != STATE_INVALID) {i++;}
            tcb[i].state = STATE_UNRUN;
            stringCopy(tcb[i].name, name);// store the thread name----DONE
            tcb[i].pid = fn;
            tcb[i].sp = (uint8_t *) mallocFromHeap(stackBytes)+ stackBytes; // allocate stack space and store top of stack in sp and spInit----DONE
            tcb[i].spInit = tcb[i].sp;
            tcb[i].priority = priority;
            tcb[i].srd = 0;
            // increment task count
            taskCount++;
            ok = true;
        }
    }
    // REQUIRED: allow tasks switches again
    return ok;
}

// REQUIRED: modify this function to restart a thread
void restartThread(_fn fn)
{
}

// REQUIRED: modify this function to stop a thread
// REQUIRED: remove any pending semaphore waiting
// NOTE: see notes in class for strategies on whether stack is freed or not
void stopThread(_fn fn)
{
}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
}

bool createSemaphore(uint8_t semaphore, uint8_t count)
{
    bool ok = (semaphore < MAX_SEMAPHORES);
    {
        semaphores[semaphore].count = count;
    }
    return ok;
}


// REQUIRED: modify this function to start the operating system
// by calling scheduler, setting PSP, ASP bit, TMPL bit, and PC
void startRtos()
{
    taskCurrent = rtosScheduler(); //saves the task ID in taskCurrent to be reused later
    setPSP(tcb[taskCurrent].sp);
    //call a function to do the fn and set the tmpl
    _fn fn = (_fn)tcb[taskCurrent].pid; //sets the location of idle's fuction to fn
    setTMPLbit();
    fn();

}

// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield() // PSP unprivilaged
{
    __asm("     SVC     #6");    // calls svcISR with it's unique number
}

// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{
    __asm("     SVC     #7");
}

// REQUIRED: modify this function to wait a semaphore using pendsv
void wait(int8_t semaphore)
{

    __asm("     SVC     #8");
}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
    __asm("     SVC     #9");
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr()
{
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
void pendSvIsr()
{
    uint8_t pid;

    pid = 0;
    char str[10];
    putsUart0("PENDSV\n");
    putsUart0("PID:");
    selfIToA(pid, str, 10);
    putsUart0(str);
    putsUart0("\n");

    pushR4toR11();
    popR11toR4();


    pushToPSP(0x61000000);                  // xPSR
    //pushToPSP(tcb[taskCurrent].sp);         //PC



    if((NVIC_FAULT_STAT_DERR & NVIC_FAULT_STAT_R) == NVIC_FAULT_STAT_DERR)
    {
        NVIC_FAULT_STAT_R  |= ~NVIC_FAULT_STAT_DERR;
    }
    if((NVIC_FAULT_STAT_IERR & NVIC_FAULT_STAT_R) == NVIC_FAULT_STAT_IERR)
    {
        NVIC_FAULT_STAT_R  |= ~NVIC_FAULT_STAT_IERR;
    }




}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr()
{
    NVIC_INT_CTRL_R|=NVIC_INT_CTRL_PEND_SV; // resets the SV bit
}

// REQUIRED: code this function
void mpuFaultIsr()
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

// REQUIRED: code this function
void hardFaultIsr()
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

// REQUIRED: code this function
void busFaultIsr()
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

// REQUIRED: code this function
void usageFaultIsr()
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
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
// REQUIRED: Add initialization for blue, orange, red, green, and yellow LEDs
//           6 pushbuttons
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

// REQUIRED: add code to return a value from 0-63 indicating which of 6 PBs are pressed
uint8_t readPbs()
{
    // if the push button is pressed add the value of the button to the total value of pushed buttons and return that value
    uint8_t pbval = 0;
    if(!getPinValue(PB0))
    {
        pbval += 1;
    }
    if(!getPinValue(PB1))
    {
        pbval += 2;
    }
    if(!getPinValue(PB2))
    {
        pbval += 4;
    }
    if(!getPinValue(PB3))
    {
        pbval += 8;
    }
    if(!getPinValue(PB4))
    {
        pbval += 16;
    }
    if(!getPinValue(PB5))
    {
        pbval += 32;
    }
    return pbval;
}

//-----------------------------------------------------------------------------
// YOUR UNIQUE CODE
// REQUIRED: add any custom code in this space
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// stringCopy
//-----------------------------------------------------------------------------
char* stringCopy(char * string_out , char * string_in)
{
    char pointer = string_out; // sets the
    uint8_t i = 0;
    for(i = 0; i < 16 ; i++)
    {
        string_out[i] = string_in[i];    //copies each char to the new string
        if(string_in[i] == '\0')         //checks to see if at end of string so random data is not copied
        {
            break;
        }
    }
    return pointer;

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
// ------------------------------------------------------------------------------
//  Task functions
// ------------------------------------------------------------------------------

// one task must be ready at all times or the scheduler will fail
// the idle task is implemented for this purpose
void idle()
{
    while(true)
    {
        setPinValue(ORANGE_LED, 1);
        waitMicrosecond(1000);
        setPinValue(ORANGE_LED, 0);
        yield();
    }
}

void flash4Hz()
{
    while(true)
    {
        setPinValue(GREEN_LED, !getPinValue(GREEN_LED));
        sleep(125);
    }
}

void oneshot()
{
    while(true)
    {
        wait(flashReq);
        setPinValue(YELLOW_LED, 1);
        sleep(1000);
        setPinValue(YELLOW_LED, 0);
    }
}

void partOfLengthyFn()
{
    // represent some lengthy operation
    waitMicrosecond(990);
    // give another process a chance to run
    yield();
}

void lengthyFn()
{
    uint16_t i;
    uint8_t *p;

    // Example of allocating memory from stack
    // This will show up in the pmap command for this thread
    p = mallocFromHeap(1024);
    *p = 0;

    while(true)
    {
        wait(resource);
        for (i = 0; i < 5000; i++)
        {
            partOfLengthyFn();
        }
        setPinValue(RED_LED, !getPinValue(RED_LED));
        post(resource);
    }
}

void readKeys()
{
    uint8_t buttons;
    while(true)
    {
        wait(keyReleased);
        buttons = 0;
        while (buttons == 0)
        {
            buttons = readPbs();
            yield();
        }
        post(keyPressed);
        if ((buttons & 1) != 0)
        {
            setPinValue(YELLOW_LED, !getPinValue(YELLOW_LED));
            setPinValue(RED_LED, 1);
        }
        if ((buttons & 2) != 0)
        {
            post(flashReq);
            setPinValue(RED_LED, 0);
        }
        if ((buttons & 4) != 0)
        {
            restartThread(flash4Hz);
        }
        if ((buttons & 8) != 0)
        {
            stopThread(flash4Hz);
        }
        if ((buttons & 16) != 0)
        {
            setThreadPriority(lengthyFn, 4);
        }
        yield();
    }
}

void debounce()
{
    uint8_t count;
    while(true)
    {
        wait(keyPressed);
        count = 10;
        while (count != 0)
        {
            sleep(10);
            if (readPbs() == 0)
                count--;
            else
                count = 10;
        }
        post(keyReleased);
    }
}

void uncooperative()
{
    while(true)
    {
        while (readPbs() == 8)
        {
        }
        yield();
    }
}

void errant()
{
    uint32_t* p = (uint32_t*)0x20000000;
    while(true)
    {
        while (readPbs() == 32)
        {
            *p = 0;
        }
        yield();
    }
}

void important()
{
    while(true)
    {
        wait(resource);
        setPinValue(BLUE_LED, 1);
        sleep(1000);
        setPinValue(BLUE_LED, 0);
        post(resource);
    }
}

// REQUIRED: add processing for the shell commands through the UART here
void shell()
{
//    USER_DATA data;
//    bool valid;
//    while (true)
//    {
//        valid = false;
//
//        // Get the string from the user
//        getsUart0(&data);
//        lowercase(&data);
//        //putsUart0("Invalid command \0");
//
//        // Parse fields
//        parseFields(&data);
//
//        if (isCommand(&data, "preempt", 1))
//        {
//            char* str = getFieldString(&data, 1);
//
//            if(strCmp(str, "on") == 0)
//            {
//                preempt(1);
//                valid = true;
//            }
//            else if(strCmp(str, "off") == 0)
//            {
//                preempt(0);
//                valid = true;
//            }
//        }
//        if (isCommand(&data, "reboot", 0))
//        {
//            valid = true;
//            putsUart0("REBOOTING......");
//            putcUart0('\n');
//            reboot();
//            // APINT
//        }
//        if (isCommand(&data, "ps", 0))
//        {
//            valid = true;
//            ps();
//        }
//        if (isCommand(&data, "ipcs", 0))
//        {
//            valid = true;
//            ipcs();
//        }
//        if (isCommand(&data, "kill", 1))
//        {
//            int pidint;
//            char* pid = getFieldString(&data, 1);
//            valid = true;
//            pidint = selfAtoi(pid);
//            kill(pidint);
//        }
//        if (isCommand(&data, "pmap", 1))
//        {
//            int pidint;
//            char* pid = getFieldString(&data, 1);
//
//            pidint = selfAtoi(pid);
//            pmap(pidint);
//            valid = true;
//        }
//        if (isCommand(&data, "sched", 1))
//        {
//            char* str = getFieldString(&data, 1);
//
//            if(strCmp(str, "prio") == 0)
//            {
//                sched(1);
//                valid = true;
//            }
//            else if(strCmp(str, "rr") == 0)
//            {
//                sched(0);
//                valid = true;
//            }
//        }
//
//        if (isCommand(&data, "pidof", 1))
//        {
//
//            char* pid = getFieldString(&data, 1);
//            valid = true;
//            pidof(pid);
//        }
//        if (isCommand(&data, "run", 1))
//        {
//
//            char* pid = getFieldString(&data, 1);
//            valid = true;
//            setPinValue(RED_LED, 1);
//        }
//        // Look for error
//        if (!valid)
//            putsUart0("Invalid command\n");
//
//    }
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    bool ok;

    // Initialize hardware
    initHw();
    initUart0();
    initMpu();
    initRtos();

    // Setup UART0 baud rate
    setUart0BaudRate(115200, 40e6);

    // Power-up flash
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(250000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(250000);

    // Initialize semaphores
    createSemaphore(keyPressed, 1);
    createSemaphore(keyReleased, 0);
    createSemaphore(flashReq, 5);
    createSemaphore(resource, 1);

    // Add required idle process at lowest priority
    ok =  createThread(idle, "Idle", 7, 1024);

//    // Add other processes
//    ok &= createThread(lengthyFn, "LengthyFn", 6, 1024);
//    ok &= createThread(flash4Hz, "Flash4Hz", 4, 1024);
//    ok &= createThread(oneshot, "OneShot", 2, 1024);
//    ok &= createThread(readKeys, "ReadKeys", 6, 1024);
//    ok &= createThread(debounce, "Debounce", 6, 1024);
//    ok &= createThread(important, "Important", 0, 1024);
//    ok &= createThread(uncooperative, "Uncoop", 6, 1024);
//    ok &= createThread(errant, "Errant", 6, 1024);
//    ok &= createThread(shell, "Shell", 6, 2048);

    // Start up RTOS
    if (ok)
    {
        setPSP((uint32_t *)0x20008000);
        setASPbit();
        startRtos(); // never returns
    }
    else
        setPinValue(RED_LED, 1);

    return 0;
}
