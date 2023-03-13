#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"

#include "HardwareEmu.h"
#include "ControlPanelClient.h"

/** INFORMATIONS SUR LES PERIPHERIQUES ET COMMANDES
---------------------------------------------------
'V' : Vitesse du moteur (0-50)
'D' : Angle de braquage des roues avant en 1/10° de degré (10 -> 1°)
'T' : Vitesse de rotation de la tourelle du télémètre en 1/10°/s
'R' : Lecture de l'Azimuth de la tourelle en 1/10°
'U' : Distance mesure par le télémètre en 1/100 de mètre (en cm)
'X' : Position absolue X en cm
'Y' : Position absolue Y en cm
'Z' : Position absolue Z en cm
'N' : Numéro de la voiture (en fonction de l'ordre de connection)
'E' : Lecture des evènements (cf Annexe 2)
'H' : Donne le temps de course actuel
'S' : Temps du tour précédent
'M' : Mode de course :
    8 bits de poids fort: 1 Attente, 2 course, 3 essais libres)
    8 bits de poids faible : numero de piste
'C' : Informations sur le dernier capteur touché :
    8 bits de poids faible : numéro du capteur
    8 bits de poids fort : couleur ('R','J' ou 'V')
'J' : Proposition d'un code de dévérouillage.
    Une valeur de 0 à 5 par quartet.
'j' : Récupération du résultat de dernier code envoyé.
    0x77 si aucun code n'a été soumis.
    <0 si la réponse n'est pas disponible.
    0x0a0b avec a-> nombre de couleurs bien placées et b -> couleurs présentes mais mal placées.
'I' : Définition du nom du véhicule.
    Doit débuter par le caractère '#' et entraine le chargement de la configuration de piste correspondant au nom du véhicule si le nom se termine par '*'
'K' : Téléportation de la voiture sur le troncon de piste N (correspondant au capteur vert numero N).
    Attention à n'utiliser que pour des tests, les scores sont invalidés !
Contenu du Capteur de couleur;
Bit 0 : Point de passage Vert, remis à zéro lors de la lecture du périphérique 'C'
    1 : Point de passage Jaune, remis à zéro lors de la lecture du périphérique 'C'
    2 : Point de passage Rouge, remis à zéro lors de la lecture du périphérique 'C'
    3 : Point de passage Bleu, remis à zéro lors de la lecture du périphérique 'C'
    4 : Point de passage Cyan, remis à zéro lors de la lecture du périphérique 'C'
    5 : non utilisés
    6 : Collision avec le sol, Remise à zéro au changement de piste.
    7 : Point de passage course (vert), remis à zéro lors de la lecture du périphérique 'C'
    8 : La piste a changé , remis à zéro lors de la lecture du périphérique 'M'
    9 : Le mode de course a changé , remis à zéro lors de la lecture du périphérique 'M'
   10 : non utilisé
   11 : Le dernier point de passage est atteint la course est finie , remis à zéro au changement du mode de course.
   12 : La voiture est sortie de la piste.
   13 : Utilisation de la fonction de téléportation. Classement Invalidé. Remis à zero au changement de piste ou du mode de course.
   14 : Faux départ -> destruction de la voiture , remise à zéro au changement du mode de course.
   15 : Le Feu est Rouge ou orange
**/


int consigne_vitesse = 0;
int distance_telem = 0;
int consigne_distance = 0;
int avancer = 0;
int key = 0;
int num_capteur ;
int col_capteur ;
int commande = 1;
SemaphoreHandle_t CanSem;


unsigned short coleur_cap = 0;

//Q1

void task_1(void * queue){
int i;
char nom[]="#VROUM*";
CanFrame cmd={{'I',0,0}};
	for(i=0;i<8 && nom[i];i++){
		cmd.data.val=nom[i];
		while(!CanTxReady())
            vTaskDelay(pdMS_TO_TICKS(1));
        CanTxSendFrame(cmd);
		vTaskDelay(pdMS_TO_TICKS(10));
	}
    while(1){
            char tmp[32];
            sprintf(tmp,"ad00:%d ",ad00);
            lcd_com(0x80);
            lcd_str(tmp);
            vTaskDelay(pdMS_TO_TICKS(100));
    }
}


// Q2 - Q3
/*
void task_1(void * queue){
    CanFrame com ;
    com.data.id='T';
    com.data.rtr=0;
    com.data.val=450;
    if (CanTxReady()){
        CanTxSendFrame (com);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    com.data.val=-450;
    if (CanTxReady()){
        CanTxSendFrame (com);
    }
}*/

// Q4
/*
void task_1(void * queue){
int i;
char nom[]="#VROUM*";
CanFrame cmd ;
cmd.data.id='T';
cmd.data.rtr=0;
cmd.data.val=450;

	while (1){
		cmd.data.val=ad00;
		while(!CanTxReady())
            vTaskDelay(pdMS_TO_TICKS(1));
        CanTxSendFrame(cmd);
		vTaskDelay(pdMS_TO_TICKS(10));
	}


}*/

