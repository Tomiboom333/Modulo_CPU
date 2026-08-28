#include "induSPI.h"
#include "stm32f103xb.h"
SPI_HandleTypeDef hspi1;
estAct_t estAct;

static uint8_t spiTxBuffer[4];
static uint8_t spiRxBuffer[4];

static uint8_t InTxBuffer;
static uint8_t InRxBuffer[3];

/* Protocolo local CPU<->MBUS, independiente del protocolo Modbus del UART. */
#define MBUS_SPI_REQUEST 0xA5  /* seguido por longitud y trama Modbus RTU */
#define MBUS_SPI_RESPONSE 0x5A /* solicita la respuesta ya procesada */
#define MBUS_SPI_MAX_DATA 128
#define MBUS_SPI_FRAME_SIZE (MBUS_SPI_MAX_DATA + 2)

static uint8_t mbusSpiTx[MBUS_SPI_MAX_DATA + 2];
static uint8_t mbusSpiRx[MBUS_SPI_FRAME_SIZE];
static const uint8_t modbusExceptionMask = 0x80;
/* La cola evita que una llamada del usuario tenga que iniciar el bus. */
#define MBUS_QUEUE_SIZE 4
static uint8_t mbusQueue[MBUS_QUEUE_SIZE][MBUS_SPI_MAX_DATA];
static uint8_t mbusQueueLength[MBUS_QUEUE_SIZE];
/* Head apunta a la orden que se procesara; tail, a la proxima posicion libre. */
static uint8_t mbusQueueHead;
static uint8_t mbusQueueTail;
static uint8_t mbusQueueCount;
/* Se conserva la ultima respuesta para que el usuario pueda leerla despues. */
static uint8_t mbusLastResponse[MBUS_SPI_MAX_DATA];
static uint8_t mbusLastResponseLength;

typedef enum {
  SPI_IDLE,
  SPI_READING_INPUTS,
  SPI_INPUTS_READY,
  SPI_WRITING_OUTPUTS,
  SPI_ERROR
} spiState_t;

static volatile spiState_t spiState = SPI_IDLE;

static uint16_t entCpu[4] = {GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15};
static uint16_t salCpu[4] = {GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6};

static void spi_deassert_all_cs(void)
{
    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(MBUS_SPI_CS_GPIO_Port, MBUS_SPI_CS_Pin, GPIO_PIN_SET);
}

static void spi_wait_ready(void)
{
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
    }
}

