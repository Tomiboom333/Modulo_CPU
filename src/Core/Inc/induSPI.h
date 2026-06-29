#ifndef INDUSPI_H
#define INDUSPI_H

#define LOW 0
#define HIGH 1

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
    uint8_t modIa[4];
    uint8_t modOa[4];
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


void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_SPI1_Init(void);

#ifdef __cplusplus
}
#endif//c++

#endif//.h

