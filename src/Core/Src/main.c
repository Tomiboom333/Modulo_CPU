#include "main.h"
#include "induSPI.h"
bool estado = false;

void algo(){
    digWrite(MOD_OUT_1, SAL_1, HIGH);
    digWrite(MOD_OUT_1, SAL_2, HIGH);
}

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
  }
}