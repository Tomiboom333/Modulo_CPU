#include "main.h"
#include "induSPI.h"
bool estado = false;

void algo(){
  //digWrite(MOD_IO, IO_SAL_1, HIGH);
  //digWrite(MOD_CPU, IO_SAL_1, LOW);]
  //digRead(MOD_IO, IO_ENT_1);
  if(digRead(MOD_1, IO_ENT_1)== 1){
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 1);
    digWrite(MOD_1, IO_SAL_1, HIGH);
    
  }else{
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 0);
    digWrite(MOD_1, IO_SAL_1, LOW);
  }
}
void algo2(){
  digWrite(MOD_1, IO_SAL_1, HIGH);
  readHoldingRegisters(0x0001, 3, 2);
  
}

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
    
    HAL_Delay(1000);
  }
}