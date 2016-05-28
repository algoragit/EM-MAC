#include "muchmac.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/rime/timesynch.h"
#include "dev/leds.h"
#include "lib/memb.h"
#include "lib/list.h"
#include "lib/random.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTADDR(addr) PRINTF("%d.%d", addr->u8[0], addr->u8[1])
#else
#define PRINTF(...)
#define PRINTADDR(addr)
#endif

#define ON_PERIOD	RTIMER_SECOND/2

static struct ctimer ct;
static struct pt pt;
static struct rtimer rt;
static int first_time=1;
static int channel;
static int channel_list[16];
static int channel_list_n[16];
unsigned int neighbor_time_ticks_temp=0;
static int channel_pointer=0;
static unsigned int rand_num;
static unsigned int rand_num_neighbor;
unsigned int random_time_to_wake;
static int multiplier=15213;
static int increment=11237;

// Test advanced prediction
unsigned int last_cycle_ticks=0;
unsigned int last_cycle_seconds=0;
unsigned int last_cycle_seed=0;
int from_last_state = 0;

int tdma_on=0;


// queue
struct muchmac_queue_item {
	struct muchmac_queue_item *next;
	struct queuebuf *buf;
	mac_callback_t sent_callback;
	void *sent_ptr;
};

#define MAX_QUEUE_SIZE	4
MEMB(muchmac_queue_memb, struct muchmac_queue_item, MAX_QUEUE_SIZE);
LIST(muchmac_queue_unicast);
//LIST(muchmac_queue_broadcast);

PROCESS(send_packet, "muchmac send process");

static volatile unsigned char radio_is_on = 0;
//static volatile unsigned char in_broadcast_slot = 0;
//static volatile unsigned char in_unicast_slot = 0;

static int
turn_on()
{
	radio_is_on = 1;
	return NETSTACK_RADIO.on();
}

static int
turn_off()
{
	radio_is_on = 0;
	return NETSTACK_RADIO.off();
}

static unsigned int
get_rand_num()
{
	if (first_time==0){
		rand_num = rand_num*multiplier+increment;
	}else{
		first_time=0;
	}
	return rand_num;
}

int get_channel(){
	channel = channel_list[channel_pointer];
	channel_pointer = (channel_pointer + 1) % 16;
	//printf("Channel from get_channel: %d from pointer: %u\n", channel, channel_pointer);
	return channel;
}

