#include "induSPI.h"
#include "stm32f103xb.h"
estAct_t estAct;

static uint8_t spiTxBuffer[5];
static uint8_t spiRxBuffer[5];
static volatile bool spiTransferInProgress = false;
static volatile bool spiInputsReady = false;

void induInit(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
}

void digWrite(int salida, bool estado)
{
    estAct.modOd[salida] = estado; // guardo el estado deseado de la salida elegida.
}

void anWrite(int salida, uint8_t valor)
{
    estAct.modOa[salida] = valor; // guardo el valor deseado (0 a 255) de la salida elegida.
}

bool digRead(int entrada)
{
    return estAct.modId[entrada];
}

uint8_t anRead(int entrada)
{
    return estAct.modIa[entrada];
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1) {
        spiTransferInProgress = false;
        plc_store_spi_inputs();
    }
}

static void plc_store_spi_inputs(void)
{
    for (int i = 0; i < 8; i++) {
        estAct.modId[i] = (spiRxBuffer[0] >> i) & 0x01;
    }

    for (int i = 0; i < 4; i++) {
        estAct.modIa[i] = spiRxBuffer[i + 1];
    }

    spiInputsReady = true;
}


void plc_read_inputs(void)
{
    if (spiTransferInProgress) {
        return;
    }

    spiTxBuffer[0] = 0x01;
    for (int i = 1; i < 5; i++) {
        spiTxBuffer[i] = 0x00;
    }

    spiTransferInProgress = true;
    HAL_SPI_TransmitReceive_IT(&hspi1, spiTxBuffer, spiRxBuffer, 5);
}
void plc_write_outputs(){
    uint8_t buf[5];
}
void plc_run_cycle(void)
{
    if (!spiTransferInProgress && !spiInputsReady) {
        plc_read_inputs();
    }

    if (spiInputsReady) {
        //codigo del usuario
        plc_write_outputs();
        spiInputsReady = false;
    }
}


