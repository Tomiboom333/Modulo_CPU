#ifndef INDUSPI_H
#define INDUSPI_H

#define LOW 0
#define HIGH 1

#define MOD_CPU 0
#define MOD_IO 1
#define MOD_MBUS 2

#define IO_ENT_1 0
#define IO_ENT_2 1
#define IO_ENT_3 2
#define IO_ENT_4 3
#define IO_ENT_5 4
#define IO_ENT_6 5
#define IO_ENT_7 6
#define IO_ENT_8 7

#define IO_SAL_1 0
#define IO_SAL_2 1
#define IO_SAL_3 2
#define IO_SAL_4 3
#define IO_SAL_5 4
#define IO_SAL_6 5
#define IO_SAL_7 6
#define IO_SAL_8 7

#define CPU_ENT_1 0
#define CPU_ENT_2 1
#define CPU_ENT_3 2
#define CPU_ENT_4 3

#define CPU_SAL_1 0
#define CPU_SAL_2 1
#define CPU_SAL_3 2
#define CPU_SAL_4 3


#include "stm32f103xb.h"
#include "stdbool.h"
#include "stdint.h"
#include "main.h"
typedef struct 
{
    bool cpuId[4];
    bool cpuOd[4];
    bool modId[8];
    bool modOd[8];
    uint8_t modIa[2];
    uint8_t modOa[2];
}estAct_t;
SPI_HandleTypeDef hspi1;

#ifdef __cplusplus
extern "C" {
#endif

//empezamos
void induInit();
void moduleDet();

void digWrite(int modulo, int salida, bool estado);

void anWrite(int salida, uint8_t valor);

bool digRead(int modulo, int entrada);

uint8_t anRead(int entrada);

void plc_read_inputs();
void plc_write_outputs();
void plc_run_cycle();
void plc_store_spi_inputs();


#ifdef __cplusplus
}
#endif//c++

#endif//.h

