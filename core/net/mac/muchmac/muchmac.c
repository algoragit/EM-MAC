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
static int unvisited_channels;
static int *channel_list;
static int *channel_list_copy;
static int channel_n;
static int unvisited_channels_n;
static int *channel_list_n;
static int *channel_list_copy_n;
static unsigned int rand_num;
static unsigned int rand_num_neighbor;
static int multiplier=15213;
static int increment=11237;

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
//static volatile unsigned char in_broadcast_slot = 0;[2]
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

#define BROADCAST_CHANNEL	26
static unsigned int
get_rand_num()
{
	if (first_time==0){
		rand_num = rand_num*multiplier+increment;
	}else{
		rand_num = (linkaddr_node_addr.u8[0])*multiplier+increment;
		first_time=0;
	}
	linkaddr_t dest;
	dest.u8[0]=2;
	get_neighbor_channel_and_time(&dest.u8[0]);
	printf("\n\n%d\n", dest.u8[0]);
	//get_channel(rand_num);
	//printf("Random number generated: %u\n", rand_num);

	return rand_num;
}

int get_channel(unsigned int random4chan){
	int i=0;
	if (unvisited_channels == 1){
		channel = channel_list[0];
		unvisited_channels=16;
		for (i=0; i<16; i++){
			channel_list[i]=i+11;
		}
	}else{
		//int random_pointer=random4chan % unvisited_channels;
		channel = channel_list[random4chan % unvisited_channels];
		unvisited_channels--;
		i=0;
		int j=0;
		/*printf("\nrandom_pointer: (%d)\n", random_pointer);
		for (i=0;i<=unvisited_channels;i++)
			printf("%d ", channel_list[i]);
		printf("\n");*/
		i=0;
		while (i<=unvisited_channels){
			if (i!=(random4chan % (unvisited_channels+1))){
				channel_list_copy[j]=channel_list[i];
				i++;
				j++;
			}else{
				i++;
			}
		}
		/*printf("Copy: ");
		for (i=0;i<unvisited_channels;i++)
			printf("%d ", channel_list_copy[i]);
		printf("\n");*/
		channel_list=channel_list_copy;
	}
	//printf("Channel from get_channel: %d from random4chan: %u\n", channel, random4chan);
	return channel;
}

int get_neighbor_channel(unsigned int random4chan){
	int i=0;
	if (unvisited_channels_n == 1){
		channel_n = channel_list_n[0];
		unvisited_channels_n=16;
		for (i=0; i<16; i++){
			channel_list_n[i]=i+11;
		}
	}else{
		//int random_pointer=random4chan % unvisited_channels;
		channel_n = channel_list_n[random4chan % unvisited_channels_n];
		unvisited_channels_n--;
		i=0;
		int j=0;
		/*printf("\nrandom_pointer: (%d)\n", random_pointer);
		for (i=0;i<=unvisited_channels;i++)
			printf("%d ", channel_list[i]);
		printf("\n");*/
		i=0;
		while (i<=unvisited_channels_n){
			if (i!=(random4chan % (unvisited_channels_n+1))){
				channel_list_copy_n[j]=channel_list_n[i];
				i++;
				j++;
			}else{
				i++;
			}
		}
		/*printf("Copy: ");
		for (i=0;i<unvisited_channels;i++)
			printf("%d ", channel_list_copy[i]);
		printf("\n");*/
		channel_list_n=channel_list_copy_n;
	}
	//printf("Channel from get_neighbor_channel: %d from random4chan: %u\n", channel_n, random4chan);
	return channel_n;
}

int
get_neighbor_channel_and_time(const linkaddr_t *addr)
{
	// The first random number generated for each node is used to generate the first wake-up channel.
	int neighbor_channel;
	rand_num_neighbor=(addr->u8[0])*multiplier+increment;
	printf("rand_num_neighbor (0): %u, ", rand_num_neighbor);
	neighbor_channel=get_neighbor_channel(rand_num_neighbor);
	printf("neighbor_channel (0): %u\n", neighbor_channel);
	int i;
	unsigned int neighbor_time_seconds=0;
	unsigned int neighbor_time_ticks_acc=rand_num_neighbor;
	unsigned int neighbor_time_ticks_temp=0;
	int local_seconds = clock_seconds();
	while (local_seconds+1 > neighbor_time_seconds){
		rand_num_neighbor=rand_num_neighbor*multiplier+increment;
		printf("rand_num_neighbor (1): %u\n", rand_num_neighbor);
		neighbor_time_ticks_temp=rand_num_neighbor;  // We must use the same expression used in the powercycle() function (NOT DONE)
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
		//rand_num_neighbor[1]=rand_num_neighbor[0];
		rand_num_neighbor=rand_num_neighbor*multiplier+increment;
		printf("rand_num_neighbor (2): %u, ", rand_num_neighbor);
		neighbor_channel=get_neighbor_channel(rand_num_neighbor);
		printf("neighbor_channel (2): %u\n", neighbor_channel);
	}
	unvisited_channels_n=16;
	for (i=0; i<16; i++){
		channel_list_n[i]=i+11;
	}
	//printf("Neighbor: \nChannel: %u, Seconds: %u, Ticks: %u\nLocal:\nTicks: %u, Seconds: %u, Ticks: %u\n\n", channel, neighbor_time_seconds, neighbor_time_ticks_acc, RTIMER_NOW(), clock_seconds());
	return neighbor_channel;
}

static char
powercycle(struct rtimer *t, void *ptr)
{
	PT_BEGIN(&pt);

	while (1) {
		channel = get_channel(get_rand_num(&linkaddr_node_addr));
		NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
		//printf("Channel: %u Node number: %d\n", channel, linkaddr_node_addr.u8[0]);
		turn_on();
		if (list_tail(muchmac_queue_unicast) != NULL) {
			process_poll(&send_packet);
		}
		rtimer_set(t, RTIMER_NOW() + ON_PERIOD, 0, (rtimer_callback_t)powercycle, NULL);
		PT_YIELD(&pt);

		// unicast slot end
		turn_off();
		rtimer_set(t, timesynch_time_to_rtimer(get_rand_num()%RTIMER_SECOND + RTIMER_SECOND/2), 0, (rtimer_callback_t)powercycle, NULL);
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
			break;static int channel;
			static int unvisited_channels;
			static int *channel_list;
			static int *channel_list_copy;
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
			PRINTF("muchmac: changing to receiver channel %d\n", channel);
			NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);
			//        }
			queuebuf_to_packetbuf(list->buf);
			send(sent_callback, ptr);
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
	rtimer_set(&rt, timesynch_time_to_rtimer(0), 0, (rtimer_callback_t)powercycle, NULL);
}

static void
init(void)
{
	on();//        }

	PT_INIT(&pt);
	int i;
	unvisited_channels=16;
	channel_list = (int*)malloc(16*sizeof(int));
	channel_list_copy = (int*)malloc(16*sizeof(int));
	unvisited_channels_n=16;
	channel_list_n = (int*)malloc(16*sizeof(int));
	channel_list_copy_n = (int*)malloc(16*sizeof(int));
	printf("Channel Lists initialized!!!\n\n");
	for (i=0; i<16; i++){
		channel_list[i]=i+11;
	}
	for (i=0; i<16; i++){
		channel_list_n[i]=i+11;
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
