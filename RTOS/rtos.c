// RTOS Framework - Fall 2022
// J Losh

// Student Name: Fernanda Villa, Sworup Bhattarai

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
#define PCoffset    6
#define Comoffset   1
#define keyPressed  1
#define keyReleased 2
#define flashReq    3
#define resource    4
#define YIELD       6
#define SLEEP       7
#define WAIT        8
#define POST        9
#define REBOOT      10
#define PS          11
#define IPCS        12
#define KILL        13
#define PMAP        14
#define PREEMPT     15
#define SCHED       16
#define PIDOF       17
#define RUN         18
#define KBHIT       19
#define MALLOC      5
#define MiliSec     0x9C3F


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

// task
#define STATE_INVALID    0 // no task
#define STATE_UNRUN      1 // task has never been run
#define STATE_READY      2 // has run, can resume at any time
#define STATE_DELAYED    3 // has run, but now awaiting timer
#define STATE_BLOCKED    4 // has run, but now blocked by semaphore
#define STATE_STOPPED    5

#define MAX_TASKS 12       // maximum number of valid tasks
uint8_t taskCurrent = 0;   // index of last dispatched task
uint8_t taskCount = 0;     // total number of valid tasks
uint32_t  *heap = ( uint32_t *)0x20001000;
uint32_t prioLastRun[7];
uint8_t sched = 0;
uint16_t preemptOnOff = 0;
uint32_t numSwitch = 0;

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
    uint32_t srd;                  // MPU subregion disable bits (one per 1 KiB) (sET TO 1 MEANS YOU CAN READ AND WRITE )
    char name[16];                 // name of task used in ps command
    void *semaphore;               // pointer to the semaphore that is blocking the thread
} tcb[MAX_TASKS];


//-----------------------------------------------------------------------------
// Memory Manager and MPU Funcitons
//-----------------------------------------------------------------------------

// TODO: add your malloc code here and update the SRD bits for the current thread
void* mallocFromHeap(uint32_t size_in_bytes)
{
    char str[50];
    void * p = heap;
    if(!(size_in_bytes % 1024 == 0))
    {
        size_in_bytes = (((size_in_bytes + (1024 - 1 ))/1024) * 1024);
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
    if(sched == 1)
    {

        uint8_t i = 0;
        uint8_t ready = 0;
        uint8_t oncethrough = 0;

        //PRIO0
        i = prioLastRun[0]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {
            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 0) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[0] = (i % MAX_TASKS);
                        return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }



        //PRIO1
        i = prioLastRun[1]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {

            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 1) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[1] = (i % MAX_TASKS);
                        return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }


        //PRIO2
        i = prioLastRun[2]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {

            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 2) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[2] = (i % MAX_TASKS);
                       return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }


        //PRIO3
        i = prioLastRun[3]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {

            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 3) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[3] = (i % MAX_TASKS);
                        return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }


        //PRIO4
        i = prioLastRun[4]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {

            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 4) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[4] = (i % MAX_TASKS);
                        return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }


        //PRIO5
        i = prioLastRun[5]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {

            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 5) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[5] = (i % MAX_TASKS);
                        return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }


        //PRIO6
        i = prioLastRun[6]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {

            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 6) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[6] = (i % MAX_TASKS);
                        return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }


        //PRIO7
        i = prioLastRun[7]; // sets i to be the last run command for that prio, e.g. prio 0 last ran tcb 3
        while(ready != 1)
        {

            if(oncethrough >= 1) // looks to see if the value id grater than the last run thing
            {
                if(tcb[i % MAX_TASKS].priority == 7) //is it the same prio
                {
                    if(tcb[(i % MAX_TASKS)].state == STATE_READY || tcb[(i % MAX_TASKS)].state == STATE_UNRUN) // is it ready/unrun
                    {
                        prioLastRun[7] = (i % MAX_TASKS);
                        return (i % MAX_TASKS);
                    }
                }
            }
            oncethrough++;
            i++;
            if(i >= 24) // i aint found nada(looped twice)
            {
                oncethrough = 0;

                break; // onto next prio
            }
        }
    }
    if(sched == 0)
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
    uint8_t i, j,p;
        for(i = 0; i < MAX_TASKS - 1; i++) // checks for the next task to run in queue
        {
            if(tcb[i].pid == fn)
            {
                tcb[i].sp =  tcb[i].spInit;
                tcb[i].state = STATE_UNRUN;
            }
        }
}

