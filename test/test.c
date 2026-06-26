#include "main.h"
#include "induSPI.h"

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(*algo);
  }
}
void algo(){
  if(digRead(1)) digWrite(1, HIGH);
}