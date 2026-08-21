#include "induSPI.h"
#include "stm32f103xb.h"
SPI_HandleTypeDef hspi1;
estAct_t estAct;

static uint8_t spiTxBuffer[4];
static uint8_t spiRxBuffer[4];

typedef enum {
  SPI_IDLE,
  SPI_READING_INPUTS,
  SPI_INPUTS_READY,
  SPI_WRITING_OUTPUTS
} spiState_t;

static volatile spiState_t spiState = SPI_IDLE;

static uint16_t entCpu[4] = {GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15};
static uint16_t salCpu[4] = {GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6};

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
        estAct.modId[i] = (spiRxBuffer[0] >> i) & 0x1;
    }

    for (int i = 0; i < 2; i++) {
        estAct.modIa[i] = spiRxBuffer[i + 1];
    }

}


void plc_read_inputs(void){
    for(int i= 0; i<4;i++){
        estAct.cpuId[i] = HAL_GPIO_ReadPin(GPIOB, entCpu[i]);
    }

  if (spiState != SPI_IDLE) {
    return;
  }

    spiTxBuffer[0] = 0x01;
    
    for (int i = 1; i < 4; i++) {//asegurarse que lo demas es 0
        spiTxBuffer[i] = 0x00;
    }

    /* Una lectura SPI necesita transmitir para generar el reloj. */
    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_RESET);

    for (int i = 0; i < 4; i++) {
        spiRxBuffer[i] = 0x00;
    }

    spiState = SPI_READING_INPUTS;
    if (HAL_SPI_TransmitReceive_IT(&hspi1, spiTxBuffer, spiRxBuffer, 4) != HAL_OK) {
      spiState = SPI_IDLE;
      HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_SET);
    }

}

void plc_write_outputs(void){
    for (int i = 0; i < 4; i++) {
        HAL_GPIO_WritePin(GPIOB, salCpu[i], estAct.cpuOd[i]);
    }

    if (spiState != SPI_INPUTS_READY) {
      return;
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

    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_SET);
    spiState = SPI_WRITING_OUTPUTS;
    if (HAL_SPI_Transmit_IT(&hspi1, spiTxBuffer, 4) != HAL_OK) {
      spiState = SPI_IDLE;
      HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi){
    if (hspi == &hspi1) {

          HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
          spiState = SPI_IDLE;
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
    if (hspi == &hspi1) {
    HAL_GPIO_WritePin(SPI1_NSS2_GPIO_Port, SPI1_NSS2_Pin, GPIO_PIN_SET);
        plc_store_spi_inputs();
    spiState = SPI_INPUTS_READY;
    }
}



void plc_run_cycle(void (*fuser)(void)){
    plc_read_inputs();

  /* La logica espera al callback; nunca se ejecuta dentro de la ISR. */
  if (spiState == SPI_INPUTS_READY) {
    fuser();
    plc_write_outputs();
  }
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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
  HAL_GPIO_WritePin(GPIOA, SPI1_NSS2_Pin|SPI1_NSS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pins : SPI1_NSS2_Pin SPI1_NSS_Pin */
  GPIO_InitStruct.Pin = SPI1_NSS2_Pin|SPI1_NSS_Pin;
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
