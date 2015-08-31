
#include "net/netstack.h"
#include "net/mac/EM-MAC/LCG.h"
#include "sys/rtimer.h"
#include "sys/clock.h"
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
	next_seed=((15213*current_seed)+11237);
	//printf("Next_seed: %u\n", next_seed);
	return next_seed;
}

void generate_ch_list(int *ch_list, int seed, int no_channels){
	int i=0;
	seed=(unsigned int)(seed);
	//printf("Inside the function: \n");
	int test;
	for (i=0; i < no_channels; i++){
		seed = ((15213*(seed))+11237);
		*(ch_list+i) = seed % (unsigned int)(16) + 11;
		//printf("%u  ", (unsigned int)(seed % (unsigned int)16));
		//printf("seed: %u -> %d\n", seed, *(ch_list+i));
	}
}

unsigned int get_neighbor_wake_up_time(neighbor_state v)
{
	unsigned int current_seed = v.last_seed;
	unsigned int next_wake_secs = v.wake_time_seconds;
	unsigned int next_wake_tics = v.wake_time_tics;
	int diff_secs = v.m;
	long int diff_tics = v.n;

	/* Generating Channel list for the neighbor */
	unsigned int channel_list[16]={0};
	generate_ch_list(&channel_list, v.node_link_addr.u8[7], 16);
	/*current_seed = 1;
	next_wake_secs = 0;
	next_wake_tics = 0;
	diff_secs = 0;
	diff_tics = 0;*/

	/* TODO What should be done if the state is empty? */

	unsigned long local_seconds = clock_seconds();
	unsigned int iteration=0;
	unsigned int seed_temp;
	printf("%d. Neighbor: %d  STARTING point: tics=%u secs=%u seed=%u LOCAL_SECS:%lu\n",
			iteration, v.node_link_addr.u8[7], next_wake_tics, next_wake_secs, current_seed, local_seconds);
	//printf("Neighbor: %d   local seconds: %u next_wake_secs: %u  local_tics: %u   next_wake_tics: %u\n", v.node_link_addr.u8[7], local_seconds, next_wake_secs, RTIMER_NOW(), next_wake_tics);
	unsigned int next_wake_tics_temp=0;
	while ((next_wake_secs <= (local_seconds - diff_secs)) /*&& (next_wake_tics < (RTIMER_NOW()-diff_secs))*/){
		iteration++;
		seed_temp=current_seed;
		current_seed=((15213*current_seed)+11237);
		next_wake_tics_temp = current_seed%RTIMER_SECOND + RTIMER_SECOND/2;

		/**************************************************************************************************************************************************************************************/
		if (next_wake_tics < RTIMER_SECOND){
			if ((next_wake_tics_temp + next_wake_tics + 1000) < next_wake_tics){
				next_wake_secs+=2;
			}
			if ((next_wake_tics_temp + next_wake_tics + 1000) > RTIMER_SECOND){
				next_wake_secs++;
			}
		}
		if (next_wake_tics > RTIMER_SECOND){
			if ((next_wake_tics_temp + next_wake_tics + 1000) < RTIMER_SECOND){
				next_wake_secs++;
			}
			if (((next_wake_tics_temp + next_wake_tics + 1000) > RTIMER_SECOND) && ((next_wake_tics_temp + next_wake_tics + 1000) < next_wake_tics)){
				next_wake_secs+=2;
			}
		}
		// Adding the time spent awake by the node
		next_wake_tics += (next_wake_tics_temp + 1000);
		local_seconds = clock_seconds();
		/*printf("%u. ", iteration);
		printf("Neighbor: %d  POINT: t_seed:%u, c_seed=%u secs=%u tics_t=%u tics_a=%u  L_SECS:%lu\n",
				v.node_link_addr.u8[7], seed_temp, current_seed, next_wake_secs, next_wake_tics_temp, next_wake_tics, local_seconds);*/
		// Adding the time spent awake by the node
		/*if ((next_wake_tics + ON_PERIOD) > RTIMER_SECOND){
			next_wake_tics = (next_wake_tics + ON_PERIOD)%RTIMER_SECOND;
			next_wake_secs++;
		}else {
			next_wake_tics += ON_PERIOD;
		}*/
		//neighbor_channel = channel_list_n[channel_pointer_n%16];
		/*if (linkaddr_node_addr.u8[0] == 2){
					printf("Channel: %d, Time: %u      ", neighbor_channel, next_wake_tics_temp);
					printf("Predicted: Ticks: %u, Seconds: %d    ", next_wake_tics, next_wake_secs);
					printf("Local    : Ticks: %u, Seconds: %d\n", RTIMER_NOW()%RTIMER_SECOND, local_seconds);
				}*/
		/*channel_pointer_n++;
		rand_num_neighbor=rand_num_neighbor*multiplier+increment;
		if (neighbor_channel == channel_list_n[15]){
			last_cycle_ticks=next_wake_tics;
			last_cycle_seconds=next_wake_secs;
			last_cycle_seed=rand_num_neighbor;
			from_last_state = 1;
			//printf("LastCycleState: Ticks: %u, Seconds: %u, Seed: %u, Channel: %d\n", last_cycle_ticks, last_cycle_seconds, last_cycle_seed, neighbor_channel);
		}*/
		//local_seconds = clock_seconds()- 31;
		/**************************************************************************************************************************************************************************************/
		//printf(" next_wake: %u\n", next_wake_tics);
	}

	printf("%u. ", iteration);
	printf("Neighbor: %d  POINT: t_seed:%u, c_seed=%u secs=%u tics_t=%u tics_a=%u  L_SECS:%lu\n\n",
			v.node_link_addr.u8[7], seed_temp, current_seed, next_wake_secs, next_wake_tics_temp, next_wake_tics, local_seconds);
	return current_seed;
}
