

#include "list.h"
#include "linkaddr.h"

typedef struct {
	char name [10];
	linkaddr_t 		node_link_addr;
	unsigned int 	wake_time_tics;
	int 			d_secs;							// time difference in seconds
	unsigned int 	last_seed;
	uint8_t			last_channel;
	unsigned int 	wake_time_seconds;
	int 			d_tics;                       // time difference in tics
	unsigned int 	blacklist;
	unsigned short	consecutive_failed_tx;
} neighbor_state;

int check_if_neighbor_exist(list_t  Neighbors,linkaddr_t addr);
neighbor_state get_neighbor_state (list_t  Neighbors,linkaddr_t addr);
