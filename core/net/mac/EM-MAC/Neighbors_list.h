

#include "list.h"
#include "linkaddr.h"

typedef struct {
      char name [10];
      linkaddr_t 	node_link_addr;
	  unsigned int 	wake_time_tics;
	  int 			m;							// time difference in seconds
	  unsigned int 	last_seed;
	  uint8_t		last_channel;
	  unsigned int 	wake_time_seconds;
	  long int 		n;                       // time difference in tics
	  unsigned int 	blacklist;
  } neighbor_state;
  
  int check_if_neighbor_exist(list_t  Neighbors,linkaddr_t addr);
  neighbor_state get_neighbor_state (list_t  Neighbors,linkaddr_t addr);
