#include "main.h"
#include "induSPI.h"

void algo(){
  if(digRead(MOD_IO, IO_SAL_1)) digWrite(MOD_IO, IO_SAL_1, HIGH);
}

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
  }
}
