#include "main.h"
#include "induSPI.h"
bool estado = false;

void algo(){
  digWrite(MOD_OUT_1, SAL_1, HIGH);
  digRead(MOD_IN_1, ENT_1 );
}
void algo2(){
  digWrite(MOD_OUT_1, SAL_1, LOW);
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