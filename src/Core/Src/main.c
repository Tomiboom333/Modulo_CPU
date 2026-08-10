#include "main.h"
#include "induSPI.h"

void algo(){
  digWrite(MOD_IO, IO_SAL_1, HIGH);
  //digWrite(MOD_IO, IO_SAL_2, HIGH);
  //anWrite(IO_SAL_1, 255);
  anWrite(1, 255);
}
void algo2(){
  //digWrite(MOD_IO, IO_SAL_1, 0);
  digWrite(MOD_CPU, CPU_SAL_1, 1);
  if(digRead(MOD_IO, IO_ENT_1)){
    digWrite(MOD_CPU, CPU_SAL_1, 1);
  }
  else{
    digWrite(MOD_CPU, CPU_SAL_1, 0);
  }
}
int main(void)
{
  induInit();
  while(1){
    //plc_run_cycle(algo);
    //HAL_Delay(1000);
    plc_run_cycle(algo2);
    //HAL_Delay(1000);
  }
}
