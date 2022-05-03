#include "user_main.h"
#include "main.h"
#include "usbd_cdc_if.h"
#include "string.h"

//#include "FreeRTOS.h"
//#include "task.h"
//#include "semphr.h"
//#include "stdio.h"

uint8_t userRxData[64];
char txData[64];

int user_main(void){

  while(1){

    if(strlen((char *) userRxData) > 0) {
      strcpy(txData, "echo: ");
      strcat(txData, (char *) userRxData);
      strcat(txData, (char *) '\n');
  //    CDC_Transmit_FS((uint8_t*) txData, strlen(txData));
      CDC_Transmit_FS((uint8_t*) txData, strlen(txData));
      memset(txData, '\0', 64);
      memset(userRxData, '\0', 64);
    }

    HAL_Delay(1);

  }
}
