#ifndef _MAIN_H_
#define _MAIN_H_

#define ENABLE_SC   3
#define DISABLE_SC  8

#define ENABLE_ALARM 4
#define DISABLE_ALARM 9

typedef enum
{
    LOCAL_MODE,
    NORMAL_MODE,
    SMARTCONFIG,
    FOTA
} _status;

typedef struct 
{
    char action[50];
    unsigned int roomID;
    unsigned int co2;
    unsigned int co;
    unsigned int tvoc;
    double temp;
    double hum;
    char url[100];
    unsigned int process;
    char number[20];
} _message_object;


#endif