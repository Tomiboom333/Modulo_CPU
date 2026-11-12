#include "main.h"
#include "induSPI.h"
bool estado = false;
uint8_t valor;
void boton(){
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
void analogicas(){
  valor = anRead(MOD_IN_1, 0);
  if(valor > 128){
    digWrite(MOD_OUT_1, SAL_0, LOW);
    digWrite(MOD_OUT_1, SAL_1, HIGH);
  }else if(valor < 128){
    digWrite(MOD_OUT_1, SAL_0, HIGH);
    digWrite(MOD_OUT_1, SAL_1, LOW);
  }
}
void dinamo(){
  anWrite(MOD_OUT_1, 1, 128);
}

int main(void)
{
  induInit();
  while(1){
    plc_run_cycle(dinamo);
  }
}