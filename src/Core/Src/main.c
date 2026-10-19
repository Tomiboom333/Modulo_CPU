#include "main.h"
#include "induSPI.h"
#include "plc_ciclo.h"
#include "mbus_funcs.h"
#include "mbus_esclavo.h"

/*
 * Modulo CPU de InduCore - version Inducore Innovatech.
 *
 * La logica se programa en C, en la funcion algo(), con:
 *   digRead(MOD_IN_x, ENT_x)        entrada digital
 *   anRead(MOD_IN_x, AN_x)          entrada analogica, 0..4095 (12 bits)
 *   digWrite(MOD_OUT_x, SAL_x, v)   salida digital
 *   anWrite(MOD_OUT_x, AN_x, v)     salida analogica, 0..4095
 *   modEntradasOk(MOD_IN_x), modSalidasOk(MOD_OUT_x)   el modulo responde
 *
 * Medidas de seguridad (ver README):
 *   - CRC-8 en todas las tramas SPI y confirmacion (ACK) de cada escritura.
 *   - Modulos que dejan de responder pasan a offline y se reconectan solos.
 *   - Watchdog en la CPU (~1 s) y en los modulos (~250 ms).
 *   - El modulo de salidas apaga todo si la CPU deja de escribirle (500 ms).
 */

/* 1 = el PLC responde como esclavo Modbus por el USB (para un SCADA o HMI).
 * Queda en 0 para la muestra: no se probo en las placas. */
#define MODBUS_ESCLAVO 0

/* Logica del usuario: se ejecuta una vez por ciclo (cada PLC_PERIODO_MS) */
void algo(void)
{
  /* Misma logica que el firmware anterior, con analogicas de 12 bits:
   * la mitad de la escala ahora es 2048 (antes 128). */
  uint16_t value = anRead(MOD_IN_1, AN_1);
  if (value >= 2048) {
    digWrite(MOD_OUT_1, SAL_3, HIGH);
  } else {
    digWrite(MOD_OUT_1, SAL_3, LOW);
  }
  anWrite(MOD_OUT_1, AN_1, 4095);   /* 100 % (antes 255) */
  anWrite(MOD_OUT_1, AN_2, 2047);   /* 50 %  (antes 127) */
}

/* Mientras la CPU espera el proximo ciclo se atiende Modbus */
void plc_tarea_en_espera(void)
{
  mb_procesar();
#if MODBUS_ESCLAVO
  mbs_procesar();
#endif
}

int main(void)
{
  induInit();          /* reloj, GPIO, SPI, UART, deteccion de modulos y watchdog */
  plc_ciclo_init();
#if MODBUS_ESCLAVO
  mbs_init();
#endif

  while (1) {
    plc_esperar_ciclo();
    plc_run_cycle(algo);   /* leer entradas -> algo() -> escribir salidas */
    mb_procesar();
  }
}
