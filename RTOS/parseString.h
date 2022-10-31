/*
 * parseString.h
 *
 *  Created on: Oct 21, 2022
 *      Author: Sworup
 */

#ifndef PARSESTRING_H_
#define PARSESTRING_H_
#include <stdbool.h>
#include <stdint.h>



#define MAX_CHARS 80
#define MAX_FIELDS 5

typedef struct _USER_DATA
{
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;


void swap(char *x, char *y);
char* reverse(char *buf, int i, int j);
char* selfIToA(int value, char* buffer, int base);
void getsUart0(USER_DATA *d);
void parseFields(USER_DATA *d);
char* getFieldString(USER_DATA* d, uint8_t fieldNumber);
int strCmp(const char* s1, const char* s2);
int32_t getFieldInteger(USER_DATA* d, uint8_t fieldNumber);
bool isCommand(USER_DATA* d, const char* strCommand, uint8_t minArguments);
void lowercase(USER_DATA* d);






#endif /* PARSESTRING_H_ */
