#include "main.h"
#include "induSPI.h"
bool estado = false;
void algo(){
  if(digRead(MOD_IO, IO_ENT_1)== 1){
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 1);
    digWrite(MOD_CPU, CPU_ENT_1, HIGH);
    digWrite(MOD_IO, IO_SAL_1, LOW);
  }else{
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 0);
    digWrite(MOD_CPU, CPU_ENT_1, LOW);
    digWrite(MOD_IO, IO_SAL_1, HIGH);
  }
}

void algo2(){
  //digWrite(MOD_CPU, IO_SAL_1, HIGH);
  digWrite(MOD_IO, IO_SAL_1, LOW);
  //digRead(MOD_IO, IO_ENT_1);
}
void algo3(){
  //digWrite(MOD_IO, IO_SAL_1, HIGH);
  //digWrite(MOD_CPU, IO_SAL_1, LOW);]
  //digRead(MOD_IO, IO_ENT_1);
  if(digRead(MOD_IO, IO_ENT_1)== 1){
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 1);
    digWrite(MOD_CPU, CPU_ENT_1, HIGH);
    digWrite(MOD_IO, IO_SAL_1, HIGH);
    
  }else{
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 0);
    digWrite(MOD_IO, IO_SAL_1, LOW);
    digWrite(MOD_CPU, IO_SAL_1, LOW);
  }
}
int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo3);
  }
}