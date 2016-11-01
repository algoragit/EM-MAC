
#include "net/netstack.h"
#include "net/mac/EM-MAC/LCG.h"
#include "sys/rtimer.h"
#include "sys/clock.h"
#include "linkaddr.h"

void generate_ch_list(int *ch_list, int seed, int no_channels){
	int i=0;
	seed=(unsigned int)(seed);
	for (i=0; i < no_channels; i++){
		seed = ((15213*(seed))+11237);
		*(ch_list+i) = seed % (unsigned int)(no_channels) + 11;
	}
}

unsigned int get_neighbor_wake_up_time(neighbor_state v, uint8_t *neighbor_channel, unsigned int time_in_advance, unsigned int *iteration_out)
{
	unsigned int current_seed = v.last_seed;
	unsigned int next_wake_secs = v.wake_time_seconds;
	unsigned int next_wake_tics = v.wake_time_tics;
	uint8_t last_channel = v.last_channel;
	uint8_t channel_list_index=0;
	int diff_secs = v.d_secs;
	int diff_tics = v.d_tics;
	int delta;
	int done=0;

	/* Generating Channel list for the neighbor */
	unsigned int channel_list[16]={0};
	generate_ch_list(&channel_list, v.node_link_addr.u8[7], 16);
	while (channel_list[channel_list_index] != last_channel && channel_list_index<16){
		channel_list_index++;
	}

	unsigned long local_seconds = clock_seconds();
	unsigned int iteration=0;
	unsigned int seed_temp;
<<<<<<< HEAD
	// Correct the time for the clock difference with the neighbor
	if (diff_tics>=0){
		next_wake_secs=next_wake_secs + diff_secs + (next_wake_tics + diff_tics)/RTIMER_SECOND;
		next_wake_tics= (next_wake_tics + diff_tics)%RTIMER_SECOND;
	} else {
		next_wake_secs=next_wake_secs + diff_secs - (next_wake_tics + diff_tics)/RTIMER_SECOND;
		next_wake_tics= (next_wake_tics + diff_tics)%RTIMER_SECOND;
	}
	next_wake_secs = next_wake_secs - (next_wake_tics - time_in_advance)/RTIMER_SECOND;
	next_wake_tics = (next_wake_tics - time_in_advance)%RTIMER_SECOND;
	next_wake_tics = next_wake_tics + (next_wake_secs%2)*RTIMER_SECOND;
	unsigned int next_wake_tics_temp=0;
	while((next_wake_secs < clock_seconds()) || ((next_wake_secs == clock_seconds()) && (next_wake_tics <= RTIMER_NOW()))){
		iteration++;
		seed_temp=current_seed;
		current_seed=((15213*current_seed)+11237);
		next_wake_tics_temp = RTIMER_SECOND/10000 + current_seed%RTIMER_SECOND + RTIMER_SECOND/2;
		next_wake_secs += next_wake_tics_temp/RTIMER_SECOND + (next_wake_tics_temp%RTIMER_SECOND+next_wake_tics%RTIMER_SECOND)/RTIMER_SECOND;
		next_wake_tics += next_wake_tics_temp;
		channel_list_index=(channel_list_index+1)%16;
	}
	*neighbor_channel=channel_list[channel_list_index];
	iteration_out[0]=local_seconds;
	iteration_out[1]=RTIMER_NOW();
	iteration_out[2]=next_wake_tics;
	iteration_out[3]=next_wake_secs;
	iteration_out[4]=next_wake_tics%RTIMER_SECOND;
	iteration_out[5]=diff_secs;
	//printf("_wait_bef:n_sec:%u n_tic:%u diff_s:%d diff_t:%d now:%u\n",
			//next_wake_secs, next_wake_tics, diff_secs, diff_tics, RTIMER_NOW());
	return next_wake_tics - RTIMER_SECOND/728;
=======
	/*printf("%d.%d  STARTING point: secs=%u tics=%u LOCAL_SECS:%lu\n",
			iteration, v.node_link_addr.u8[7], next_wake_secs, next_wake_tics, local_seconds);*/
	unsigned int next_wake_tics_temp=0;
	while ((next_wake_secs < (local_seconds - diff_secs)) || ((next_wake_tics < (unsigned int)((long int)(RTIMER_NOW())-diff_tics)) && (next_wake_secs == (local_seconds - diff_secs)))){
		iteration++;
		seed_temp=current_seed;
		current_seed=((15213*current_seed)+11237);
		next_wake_tics_temp = current_seed%RTIMER_SECOND + RTIMER_SECOND/2;

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
		// Adding the time spent awake by the node
		next_wake_tics += next_wake_tics_temp;
		local_seconds = clock_seconds();
		/*printf("%u. ", iteration);
		printf("Neighbor: %d  POINT: c_seed=%u secs=%u tics_t=%u tics_a=%u  L_SECS:%lu\n",
				v.node_link_addr.u8[7], current_seed, next_wake_secs, next_wake_tics_temp, next_wake_tics, local_seconds);*/

		// Adding the time spent awake by the node
		/*if ((next_wake_tics + ON_PERIOD) > RTIMER_SECOND){
			next_wake_tics = (next_wake_tics + ON_PERIOD)%RTIMER_SECOND;
			next_wake_secs++;
		}else {
			next_wake_tics += ON_PERIOD;
		}
		//neighbor_channel = channel_list_n[channel_pointer_n%16];
		if (linkaddr_node_addr.u8[0] == 2){
					printf("Channel: %d, Time: %u      ", neighbor_channel, next_wake_tics_temp);
					printf("Predicted: Ticks: %u, Seconds: %d    ", next_wake_tics, next_wake_secs);
					printf("Local    : Ticks: %u, Seconds: %d\n", RTIMER_NOW()%RTIMER_SECOND, local_seconds);
				}
		channel_pointer_n++;
		rand_num_neighbor=rand_num_neighbor*multiplier+increment;
		if (neighbor_channel == channel_list_n[15]){
			last_cycle_ticks=next_wake_tics;
			last_cycle_seconds=next_wake_secs;
			last_cycle_seed=rand_num_neighbor;
			from_last_state = 1;
			//printf("LastCycleState: Ticks: %u, Seconds: %u, Seed: %u, Channel: %d\n", last_cycle_ticks, last_cycle_seconds, last_cycle_seed, neighbor_channel);
		}
		//local_seconds = clock_seconds()- 31;*/
		/**************************************************************************************************************************************************************************************/
		//printf(" next_wake: %u\n", next_wake_tics);
	}
	/*printf("%u.%d secs=%u tics_a=%u tics_t=%u L_SECS:%lu L_TICS:%u DIFF_TICS:%ld DIFF_SECS:%d\n",
			iteration, v.node_link_addr.u8[7], next_wake_secs, next_wake_tics, next_wake_tics_temp, local_seconds, RTIMER_NOW(), diff_tics, diff_secs);*/
	//printf("%u\n", RTIMER_NOW()-start_tics);
	/*printf("Test Channels: \n");
	unsigned int seed=linkaddr_node_addr.u8[7];
	int i=0;
	for (i=0; i < 32; i++){
		seed = ((15213*(seed))+11237);
		seed = ((15213*(seed))+11237);
		printf("%d ", seed % (unsigned int)(16) + 11);
		if (i==15) printf("\n");
	}
	printf("\n");*/

	return next_wake_tics+diff_tics;
>>>>>>> ea47d4cb6aa2071e4e5f4654b7d749e755f3f02b
}
