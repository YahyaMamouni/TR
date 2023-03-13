#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#ifdef WIN32
#include <winsock.h>
#endif // WIN32

//#define UseQdmTouche

SOCKET SocketClient=INVALID_SOCKET;
int ConnectOk=0;

short ad00,ad01;
unsigned char BP_Gr,BP_Dr,BP_Mr;
unsigned char BP_Gs,BP_Ds,BP_Ms;
unsigned int _led_v=0,_led_j=0,_led_r=0;



#ifdef UseQdmTouche
QueueHandle_t QdmTouche;
#endif
unsigned long key_interrupt_level=0;
static int last_key=-1;

int KeyboardGetLastKey(void){
    return last_key;
}

void KeyboardSetLastKey(char key){
    last_key=key;
    if (key_interrupt_level>portINTERRUPT_TICK)
        vPortGenerateSimulatedInterrupt(key_interrupt_level);
}

void fControlPanelRcv(void * p)
{
unsigned long arg=1;
    printf ("Control Panel Waiting \n");
    ioctlsocket (SocketClient, FIONBIO, &arg);
    while(ConnectOk)
    {
        char buff[128]={0};
        char p,m,r;
        int n,val;

        n=-1;
        while(n==-1){
            //taskENTER_CRITICAL();
            n=recv(SocketClient,buff,128,0);
            //taskEXIT_CRITICAL();
            vTaskDelay(50);
        }


        //printf("Recv %d\n",n);
        buff[n]='\0';

        n=0;
        while((r=sscanf(&buff[n],"%c%c%x",&p,&m,&val))==3){
            //printf ("Recu (%d/%d) : %s\n",n,r,&buff[n]);
            if (m==':')
            {
                switch(p)
                {
                case 'A':
                    ad00=val;
                    break;
                case 'B':
                    ad01=val;
                    break;
                case 'K':
#ifdef UseQdmTouche
                    xQueueSend(QdmTouche,&val,portMAX_DELAY);
#endif // UseQdmTouche
                    KeyboardSetLastKey(val);
                    break;
                case 'P':
                    BP_Gr=val;
                    break;
                case 'Q':
                    BP_Mr=val;
                    break;
                case 'R':
                    BP_Dr=val;
                    break;
                }
            }
            while (buff[n]!='\n' && buff[n]!='\r' && buff[n])
                n++;
            while ((buff[n]=='\n' || buff[n]=='\r') && buff[n])
                n++;
            if (!buff[n])
                break;

        }

    }
    vTaskDelete(NULL);
}

void lcd_putc(char c)
{
    char tmp[8];
    sprintf(tmp,"L:%4.4X\n",(unsigned char) c);
    send(SocketClient,tmp,strlen(tmp),0);
}
void lcd_com(char c)
{
    char tmp[8];
    sprintf(tmp,"l:%4.4X\n",(unsigned char) c);
    send(SocketClient,tmp,strlen(tmp),0);

}

void lcd_str(char * s)
{
    while(*s)
        lcd_putc(*s++);
}

void led_j(int on_off)
{
    char tmp[8];
    if (on_off)
        sprintf(tmp,"V:0002\n");
    else
        sprintf(tmp,"v:0002\n");
    send(SocketClient,tmp,strlen(tmp),0);
    _led_j = on_off;
}

void led_r(int on_off)
{
    char tmp[8];
    if (on_off)
        sprintf(tmp,"V:0001\n");
    else
        sprintf(tmp,"v:0001\n");
    send(SocketClient,tmp,strlen(tmp),0);
    _led_r = on_off;
}

void led_v(int on_off)
{
    char tmp[8];
    if (on_off)
        sprintf(tmp,"V:0004\n");
    else
        sprintf(tmp,"v:0004\n");
    send(SocketClient,tmp,strlen(tmp),0);
    _led_v = on_off;
}

void ControlPanelInitAddr(char * adresse,int key_interrupt_level_)
{
    TaskHandle_t hControlPanelRcv;


    WSADATA init_win32;
    SOCKADDR_IN info;

#ifdef UseQdmTouche
    QdmTouche=xQueueCreate(10,sizeof(short));
#endif // UseQdmTouche

    key_interrupt_level=key_interrupt_level_;
    if (key_interrupt_level)
        printf ("Clavier connecté à l'interruption %ld.\n",key_interrupt_level);
    else
        printf("Interruption clavier non connecté.\n");

    ConnectOk=0;
    if (WSAStartup(MAKEWORD(2,2),&init_win32))
    {
        printf(" Erreur %d",WSAGetLastError());
        printf ("Control Panel non disponible, Socket Start-up Fail.\n");
        return;
    }
    if ((SocketClient=socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET)
    {
        printf("%d",WSAGetLastError());
        printf ("Control Panel non disponible, Socket Create Fail.\n");
        return;
    }
    info.sin_family=AF_INET;
    info.sin_addr.s_addr=inet_addr(adresse);
    info.sin_port=htons(44444);
    if (connect(SocketClient,(struct sockaddr*)&info,sizeof(info)))
    {
        printf("%d\n",WSAGetLastError());
        printf ("Control Panel non disponible, Server Unreachable\n");
        return;
    }
    ConnectOk=1;
    xTaskCreate(fControlPanelRcv,"Task_ControlPanel_Rcv",configMINIMAL_STACK_SIZE,NULL,0,&hControlPanelRcv);
    printf ("Control Panel connecté.\n");
}

void ControlPanelInit(int key_interrupt_level_)
{
    char localhost[]="127.0.0.1";
    ControlPanelInitAddr(localhost,key_interrupt_level_);
}