// Q6
void task_azimuth(){
CanFrame cmd, receive ;
cmd.data.id='R';
cmd.data.rtr=1;
int angle;
char tmp[32];
    while(1){
            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
            CanTxSendFrame(cmd);
            vTaskDelay(pdMS_TO_TICKS(250));
            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
            CanRxReadFrame(&receive);
            angle=(signed short)receive.data.val;


            sprintf(tmp,"Azimuth:%d ",angle/10);
            lcd_com(0xC0);
            lcd_str(tmp);
            vTaskDelay(pdMS_TO_TICKS(250));
    }
}

//Q7-8
void task_auto_reg(){
CanFrame cmd, receive,reg ;
cmd.data.id='R';
cmd.data.rtr=1;
reg.data.id='T';
reg.data.rtr = 0;
int angle;
char tmp[32];
    while(1){
            xSemaphoreTake(CanSem,portMAX_DELAY);
            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
            CanTxSendFrame(cmd);
            vTaskDelay(pdMS_TO_TICKS(25));
            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
            CanRxReadFrame(&receive);
            angle=(signed short)receive.data.val;


            if (450 != angle){
                reg.data.val = 450 - angle;
                while(!CanTxReady())
                    vTaskDelay(pdMS_TO_TICKS(2));
                CanTxSendFrame(reg);
                vTaskDelay(pdMS_TO_TICKS(2));

            }
        xSemaphoreGive(CanSem);
        vTaskDelay(pdMS_TO_TICKS(2));
        }

}

void timer1(TimerHandle_t p){
    static int j=0;
    j=(j)?0:1;
    led_j(j);

}