static int
get_neighbor_channel_and_time(const linkaddr_t *addr)
{
	rand_num_neighbor = (addr->u8[0])*multiplier+increment;
	int i=0;
	int *channel_list_temp;
	int *channel_list_copy;
	channel_list_temp = (int*)malloc(16*sizeof(int));
	channel_list_copy = (int*)malloc(16*sizeof(int));
	for (i=0; i<16; i++){
		channel_list_temp[i]=i+11;
	}
	int unvisited_ch=16;
	i=0;
	//printf("Channel List for neighbor: ");
	while (unvisited_ch > 0){
		channel_list_n[i] = channel_list_temp[rand_num_neighbor % unvisited_ch];
		//printf("%d ", channel_list_n[i]);
		i++;
		int p=0;
		int j=0;
		while (p <= unvisited_ch){
			if (p != (rand_num_neighbor % (unvisited_ch))){
				channel_list_copy[j]=channel_list_temp[p];
				p++;
				j++; }else{	p++; }
		}
		channel_list_temp=channel_list_copy;
		rand_num_neighbor = rand_num_neighbor*multiplier+increment;
		unvisited_ch--;
	}
	//printf("\n");
	int channel_pointer_n = 0;
	int neighbor_channel;
	unsigned int neighbor_time_seconds=0;
	unsigned int neighbor_time_ticks_acc=0;//rand_num_neighbor%RTIMER_SECOND + RTIMER_SECOND/2;
	neighbor_time_ticks_temp=0;
	if (from_last_state == 1){

		neighbor_time_ticks_acc = last_cycle_ticks;
		neighbor_time_seconds = last_cycle_seconds;
		rand_num_neighbor = last_cycle_seed;
	}
	int local_seconds = clock_seconds()- 31; // (-31) because of the interval before starting to powercycle defined in init() function
	while (local_seconds > neighbor_time_seconds){
		neighbor_time_ticks_temp=rand_num_neighbor % RTIMER_SECOND + RTIMER_SECOND/2;
		//printf("ticks: %u\n", RTIMER_NOW());
		// Adding next wake-up interval
		if (((neighbor_time_ticks_temp/2) + (neighbor_time_ticks_acc/2)) > (RTIMER_SECOND)){
			neighbor_time_ticks_acc = (neighbor_time_ticks_temp/2 + neighbor_time_ticks_acc/2) % RTIMER_SECOND;
			neighbor_time_seconds += 2;
		}else if (((neighbor_time_ticks_temp) + (neighbor_time_ticks_acc)) > (RTIMER_SECOND)){
			neighbor_time_ticks_acc=(neighbor_time_ticks_temp + neighbor_time_ticks_acc) % RTIMER_SECOND;
			neighbor_time_seconds++;
		}else{
			neighbor_time_ticks_acc += neighbor_time_ticks_temp;
		}
		// Adding the time spent awake by the node
		if ((neighbor_time_ticks_acc + ON_PERIOD) > RTIMER_SECOND){
			neighbor_time_ticks_acc = (neighbor_time_ticks_acc + ON_PERIOD)%RTIMER_SECOND;
			neighbor_time_seconds++;
		}else {
			neighbor_time_ticks_acc += ON_PERIOD;
		}
		neighbor_channel = channel_list_n[channel_pointer_n%16];
		/*if (linkaddr_node_addr.u8[0] == 2){
			printf("Channel: %d, Time: %u      ", neighbor_channel, neighbor_time_ticks_temp);
			printf("Predicted: Ticks: %u, Seconds: %d    ", neighbor_time_ticks_acc, neighbor_time_seconds);
			printf("Local    : Ticks: %u, Seconds: %d\n", RTIMER_NOW()%RTIMER_SECOND, local_seconds);
		}*/
		channel_pointer_n++;
		rand_num_neighbor=rand_num_neighbor*multiplier+increment;
		if (neighbor_channel == channel_list_n[15]){
			last_cycle_ticks=neighbor_time_ticks_acc;
			last_cycle_seconds=neighbor_time_seconds;
			last_cycle_seed=rand_num_neighbor;
			from_last_state = 1;
			//printf("LastCycleState: Ticks: %u, Seconds: %u, Seed: %u, Channel: %d\n", last_cycle_ticks, last_cycle_seconds, last_cycle_seed, neighbor_channel);
		}
		local_seconds = clock_seconds()- 31;

	}
	//printf("\n");
	//int neighbor_channel=rand_num_neighbor[1]%16+11;
	//printf("Neighbor: \nChannel: %u, Seconds: %u, Ticks: %u\nLocal:\nTicks: %u, Seconds: %u, Ticks: %u\n\n", channel, neighbor_time_seconds, neighbor_time_ticks_acc, RTIMER_NOW(), clock_seconds());
	return neighbor_channel;
}

static char
powercycle(struct rtimer *t, void *ptr)
{
	PT_BEGIN(&pt);

	while (1) {
		//printf("RtimerNow starting powercycle: %u\n", RTIMER_NOW());
		//if (linkaddr_node_addr.u8[0] != 2){
			channel = get_channel();
		//}
		//turn_on();
		if (list_tail(muchmac_queue_unicast) != NULL) {
			process_poll(&send_packet);
		}
		/*if (linkaddr_node_addr.u8[0] == 2){
			linkaddr_t dest;
			dest.u8[0]=1;
			channel = get_neighbor_channel_and_time(&dest.u8[0]);
		}*/
		NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
		rtimer_set(t, RTIMER_NOW() + ON_PERIOD, 0, (rtimer_callback_t)powercycle, NULL);
		//printf("RtimerNow: %u Channel: %d\n", RTIMER_NOW(), channel);
		//turn_on();
		/*if (linkaddr_node_addr.u8[0] == 2 || linkaddr_node_addr.u8[0] == 1){
			printf("Channel: %d, Random_time_to_wake: %u\n", channel, random_time_to_wake);
		}*/
		PT_YIELD(&pt);

		// unicast slot end
		//turn_off();
		//if (linkaddr_node_addr.u8[0] == 2){
			//random_time_to_wake=neighbor_time_ticks_temp;
		//}else{
			random_time_to_wake=get_rand_num() % RTIMER_SECOND + RTIMER_SECOND/2;
		//}
		//printf("Random_time_to_wake: %u\n", random_time_to_wake);
		//rtimer_set(t, timesynch_time_to_rtimer(random_time_to_wake), 0, (rtimer_callback_t)powercycle, NULL);
		//rtimer_set(t, RTIMER_NOW()+timesynch_time_to_rtimer(random_time_to_wake), 0, (rtimer_callback_t)powercycle, NULL);
		rtimer_set(t, RTIMER_NOW()+random_time_to_wake, 0, (rtimer_callback_t)powercycle, NULL);
		//rtimer_set(t, timesynch_time_to_rtimer(get_rand_num(1)%RTIMER_SECOND + RTIMER_SECOND/2), 0, (rtimer_callback_t)powercycle, NULL);
		//printf("RtimerNow ending powercycle: %u\n", RTIMER_NOW());
		PT_YIELD(&pt);
	}

	PT_END(&pt);
}


