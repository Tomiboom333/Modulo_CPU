#include "main.h"
#include "induSPI.h"
bool estado = false;

void algo(){
  digWrite(MOD_OUT_1, SAL_0, HIGH);
  digWrite(MOD_OUT_1, SAL_1, LOW);
  readHoldingRegisters(0x0001, 0x0010, 0x0002);
}
void algo2(){
  digWrite(MOD_OUT_1, SAL_0, LOW);
  digWrite(MOD_OUT_1, SAL_1, HIGH);
  readHoldingRegisters(0x0001, 0x0001, 0x0001);
}
void algo3(){
  estado = digRead(MOD_IN_1, 0);
  digWrite(MOD_OUT_1, SAL_0, LOW);
  digWrite(MOD_OUT_1, SAL_1, LOW);
  digWrite(MOD_OUT_1, SAL_2, HIGH); 
}

int main(void)
{
  induInit();
  while(1){
    
    plc_run_cycle(algo);
    HAL_Delay(2000);
    plc_run_cycle(algo2);
    HAL_Delay(2000);
    
  }
}