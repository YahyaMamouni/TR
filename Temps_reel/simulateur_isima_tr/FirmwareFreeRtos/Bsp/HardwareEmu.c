#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "remote_socket_client.h"

#include "HardwareEmu.h"
#include "ControlPanelClient.h"


#define local_periph "127.0.0.1"

static Remote_Client remote;


static unsigned short SensorColorValue = 0;

static TaskHandle_t hTHwCanRx;
static TaskHandle_t hTHwCanTx;

static unsigned int CanRxInterruptNo = 0;
static unsigned int CanTxInterruptNo = 0;
static unsigned int SensorColorInterruptNo = 0;

extern unsigned char BP_Gs,BP_Ms,BP_Ds;
extern int _led_r,_led_j,_led_v;

static int HWCanRxN = 0;
static CanFrame HWCanRxFrame={0};

static int HWCanTxN = 0;
static CanFrame HWCanTxFrame={0};

static int HWCanTxUrgentN = 0;
static CanFrame HWCanTxUrgentFrame={0};

// Concurrent access may fail !
int CanTxReady(){
    return HWCanTxN == 0;
}

int CanRxFrameAvaible(){
    return HWCanRxN == 1;
}

void CanRxReadFrame(CanFrame* rep){
    if (HWCanRxN==1){
        memcpy(rep,&HWCanRxFrame,sizeof(CanFrame));
        HWCanRxN=0;
    }else{
        printf("HW CanRx read fail. No Frame available.\n");
    }
}

void CanTxSendFrame(CanFrame com){
    taskENTER_CRITICAL(); // Test and set  CanTx Buffer protection.
    if (HWCanTxN == 0){
        HWCanTxN = 1; //Tx buffer is now protected.
        taskEXIT_CRITICAL();
        memcpy(&HWCanTxFrame,&com,sizeof(CanFrame)); // Data are copied to CanTx Buffer
        xTaskNotifyGive(hTHwCanTx); // Deleguate sending frame to TCanTx,
    }else{
        taskEXIT_CRITICAL();
        printf("HW CanFrame %c Send Failed, Peripherial not available, race condition.",com.data.id);
    }
}

void CanTxSendFrameFromIsr(CanFrame com,BaseType_t *pxHigherPriorityTaskWoken){
    if (HWCanTxN == 0){
        HWCanTxN = 1; //Tx buffer is now protected.
        memcpy(&HWCanTxFrame,&com,sizeof(CanFrame));
        vTaskNotifyGiveFromISR(hTHwCanTx,pxHigherPriorityTaskWoken); // Deleguate sending frame to TCanTx,
    }else{
        printf("HW CanFrame %c Send Failed, Peripherial not available.",com.data.id);
    }
}

#define INPUT_TYPE_INVALID      0
#define INPUT_TYPE_PRESSED      1
#define INPUT_TYPE_RELEASED     2
#define INPUT_TYPE_AD00         3
#define INPUT_TYPE_AD01         4
#define INPUT_TYPE_KEY          5
#define INPUT_TYPE_CHECK_LED    6


#define LED_ON  (1<<4)

#define CHECK_OK (1<<11)

#define INPUT(type,value) ((type<<12)|(value))
#define INPUT_TYPE(inp) (inp>>12)
#define INPUT_VALUE(inp) (inp & ((1<<12)-1))

void CanTxSendUrgentFrame(CanFrame rep){
    taskENTER_CRITICAL(); // Test and set  CanTx Buffer protection.
    if (HWCanTxUrgentN == 0){
        HWCanTxUrgentN = 1; //Tx buffer is now protected.
        taskEXIT_CRITICAL();
        memcpy(&HWCanTxUrgentFrame,&rep,sizeof(CanFrame)); // Data are copied to CanTx Buffer
        xTaskNotifyGive(hTHwCanTx); // Deleguate sending frame to TCanTx,
    }else{
        taskEXIT_CRITICAL();
        printf("HW CanFrame %c Urgent Send Failed, Peripherial not available, race condition.",rep.data.id);
    }
}