#define WAIT_FOR_SEND	RTIMER_SECOND/10

PROCESS_THREAD(send_packet, ev, data)
{
	PROCESS_BEGIN();

	while (1) {
		PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

		list_t muchmac_queue;

		muchmac_queue = muchmac_queue_unicast;

		if (list_tail(muchmac_queue) != NULL) {
			// busy wait to account for clock drift
			rtimer_clock_t t0 = RTIMER_NOW();

			while (RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + WAIT_FOR_SEND)) ;

			struct muchmac_queue_item *item = list_tail(muchmac_queue);
			queuebuf_to_packetbuf(item->buf);

			PRINTF("muchmac: send packet %p from queue\n", item->buf);
			int ret;
			packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);

			if (!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_null)) {
				//int channel = get_rand_num(packetbuf_addr(PACKETBUF_ADDR_RECEIVER))%16+11;
				PRINTF("muchmac: changing to receiver channel %d\n", channel);
				NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
			}

			if (NETSTACK_FRAMER.create_and_secure() < 0) {
				ret = MAC_TX_ERR_FATAL;
			} else {
				switch (NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen())) {
				case RADIO_TX_OK:
					ret = MAC_TX_OK;
					break;
				case RADIO_TX_COLLISION:
					ret = MAC_TX_COLLISION;
					break;
				case RADIO_TX_NOACK:
					ret = MAC_TX_NOACK;
					break;
				default:
					ret = MAC_TX_ERR;
					break;
				}
			}

			mac_call_sent_callback(item->sent_callback, item->sent_ptr, ret, 1);

			list_remove(muchmac_queue, item);
			memb_free(&muchmac_queue_memb, item);

			// switch back to our channel
			if (!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_null)) {
				linkaddr_t addr = linkaddr_node_addr;
				//int channel = get_rand_num(&addr)%16+11;
				PRINTF("muchmac: changing back to channel %d\n", channel);
				NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
			}
		}
	}

	PROCESS_END();
}

static void
send(mac_callback_t sent_callback, void *ptr)
{
	int ret;

	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);

	if (NETSTACK_FRAMER.create_and_secure() < 0) {
		ret = MAC_TX_ERR_FATAL;
	} else {
		switch (NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen())) {
		case RADIO_TX_OK:
			ret = MAC_TX_OK;
			break;
		case RADIO_TX_COLLISION:
			ret = MAC_TX_COLLISION;
			break;
		case RADIO_TX_NOACK:
			ret = MAC_TX_NOACK;
			break;
		default:
			ret = MAC_TX_ERR;
			break;
		}
	}

	mac_call_sent_callback(sent_callback, ptr, ret, 1);
}