// Q5
void task_ad01(){
    while(1){
            char tmp[32];
            sprintf(tmp,"ad01:%d ",ad01);
            lcd_com(0x80);
            lcd_str(tmp);
            vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// Q9

uint32_t ulInterrupt_Keyboard(){
    key = KeyboardGetLastKey();
    /*if(key >= 48 && key <= 57){
        consigne_vitesse = (key - 48) * 5;
        led_r((consigne_vitesse > 10)?1:0);
    }*/
    avancer = 1;
    return pdFALSE;

}

void task_avance(){
    CanFrame com;
    com.data.id='V';
    com.data.rtr=0 ;
    int begin = 0;
    while (1){
        if (avancer){
            begin++;
        }
        if(begin == 1){
            avancer = 0;
            if(key <= 57 && key >= 48){//*
                com.data.val= (key-48)* 5;

            }else if( key == 0){
                com.data.val= 45;
            }
            xSemaphoreTake(CanSem,portMAX_DELAY);
            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
            CanTxSendFrame(com);
            xSemaphoreGive(CanSem);
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
/*
void task_vitesse(){
int i;
char nom[]="#VROUM*";
CanFrame cmd ;
cmd.data.id='V';
cmd.data.rtr=0;
cmd.data.val=0;

	while (1){
        //
		cmd.data.val=consigne_vitesse;
		xSemaphoreTake(CanSem,portMAX_DELAY);
		while(!CanTxReady())
            vTaskDelay(pdMS_TO_TICKS(2));

        CanTxSendFrame(cmd);
        xSemaphoreGive(CanSem);
        vTaskDelay(pdMS_TO_TICKS(2));
	}


}*/

// Q10
void task_distance_mur(){
    CanFrame com = {{'U',1,0}};
    CanFrame receive;

    while(1){
            //
            xSemaphoreTake(CanSem,portMAX_DELAY);
            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));

            CanTxSendFrame(com);
            vTaskDelay(pdMS_TO_TICKS(25));

            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
            CanRxReadFrame(&receive);
            distance_telem=(signed short)receive.data.val;
                //

            xSemaphoreGive(CanSem);
            vTaskDelay(pdMS_TO_TICKS(2));



    }


}

void task_reg_roue(){
CanFrame cmd, receive,reg ;
cmd.data.id='R';
cmd.data.rtr=0;
reg.data.id='D';
reg.data.rtr = 0;
int angle;
char tmp[32];

    while(1){
            //
            if (commande){
                xSemaphoreTake(CanSem,portMAX_DELAY);
                while(!CanTxReady()){

                    CanTxSendFrame(cmd);
                    vTaskDelay(pdMS_TO_TICKS(2));

                    while (!CanTxReady()){
                        CanRxReadFrame(&receive);
                        angle=(signed short)receive.data.val;
                    }
                }
                consigne_distance = -(1000 * sin(3.14/4) - distance_telem);
                //printf("regu : %d \n", consigne_distance);
                if (distance_telem < 1000 && distance_telem > 0){
                    reg.data.val = consigne_distance;
                }
                while(!CanTxReady())
                    vTaskDelay(pdMS_TO_TICKS(2));
                CanTxSendFrame(reg);
                xSemaphoreGive(CanSem);


                }
                vTaskDelay(pdMS_TO_TICKS(2));
            }

            //


}

void task_read_ModeCourse(){
    vTaskDelay(pdMS_TO_TICKS(3000));
    CanFrame com, rep ;
    int alpha ;
    com.data.id='M';
    com.data.rtr=1 ;
    while(1){
        xSemaphoreTake(CanSem,portMAX_DELAY);//*

        while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
        CanTxSendFrame (com) ;
        while(!CanRxFrameAvaible())
            vTaskDelay(pdMS_TO_TICKS(2));
        CanRxReadFrame (&rep) ;
        vTaskDelay(pdMS_TO_TICKS(2));
        //distance = alpha;
        xSemaphoreGive(CanSem);

        //verifier
        alpha= rep.data.val & 0b1000000000000000;
        if (!alpha){
            avancer= 1;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}


void task_capteurs(){
    int premier_passage_cap_1 = 1;
    int premier_passage_cap_2 = 1;
    CanFrame com, rep ;
    com.data.id='C';
    com.data.rtr=1 ;

    //vitesse
    CanFrame com_v;
    com_v.data.id='V';
    com_v.data.rtr=0 ;

    while(1){
        xSemaphoreTake(CanSem,portMAX_DELAY);//*

        while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
        CanTxSendFrame (com) ;
        while(!CanRxFrameAvaible())
            vTaskDelay(pdMS_TO_TICKS(2));
        CanRxReadFrame (&rep) ;
        vTaskDelay(pdMS_TO_TICKS(2));
        //distance = alpha;


        //verifier
        num_capteur= rep.data.val & 0b0000000011111111;
        col_capteur= rep.data.val & 0b1111111100000000;
        if (num_capteur == 255 && col_capteur == 30208)
        {
            if (premier_passage_cap_1){
                commande = !commande;
                //printf("%d\n", commande);

            }

            premier_passage_cap_1 = 0;
            premier_passage_cap_2 = 1;
        }


        if (num_capteur == 254 && col_capteur == 30208)
        {
            if (premier_passage_cap_2){
                commande = !commande;
                //printf("%d\n", commande);

                com_v.data.val= 45;
                while(!CanTxReady())
                    vTaskDelay(pdMS_TO_TICKS(2));
                CanTxSendFrame(com_v);


                vTaskDelay(pdMS_TO_TICKS(2));
            }

            premier_passage_cap_2 = 0;
            premier_passage_cap_1 = 1;
        }



        if (num_capteur == 255 && col_capteur == 29184)
        {

            com_v.data.val= 20;
            while(!CanTxReady())
                vTaskDelay(pdMS_TO_TICKS(2));
            CanTxSendFrame(com_v);


            vTaskDelay(pdMS_TO_TICKS(2));


        }

        xSemaphoreGive(CanSem);
        //printf("n_capteur = %d et coul=%d\n",num_capteur,col_capteur);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

/*
void task_capteurs(){

    while(1){
        xSemaphoreTake(CanSem,portMAX_DELAY);//*

        coleur_cap = SensorColorRead();
        printf("color cap : %hu\n", coleur_cap);
        xSemaphoreGive(CanSem);


        vTaskDelay(pdMS_TO_TICKS(2));
    }
}*/


int main()
{
TaskHandle_t htask_1;
TimerHandle_t htimer1;
CanSem = xSemaphoreCreateMutex();

    HardwareInit(8,9);
    ControlPanelInit(7);
    //ControlPanelInitAddr("192.168.1.221",7);

    // Control panel
    vPortSetInterruptHandler(7, ulInterrupt_Keyboard);

    // Init sensor
    SensorColorInit(6);


    printf("Demarrage des taches\n");
    xTaskCreate(task_1,"Tache 1",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    //xTaskCreate(task_ad01,"Tache ad01",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    //xTaskCreate(task_azimuth,"Tache azimuth",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    xTaskCreate(task_auto_reg,"Tache regulation",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    xTaskCreate(task_distance_mur,"distance mur",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    //xTaskCreate(task_vitesse,"Tache vitesse",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    xTaskCreate(task_reg_roue,"regu roue",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    xTaskCreate(task_read_ModeCourse,"attend vert",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);
    xTaskCreate(task_avance,"avancer",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);

    xTaskCreate(task_capteurs,"capteurs",configMINIMAL_STACK_SIZE,NULL,6,&htask_1);

    htimer1=xTimerCreate("Timer 1",250,pdTRUE,NULL,timer1);
    xTimerStart(htimer1,0);
    vTaskStartScheduler();
    HardwareHalt();
    return 0;
}
