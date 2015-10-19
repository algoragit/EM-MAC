
#include "net/netstack.h"
#include "net/mac/EM-MAC/LCG.h"
#include "sys/rtimer.h"
#include "sys/clock.h"
#include "linkaddr.h"

void generate_ch_list(int *ch_list, int seed, int no_channels){
	int i=0;
	seed=(unsigned int)(seed);
	//printf("Inside the function: \n");
	int test;
	for (i=0; i < no_channels; i++){
		seed = ((15213*(seed))+11237);
		*(ch_list+i) = seed % (unsigned int)(no_channels) + 11;
		//printf("%u  ", (unsigned int)(seed % (unsigned int)16));
		//printf("seed: %u -> %d\n", seed, *(ch_list+i));
	}
}

unsigned int get_neighbor_wake_up_time(neighbor_state v, uint8_t *neighbor_channel, unsigned int *iteration_out)
{
	//unsigned int start_tics=RTIMER_NOW();
	unsigned int current_seed = v.last_seed;
	unsigned int next_wake_secs = v.wake_time_seconds;
	unsigned int next_wake_tics = v.wake_time_tics;
	uint8_t last_channel = v.last_channel;
	uint8_t channel_list_index=0;
	int diff_secs = v.m;
	long int diff_tics = v.n;

	/* Generating Channel list for the neighbor */
	unsigned int channel_list[16]={0};
	generate_ch_list(&channel_list, v.node_link_addr.u8[7], 16);
	while (channel_list[channel_list_index] != last_channel && channel_list_index<16){
		channel_list_index++;
	}
	//printf("ch:%d, index:%d ch_f:%d\n", last_channel, channel_list_index, channel_list[channel_list_index]);
	/*current_seed = 1;
	next_wake_secs = 0;
	next_wake_tics = 0;
	diff_secs = 0;
	diff_tics = 0;*/

	/* TODO What should be done if the state is empty? */

	unsigned long local_seconds = clock_seconds();
	unsigned int iteration=0;
	unsigned int seed_temp;
	/*printf("%d.%d  STARTING point: secs=%u tics=%u LOCAL_SECS:%lu\n",
			iteration, v.node_link_addr.u8[7], next_wake_secs, next_wake_tics, local_seconds);*/
	unsigned int next_wake_tics_temp=0;
	while ((next_wake_secs < (local_seconds - diff_secs)) ||
			((next_wake_tics < (unsigned int)((long int)(RTIMER_NOW())-diff_tics)) && (next_wake_secs == (local_seconds - diff_secs)))){
		iteration++;
		seed_temp=current_seed;
		current_seed=((15213*current_seed)+11237);
		next_wake_tics_temp = 3+current_seed%RTIMER_SECOND + RTIMER_SECOND/2;

		/**************************************************************************************************************************************************************************************/
		if (next_wake_tics < RTIMER_SECOND){
			if ((next_wake_tics_temp + next_wake_tics) < next_wake_tics){
				next_wake_secs+=2;
			}
			if ((next_wake_tics_temp + next_wake_tics) > RTIMER_SECOND){
				next_wake_secs++;
			}
		}
		if (next_wake_tics > RTIMER_SECOND){
			if ((next_wake_tics_temp + next_wake_tics) < RTIMER_SECOND){
				next_wake_secs++;
			}
			if (((next_wake_tics_temp + next_wake_tics) > RTIMER_SECOND) && ((next_wake_tics_temp + next_wake_tics) < next_wake_tics)){
				next_wake_secs+=2;
			}
		}
		next_wake_tics += next_wake_tics_temp;
		channel_list_index=(channel_list_index+1)%16;
		local_seconds = clock_seconds();
		//printf("p_ch: %d\n", channel_list[channel_list_index]);
		/*printf("Neighbor: %d  POINT: c_seed=%u secs=%u tics_t=%u tics_a=%u  L_SECS:%lu\n",
				v.node_link_addr.u8[7], current_seed, next_wake_secs, next_wake_tics_temp, next_wake_tics, local_seconds);*/
		//if(linkaddr_node_addr.u8[7]==1 && local_seconds>1260 && v.node_link_addr.u8[7]==3){
			printf("debug:%u %u %u\n",next_wake_tics,next_wake_tics+diff_tics,channel_list[channel_list_index]);
		//}
		//printf(" next_wake: %u\n", next_wake_tics);*/
	}
	/*printf("%u.%d secs=%u tics_a=%u tics_t=%u L_SECS:%lu L_TICS:%u DIFF_TICS:%ld DIFF_SECS:%d\n",
			iteration, v.node_link_addr.u8[7], next_wake_secs, next_wake_tics, next_wake_tics_temp, local_seconds, RTIMER_NOW(), diff_tics, diff_secs);
	//printf("%u\n", RTIMER_NOW()-start_tics);
	printf("Test Channels: \n");
	unsigned int seed=linkaddr_node_addr.u8[7];
	int i=0;
	for (i=0; i < 32; i++){
		seed = ((15213*(seed))+11237);
		seed = ((15213*(seed))+11237);
		printf("%d ", seed % (unsigned int)(16) + 11);
		if (i==15) printf("\n");
	}*/
	printf("debug:%d.%u ", v.node_link_addr.u8[7], iteration);
	printf("next_wake: %u %ld %u sec_diff:%u\n", next_wake_tics, diff_tics, RTIMER_NOW(), next_wake_secs-v.wake_time_seconds);
	*neighbor_channel=channel_list[channel_list_index];
	*iteration_out=iteration;
	return next_wake_tics+diff_tics /*+ (3*iteration - 45)*/-45;
}
