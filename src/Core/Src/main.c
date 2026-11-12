#include "main.h"
#include "induSPI.h"
bool estado = false;

void algo(){
  estado = digRead(MOD_IN_1, ENT_0);
  if(estado){
    digWrite(MOD_OUT_1, SAL_0, HIGH);
    digWrite(MOD_OUT_1, SAL_1, LOW);
  }
  else{
    digWrite(MOD_OUT_1, SAL_0, LOW);
    digWrite(MOD_OUT_1, SAL_1, HIGH);
  }
}
void algo2(){
  digWrite(MOD_OUT_1, SAL_0, LOW);
  digWrite(MOD_OUT_1, SAL_1, LOW);

  digWrite(MOD_OUT_1, SAL_2, LOW);
  digWrite(MOD_OUT_1, SAL_3, LOW);
}

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
  }
}