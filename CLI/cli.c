//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "clock.h"
#include "wait.h"
#include "uart0.h"
#include "gpio.h"
#include "tm4c123gh6pm.h"

#define RED_LED PORTF,1
#define GREEN_LED PORTF,3



//-----------------------------------------------------------------------------
// the maximum number of characters that can be accepted from the user and the structure for holding UI information
//-----------------------------------------------------------------------------

#define MAX_CHARS 80
#define MAX_FIELDS 5

typedef struct _USER_DATA
{
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;



//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    //Enable Port
    enablePort(PORTF);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(RED_LED);

}

//-----------------------------------------------------------------------------
// swap
//-----------------------------------------------------------------------------
void swap(char *x, char *y)
{
    char t = *x; *x = *y; *y = t;
}
//-----------------------------------------------------------------------------
// reverse
//-----------------------------------------------------------------------------
char* reverse(char *buf, int i, int j)
{
    while (i < j) {
        swap(&buf[i++], &buf[j--]);
    }
    return buf;
}
//-----------------------------------------------------------------------------
// selfIToA ---- code implementation based on code from geeksforgeeks, https://www.geeksforgeeks.org/implement-itoa/
//-----------------------------------------------------------------------------
char* selfIToA(int value, char* buffer, int base)
{

    if (base < 2 || base > 32) {
        return buffer;
    }
    int n = abs(value);
    int i = 0;
    while (n)
    {
        int r = n % base;
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
    int result = 0;
    int i;

    for (i = 0; str[i] != '\0'; ++i)
    {
        result = result * 10 + str[i] - '0';
    }
    return result;
}

//-----------------------------------------------------------------------------
// getsUart0
//-----------------------------------------------------------------------------

void getsUart0(USER_DATA *d)
{
    int count = 0;

    while(true)
    {
        char c = getcUart0();
        if(c == 8 || c == 127 && count > 0)
        {
            count--;
        }
        else if (c == 13)
        {
            d->buffer[count] = '\0';
            return 0;
        }
        else if (c >= 32)
        {
            d->buffer[count] = c;
            count++;
            if (count == MAX_CHARS)
            {
                d->buffer[count] = '\0';
                return 0;
            }
        }
    }

}

//-----------------------------------------------------------------------------
// parseFields
//-----------------------------------------------------------------------------
void parseFields(USER_DATA *d)
{
    char prevtype = 'd';
    d->fieldCount = 0;
    uint8_t i;
    //set 1 2
    for(i = 0; i < MAX_CHARS && d->buffer[i] != '\0'; i++)
    {
        if (prevtype == 'd')
        {
            if((d->buffer[i] >= 65 && d->buffer[i] <= 90) || (d->buffer[i] >= 97 && d->buffer[i] <= 122)) // char
            {
               d->fieldPosition[d->fieldCount] = i;
               d->fieldType[d->fieldCount] = 'a';
               d->fieldCount += 1;
               prevtype = 'a';
            }
            else if ( d->buffer[i] >= 48 && d->buffer[i] <= 57|| d->buffer[i] == 46)  // number
            {
                d->fieldPosition[d->fieldCount] = i;
                d->fieldType[d->fieldCount] = 'n';
                d->fieldCount += 1;
                prevtype = 'n';
            }
            else
            {
                d->buffer[i] = '\0';
                prevtype = 'd';
            }
        }

        else if(d->buffer[i] ==32 || d->buffer[i] == 44 || d->buffer[i] == 124)
        {
            prevtype = 'd';
            d->buffer[i] = '\0';
        }
    }

}
//-----------------------------------------------------------------------------
// getFieldString
//-----------------------------------------------------------------------------
char* getFieldString(USER_DATA* d, uint8_t fieldNumber)
{
    //return the value of a field requested if the field number is in range
    if (fieldNumber <= d->fieldCount)
        {
            return &d->buffer[d->fieldPosition[fieldNumber]];
        }
    else
    {
        return 0;
    }

}

//-----------------------------------------------------------------------------
// String Compare
//-----------------------------------------------------------------------------

int strCmp(const char* s1, const char* s2)
{
    while(*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}


//-----------------------------------------------------------------------------
// getFieldInteger
//-----------------------------------------------------------------------------
int32_t getFieldInteger(USER_DATA* d, uint8_t fieldNumber)
{

     char *word = getFieldString(d,fieldNumber);

    int num = selfAtoi(word);

    return num;

}


//-----------------------------------------------------------------------------
// isCommand
//-----------------------------------------------------------------------------

bool isCommand(USER_DATA* d, const char* strCommand, uint8_t minArguments)
{

    if ( (d->fieldCount - 1) >= minArguments && ( strCmp(d->buffer, strCommand) == 0 ) )
    {
        return true;
    }
    else
        return false;

}

//-----------------------------------------------------------------------------
// PS
//-----------------------------------------------------------------------------
void ps()
{
    putsUart0("PS called");
    putcUart0('\n');
}

//-----------------------------------------------------------------------------
// IPCS
//-----------------------------------------------------------------------------
void ipcs()
{
    putsUart0("IPCS called");
    putcUart0('\n');
}
//-----------------------------------------------------------------------------
// startUpFlash()
//-----------------------------------------------------------------------------
void startUpFlash()
{
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(50000);
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
}
//-----------------------------------------------------------------------------
// kill
//-----------------------------------------------------------------------------
void kill(uint32_t pid)
{
    char str[10];
    putsUart0("killed pid: ");
    selfIToA(pid, str, 10);
    putsUart0(str);
    putcUart0('\n');
}
//-----------------------------------------------------------------------------
// pmap
//-----------------------------------------------------------------------------
void pmap(uint32_t pid)
{
    char str[10];
    putsUart0("Memory usage by ");
    selfIToA(pid, str, 10);
    putsUart0(str);
    putcUart0('\n');
}
//-----------------------------------------------------------------------------
// preempt
//-----------------------------------------------------------------------------
void preempt(bool on)
{
    if(on == 1)
        putsUart0("preempt on");
    else
        putsUart0("preempt off");
    putcUart0('\n');
}
//-----------------------------------------------------------------------------
// sched
//-----------------------------------------------------------------------------
void sched(bool prio_on)
{
    if(prio_on == 1)
        putsUart0("sched prio");
    else
        putsUart0("sched rr");
    putcUart0('\n');
}
//-----------------------------------------------------------------------------
// pidof
//-----------------------------------------------------------------------------
pidof(const char name[])
{
    putsUart0(name);
    putsUart0(" launched");
    putcUart0('\n');

}
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------


int main(void)
{
    USER_DATA data;
    initHw();
    initUart0();

    // Setup UART0 baud rate
    setUart0BaudRate(115200, 40e6);
    startUpFlash();



    bool valid;
    // Setup UART0 baud rate
    while(true)
    {
        valid = false;
        putcUart0('>');
        // Get the string from the user
        getsUart0(&data);
        //putsUart0("Invalid command \0");

        // Echo back to the user of the TTY interface for testing
        #ifdef DEBUG
        putsUart0(data.buffer);
        putcUart0('\n');
        #endif
        // Parse fields
        parseFields(&data);

        // Echo back the parsed field data (type and fields)
        #ifdef DEBUG
        uint8_t i;
        for (i = 0; i < data.fieldCount; i++)
        {
        putcUart0(data.fieldType[i]);
        putcUart0('\t');
        putsUart0(&data.buffer[data.fieldPosition[i]]);
        putcUart0('\n');
        }
        #endif
        // Command evaluation
        // set add, data → add and data are integers

        // alert ON|OFF → alert ON or alert OFF are the expected commands
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
        if (isCommand(&data, "reboot", 0))
        {
            valid = true;
            putsUart0("REBOOTING......");
            putcUart0('\n');
            startUpFlash();
        }
        if (isCommand(&data, "ps", 0))
        {
            valid = true;
            ps();
        }
        if (isCommand(&data, "ipcs", 0))
        {
            valid = true;
            ipcs();
        }
        if (isCommand(&data, "kill", 1))
        {
            int pidint;
            char* pid = getFieldString(&data, 1);
            valid = true;
            pidint = selfAtoi(pid);
            kill(pidint);
        }
        if (isCommand(&data, "pmap", 1))
        {
            int pidint;
            char* pid = getFieldString(&data, 1);

            pidint = selfAtoi(pid);
            pmap(pidint);
            valid = true;
        }
        if (isCommand(&data, "sched", 1))
        {
            char* str = getFieldString(&data, 1);

            if(strCmp(str, "prio") == 0)
            {
                sched(1);
                valid = true;
            }
            else if(strCmp(str, "rr") == 0)
            {
                sched(0);
                valid = true;
            }
        }

        if (isCommand(&data, "pidof", 1))
        {

            char* pid = getFieldString(&data, 1);
            valid = true;
            pidof(pid);
        }
        if (isCommand(&data, "run", 1))
        {

            char* pid = getFieldString(&data, 1);
            valid = true;
            setPinValue(RED_LED, 1);
        }
        // Look for error
        if (!valid)
            putsUart0("Invalid command\n");

    }
}
