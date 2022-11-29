#include "parseString.h"
#include "tm4c123gh6pm.h"



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
// selfAtoi code implementation based on code from geeksforgeeks, https://www.geeksforgeeks.org/write-your-own-atoi/
//-----------------------------------------------------------------------------
int selfAtoi(char* str)//
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
        yield();
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
// lowercase
//-----------------------------------------------------------------------------
void lowercase(USER_DATA* d)
{
    int i;
    for (i = 0; d->buffer[i]!='\0'; i++)
    {
        if(d->buffer[i] >= 'A' && d->buffer[i] <= 'Z')
        {
            d->buffer[i] = d->buffer[i] +32;
        }
    }

}

uint32_t hex2int(char *hex) //based on code from https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int
{
    uint32_t val = 0;
    while (*hex)
    {
        // get current character then increment
        uint8_t byte = *hex++;
        // transform hex character to the 4bit equivalent number, using the ascii table indexes
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;
        // shift 4 to make space for new digit, and add the 4 bits of the new digit
        val = (val << 4) | (byte & 0xF);
    }
    return val;
}


