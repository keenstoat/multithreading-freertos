#include "user_main.h"
#include "main.h"

#include <string>
#include <queue>
#include <list>

#include "usb_device.h"
#include "usbd_cdc_if.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define USE_MUTEX
#define HASH '#'
#define ASTERISK '*'
#define AMPERSAND '&'

using namespace std;


void mainTask(void *);
void workerTask(void *);

void processInput(std::string) ;
uint8_t sendOverUsb(std::string);
void addTask();
void pushPendingQueue(std::string) ;
std::string popFrontPendingQueue();
void showPendingQueue();
void showSolvedQueue();
void clearSolvedQueue();

extern USBD_HandleTypeDef hUsbDeviceFS;
static char inputBuffer[APP_RX_DATA_SIZE] = {0};
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

StaticSemaphore_t pendingQueueMutexBuffer;
SemaphoreHandle_t pendingQueueMutex;
StaticSemaphore_t solvedQueueMutexBuffer;
SemaphoreHandle_t solvedQueueMutex;

list<string> solvedQueue;
list<string> pendingQueue;


int user_main(void){

  MX_USB_DEVICE_Init();


  pendingQueueMutex = xSemaphoreCreateMutexStatic(&pendingQueueMutexBuffer);
  solvedQueueMutex = xSemaphoreCreateMutexStatic(&solvedQueueMutexBuffer);
  configASSERT(pendingQueueMutex);
  configASSERT(pendingQueueMutex);

  xTaskCreate(
      mainTask,
      "mainTask",
      configMINIMAL_STACK_SIZE,
      ( void * ) NULL,
      configMAX_PRIORITIES - 1,
      NULL
  );

  vTaskStartScheduler();
  // MUST NOT reach this while
  while(1);
}


void mainTask(void * arg) {

  while(1) {

    memcpy(inputBuffer, UserRxBufferFS, strlen((char *) UserRxBufferFS));
    memset(UserRxBufferFS, '\0', APP_RX_DATA_SIZE);

    string input = inputBuffer;
    if(!input.empty()) {

      string response = "Message size: " + std::to_string(input.length()) + "\n";
      sendOverUsb(response);

      processInput(input);
      sendOverUsb("message processed\n\n");

      input.clear();
      memset(inputBuffer, '\0', APP_RX_DATA_SIZE);
    }

//    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

void workerTask(void * arg) {
  while(1) {

//    showPendingQueue();
    TaskStatus_t xTaskDetails;
    vTaskGetInfo( NULL, &xTaskDetails, pdTRUE, eInvalid );
//    sendOverUsb("TaskId: " + std::to_string(xTaskDetails.xTaskNumber) + "\n");
    printf("TaskId: %d\n", (int) xTaskDetails.xTaskNumber);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void processInput(string input) {

  char cmd = '\0';
  string payload = "";

  for(int index = 0; index < (int)input.length(); index++) {
    if(input[index] == ASTERISK) {

      if(cmd == HASH) {
        pushPendingQueue(payload);

      } else if(cmd == AMPERSAND) {

        if(payload == "showpending") {
          showPendingQueue();
        } else if (payload == "showresolved") {
          showSolvedQueue();
        } else if (payload == "clearq") {
          clearSolvedQueue();
        } else if (payload == "pop") { // TODO just for test
          sendOverUsb("Element: " + popFrontPendingQueue() + "\n");
        } else if (payload == "addtask") {
          addTask();
        } else {// it is a thread update
          sendOverUsb("T: " + payload + "\n");
        }

      } else {
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

uint8_t sendOverUsb(string msg) {

//  memcpy(UserTxBufferFS, msg.c_str(), (int) msg.length());
  printf(msg.c_str());
  while(CDC_Transmit_FS((uint8_t*)msg.c_str(), (uint32_t) msg.length()) != 0);
//  uint8_t usbStatus =  CDC_Transmit_FS(UserTxBufferFS, (uint32_t) msg.length());
//  printf(("USB: " + std::to_string(usbStatus) + "\n").c_str());
  return 0;
}

void addTask() {
  xTaskCreate(
    workerTask,
    "worker",
    configMINIMAL_STACK_SIZE,
    ( void * ) NULL,
    0,
    NULL
  );
//  sendOverUsb("T: " + payload + "\n");
}

void pushPendingQueue(string problem) {

#ifdef USE_MUTEX
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
#endif
    pendingQueue.push_back(problem);
#ifdef USE_MUTEX
    xSemaphoreGive(pendingQueueMutex);
  }
#endif
}

std::string popFrontPendingQueue() {
  string element;

#ifdef USE_MUTEX
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
#endif

    element = pendingQueue.front();
    pendingQueue.pop_front();
#ifdef USE_MUTEX
    xSemaphoreGive(pendingQueueMutex);
  }
#endif
  return element;
}

void showPendingQueue() {

#ifdef USE_MUTEX
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
#endif
//    list<string>::iterator it;
//    for (it = pendingQueue.begin(); it != pendingQueue.end(); ++it) {
//      string element = ((string) *it) + "\n";
//      sendOverUsb(element);
//    }
    for(string element: pendingQueue) {
      sendOverUsb(element + "\n");
    }
#ifdef USE_MUTEX
    xSemaphoreGive(pendingQueueMutex);
  }
#endif
}

void showSolvedQueue() {
    list<string>::iterator it;
    for (it = solvedQueue.begin(); it != solvedQueue.end(); ++it) {
      string element = ((string) *it) + "\n";
      sendOverUsb(element);
    }
}

void clearSolvedQueue(){
  solvedQueue.clear();
}





