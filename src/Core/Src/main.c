#include "main.h"
#include "induSPI.h"
bool estado = false;

void algo(){
  digWrite(MOD_OUT_1, SAL_1, HIGH);
  if (digRead(MOD_IN_1, ENT_1)||digRead(MOD_IN_1, ENT_2)||digRead(MOD_IN_1, ENT_3)||digRead(MOD_IN_1, ENT_4)||digRead(MOD_IN_1, ENT_5)||digRead(MOD_IN_1, ENT_6)||digRead(MOD_IN_1, ENT_7)||digRead(MOD_IN_1, ENT_8)){
    digWrite(MOD_OUT_1, SAL_3, HIGH);
  }
  if (!(digRead(MOD_IN_1, ENT_1)||digRead(MOD_IN_1, ENT_2)||digRead(MOD_IN_1, ENT_3)||digRead(MOD_IN_1, ENT_4)||digRead(MOD_IN_1, ENT_5)||digRead(MOD_IN_1, ENT_6)||digRead(MOD_IN_1, ENT_7)||digRead(MOD_IN_1, ENT_8))){
    digWrite(MOD_OUT_1, SAL_3, LOW);
  }
  anWrite(MOD_OUT_1, SAL_1, anRead(MOD_IN_1, ENT_1));
}
void algo2(){
  digWrite(MOD_OUT_1, SAL_1, LOW);
  digWrite(MOD_OUT_1, SAL_2, LOW);
  anWrite(MOD_OUT_1, SAL_1, 191);
}

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
    // HAL_Delay(2500);
    // plc_run_cycle(algo2);
    // HAL_Delay(2500);
  
  }
}