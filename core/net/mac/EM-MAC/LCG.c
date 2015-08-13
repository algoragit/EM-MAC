
#include "net/netstack.h"
#include "net/mac/EM-MAC/LCG.h"
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
unsigned int get_rand_wake_up_time( unsigned int initial_seed, unsigned short cycle_num)
{
	   unsigned int time_min=8192;  //tics equivalent to 0.5ms
	   unsigned int time_max=24575; //tics equivalent to 1.5ms
	   unsigned int i;
	   unsigned int res=initial_seed;
	   for(i=0;i<cycle_num;i++)
	   {
		   res=(15213*res)+11237;
	   }
	   res=res/2;
	   if(res<time_min){
	      res=res+time_min;}
	      else if(res>time_max){
	      res=res-time_min;}
	      return res;
}

