#include "contiki.h"
#include <stdio.h>

PROCESS(Event_Timer_Test,"Event_Timer_Test");
AUTOSTART_PROCESSES(&Event_Timer_Test);

PROCESS_THREAD(Event_Timer_Test, ev, data)
{
   static int cont=0;
   static struct etimer one_sec;
   static clock_time_t interval = CLOCK_SECOND;
   PROCESS_BEGIN();
   printf("Initial clock time is %d seconds\n",(clock_time()/CLOCK_SECOND));
   etimer_set(&one_sec,interval);
   while (1)
  	{
          PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&one_sec));
          etimer_reset(&one_sec);
          printf("Partial clock time is %d seconds\n",(clock_time()/CLOCK_SECOND));
        }
   PROCESS_END();
}
			
