
#include "net/netstack.h"
#include "net/mac/EM-MAC/LCG.h"
#include "sys/rtimer.h"
#include "sys/clock.h"
#include "linkaddr.h"

void generate_ch_list(int *ch_list, int seed, int no_channels){
	int i=0;
	no_channels = no_channels + 1;
	int first_channel = 11;
	int excluded_channel_index= 16 - first_channel;
	int channels[no_channels];
	int available[no_channels];
	int index2check;
	int done = 0;

	for (i=0; i < no_channels; i++){
		channels[i] = first_channel + i;
		available[i] = 1;
		if (i==excluded_channel_index){
			available[i] = 0;
		}
	}
	seed=(unsigned int)(seed);
	for (i=0; i < no_channels-1; i++){
		seed = ((15213*(seed))+11237);
		index2check = seed % (unsigned int)(no_channels);
		done=0;
		while (done==0){
			if (available[index2check]==1){
				*(ch_list+i)=channels[index2check];
				available[index2check]=0;
				done=1;
			}
			else{
				index2check=(index2check+1)%no_channels;
				done=0;
			}
		}
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
	unsigned int channel_list[15]={0};
	generate_ch_list(&channel_list, v.node_link_addr.u8[7], 15);
	while (channel_list[channel_list_index] != last_channel && channel_list_index<15){
		channel_list_index++;
	}

	unsigned long local_seconds = clock_seconds();
	unsigned int iteration=0;
	unsigned int seed_temp;
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
		channel_list_index=(channel_list_index+1)%15;
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
}
