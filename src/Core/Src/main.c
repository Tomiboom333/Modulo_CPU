#include "main.h"
#include "induSPI.h"
bool estado = false;

void algo(){
  digWrite(MOD_1, IO_SAL_1, HIGH);
}
void algo2(){
  digWrite(MOD_1, IO_SAL_1, LOW);
}

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
    HAL_Delay(1000);
    plc_run_cycle(algo2);
    HAL_Delay(1000);
  }
}