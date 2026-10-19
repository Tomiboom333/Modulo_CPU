#ifndef PLC_CICLO_H
#define PLC_CICLO_H

/*
 * plc_ciclo.h - Tiempo de ciclo fijo del PLC.
 *
 * Cada ciclo: espera el periodo -> lee entradas -> ejecuta la logica en C
 * -> escribe salidas. Mientras espera, refresca el watchdog y atiende Modbus.
 */

#include <stdint.h>

/* Periodo del ciclo en ms (el firmware anterior usaba HAL_Delay(10)) */
#ifndef PLC_PERIODO_MS
#define PLC_PERIODO_MS 10
#endif

/* Memoria de usuario compartida con el Modbus esclavo (holding 1024..1087) */
#define PLC_N_MW 64
extern uint16_t plc_mw[PLC_N_MW];

/* Diagnostico */
extern volatile uint32_t plc_ciclos;            /* ciclos ejecutados */
extern volatile uint32_t plc_ciclos_excedidos;  /* ciclos que tardaron mas que el periodo */
extern volatile uint32_t plc_periodo_ms;

#ifdef __cplusplus
extern "C" {
#endif

void plc_ciclo_init(void);
/* Espera hasta el inicio del proximo ciclo */
void plc_esperar_ciclo(void);
/* Se ejecuta repetidamente mientras se espera (por defecto vacia; main.c la redefine) */
void plc_tarea_en_espera(void);

#ifdef __cplusplus
}
#endif

#endif /* PLC_CICLO_H */