static void THwCanRx(void* q){
    unsigned char id,rtr;
    short val;
    unsigned short inp;

    while(1){
        //printf("%8.8d ==== Waiting for frame\n",xTaskGetTickCount());
        remote_receive(&remote,&id,&rtr,&val);
        //printf("%8.8d <<== %c %c %04X\n",xTaskGetTickCount(),id,rtr,(unsigned short) val);
        taskENTER_CRITICAL();
        switch (id){
            case '!':
                SensorColorValue = val;
                if (SensorColorInterruptNo>portINTERRUPT_TICK)
                    vPortGenerateSimulatedInterrupt(SensorColorInterruptNo);
                break;
            case '#':
                inp = val;
                switch(INPUT_TYPE(inp)){
                    case INPUT_TYPE_PRESSED:
                        if (INPUT_VALUE(inp)==1)
                            BP_Gs = 1;
                        else if (INPUT_VALUE(inp)==2)
                            BP_Ms = 1;
                        else if (INPUT_VALUE(inp)==3)
                            BP_Ds = 1;
                        break;
                    case INPUT_TYPE_RELEASED:
                        if (INPUT_VALUE(inp)==1)
                            BP_Gs = 0;
                        else if (INPUT_VALUE(inp)==2)
                            BP_Ms = 0;
                        else if (INPUT_VALUE(inp)==3)
                            BP_Ds = 0;
                        break;
                    case INPUT_TYPE_AD00:
                        ad00 = INPUT_VALUE(inp);
                        break;
                    case INPUT_TYPE_AD01:
                        ad01 = INPUT_VALUE(inp);
                        break;
                    case INPUT_TYPE_KEY:
                        KeyboardSetLastKey(INPUT_VALUE(inp));
                        break;
                    case INPUT_TYPE_CHECK_LED:
                        switch (INPUT_VALUE(inp) & 0x03){
                        case 1:
                            if (inp & LED_ON)
                                if (_led_r)
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) | CHECK_OK}});
                                else
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) & ~CHECK_OK}});
                            else
                                if (_led_r)
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) & ~CHECK_OK}});
                                else
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) | CHECK_OK}});
                            break;
                        case 2:
                            if (inp & LED_ON)
                                if (_led_j)
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) | CHECK_OK}});
                                else
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) & ~CHECK_OK}});
                            else
                                if (_led_j)
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) & ~CHECK_OK}});
                                else
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) | CHECK_OK}});
                            break;
                        case 3:
                            if (inp & LED_ON)
                                if (_led_v)
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) | CHECK_OK}});
                                else
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) & ~CHECK_OK}});
                            else
                                if (_led_v)
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) & ~CHECK_OK}});
                                else
                                    CanTxSendUrgentFrame((CanFrame){{.id='#',.rtr=0,.val=INPUT_VALUE(inp) | CHECK_OK}});
                            break;
                        }
                }
                break;
            default:
                if (HWCanRxN==0){
                    HWCanRxFrame.data.id=id;
                    HWCanRxFrame.data.rtr=rtr;
                    HWCanRxFrame.data.val=val;
                    HWCanRxN = 1;
                    if (CanRxInterruptNo>portINTERRUPT_TICK)
                        vPortGenerateSimulatedInterrupt(CanRxInterruptNo);
                } else {
                    printf("HW CanRx Frame %c lost\n",id);
                }
                break;
        }
        taskEXIT_CRITICAL();
    }
}

// CanTx Send background Task
void THwCanTx(void* q){
    unsigned char id,rtr;
    short val;
    while(1){
        if (ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(2000)) == 0){ // Wait notification asking frame send
            if ((HWCanTxN==1) || (HWCanTxUrgentN==1))
                printf("ERROR : Can Tx frame pending without notification.\n");
            else
                continue;
        };

        taskENTER_CRITICAL(); // Protect concurrent access to CanTx buffer
        if (HWCanTxUrgentN == 1){
                id = HWCanTxUrgentFrame.data.id;
                rtr = HWCanTxUrgentFrame.data.rtr?'?':':';
                val = HWCanTxUrgentFrame.data.val; // Frame builded
            taskEXIT_CRITICAL();

            remote_send(&remote,id,rtr,val); // Send frame

            taskENTER_CRITICAL();
                HWCanTxUrgentN = 0; // Buffer is avaible
            taskEXIT_CRITICAL();
            continue;
        }

        if (HWCanTxN == 1){
                id = HWCanTxFrame.data.id;
                rtr = HWCanTxFrame.data.rtr?'?':':';
                val = HWCanTxFrame.data.val; // Frame builded
            taskEXIT_CRITICAL();

            remote_send(&remote,id,rtr,val); // Send frame

            taskENTER_CRITICAL();
                HWCanTxN = 0; // Buffer is avaible
                vPortGenerateSimulatedInterrupt(CanTxInterruptNo); // Tx Frame gone interrupt, used to send next frame.
            taskEXIT_CRITICAL();
        }else{
                printf("SPURIOUS CanTX Send Frame.\n");
            taskEXIT_CRITICAL();
        }
    }
}

void SensorColorInit(unsigned int _SensorColorInterruptNo){
    SensorColorInterruptNo = _SensorColorInterruptNo;
}

unsigned short SensorColorRead(void){
    return SensorColorValue;
}

void HardwareInit(int _CanRxInterruptNo_,int _CanTxInterruptNo_){
    remote_init(local_periph,&remote);

    CanRxInterruptNo = _CanRxInterruptNo_;
    CanTxInterruptNo = _CanTxInterruptNo_;

    xTaskCreate(THwCanRx,"TCanRx",configMINIMAL_STACK_SIZE,NULL,0,&hTHwCanRx); // Lowest priority, recv block and task will hang up all others lower priority tasks.
    xTaskCreate(THwCanTx,"TCanTx",configMINIMAL_STACK_SIZE,NULL,configMAX_PRIORITIES-2,&hTHwCanTx); // High priority, must send frames as soon as possible
}

void HardwareHalt(void){
    vTaskDelete(hTHwCanTx);
    vTaskDelete(hTHwCanRx);
    CanRxInterruptNo = 0;
    CanTxInterruptNo = 0;
    remote_close(&remote);
}
