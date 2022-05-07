//#include <MyQueue.h>
#include "user_main.h"
#include "main.h"
#include "stm32f4xx_it.h"

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

void serveUsbISRTask(void *);
void bossTask(void *);
void workerTask(void *);
void toggleLed(uint16_t);

void processInput(string) ;
uint8_t sendOverUsb(string);
void addWorkerTask(uint8_t);
void pushPendingQueue(string) ;
string popFrontPendingQueue();
void showPendingQueue();
void showSolvedQueue();
void clearSolvedQueue();

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

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
//Queue myPendingQ;


//TaskHandle_t taskArray[MAX_WORKER_TASKS];
//uint8_t taskArrayIndex = 0;
uint16_t ledArray[4];
StaticSemaphore_t gpioDMutexBuffer;
SemaphoreHandle_t gpioDMutex;

//StaticTask_t mainTaskBuff;
//StackType_t mainTaskStack[ configMINIMAL_STACK_SIZE ];

StaticTask_t xTaskBufferArray[4];
StackType_t xStackArray[4][ configMINIMAL_STACK_SIZE ];

TaskHandle_t serveUsbISRTaskHandle;

int user_main(void){

  MX_USB_DEVICE_Init();

  pendingQueueMutex = xSemaphoreCreateMutexStatic(&pendingQueueMutexBuffer);
  solvedQueueMutex = xSemaphoreCreateMutexStatic(&solvedQueueMutexBuffer);
  gpioDMutex = xSemaphoreCreateMutexStatic(&gpioDMutexBuffer);

//  xTaskCreate(
//    serveUsbISRTask,
//    "serveUsbISRTask",
//    configMINIMAL_STACK_SIZE,
//    ( void * ) NULL,
//    TASK_PRIORITY,
//    &serveUsbISRTaskHandle
//   );

  xTaskCreate(
      bossTask,
      "bossTask",
      configMINIMAL_STACK_SIZE,
      ( void * ) NULL,
      TASK_PRIORITY,
      &serveUsbISRTaskHandle
  );

//  ledArray[0] = LED_GREEN_Pin;
//  addWorkerTask(0);
//
//  ledArray[1] = LED_ORANGE_Pin;
//  addWorkerTask(1);
//
//  ledArray[2] = LED_RED_Pin;
//  addWorkerTask(2);
//
//  ledArray[3] = LED_BLUE_Pin;
//  addWorkerTask(3);

  vTaskStartScheduler();
  // MUST NOT reach this while
  while(1);
}

void serveUsbISRTask(void * arg) {

//  vTaskSuspend(NULL);
  while(1) {
//    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
    printf("Serving USB ISR...");
    vTaskSuspend(NULL);
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

void workerTask(void * arg) {
  while(1) {

    uint16_t GPIO_PIN = *((uint16_t *) arg);
    toggleLed(GPIO_PIN);
//    string pendingProblem = popFrontPendingQueue();
//    if(!pendingProblem.empty()){
//      uint32_t iniTick = (uint32_t) xTaskGetTickCount();
//
//      stringstream ss(pendingProblem);
//      string token;
//      while(ss >> token) {
//        printf((token + "\n").c_str());
//      }
//
//      uint32_t endTick = (uint32_t) xTaskGetTickCount();
//
//      int durationMs = pdTICKS_TO_MS(endTick - iniTick);
//      printf("Duration: %d ms \n\n", durationMs);
//    }

//    TaskStatus_t xTaskDetails;
//    vTaskGetInfo( NULL, &xTaskDetails, pdTRUE, eInvalid );
//    printf("TaskId: %d\n", (int) xTaskDetails.xTaskNumber);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void toggleLed(uint16_t GPIO_PIN) {

#ifdef USE_MUTEX
  if(xSemaphoreTake(gpioDMutex, ( TickType_t ) 1 ) == pdTRUE) {
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN);
    xSemaphoreGive(gpioDMutex);
  }
#else
  pendingQueue.push_back(problem);
#endif
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
//          addWorkerTask();
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

void addWorkerTask(uint8_t index) {


  uint16_t * GPIO_PIN = &ledArray[index];
  StackType_t * xStack =  (StackType_t *) &xStackArray[index];
  StaticTask_t * xTaskBuffer = &xTaskBufferArray[index];



  uint8_t numberOfWorkerTasks = uxTaskGetNumberOfTasks() - 1;

  string taskName = "worker" + std::to_string(numberOfWorkerTasks);
  xTaskCreateStatic(
    workerTask,
    taskName.c_str(),
    configMINIMAL_STACK_SIZE,
    (void *) GPIO_PIN,
    TASK_PRIORITY,
    xStack,
    xTaskBuffer
  );
  printf("%s created\n", taskName.c_str());

}

void pushPendingQueue(string problem) {

#ifdef USE_MUTEX
  if(xSemaphoreTake(pendingQueueMutex, ( TickType_t ) 1 ) == pdTRUE) {
    pendingQueue.push_back(problem);
//    myPendingQ.push(problem);
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
//    string queues = "";
//    Node * currentNode = myPendingQ.front;
//    while(currentNode != NULL) {
//      printf("iterating...\n");
//      queues += currentNode->problem + "\n";
//      currentNode = currentNode->next;
//    }
//    sendOverUsb(queues);


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





