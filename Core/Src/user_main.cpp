#include "user_main.h"
#include "main.h"

#include "stdio.h"
#include "string.h"
#include <string>

#include "usb_device.h"
#include "usbd_cdc_if.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"




void mainTask(void * arg);


uint8_t userRxData[64];
char txData[64];

int user_main(void){


  xTaskCreate(
      mainTask,
      "mainTask",
      configMINIMAL_STACK_SIZE,
      ( void * ) NULL,
      0,
      NULL
  );

  vTaskStartScheduler();
  // MUST NOT reach this while
  while(1);
}






void mainTask(void * arg) {

  MX_USB_DEVICE_Init();

  std::string input = (char *) userRxData;
  while(1) {

    if(strlen((char *) userRxData) > 0) {




      strcpy(txData, "echo: ");
      strcat(txData, (char *) userRxData);
      strcat(txData, (char *) '\n');
      CDC_Transmit_FS((uint8_t*) txData, strlen(txData));
      memset(txData, '\0', 64);
      memset(userRxData, '\0', 64);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

}
