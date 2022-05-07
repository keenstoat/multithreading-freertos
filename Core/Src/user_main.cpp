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
#define MAX_WORKER_TASKS 7
#define TASK_PRIORITY 1

using namespace std;


void mainTask(void *);
void workerTask(void *);

void processInput(string) ;
uint8_t sendOverUsb(string);
void addWorkerTask();
void pushPendingQueue(string) ;
string popFrontPendingQueue();
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

TaskHandle_t taskArray[MAX_WORKER_TASKS];
uint8_t taskArrayIndex = 0;


int user_main(void){

  MX_USB_DEVICE_Init();


  pendingQueueMutex = xSemaphoreCreateMutexStatic(&pendingQueueMutexBuffer);
  solvedQueueMutex = xSemaphoreCreateMutexStatic(&solvedQueueMutexBuffer);


  xTaskCreate(
      mainTask,
      "mainTask",
      configMINIMAL_STACK_SIZE,
      ( void * ) NULL,
      TASK_PRIORITY,
      NULL
  );

  addWorkerTask();

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

      string response = "Message received " +
          std::to_string(input.length()) + " Bytes\n";
      sendOverUsb(response);

      processInput(input);
      sendOverUsb("Message processed\n\n");

      input.clear();
      memset(inputBuffer, '\0', APP_RX_DATA_SIZE);
    }

//    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

void workerTask(void * arg) {
  while(1) {

    string pendingProblem = popFrontPendingQueue();
    if(!pendingProblem.empty()){
      uint32_t iniTick = (uint32_t) xTaskGetTickCount();

      stringstream ss(pendingProblem);
      string token;
      while(ss >> token) {
        printf((token + "\n").c_str());
      }

      uint32_t endTick = (uint32_t) xTaskGetTickCount();

      int durationMs = pdTICKS_TO_MS(endTick - iniTick);
      printf("Duration: %d ms \n\n", durationMs);
    }

//    TaskStatus_t xTaskDetails;
//    vTaskGetInfo( NULL, &xTaskDetails, pdTRUE, eInvalid );
//    printf("TaskId: %d\n", (int) xTaskDetails.xTaskNumber);
    vTaskDelay(pdMS_TO_TICKS(100));
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
          addWorkerTask();
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

void addWorkerTask() {

  uint8_t numberOfWorkerTasks = uxTaskGetNumberOfTasks() - 1;
//  if(numberOfWorkerTasks < MAX_WORKER_TASKS) {
    string taskName = "worker" + std::to_string(numberOfWorkerTasks);
    xTaskCreate(
      workerTask,
      taskName.c_str(),
      configMINIMAL_STACK_SIZE,
      ( void * ) NULL,
      TASK_PRIORITY,
      NULL
    );
    printf("%s created\n", taskName.c_str());
//  } else {
//    printf("Cannot create worker No: %d\n", numberOfWorkerTasks);
//  }


}

void pushPendingQueue(string problem) {

#ifdef USE_MUTEX
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
    pendingQueue.push_back(problem);
    xSemaphoreGive(pendingQueueMutex);
  }
#else
  pendingQueue.push_back(problem);
#endif
}

string popFrontPendingQueue() {
  string element;
#ifdef USE_MUTEX
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
    element = pendingQueue.front();
    pendingQueue.pop_front();
    xSemaphoreGive(pendingQueueMutex);
  }
#else
  element = pendingQueue.front();
  pendingQueue.pop_front();
#endif
  return element;
}

void showPendingQueue() {
#ifdef USE_MUTEX
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
    for(string element: pendingQueue) {
      sendOverUsb(element + "\n");
    }
    xSemaphoreGive(pendingQueueMutex);
  }
#else
  list<string>::iterator it;
  for (it = pendingQueue.begin(); it != pendingQueue.end(); ++it) {
    string element = ((string) *it) + "\n";
    sendOverUsb(element);
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
#ifdef USE_MUTEX
  if(xSemaphoreTake(solvedQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
    solvedQueue.clear();
    xSemaphoreGive(pendingQueueMutex);
  }
#else
  solvedQueue.clear();
#endif

}





