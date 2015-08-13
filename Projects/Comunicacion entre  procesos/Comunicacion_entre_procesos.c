#include "contiki.h"
#include <stdio.h>

PROCESS(proccess_testing ,"proccess_testing");
PROCESS(proccess_sender ,"proccess_sender");
AUTOSTART_PROCESSES (&proccess_testing,&proccess_sender);

PROCESS_THREAD(proccess_sender,ev,data)
{
	PROCESS_BEGIN();
        process_post(&proccess_testing,PROCESS_EVENT_CONTINUE,NULL);
        printf("Continua hasta detenerse");
        PROCESS_END();
} 

PROCESS_THREAD(proccess_testing,ev,data)
{
	PROCESS_BEGIN();
		
                        PROCESS_YIELD();
			printf("\nEvent numer is:%d\n",ev);
	
	PROCESS_END();
} 