static uint16_t mbus_crc16(const uint8_t *data, uint8_t length)
{
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

static mbusStatus_t mbus_spi_exchange(const uint8_t *request, uint8_t requestLength,
                    uint8_t *response, uint8_t *responseLength)
{
  uint8_t header[2] = {MBUS_SPI_REQUEST, requestLength};
  uint8_t poll = MBUS_SPI_RESPONSE;
  uint8_t ignored;

  if (requestLength > MBUS_SPI_MAX_DATA || response == NULL || responseLength == NULL) {
    return MBUS_INVALID_ARGUMENT;
  }

  spi_deassert_all_cs();
  spi_wait_ready();

  HAL_GPIO_WritePin(MBUS_SPI_CS_GPIO_Port, MBUS_SPI_CS_Pin, GPIO_PIN_RESET);

  if (HAL_SPI_Transmit(&hspi1, header, sizeof(header), 20) != HAL_OK ||
    HAL_SPI_Transmit(&hspi1, (uint8_t *)request, requestLength, 50) != HAL_OK) {
    /* Fase 1: entregar al esclavo una trama Modbus RTU completa. */
    spi_deassert_all_cs();
    return MBUS_SPI_ERROR;
  }
  spi_deassert_all_cs();

  /* El esclavo usa ahora su UART y necesita tiempo para consultar al equipo. */
  HAL_Delay(5);

  /* Fase 2: avisar al esclavo que el maestro va a leer la respuesta. */
  spi_wait_ready();
  HAL_GPIO_WritePin(MBUS_SPI_CS_GPIO_Port, MBUS_SPI_CS_Pin, GPIO_PIN_RESET);


  if (HAL_SPI_TransmitReceive(&hspi1, &poll, &ignored, 1, 20) != HAL_OK) {//revisar!!!!
    spi_deassert_all_cs();
    return MBUS_SPI_ERROR;
  }
  spi_deassert_all_cs();

  HAL_Delay(1);

  /* Fase 3: leer siempre el marco fijo [estado, longitud, datos]. */
  spi_wait_ready();
  HAL_GPIO_WritePin(MBUS_SPI_CS_GPIO_Port, MBUS_SPI_CS_Pin, GPIO_PIN_RESET);
  for (uint16_t i = 0; i < MBUS_SPI_FRAME_SIZE; i++) {
    mbusSpiTx[i] = 0;
  }
  if (HAL_SPI_TransmitReceive(&hspi1, mbusSpiTx, mbusSpiRx,
                MBUS_SPI_FRAME_SIZE, 100) != HAL_OK) {
    spi_deassert_all_cs();
    return MBUS_SPI_ERROR;
  }
  spi_deassert_all_cs();

  if (mbusSpiRx[0] != MBUS_OK || mbusSpiRx[1] > MBUS_SPI_MAX_DATA) {
    /* El estado distinto de cero lo genera el esclavo al fallar UART/Modbus. */
    return (mbusSpiRx[0] == MBUS_EXCEPTION) ? MBUS_EXCEPTION : MBUS_INVALID_RESPONSE;
  }
  *responseLength = mbusSpiRx[1];
  for (uint8_t i = 0; i < *responseLength; i++) {
    response[i] = mbusSpiRx[i + 2];
  }
  return MBUS_OK;
}

static mbusStatus_t mbus_queue_read(uint8_t slaveAddress, uint8_t function, uint16_t position, uint16_t quantity)
{
  uint8_t *request;
  uint16_t crc;

  if (quantity == 0) {
    return MBUS_INVALID_ARGUMENT;
  }
  if (mbusQueueCount >= MBUS_QUEUE_SIZE) {
    return MBUS_QUEUE_FULL;
  }
    /* Una solicitud de lectura Modbus tiene 8 bytes: direccion, funcion,
      posicion, cantidad y CRC en orden little-endian. */
    request = mbusQueue[mbusQueueTail];
  request[0] = slaveAddress;
  request[1] = function;
  request[2] = (uint8_t)(position >> 8);
  request[3] = (uint8_t)position;
  request[4] = (uint8_t)(quantity >> 8);
  request[5] = (uint8_t)quantity;
  crc = mbus_crc16(request, 6);
  request[6] = (uint8_t)crc;
  request[7] = (uint8_t)(crc >> 8);
  mbusQueueLength[mbusQueueTail] = 8;
  mbusQueueTail = (uint8_t)((mbusQueueTail + 1) % MBUS_QUEUE_SIZE);
  mbusQueueCount++;
  return MBUS_OK;
}

mbusStatus_t mbus_read_coils(uint8_t slaveAddress, uint16_t position, uint16_t quantity)
{
  return mbus_queue_read(slaveAddress, 0x01, position, quantity);
}

mbusStatus_t mbus_read_input_contacts(uint8_t slaveAddress, uint16_t position, uint16_t quantity)
{
  return mbus_queue_read(slaveAddress, 0x02, position, quantity);
}

mbusStatus_t mbus_read_holding_registers(uint8_t slaveAddress, uint16_t position, uint16_t quantity)
{
  return mbus_queue_read(slaveAddress, 0x03, position, quantity);
}

mbusStatus_t mbus_read_input_registers(uint8_t slaveAddress, uint16_t position, uint16_t quantity)
{
  return mbus_queue_read(slaveAddress, 0x04, position, quantity);
}

static mbusStatus_t mbus_queue_write(uint8_t slaveAddress, uint8_t function,
                                      uint16_t position, uint16_t value)
{
  uint8_t *request;
  uint16_t crc;

  if (mbusQueueCount >= MBUS_QUEUE_SIZE) {
    return MBUS_QUEUE_FULL;
  }
    /* Las escrituras simples usan la misma estructura, pero value ocupa los
      dos bytes donde las lecturas almacenan quantity. */
    request = mbusQueue[mbusQueueTail];
  request[0] = slaveAddress;
  request[1] = function;
  request[2] = (uint8_t)(position >> 8);
  request[3] = (uint8_t)position;
  request[4] = (uint8_t)(value >> 8);
  request[5] = (uint8_t)value;
  crc = mbus_crc16(request, 6);
  request[6] = (uint8_t)crc;
  request[7] = (uint8_t)(crc >> 8);
  mbusQueueLength[mbusQueueTail] = 8;
  mbusQueueTail = (uint8_t)((mbusQueueTail + 1) % MBUS_QUEUE_SIZE);
  mbusQueueCount++;
  return MBUS_OK;
}

static mbusStatus_t mbus_queue_frame(const uint8_t *frame, uint8_t frameLength)
{
  if (frame == NULL || frameLength > MBUS_SPI_MAX_DATA || mbusQueueCount >= MBUS_QUEUE_SIZE) {
    return (frameLength > MBUS_SPI_MAX_DATA) ? MBUS_INVALID_ARGUMENT : MBUS_QUEUE_FULL;
  }
  for (uint8_t i = 0; i < frameLength; i++) {
    mbusQueue[mbusQueueTail][i] = frame[i];
  }
  mbusQueueLength[mbusQueueTail] = frameLength;
  mbusQueueTail = (uint8_t)((mbusQueueTail + 1) % MBUS_QUEUE_SIZE);
  mbusQueueCount++;
  return MBUS_OK;
}

/* Las siguientes funciones conservan los mismos nombres del proyecto MBUS,
   pero en el CPU la orden queda pendiente hasta la siguiente iteracion PLC. */
mbusStatus_t readCoils(uint8_t address, uint16_t position, uint16_t quantity)
{
  return mbus_read_coils(address, position, quantity);
}

mbusStatus_t writeSingleCoil(uint8_t address, uint16_t position, uint16_t value)
{
  return mbus_write_single_coil(address, position, value != 0);
}

mbusStatus_t readInputContacts(uint8_t address, uint16_t position, uint16_t quantity)
{
  return mbus_read_input_contacts(address, position, quantity);
}

mbusStatus_t readHoldingRegisters(uint8_t address, uint16_t position, uint16_t quantity)
{
  return mbus_read_holding_registers(address, position, quantity);
}

mbusStatus_t writeSingleRegister(uint8_t address, uint16_t position, uint16_t value)
{
  return mbus_write_single_register(address, position, value);
}

mbusStatus_t readAnalogInputs(uint8_t address, uint16_t position, uint16_t quantity)
{
  return mbus_read_input_registers(address, position, quantity);
}

mbusStatus_t writeMultipleCoils(uint8_t address, uint16_t position,
                                const uint8_t *values, uint16_t quantity)
{
  uint8_t frame[MBUS_SPI_MAX_DATA];
  uint16_t crc;
  uint8_t byteCount;

  if (values == NULL || quantity == 0 || quantity > 952) {
    return MBUS_INVALID_ARGUMENT;
  }
  byteCount = (uint8_t)((quantity + 7) / 8);
  if ((uint16_t)(9 + byteCount) > MBUS_SPI_MAX_DATA) {
    return MBUS_INVALID_ARGUMENT;
  }
  frame[0] = address;
  frame[1] = 0x0F;
  frame[2] = (uint8_t)(position >> 8);
  frame[3] = (uint8_t)position;
  frame[4] = (uint8_t)(quantity >> 8);
  frame[5] = (uint8_t)quantity;
  frame[6] = byteCount;
  for (uint8_t i = 0; i < byteCount; i++) frame[7 + i] = 0;
  for (uint16_t i = 0; i < quantity; i++) {
    if (values[i]) frame[7 + i / 8] |= (uint8_t)(1 << (i % 8));
  }
  crc = mbus_crc16(frame, (uint8_t)(7 + byteCount));
  frame[7 + byteCount] = (uint8_t)crc;
  frame[8 + byteCount] = (uint8_t)(crc >> 8);
  return mbus_queue_frame(frame, (uint8_t)(9 + byteCount));
}

mbusStatus_t writeMultipleRegisters(uint8_t address, uint16_t position,
                                    const uint16_t *values, uint16_t quantity)
{
  uint8_t frame[MBUS_SPI_MAX_DATA];
  uint16_t crc;
  uint16_t frameLength;

  if (values == NULL || quantity == 0 || quantity > 59) {
    return MBUS_INVALID_ARGUMENT;
  }
  frameLength = (uint16_t)(9 + quantity * 2);
  frame[0] = address;
  frame[1] = 0x10;
  frame[2] = (uint8_t)(position >> 8);
  frame[3] = (uint8_t)position;
  frame[4] = (uint8_t)(quantity >> 8);
  frame[5] = (uint8_t)quantity;
  frame[6] = (uint8_t)(quantity * 2);
  for (uint16_t i = 0; i < quantity; i++) {
    frame[7 + i * 2] = (uint8_t)(values[i] >> 8);
    frame[8 + i * 2] = (uint8_t)values[i];
  }
  crc = mbus_crc16(frame, (uint8_t)(7 + quantity * 2));
  frame[7 + quantity * 2] = (uint8_t)crc;
  frame[8 + quantity * 2] = (uint8_t)(crc >> 8);
  return mbus_queue_frame(frame, (uint8_t)frameLength);
}

mbusStatus_t mbus_write_single_register(uint8_t slaveAddress, uint16_t position, uint16_t value)
{
  return mbus_queue_write(slaveAddress, 0x06, position, value);
}

mbusStatus_t mbus_write_single_coil(uint8_t slaveAddress, uint16_t position, bool value)
{
  return mbus_queue_write(slaveAddress, 0x05, position, value ? 0xFF00 : 0x0000);
}

mbusStatus_t mbus_process(void)
{
  uint8_t responseLength;
  mbusStatus_t status;

  if (mbusQueueCount == 0) {
    return MBUS_NO_COMMAND;
  }
    /* mbus_spi_exchange realiza las tres fases SPI: enviar la trama, avisar
      que se solicitara la respuesta y leer el marco de respuesta. */
    status = mbus_spi_exchange(mbusQueue[mbusQueueHead], mbusQueueLength[mbusQueueHead],
                             mbusLastResponse, &responseLength);
  mbusQueueHead = (uint8_t)((mbusQueueHead + 1) % MBUS_QUEUE_SIZE);
  mbusQueueCount--;
  if (status != MBUS_OK) {
    return status;
  }
  mbusLastResponseLength = responseLength;
    /* Una respuesta Modbus RTU termina con dos bytes CRC. El byte de funcion
      con el bit 0x80 indica una excepcion reportada por el esclavo. */
    if (responseLength < 5 || mbusLastResponse[1] & modbusExceptionMask ||
      mbus_crc16(mbusLastResponse, responseLength - 2) !=
      (uint16_t)(mbusLastResponse[responseLength - 2] |
                 (mbusLastResponse[responseLength - 1] << 8))) {
    return (mbusLastResponse[1] & modbusExceptionMask) ? MBUS_EXCEPTION : MBUS_INVALID_RESPONSE;
  }
  return MBUS_OK;
}

const uint8_t *mbus_get_last_response(uint8_t *responseLength)
{
  if (responseLength != NULL) {
    *responseLength = mbusLastResponseLength;
  }
  return mbusLastResponse;
}

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);

