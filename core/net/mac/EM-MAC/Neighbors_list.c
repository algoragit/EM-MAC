#include "list.h"
#include "Neighbors_list.h"
#include "sys/cc.h"


int check_if_neighbor_exist(list_t Neighbors,linkaddr_t addr)
{
	neighbor_state * s;
	s=list_head(Neighbors);
	while(s!=NULL)
	{

		linkaddr_t n_addr=s->node_link_addr;
		if(linkaddr_cmp(&n_addr,&addr)){
			return 1;
		}
		s = list_item_next(s);
	}
	return 0;
}

neighbor_state get_neighbor_state (list_t  Neighbors,linkaddr_t addr)
{
	neighbor_state * s;
	neighbor_state st;
	linkaddr_t n_addr;
	s=list_head(Neighbors);
	while(s!=NULL)
	{

		n_addr=s->node_link_addr;

		if(linkaddr_cmp(&n_addr,&addr)){

			st.blacklist=s->blacklist;
			st.wake_time_tics=s->wake_time_tics;
			st.d_secs=s->d_secs;
			st.last_seed=s->last_seed;
			st.last_channel=s->last_channel;
			st.wake_time_seconds=s->wake_time_seconds;
			st.d_tics=s->d_tics;
			st.consecutive_failed_tx=s->consecutive_failed_tx;
			linkaddr_copy(&st.node_link_addr,&addr);
			return st;
		}
		s = list_item_next(s);
	}
	return st;
}
