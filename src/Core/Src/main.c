#include "main.h"
#include "induSPI.h"

void algo(){
  if(digRead(1,1)) digWrite(1, 1, HIGH);
}
int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(algo);
  }
}