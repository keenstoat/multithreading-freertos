//#include <MyQueue.h>
#include "user_main.h"
#include "main.h"

#include <string>
#include <queue>
#include <list>
#include <sstream>
#include "usb_device.h"
#include "usbd_cdc_if.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define USE_MUTEX
#define HASH '#'
#define ASTERISK '*'
#define AMPERSAND '&'
#define WORKER_TASKS 4
#define TASK_PRIORITY 1

using namespace std;

void serveUsbISRTask(void *);
void bossTask(void *);
void workerTask(void *);
void toggleLEDs(void *);

void processInput(string) ;
void sendOverUsb(string);
void addWorkerTask(uint8_t);
void pushPendingQueue(string);
void pushSolvedQueue(string);
string popFrontPendingQueue();
void showPendingQueue();
void showSolvedQueue();
void clearSolvedQueue();


static char inputBuffer[APP_RX_DATA_SIZE];
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

StaticSemaphore_t pendingQueueMutexBuffer;
SemaphoreHandle_t pendingQueueMutex;

StaticSemaphore_t solvedQueueMutexBuffer;
SemaphoreHandle_t solvedQueueMutex;

StaticSemaphore_t usbSemBuffer;
SemaphoreHandle_t usbBinSemaphore; //externed in usbd_cdc_if

BaseType_t xHigherPriorityTaskWoken; //externed in usbd_cdc_if

TaskHandle_t serveUsbISRTaskHandle;

uint16_t ledArray[WORKER_TASKS];
StaticSemaphore_t gpioDMutexBuffer;
SemaphoreHandle_t gpioDMutex;

list<string> solvedQueue;
list<string> pendingQueue;



int user_main(void){

  MX_USB_DEVICE_Init();

  pendingQueueMutex = xSemaphoreCreateMutexStatic(&pendingQueueMutexBuffer);
  solvedQueueMutex = xSemaphoreCreateMutexStatic(&solvedQueueMutexBuffer);
  usbBinSemaphore = xSemaphoreCreateBinaryStatic(&usbSemBuffer);
  gpioDMutex = xSemaphoreCreateMutexStatic(&gpioDMutexBuffer);

  xTaskCreate(
    serveUsbISRTask,
    "serveUsbISRTask",
    configMINIMAL_STACK_SIZE,
    ( void * ) NULL,
    TASK_PRIORITY,
    &serveUsbISRTaskHandle
   );

  xTaskCreate(
    toggleLEDs,
    "toggleLEDs",
    configMINIMAL_STACK_SIZE,
    ( void * ) NULL,
    TASK_PRIORITY,
    NULL
   );

//  xTaskCreate(
//      bossTask,
//      "bossTask",
//      configMINIMAL_STACK_SIZE,
//      ( void * ) NULL,
//      TASK_PRIORITY,
//      NULL //&serveUsbISRTaskHandle
//  );

  ledArray[0] = LED_GREEN_Pin;
  addWorkerTask(0);

  ledArray[1] = LED_ORANGE_Pin;
  addWorkerTask(1);

  ledArray[2] = LED_RED_Pin;
  addWorkerTask(2);

  ledArray[3] = LED_BLUE_Pin;
  addWorkerTask(3);

  vTaskStartScheduler();

  while(1);
}


int count = 0;

