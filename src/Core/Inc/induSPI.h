#ifndef INDUSPI_H
#define INDUSPI_H

#define LOW 0
#define HIGH 1

#define MOD_CPU 0
#define MOD_IO 1//falta borrarlo

#define MOD_O1 1
#define MOD_O2 2
#define MOD_O3 3
#define MOD_O4 4
#define MOD_O5 5

#define MOD_I1 1
#define MOD_I2 2
#define MOD_I3 3
#define MOD_I4 4
#define MOD_I5 5

#define MOD_MBUS1 1
#define MOD_MBUS2 2
#define MOD_MBUS3 3
#define MOD_MBUS4 4
#define MOD_MBUS5 5

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

typedef enum {
    MBUS_OK = 0,
    MBUS_INVALID_ARGUMENT,
    MBUS_SPI_ERROR,
    MBUS_TIMEOUT,
    MBUS_INVALID_RESPONSE,
    MBUS_EXCEPTION,
    MBUS_QUEUE_FULL,
    MBUS_NO_COMMAND
} mbusStatus_t;

typedef struct 
{
    bool cpuId[4];
    bool cpuOd[4];
    bool modId[8];
    bool modOd[8];
    uint8_t modIa[2];
    uint8_t modOa[2];
}estAct_t;
extern SPI_HandleTypeDef hspi1;

#ifdef __cplusplus
extern "C" {
#endif

//empezamos
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);
void induInit();
void moduleDet();

void digWrite(int modulo, int salida, bool estado);

void anWrite(int salida, uint8_t valor);

bool digRead(int modulo, int entrada);

uint8_t anRead(int entrada);

void plc_read_inputs();
void plc_write_outputs();
void plc_run_cycle(void (*fuser)(void));
void plc_store_spi_inputs();

/*
 * Estas funciones solo guardan la orden; no realizan ninguna transferencia.
 * Se pueden llamar varias veces antes de mbus_process(), hasta llenar la cola.
 * position y quantity corresponden directamente a los campos de Modbus RTU.
 */
mbusStatus_t mbus_read_coils(uint8_t slaveAddress, uint16_t position, uint16_t quantity);
mbusStatus_t mbus_read_input_contacts(uint8_t slaveAddress, uint16_t position, uint16_t quantity);
mbusStatus_t mbus_read_holding_registers(uint8_t slaveAddress, uint16_t position, uint16_t quantity);
mbusStatus_t mbus_read_input_registers(uint8_t slaveAddress, uint16_t position, uint16_t quantity);
mbusStatus_t mbus_write_single_register(uint8_t slaveAddress, uint16_t position, uint16_t value);
mbusStatus_t mbus_write_single_coil(uint8_t slaveAddress, uint16_t position, bool value);

/* API compatible con Modulo_MBUS. Estas funciones tambien solo encolan. */
mbusStatus_t readCoils(uint8_t address, uint16_t position, uint16_t quantity);
mbusStatus_t writeSingleCoil(uint8_t address, uint16_t position, uint16_t value);
mbusStatus_t writeMultipleCoils(uint8_t address, uint16_t position,
                                const uint8_t *values, uint16_t quantity);
mbusStatus_t readInputContacts(uint8_t address, uint16_t position, uint16_t quantity);
mbusStatus_t readHoldingRegisters(uint8_t address, uint16_t position, uint16_t quantity);
mbusStatus_t writeSingleRegister(uint8_t address, uint16_t position, uint16_t value);
mbusStatus_t writeMultipleRegisters(uint8_t address, uint16_t position,
                                    const uint16_t *values, uint16_t quantity);
mbusStatus_t readAnalogInputs(uint8_t address, uint16_t position, uint16_t quantity);

/*
 * Toma la siguiente orden de la cola y la envía al modulo MBUS por SPI.
 * La llamada espera la respuesta del esclavo Modbus y procesa una orden por
 * llamada. Devuelve MBUS_NO_COMMAND si no hay ninguna orden pendiente.
 */
mbusStatus_t mbus_process(void);
/* Devuelve la ultima respuesta Modbus RTU recibida, sin copiarla. */
const uint8_t *mbus_get_last_response(uint8_t *responseLength);


#ifdef __cplusplus
}
#endif//c++

#endif//.h

