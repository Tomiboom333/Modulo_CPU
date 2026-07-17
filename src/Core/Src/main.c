#include "main.h"
#include "induSPI.h"

void algo(){
  anWrite(IO_SAL_1, 10);
}
void algo2(){
  anWrite(IO_SAL_1, 10);
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
