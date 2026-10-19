/*
 * plc_ciclo.c - Tiempo de ciclo fijo del PLC (ver plc_ciclo.h).
 */

#include "plc_ciclo.h"
#include "induSPI.h"

uint16_t plc_mw[PLC_N_MW];
volatile uint32_t plc_ciclos = 0;
volatile uint32_t plc_ciclos_excedidos = 0;
volatile uint32_t plc_periodo_ms = PLC_PERIODO_MS;

static uint32_t proximo_ciclo = 0;

__attribute__((weak)) void plc_tarea_en_espera(void)
{
}

void plc_ciclo_init(void)
{
    plc_periodo_ms = PLC_PERIODO_MS ? PLC_PERIODO_MS : 1;
    proximo_ciclo = HAL_GetTick();
}

void plc_esperar_ciclo(void)
{
    uint32_t ahora = HAL_GetTick();

    if ((int32_t)(ahora - proximo_ciclo) > 0) {
        /* El ciclo anterior se paso del periodo: se reprograma desde ahora */
        plc_ciclos_excedidos++;
        proximo_ciclo = ahora;
    } else {
        while ((int32_t)(HAL_GetTick() - proximo_ciclo) < 0) {
            plc_tarea_en_espera();
            induWatchdogRefresh();
        }
    }
    proximo_ciclo += plc_periodo_ms;
    plc_ciclos++;
}
