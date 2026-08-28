#include "main.h"
#include "induSPI.h"

static bool ordenEnviada = false;
static mbusStatus_t estadoOrden;
static uint8_t longitudRespuesta;
static const uint8_t *respuesta;

void ejemploUsuario(void)
{
  /* Esta funcion representa la logica normal del usuario. La orden solo se
     agrega una vez; si se llamara siempre, se llenaria la cola rapidamente. */
  if (!ordenEnviada) {
    estadoOrden = writeSingleRegister(1, 10, 1234);
    ordenEnviada = true;
  }
}

int main(void)
{
  induInit();

  while (1) {
    /* plc_run_cycle ejecuta, en este orden:
       leer entradas, ejemploUsuario, mbus_process y escribir salidas. */
    plc_run_cycle(ejemploUsuario);

    if (estadoOrden == MBUS_OK && ordenEnviada) {
      /* La respuesta queda disponible despues de que el ciclo ejecuto
         mbus_process(). Para una escritura, normalmente solo se verifica
         el estado; para una lectura se pueden interpretar estos bytes. */
      respuesta = mbus_get_last_response(&longitudRespuesta);
      (void)respuesta;
      (void)longitudRespuesta;
    }
  }
}

/* Para probar una lectura, reemplazar la orden anterior por:

   estadoOrden = readHoldingRegisters(1, 0, 2);

   Luego, despues de plc_run_cycle(), respuesta contiene la trama Modbus:
   respuesta[0] = direccion, respuesta[1] = funcion,
   respuesta[2] = cantidad de bytes y los datos comienzan en respuesta[3].
*/