void serveUsbISRTask(void * arg) {

//  const TickType_t xBlockTime = pdMS_TO_TICKS(500);
  while(1) {

    printf("Serving USB ISR...%d\n", count++);
    sendOverUsb("Serving USB ISR..." + std::to_string(count) + "\n");

    if(xSemaphoreTake(usbBinSemaphore, pdMS_TO_TICKS(10) ) == pdTRUE ) {
      sendOverUsb("There is USB data ready..." + std::to_string(count) + "\n");
//      printf("Serving USB ISR...%d\n", count++);
//      memcpy(inputBuffer, UserRxBufferFS, APP_RX_DATA_SIZE);
//      printf(inputBuffer);
//      memset(UserRxBufferFS, '\0', APP_RX_DATA_SIZE);
//      printf("\n\n");
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}


void bossTask(void * arg) {

  while(1) {

    memcpy(inputBuffer, UserRxBufferFS, strlen((char *) UserRxBufferFS));
    memset(UserRxBufferFS, '\0', APP_RX_DATA_SIZE);

    string input = inputBuffer;
    if(!input.empty()) {

      string response = "Message received " +
          std::to_string(input.length()) + " Bytes\n";
      sendOverUsb(response);

//      processInput(input);
      sendOverUsb("Message processed\n\n");

      input.clear();
      memset(inputBuffer, '\0', APP_RX_DATA_SIZE);
    }

//    vTaskDelay(pdMS_TO_TICKS(300));
  }
}



/*=============================== BOSS TASK FUNCTIONS ===============================*/



void processInput(string input) {

  char cmd = '\0';
  string payload = "";

  for(int index = 0; index < (int)input.length(); index++) {
    if(input[index] == ASTERISK) {

      if(cmd == HASH) { // AGREGAR UN PROBLEMA A LA COLA
        pushPendingQueue(payload);

      } else if(cmd == AMPERSAND) {
        // =============== RESOLVER ALGUN COMANDO =============
        if(payload == "showpending") {
          showPendingQueue();
        } else if (payload == "showsolved") {
          showSolvedQueue();
        } else if (payload == "clearsolved") {
          clearSolvedQueue();
        } else if (payload == "addtask") {
//          addWorkerTask();
        } else {// it is a thread update
          sendOverUsb("T: " + payload + "\n");
        }
        // =============== ======================= =============

      } else { // MENSAJE NO RECONOCIDO
        sendOverUsb("UKN: " + payload + "\n");
      }
      cmd = '\0';
      payload = "";

    } else if(input[index] == AMPERSAND || input[index] == HASH) {
      cmd = input[index];
    } else {
      payload += input[index];
    } // if
  } // for
}

/*=============================== WORKER TASK FUNCTIONS ===============================*/

void addWorkerTask(uint8_t index) {

  string taskName = "worker"+ std::to_string(index);
  xTaskCreate(
    workerTask,
    taskName.c_str(),
    configMINIMAL_STACK_SIZE,
    (void *) &ledArray[index],
    TASK_PRIORITY,
    NULL
  );
//  printf("%s created\n", taskName.c_str());

}


void workerTask(void * arg) {

  uint16_t * gpioPinPos = ((uint16_t *) arg);
  const uint16_t GPIO_PIN = *gpioPinPos;

  while(1) {

    string pendingProblem = popFrontPendingQueue();
    if(!pendingProblem.empty()){

//      *gpioPinPos = GPIO_PIN;
      uint32_t iniTs = pdTICKS_TO_MS((uint32_t) xTaskGetTickCount());

      // DO THE WORK ----------------------------
      int duration = pendingProblem.size();

//      stringstream ss(pendingProblem);
//      string token;
      while(duration-- > 0) ;

      // ----------------------------------------

      uint32_t endTs = pdTICKS_TO_MS((uint32_t) xTaskGetTickCount());
      uint32_t durationMs = endTs - iniTs;

      TaskStatus_t xTaskDetails;
      vTaskGetInfo( NULL, &xTaskDetails, pdTRUE, eInvalid );
      string solvedProblem =
          to_string(xTaskDetails.xTaskNumber) + "," +
          pendingProblem + "," +
          to_string(iniTs) + "," +
          to_string(endTs) + "," +
          to_string(durationMs) + "\n";
      pushSolvedQueue(solvedProblem);
    } else {
//      *gpioPinPos = 0x00;
//      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void toggleLEDs(void * arg) {

  while(1) {
    uint16_t leds = 00;
    for(int led = 0; led < WORKER_TASKS; led++){
      leds |= ledArray[led];
//      if(ledArray[led] == 0x00) {
//        HAL_GPIO_WritePin(GPIOD, ledArray[led], GPIO_PIN_SET);
//      } else {
//        HAL_GPIO_TogglePin(GPIOD, ledArray[led]);
//      }
    }
    HAL_GPIO_TogglePin(GPIOD, leds);
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

/*=============================== QUEUE HANDLE FUNCTIONS ===============================*/

void pushPendingQueue(string problem) {

  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 10 ) == pdTRUE) {
    pendingQueue.push_back(problem);
    xSemaphoreGive(pendingQueueMutex);
  }
}

void pushSolvedQueue(string problem) {

  if(xSemaphoreTake(solvedQueueMutex, ( TickType_t ) 10 ) == pdTRUE) {
    solvedQueue.push_back(problem);
    xSemaphoreGive(solvedQueueMutex);
  }
}


string popFrontPendingQueue() {
  string element = "";
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 10 ) == pdTRUE) {
    if(!pendingQueue.empty()) {
      element = pendingQueue.front();
      pendingQueue.pop_front();
    }
    xSemaphoreGive(pendingQueueMutex);
  }
  return element;
}

void showPendingQueue() {

  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 10 ) == pdTRUE) {
    for(string element: pendingQueue) {
      sendOverUsb(element + "\n");
    }
    xSemaphoreGive(pendingQueueMutex);
  }
}

void showSolvedQueue() {

  if(xSemaphoreTake(solvedQueueMutex, ( TickType_t ) 10 ) == pdTRUE) {
    for(string element: solvedQueue) {
      sendOverUsb(element + "\n");
    }
    xSemaphoreGive(solvedQueueMutex);
  }
}

void clearSolvedQueue(){

  if(xSemaphoreTake(solvedQueueMutex, ( TickType_t ) 10 ) == pdTRUE) {
    solvedQueue.clear();
    xSemaphoreGive(pendingQueueMutex);
  }

}



/*=============================== UTILITY FUNCTIONS ===============================*/


void sendOverUsb(string msg) {

  printf(msg.c_str());
  while(CDC_Transmit_FS((uint8_t*)msg.c_str(), (uint32_t) msg.length()) != USBD_OK);

}