void induInit(void){
    HAL_Init();
    SystemClock_Config();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    MX_GPIO_Init();
    MX_SPI1_Init();
}

void digWrite(int modulo, int salida, bool estado){
    //modulo = 0 -> CPU
    //modulo = 1 -> I/O 

    if(modulo) estAct.modOd[salida] = estado; // guardo el estado deseado de la salida elegida.
    else estAct.cpuOd[salida] = estado;
    
    //Posibilidad de agregar más módulos
    //faltaria agregar defines
    //if(modulo == 8){ 
    //    estAct.modOd[7][salida] = estado;
    //}
}

void anWrite(int salida, uint8_t valor){
    estAct.modOa[salida] = valor; // guardo el valor deseado (0 a 255) de la salida elegida.
}

bool digRead(int modulo, int entrada){
    //modulo = 0 -> CPU
    //modulo = 1 -> I/O 
    if(modulo) return estAct.modId[entrada];
    else  return estAct.cpuId[entrada];
}

uint8_t anRead(int entrada){
    return estAct.modIa[entrada];
}

// void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
//     if (hspi == &hspi1) {
//         spiTransferInProgress = false;
//         spiTransferStartTick = 0;
//         plc_store_spi_inputs();
//            /* Deassert CS (NSS) after transaction completed */
//            HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_SET);//cambiar a set
//     }
// }

