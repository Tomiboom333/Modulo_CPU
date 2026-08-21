#include "main.h"
#include "induSPI.h"
bool estado = false;
void algo(){
  if(digRead(MOD_IO, IO_ENT_1)== 1){
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 1);
  }else{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 0);
  }
}

void algo2(){
  digWrite(MOD_CPU, CPU_SAL_1, LOW);
  digRead(MOD_IO, IO_ENT_1);
}
int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
  }
}
