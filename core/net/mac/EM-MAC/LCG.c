
#include "net/netstack.h"
#include "net/mac/EM-MAC/LCG.h"
#include "sys/rtimer.h"
#include "linkaddr.h"

/*unsigned int* create_channel_list (int number_of_channels){
	int channel_list[number_of_channels];
	int i;
	unsigned int rand_num = ((15213*(linkaddr_node_addr.u8[7]))+11237);
	channel_list[0]= rand_num % 16 + 11;
	printf("Channel list: %d ", channel_list[0]);
	for (i=1; i<number_of_channels; i++){
		rand_num = ((15213*rand_num)+11237);
		channel_list[i]= rand_num % 16 + 11;
		printf("%d ", channel_list[i]);
	}
	printf("\n");
	return &channel_list;
}*/

unsigned int next_channel( unsigned int initial_seed, unsigned short cycle_num)
{
   unsigned int div;
   radio_value_t chann_min;
   radio_value_t chann_max;
   NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN,&chann_min);
   NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX,&chann_max);
   div=	chann_max+chann_min;
   div=65535/div;
   unsigned int i;
   unsigned int res=initial_seed;
   for(i=0;i<cycle_num;i++)
   {
	   res=(15213*res)+11237;
   }
   res=res/div;

   if(res<chann_min){
   res=res+chann_min;}
   else if(res>chann_max){
   res=res-chann_min;}
   return res;
}

unsigned int get_rand_wake_up_time(unsigned int current_seed)
{
	   unsigned int next_seed;
	   next_seed=((15213*current_seed)+11237)%RTIMER_SECOND + RTIMER_SECOND/2;
	   //printf("Next_seed: %u\n", next_seed);
	   return next_seed;
}