// REQUIRED: modify this function to stop a thread
// REQUIRED: remove any pending semaphore waiting
// NOTE: see notes in class for strategies on whether stack is freed or not
void stopThread(_fn fn)
{
    uint8_t i, j,p;
    for(i = 0; i < MAX_TASKS - 1; i++) // checks for the next task to run in queue
    {
        if(tcb[i].pid == fn)
        {
            if(tcb[i].state == STATE_BLOCKED)
            {
                for(j = 0; j < MAX_SEMAPHORES; j++)
                {
                    if(semaphores[(uint16_t)tcb[i].semaphore].processQueue[j] == tcb[i].pid)
                    {
                        semaphores[(uint16_t)tcb[i].semaphore].processQueue[j] = semaphores[(uint16_t)tcb[i].semaphore].processQueue[j + 1];
                        for(p = j; p < MAX_SEMAPHORES; p++ )
                        {
                            semaphores[(uint16_t)tcb[i].semaphore].processQueue[p] = semaphores[(uint16_t)tcb[i].semaphore].processQueue[p + 1];
                        }


                    }
                }
            }
            tcb[i].state = STATE_STOPPED;
            tcb[i].semaphore = 0;
        }
    }
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
    NVIC_ST_RELOAD_R = MiliSec; //39999
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_ENABLE | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_CLK_SRC;

    taskCurrent = rtosScheduler(); //saves the task ID in taskCurrent to be reused later
    tcb[taskCurrent].state = STATE_READY;
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

void* mallocc(int32_t size)
{
    __asm("     SVC     #5");
}

void reboot()
{
    __asm("     SVC     #10");
}

void* ps()
{
    __asm("     SVC     #11");
}
void* ipcs()
{
    __asm("     SVC     #12");
}
void* kill(uint32_t pid)
{
    __asm("     SVC     #13");
}

void* pmap(uint32_t pid)
{
    __asm("     SVC     #14");
}
void* preempt(uint16_t offon)
{
    __asm("     SVC     #15");
}

void* schedlr(uint8_t num)
{
    __asm("     SVC     #16");
}

void* pidof(const char name[])
{
    __asm("     SVC     #17");
}

void run(uint32_t pid)
{
    __asm("     SVC     #18");
}
void* kbhit()
{
    __asm("     SVC     #19");
}


// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr()//systic 39999 ticks N-1 (9C3F)
{
    uint16_t i;
    for(i = 0; i < MAX_TASKS; i++)
    {
        if(tcb[i].state == STATE_DELAYED)
        {
            tcb[i].ticks--;
            if(tcb[i].ticks == 0)
            {
                tcb[i].state = STATE_READY;
            }
        }
    }
    if(preemptOnOff == 1)
    {
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
    }
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
void pendSvIsr()
{

    pushR4toR11();
    tcb[taskCurrent].sp=(uint32_t )getPSP();
    taskCurrent = rtosScheduler();


    if(tcb[taskCurrent].state == STATE_READY)
    {
        setPSP(tcb[taskCurrent].sp);
        popR11toR4();
    }
    else if (tcb[taskCurrent].state == STATE_UNRUN)
    {
        tcb[taskCurrent].state = STATE_READY;
        setPSP(tcb[taskCurrent].spInit);
        pushToPSP((uint32_t) 0x61000000);            // xPSR
        pushToPSP((uint32_t) tcb[taskCurrent].pid);  //PC
        pushToPSP((uint32_t) 0xFFFFFFFD);            //LR
        pushToPSP((uint32_t) 0x00000000);            //R12-0
        pushToPSP((uint32_t) 0x00000000);
        pushToPSP((uint32_t) 0x00000000);
        pushToPSP((uint32_t) 0x00000000);
        pushToPSP((uint32_t) 0x00000000);
    }




    if((NVIC_FAULT_STAT_DERR & NVIC_FAULT_STAT_R) == NVIC_FAULT_STAT_DERR)
    {
        NVIC_FAULT_STAT_R  |= ~NVIC_FAULT_STAT_DERR;
    }
    if((NVIC_FAULT_STAT_IERR & NVIC_FAULT_STAT_R) == NVIC_FAULT_STAT_IERR)
    {
        NVIC_FAULT_STAT_R  |= ~NVIC_FAULT_STAT_IERR;
    }




}
uint8_t getSVCnum()
{
    uint32_t* PSP = (uint32_t*)getPSP();  //gets PSP mem location value
    uint32_t* PC = (uint32_t*)*(PSP + PCoffset) ; //moves to PC mem location and dereferences the command that's in PC
    uint16_t* SVC = (uint16_t*)*(((uint16_t*) PC) - Comoffset);  //Moves up one command and dereferences to get the prev SVC  #__
    uint8_t SVCnum= (uint8_t)SVC; // Grabs only the value #__ saves to SVCnum
    return SVCnum;
}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr()
{
    numSwitch++;
    uint8_t i;
    uint32_t * psp = *((uint32_t*)getPSP());
    uint8_t SVCnum = getSVCnum();
    uint32_t getrzero = *((uint32_t*)getPSP());
    switch(SVCnum) // looks to see which SVC num is stored
    {
    case YIELD://num 6
    {
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; // resets the SV bit
        break;
    }
    case SLEEP://num 7
    {
        tcb[taskCurrent].ticks = *((uint32_t*)getPSP());

        tcb[taskCurrent].state = STATE_DELAYED;

        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    }
    case WAIT://num 8
    {
        getrzero = *((uint32_t*)getPSP());
        if(semaphores[getrzero].count > 0)
        {
            semaphores[getrzero].count--;
        }
        else if(semaphores[getrzero].count == 0 )
        {

            semaphores[getrzero].processQueue[semaphores[getrzero].queueSize]  = (uint32_t) tcb[taskCurrent].pid;
            semaphores[getrzero].queueSize++;
            tcb[taskCurrent].state = STATE_BLOCKED;
            tcb[taskCurrent].semaphore == (uint32_t)getrzero;
        }
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    }
    case POST://num 9
    {
        getrzero = *((uint32_t*)getPSP());
        semaphores[getrzero].count++;
        if(semaphores[getrzero].queueSize > 0)
        {
            for(i = 0; i < MAX_TASKS - 1; i++) // checks for the next task to run in queue
            {
                if(tcb[i].pid == semaphores[getrzero].processQueue[0])
                {
                    tcb[i].state = STATE_READY;
                    tcb[i].semaphore = 0;
                }
            }
            for(i = 0; i < MAX_QUEUE_SIZE - 1; i++) // moves everyone up the queue
            {
                semaphores[getrzero].processQueue[i] = semaphores[getrzero].processQueue[i + 1];
                semaphores[getrzero].processQueue[i + 1] = 0;
            }
            semaphores[getrzero].count--;
            semaphores[getrzero].queueSize--;
        }
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    }
    case MALLOC:
    {
        uint32_t psp = *((uint32_t*)getPSP());
        void* size = (uint32_t*)mallocFromHeap(psp);

        uint32_t* psp2= ((uint32_t *)getPSP());
        *psp2 = (uint32_t)size;
        break;
    }
    case REBOOT:  //done
    {
        NVIC_APINT_R = (0x05FA0000 | NVIC_APINT_SYSRESETREQ);
        break;
    }
    case PS:
    {

        break;
    }
    case IPCS:
    {
        char str[10];
        putsUart0("Semaphore Information:\nName\t\tCount\tQueue_Size\tQueue\n");
        uint8_t i;

        putsUart0("KeyPressed\t");
        selfIToA(semaphores[1].count,str,10);
        putsUart0(str);
        putsUart0("\t");
        selfIToA(semaphores[1].queueSize,str,10);
        putsUart0(str);
        putsUart0("\t\t[ ");
        for(i = 0; i < MAX_QUEUE_SIZE; i++)
        {
            putsUart0("0x0000");
            selfIToA(semaphores[1].processQueue[i],str,16);
            putsUart0(str);
            putsUart0(" ");
        }
        putsUart0("]\n");


        putsUart0("KeyReleaced\t");
        selfIToA(semaphores[2].count,str,10);
        putsUart0(str);
        putsUart0("\t");
        selfIToA(semaphores[2].queueSize,str,10);
        putsUart0(str);
        putsUart0("\t\t[ ");
        for(i = 0; i < MAX_QUEUE_SIZE; i++)
        {
            putsUart0("0x0000");
            selfIToA(semaphores[2].processQueue[i],str,16);
            putsUart0(str);
            putsUart0(" ");
        }
        putsUart0("]\n");

        putsUart0("FlashReq\t");
        selfIToA(semaphores[3].count,str,10);
        putsUart0(str);
        putsUart0("\t");
        selfIToA(semaphores[3].queueSize,str,10);
        putsUart0(str);
        putsUart0("\t\t[ ");
        for(i = 0; i < MAX_QUEUE_SIZE; i++)
        {
            putsUart0("0x0000");
            selfIToA(semaphores[3].processQueue[i],str,16);
            putsUart0(str);
            putsUart0(" ");
        }
        putsUart0("]\n");


        putsUart0("Resource\t");
        selfIToA(semaphores[4].count,str,10);
        putsUart0(str);
        putsUart0("\t");
        selfIToA(semaphores[4].queueSize,str,10);
        putsUart0(str);
        putsUart0("\t\t[ ");
        for(i = 0; i < MAX_QUEUE_SIZE; i++)
        {
            putsUart0("0x0000");
            selfIToA(semaphores[4].processQueue[i],str,16);
            putsUart0(str);
            putsUart0(" ");
        }
        putsUart0("]\n");
        break;
    }
    case KILL:
    {
        uint32_t* psp = (uint32_t*  )*((uint32_t*)getPSP());
        stopThread((_fn)psp);
        break;

    }
    case PMAP:
    {
        uint32_t pid = *((uint32_t*)getPSP());
        break;
    }
    case PREEMPT: // done
    {
        uint16_t offon = *((uint32_t*)getPSP());
        preemptOnOff = offon;
        break;
    }
    case SCHED:  //done
    {
        uint8_t prio = *((uint32_t*)getPSP());
        sched = prio;
        break;
    }
    case PIDOF:  //done
    {
        uint8_t scomp;
        char* process = *((uint32_t*)getPSP());
        for(i = 0; i < MAX_TASKS ; i++)
        {
            scomp = strCmp(tcb[i].name, process);

            if(scomp == 0)
            {
                uint32_t* psp2= ((uint32_t *)getPSP());
                *psp2 = tcb[i].pid;
            }
        }
        break;
    }
    case RUN:
    {
        uint32_t pid = *((uint32_t*)getPSP());
        restartThread((_fn) pid);
        break;
    }
    case KBHIT:
    {
        uint32_t* psp2= ((uint32_t *)getPSP());
        *psp2 = kbhitUart0();
        break;
    }
    }

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
    uint8_t pid ;
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
    uint32_t pid;

    char str[10];
    pid = *((uint32_t*)getPSP());
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
// setSramAccessWindow
//-----------------------------------------------------------------------------
void setSramAccessWindow(uint32_t baseAdd, uint32_t size_in_bytes)
{

    uint16_t startSubregion = (baseAdd - 0x20000000) / 0x2000;
    if(!(size_in_bytes % 1024 == 0))
    {
        size_in_bytes = size_in_bytes / 1024;
        size_in_bytes++;
        size_in_bytes = size_in_bytes * 1024;
    }
    uint16_t regNum = size_in_bytes / 0x400;
    uint8_t i = 0;

    uint32_t mask = 0;

    while(i < regNum)
    {
        mask |= (1 <<  i);
        i++;
    }

    mask <<= startSubregion * 8;

    NVIC_MPU_NUMBER_R = 3;
    NVIC_MPU_ATTR_R  &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |= (mask & 0xFF) << 8;

    NVIC_MPU_NUMBER_R = 4;
    NVIC_MPU_ATTR_R  &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |= ((mask >> 8) & 0xFF) << 8;

    NVIC_MPU_NUMBER_R = 5;
    NVIC_MPU_ATTR_R  &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |=  ((mask >> 16) & 0xFF) << 8;

    NVIC_MPU_NUMBER_R = 6;
    NVIC_MPU_ATTR_R  &= ~0x0000FF00;
    NVIC_MPU_ATTR_R |=  ((mask >> 24) & 0xFF) << 8;
}

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
//void idle2()
//{
//    while(true)
//    {
//        setPinValue(RED_LED, 1);
//        waitMicrosecond(1000);
//        setPinValue(RED_LED, 0);
//        yield();
//    }
//}

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
    p = mallocc(1024);
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
    yield();
    USER_DATA data;
    bool valid;
    char str[15];
    while (true)
    {
        yield();
        valid = false;

        // Get the string from the user
        getsUart0(&data);
        lowercase(&data);
        //putsUart0("Invalid command \0");

        // Parse fields
        parseFields(&data);

        if (isCommand(&data, "preempt", 1))
        {
            char* str = getFieldString(&data, 1);

            if(strCmp(str, "on") == 0)
            {
                preempt(1);
                valid = true;
            }
            else if(strCmp(str, "off") == 0)
            {
                preempt(0);
                valid = true;
            }
        }
        else if (isCommand(&data, "reboot", 0))
        {
            valid = true;
            putsUart0("REBOOTING......");
            putcUart0('\n');
            reboot();
            // APINT
        }
        else if (isCommand(&data, "ps", 0))
        {
            valid = true;
            ps();
        }
        else if (isCommand(&data, "ipcs", 0))
        {
            valid = true;
            ipcs();
        }
        else if (isCommand(&data, "kill", 1))
        {
            uint32_t* pidint;
            char* pid = getFieldString(&data, 1);

            valid = true;
            pidint = (uint32_t*)hex2int(pid);
            kill(pidint);
        }
        else if (isCommand(&data, "pmap", 1))
        {
            int pidint;
            char* pid = getFieldString(&data, 1);

            pidint = selfAtoi(pid);
            pmap(pidint);
            valid = true;
        }
        else if (isCommand(&data, "sched", 1))
        {
            char* str = getFieldString(&data, 1);

            if(strCmp(str, "prio") == 0)
            {
                schedlr(1);
                valid = true;
            }
            else if(strCmp(str, "rr") == 0)
            {
                schedlr(0);
                valid = true;
            }
        }

        else if (isCommand(&data, "pidof", 1))
        {

            char* pid = getFieldString(&data, 1);
            uint32_t name;
            valid = true;
            name = (uint32_t)pidof(pid);
            selfIToA(name, str, 16);
            putsUart0("Process ID of ");
            putsUart0(pid);
            putsUart0(" is: 0x0000");
            putsUart0(str);
            putsUart0("\n");
        }
        else if (isCommand(&data, "run", 1))
        {

            char* pid = getFieldString(&data, 1);
            uint32_t name;
            valid = true;
            name = (uint32_t)pidof(pid);
            run(name);

        }
        // Look for error
        if (!valid)
        {
            putsUart0("Invalid command\n");
        }

    }

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
    ok =  createThread(idle, "idle", 7, 1024);
//    ok =  createThread(idle2, "Idle2", 7, 1024);
//    // Add other processes
    ok &= createThread(lengthyFn, "lengthyfn", 6, 1024);
    ok &= createThread(flash4Hz, "flash4hz", 4, 1024);
    ok &= createThread(oneshot, "oneshot", 2, 1024);
    ok &= createThread(readKeys, "readkeys", 6, 1024);
    ok &= createThread(debounce, "debounce", 6, 1024);
    ok &= createThread(important, "important", 0, 1024);
//    ok &= createThread(uncooperative, "uncoop", 6, 1024);
//    ok &= createThread(errant, "errant", 6, 1024);
    ok &= createThread(shell, "shell", 6, 2048);

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