void plc_store_spi_inputs(void){
    for (int i = 0; i < 8; i++) {
        estAct.modId[i] = ((InRxBuffer[0] >> i) & 0x1);
    }

    for (int i = 0; i < 2; i++) {
        estAct.modIa[i] = InRxBuffer[i + 1];
    }

}


void plc_read_inputs(void){
    for(int i= 0; i<4;i++){
        estAct.cpuId[i] = HAL_GPIO_ReadPin(GPIOB, entCpu[i]);
    }
    
    InTxBuffer = 0x01;

    /* Una lectura SPI necesita transmitir para generar el reloj. */
    
    for (int i = 0; i < 4; i++) {
      spiRxBuffer[i] = 0x00;
    }

    spi_deassert_all_cs();
    spi_wait_ready();

    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &InTxBuffer, 1, 5);
    HAL_SPI_Receive(&hspi1, (uint8_t*)InRxBuffer, 3, 5);

    spi_deassert_all_cs();
    plc_store_spi_inputs();
}

void plc_write_outputs(void){
    for (int i = 0; i < 4; i++) {
        HAL_GPIO_WritePin(GPIOB, salCpu[i], estAct.cpuOd[i]);
    }

    spiTxBuffer[0] = 0x02;
    spiTxBuffer[1] = 0x00;
    spiTxBuffer[2] = 0x00;
    spiTxBuffer[3] = 0x00;

    for (int i = 0; i < 8; i++) {
        if (estAct.modOd[i]) {
            spiTxBuffer[1] |= (uint8_t)(0x01 << i);
        }
    }

    /* estAct.modOa has two channels indexed 0 and 1 */
    for (int i = 0; i < 2; i++) {
        spiTxBuffer[i + 2] = estAct.modOa[i];
    }

    spi_deassert_all_cs();
    spi_wait_ready();

    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_SET);

    if (HAL_SPI_Transmit(&hspi1, spiTxBuffer, 4, 10) != HAL_OK) {
        spi_deassert_all_cs();
        return;
    }

    spi_deassert_all_cs();
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi){
    if (hspi == &hspi1) {
        spi_deassert_all_cs();
        spiState = SPI_IDLE;
    }
}




void plc_run_cycle(void (*fuser)(void)){
    plc_read_inputs();
    /* La logica del usuario puede agregar ordenes MBUS a la cola. */
    fuser();
    /* Procesa una orden MBUS pendiente de forma bloqueante antes de actualizar
       las salidas. Si no hay orden, devuelve MBUS_NO_COMMAND inmediatamente. */
    mbus_process();
    plc_write_outputs();
    HAL_Delay(10);
        
}
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  /* Los dos CS se manejan manualmente con GPIO. */
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);//faltaba esto
  HAL_NVIC_EnableIRQ(SPI1_IRQn);//faltaba esto
  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, SPI1_NSS2_Pin|SPI1_NSS_Pin|MBUS_SPI_CS_Pin, GPIO_PIN_SET);//estaba en reset

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pins : SPI1_NSS2_Pin SPI1_NSS_Pin */
  GPIO_InitStruct.Pin = SPI1_NSS2_Pin|SPI1_NSS_Pin|MBUS_SPI_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 PB5 PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