static void
send_list(mac_callback_t sent_callback, void *ptr, struct rdc_buf_list *list)
{
	if (list != NULL) {
		if (radio_is_on) {
			PRINTF("muchmac: send immediately\n");
			//        if (!(in_broadcast_slot && in_unicast_slot) && !packetbuf_holds_broadcast()) {
			//int channel = get_rand_num(packetbuf_addr(PACKETBUF_ADDR_RECEIVER))%16+11;
			int channel_n =channel;
			if ((linkaddr_node_addr.u8[0] == 2) && tdma_on==1){
				channel_n=channel;
				channel = get_neighbor_channel_and_time(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
			}
			PRINTF("muchmac: changing to receiver channel %d\n", channel);
			NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
			channel=channel_n;
			//        }
			queuebuf_to_packetbuf(list->buf);
			//turn_on();
			send(sent_callback, ptr);
			//if (!radio_is_on) {turn_off();}
			//        if (!(in_broadcast_slot && in_unicast_slot) && !packetbuf_holds_broadcast()) {
			//channel = get_rand_num(&linkaddr_node_addr)%16+11;
			PRINTF("muchmac: changing back to channel %d\n", channel);
			NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
			//        }
		} else {
			struct muchmac_queue_item* item;
			item = memb_alloc(&muchmac_queue_memb);
			if (item == NULL) {
				mac_call_sent_callback(sent_callback, ptr, MAC_TX_ERR_FATAL, 1);
			} else {
				list_t muchmac_queue;
				//        if (packetbuf_holds_broadcast()) {
				//          muchmac_queue = muchmac_queue_broadcast;
				//        } else {
				muchmac_queue = muchmac_queue_unicast;
				//        }
				item->buf = list->buf;
				item->sent_callback = sent_callback;
				item->sent_ptr = ptr;
				list_push(muchmac_queue, item);
				PRINTF("muchmac: packet %p queued\n", item->buf);
			}
		}
	}
}


static void
input(void)
{
	if (NETSTACK_FRAMER.parse() >= 0) {
		if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_node_addr) ||
				packetbuf_holds_broadcast()) {
			//uint16_t timestamp = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP);
			//printf("Timestamp: %u\n", timestamp);
			NETSTACK_MAC.input();
		}
	}
}

static int
on(void)
{
	return turn_on();
}

static int
off(int keep_radio_on)
{
	if (keep_radio_on) {
		return turn_on();
	} else {
		return turn_off();
	}
}

static unsigned short
channel_check_interval(void)
{
	return CLOCK_SECOND;
}

static void
start_tdma(void *ptr)
{
	PRINTF("muchmac: starting TDMA mode\n");
	tdma_on=1;
	rtimer_set(&rt, timesynch_time_to_rtimer(0), 0, (rtimer_callback_t)powercycle, NULL);
}

static void
init(void)
{
	on();

	PT_INIT(&pt);
	int i=0;
	rand_num = (linkaddr_node_addr.u8[0])*multiplier+increment;
	int *channel_list_temp;
	int *channel_list_copy;
	channel_list_temp = (int*)malloc(16*sizeof(int));
	channel_list_copy = (int*)malloc(16*sizeof(int));
	for (i=0; i<16; i++){
		channel_list_temp[i]=i+11;
	}
	int unvisited_ch=16;
	i=0;
	printf("Local Channel list: ");
	while (unvisited_ch > 0){
		channel_list[i] = channel_list_temp[rand_num % unvisited_ch];
		printf("%d ", channel_list[i]);
		i++;
		int p=0;
		int j=0;
		while (p <= unvisited_ch){
			if (p != (rand_num % (unvisited_ch))){
				channel_list_copy[j]=channel_list_temp[p];
				p++;
				j++;
			}else{
				p++;
			}
		}
		channel_list_temp=channel_list_copy;
		rand_num = rand_num*multiplier+increment;
		unvisited_ch--;
	}

	memb_init(&muchmac_queue_memb);
	list_init(muchmac_queue_unicast);
	//  list_init(muchmac_queue_broadcast);

	process_start(&send_packet, NULL);

	// enable both broadcast & unicast while synchronizing
	//  in_broadcast_slot = 1;
	//  in_unicast_slot = 1;

	// schedule start of TDMA mode
	ctimer_set(&ct, 30*CLOCK_SECOND, start_tdma, NULL);
}

const struct rdc_driver muchmac_driver = {
		.name = "muchmac",
		.init = init,
		.send = send,
		.send_list = send_list,
		.input = input,
		.on = on,
		.off = off,
		.channel_check_interval = channel_check_interval
};
