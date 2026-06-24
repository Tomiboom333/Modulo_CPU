#ifndef INDUSPI_H
#define INDUSPI_H

#include "stdbool.h"
#include "stdint.h"
typedef struct 
{
    bool cpuId[4];
    bool cpuOd[4];
    bool modId[8];
    bool modOd[8];
    uint8_t modIa[4];
    uint8_t modOa[4];
    
}estAct_t;
static estAct_t estAct;

#ifdef __cplusplus
extern "C" {
#endif

//empezamos

void moduleDet();

void digWrite(int salida, bool estado);

void anWrite(int salida, uint8_t valor);

bool digRead(int entrada);

uint8_t anRead(int entrada);

#ifdef __cplusplus
}
#endif//c++

#endif//.h

