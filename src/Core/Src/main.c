#include "main.h"
#include "induSPI.h"

void algo(){
  if((digRead(MOD_CPU, CPU_ENT_1))){ 
    digWrite(MOD_CPU, CPU_SAL_1, HIGH);
  }
  else{
    digWrite(MOD_CPU, CPU_SAL_1, LOW);
  }
}
int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
  }
}
