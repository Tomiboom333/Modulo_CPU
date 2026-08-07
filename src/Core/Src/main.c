#include "main.h"
#include "induSPI.h"

void algo(){
  digWrite(MOD_IO, IO_SAL_1, HIGH);
  //digWrite(MOD_IO, IO_SAL_2, HIGH);
  //anWrite(IO_SAL_1, 255);
  anWrite(1, 255);
}
void algo2(){
  digWrite(MOD_CPU, CPU_SAL_1, digRead(MOD_CPU, CPU_ENT_1));
  digRead(MOD_IO, IO_ENT_1);
}
int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo2);
  }
}
