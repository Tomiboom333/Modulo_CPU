# Modulo_CPU — versión Inducore Innovatech

Firmware del módulo CPU de InduCore con **medidas de seguridad** y la lógica
programada en **C** (sin OpenPLC). STM32F103C8, se compila con PlatformIO.

Funciona junto con las ramas `Inducore-Innovatech` de **Modulo_IN** y
**Modulo_OUT**. Las tres hablan el protocolo del backplane v2
([`docs/PROTOCOLO.md`](docs/PROTOCOLO.md)), que **no es compatible** con el
firmware de `main`: hay que flashear los tres módulos con esta rama.

## Programar la lógica

En `src/Core/Src/main.c`, función `algo()`. Se ejecuta una vez por ciclo
(`PLC_PERIODO_MS` = 10 ms, igual que antes).

```c
void algo(void)
{
  if (digRead(MOD_IN_1, ENT_1))
    digWrite(MOD_OUT_1, SAL_1, HIGH);
  else
    digWrite(MOD_OUT_1, SAL_1, LOW);

  uint16_t v = anRead(MOD_IN_1, AN_1);   /* 0..4095 */
  anWrite(MOD_OUT_1, AN_1, v);           /* 0..4095 */
}
```

| Función | Qué hace |
|---|---|
| `digRead(MOD_IN_x, ENT_x)` | entrada digital |
| `anRead(MOD_IN_x, AN_x)` | entrada analógica, **0–4095** (12 bits) |
| `digWrite(MOD_OUT_x, SAL_x, v)` | salida digital |
| `anWrite(MOD_OUT_x, AN_x, v)` | salida analógica, **0–4095** (se recorta) |
| `modEntradasOk(MOD_IN_x)` / `modSalidasOk(MOD_OUT_x)` | `true` si el módulo responde |

Diferencias con el firmware anterior:

- Las analógicas son de **12 bits**: la mitad de la escala es 2048 (antes 128)
  y el máximo es 4095 (antes 255).
- `AN_1` / `AN_2` son las constantes de los canales: `AN_1` es la bornera
  InAn1 en las entradas y AO0 en las salidas. **Antes estaban cruzados**: el
  canal 0 leía InAn2 y escribía AO1. Si cableaste el sensor o la carga en la
  bornera que funcionaba con el firmware viejo, ahora corresponde al otro
  canal (o mover el cable).
- `MOD_IN_1` / `MOD_OUT_1` son los módulos **más cerca de la CPU** en el riel.
  Antes, por el espejado de los chip select, eran los del otro extremo.

## Medidas de seguridad

| Medida | Qué protege |
|---|---|
| **CRC-8** en todas las tramas SPI | Una trama con ruido se descarta: nunca cambia una salida ni se toma como lectura válida |
| **Confirmación (ACK)** de cada escritura | La CPU sabe si el módulo de salidas aplicó lo que le mandó |
| **Detección de módulos caídos** | Tras 3 fallas seguidas el módulo pasa a *offline*; si es de entradas, sus entradas valen 0 |
| **Reconexión automática** | Cada 1 s la CPU busca módulos caídos o nuevos; vuelven solos sin reiniciar |
| **Watchdog en la CPU** (~1 s) | Si el firmware se cuelga, el micro se reinicia |
| **Watchdog en los módulos** (~250 ms) | Ídem en entradas y salidas |
| **Timeout en el módulo de salidas** (500 ms) | Si la CPU se cuelga, se reinicia o se desconecta, las salidas se apagan solas |
| **Arranque seguro** | El módulo de salidas arranca con todo apagado, incluidas las AO |

## Probar antes de la muestra

Flashear las tres placas con las ramas `Inducore-Innovatech` y revisar, con el
depurador, las variables entre paréntesis:

1. **Detección**: `contModulosIn` y `contModulosOut` tienen la cantidad de
   módulos enchufados y `slots[]` muestra cada uno `online`.
2. **Funcionamiento normal**: la lógica de `algo()` responde igual que antes y
   `spiEstadisticas.lecturasError` / `escriturasError` no crecen.
3. **Analógicas**: `anRead(MOD_IN_1, AN_1)` sigue a la bornera InAn1 y llega
   a ~4095 con 10 V. En las salidas, 0 = apagada y 4095 = 100 %. Si quedan
   invertidas, poner `AO_LOGICA_INVERTIDA 0` en Modulo_OUT.
4. **Módulo desenchufado**: con todo andando, sacar el módulo de entradas.
   Tiene que pasar a offline (`spiEstadisticas.desconexiones` sube) y, al
   volver a enchufarlo, reconectarse solo en ~1 s (`reconexiones` sube).
5. **CPU caída**: con salidas encendidas, apretar reset en la CPU o
   desenchufarla. En ~0,5 s las salidas tienen que apagarse.
6. **Estabilidad**: dejarlo andando 10–15 minutos sin que se reinicie nada.

Si algo falla y no hay tiempo de corregirlo, la versión de `main` sigue
intacta en los tres repos: flashear `main` en los tres para volver atrás.

## Para mostrar en la muestra

Las pruebas 4 y 5 sirven también como demostración en vivo:

- **"Si se cae un módulo, el PLC lo detecta y se recupera solo"**: sacar y
  volver a poner un módulo con todo funcionando.
- **"Si se cae la CPU, las salidas van a estado seguro"**: encender una salida
  (una lámpara o un LED en la bornera) y apretar reset en la CPU: se apaga.

## Otras opciones

- `MODBUS_ESCLAVO` (en `main.c`, por defecto **0**): el PLC responde como
  esclavo Modbus por el USB, para un SCADA o HMI. Mapa de registros en
  `src/Core/Inc/mbus_esclavo.h`. Quedó apagado porque no se probó en las placas.
- El maestro Modbus por RS-485 (`mbus_funcs.c`) verifica CRC, tiene timeout y
  reintenta hasta 2 veces.

## Tests en la PC

`python test_pc/correr_test.py` (requiere `gcc`) prueba el protocolo, el
backplane con módulos simulados (caídas, reconexión, tramas corruptas) y el
Modbus. Compilado para el STM32F103C8: ~11 KB de flash y ~4 KB de RAM.
