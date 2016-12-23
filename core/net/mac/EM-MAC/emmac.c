/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A null RDC implementation that uses framer for headers.
 * \author
 *         Carlos M. García-Algora <cgalgora@uclv.edu.cu>
 *         Ernesto López-Prieto <eplopez@uclv.cu>
 */
#include "contiki-conf.h"
#include "net/mac/mac-sequence.h"
#include "net/mac/EM-MAC/emmac.h"
#include "net/mac/EM-MAC/frame_EM-MAC.h"
#include "net/mac/EM-MAC/framer-EM-MAC.h"
#include "net/mac/EM-MAC/LCG.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include "sys/rtimer.h"
#include "lib/list.h"
#include "sys/cc.h"
#include "sys/timer.h"
#include "linkaddr.h"
#include "lib/memb.h"
#include "dev/leds.h"

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define ACK_WAIT_TIME                      	RTIMER_SECOND / 70
#define AFTER_ACK_DETECTED_WAIT_TIME       	RTIMER_SECOND / 250
#define DATA_PACKET_WAIT_TIME               RTIMER_SECOND / 500
#define BEACON_LENGTH						29
#define ACK_LENGTH							28
//Values of the reception state

static struct pt pt;
static struct rtimer reciever_powercycle_timer;
static struct ctimer delay_tx_timer;
static struct timer w;
static unsigned int initial_rand_seed;
static unsigned int initial_rand_seed_temp;
static unsigned int blacklist;
//static unsigned short ack_len;
static unsigned short neighbor_discovery_flag;
static unsigned int w_up_time;
static unsigned time_in_seconds;
uint8_t dummy_buf_to_flush_rxfifo[1];
uint8_t current_channel=0;
uint8_t w_up_ch = 26;
int receiving=0;
int transmitting=0;
int waiting_to_transmit=0;
int syncing=0;
//int syncing=0;
unsigned int list_of_channels[15]={0};
static rtimer_clock_t wt4powercycle;
unsigned int time_to_wait_awake;
static int offset;
static int sent_times=0;
static int incoming_packet=0;
unsigned int iteration_out[6]={0};
unsigned int time_in_advance=RTIMER_SECOND/50;
static unsigned int succ_beacon=0;
static unsigned int successful=0;
static unsigned int failed=0;
static unsigned int beacon_failed_sync=0;
static unsigned int beacon_failed_tx=0;
static unsigned int failed_COL=0;
static unsigned int fail_ACK=0;
static unsigned int failed_DEF=0;
static unsigned int failed_ERR=0;
static unsigned int successful_after_try_again=0;
static unsigned int failed_try_again=0;
static int pkts_rxed_w_tx;

/* Every neighbor has its own packet queue */
struct neighbor_queue {
	struct neighbor_queue *next;
	linkaddr_t addr;
	struct ctimer transmit_timer;
	uint8_t transmissions;
	uint8_t collisions, deferrals;
	LIST_STRUCT(queued_packet_list);
};
struct send_arg {
	mac_callback_t sent;
	void *ptr;
	struct rdc_buf_list *buf_list;
};
static struct send_arg delay_arg[4];

#define MEMB_SIZE 10    // Limits the number of neighbors
MEMB(neighbor_memb, neighbor_state, MEMB_SIZE);
LIST(Neighbors);

/*---------------------------------------------------------------------------*/
static int 	off					(int keep_radio_on);
static int 	check_if_radio_on	(void);
static int 	on					(void);
static unsigned int get_w_up_advance(unsigned short failed_tx);
static void	neighbor_discovery	(void);
static char	reception_powercycle(void);
//static int 	send_one_packet		(mac_callback_t sent, void *ptr, linkaddr_t receiver, int result);
static int 	qsend_packet			(mac_callback_t sent, void *ptr);//, linkaddr_t receiver, struct rdc_buf_list *buf_list);
//static int	process_packet		(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list);
static void	delay_packet		(void);
static void	send_list			(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list);
static void	packet_input		(void);
static int	resync_neighbor		(mac_callback_t sent, linkaddr_t receiver, void *ptr, struct rdc_buf_list *buf_list);
static unsigned short channel_check_interval(void);
static void	init				(void);


/*---------------------------------------------------------------------------*/
static linkaddr_t
parse_eb_ack(uint8_t *beacon_buf,
		int type,
		unsigned int local_time_tics,
		unsigned int local_time_seconds,
		neighbor_state *n){
	//printf("f1\n");
	linkaddr_t sender_addr;
	int add_start_byte = 16;
	sender_addr.u8[0] = beacon_buf[add_start_byte--];
	sender_addr.u8[1] = beacon_buf[add_start_byte--];
	sender_addr.u8[2] = beacon_buf[add_start_byte--];
	sender_addr.u8[3] = beacon_buf[add_start_byte--];
	sender_addr.u8[4] = beacon_buf[add_start_byte--];
	sender_addr.u8[5] = beacon_buf[add_start_byte--];
	sender_addr.u8[6] = beacon_buf[add_start_byte--];
	sender_addr.u8[7] = beacon_buf[add_start_byte--];
	//	printf("ins addr:");
	//	int i;
	//	for (i=0; i<8; i++){
	//		printf("%u ",sender_addr.u8[i]);
	//	}
	//	printf("\n");
	/*Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales*/
	add_start_byte = 17;
	unsigned int w_time= beacon_buf[add_start_byte]+ (beacon_buf[add_start_byte+1] << 8);
	add_start_byte+=2;
	unsigned int t_sec=beacon_buf[add_start_byte]+ (beacon_buf[add_start_byte+1] << 8);  		// Tiempo de wake-up en seg
	add_start_byte+=2;
	unsigned int last_generated=beacon_buf[add_start_byte]+ (beacon_buf[add_start_byte+1] << 8);	// Última semilla del generador
	add_start_byte+=2;
	unsigned int c_t_sec = beacon_buf[add_start_byte]+ (beacon_buf[add_start_byte+1] << 8); 	//tiempo actual en seconds
	add_start_byte+=2;
	uint8_t last_ch = beacon_buf[add_start_byte];					// last visited channel
	switch (type){
	case FRAME802154_BEACONFRAME:
		add_start_byte+=2;  // We account for the byte read previously + the byte with the waiting_to_transmit info
		break;
	case FRAME802154_ACKFRAME:
		add_start_byte++;  // We account only for the byte read previously
		break;
	}
	unsigned int t_tic=(beacon_buf[add_start_byte]+ (beacon_buf[add_start_byte+1] << 8))%RTIMER_SECOND; 		//tiempo actual en tics
	//				printf("RX_info: %u %u %u %u %u %u\n", w_time, t_sec,
	//						last_generated, c_t_sec, last_ch, t_tic);
	if (sender_addr.u8[7]!=0 && sender_addr.u8[7]<100){
		//					printf("Inside if\n");
		/* Update the state information for the node that sent the ACK */
		for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
			if(linkaddr_cmp(&sender_addr, &n->node_link_addr)) {
//				printf("stXp1:%u %04x %04x %04x %04x %04x(%d) %04x(%d)\n",
//						n->node_link_addr.u8[7],
//						n->wake_time_tics,
//						n->wake_time_seconds,
//						n->last_seed,
//						n->last_channel,
//						n->d_tics,
//						n->d_tics,
//						n->d_secs,
//						n->d_secs);
				n->wake_time_tics=w_time;
				n->wake_time_seconds=t_sec;
				n->last_seed=last_generated;
				n->last_channel=last_ch;
				n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
				n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
				n->consecutive_failed_tx=0;
				//printf("Neighbor found and added\n");
				//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
				//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->d_tics, local_time_seconds, c_t_sec, n->d_secs);
				break;
			}
		}
		/* If no matching encounter was found, we try to allocate a new one. */
		if(n == NULL) {
			n = memb_alloc(&neighbor_memb);
			if(n != NULL) { 	// We could not allocate memory for this encounter, so we just drop it.
				linkaddr_copy(&n->node_link_addr, &sender_addr);
				n->wake_time_tics=w_time;
				n->wake_time_seconds=t_sec;
				n->last_seed=last_generated;
				n->last_channel=last_ch;
				n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
				n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
				n->consecutive_failed_tx=0;
				list_add(Neighbors, n);
			}
		}
//		printf("btr:%04x %04x %04x %04x %04x\n",
//				n->last_seed,
//				n->wake_time_tics,
//				n->wake_time_seconds,
//				n->last_channel,
//				beacon_buf[26],
//				c_t_sec);
		//		int i;
		//		printf("i:");
		//		for (i=0; i<8;i++){
		//			printf("%u", sender_addr.u8[i]);
		//		}
		//		printf(" %u %u %u %u %u %u %u\n",
		//				beacon_buf[2],
		//				n->wake_time_tics,
		//				n->wake_time_seconds,
		//				n->d_tics,
		//				n->d_secs,
		//				n->last_channel,
		//				n->last_seed);
	}
	return sender_addr;
}
/*---------------------------------------------------------------------------*/
static void
update_neighbor_from_data_pkt(frame802154_t recieved_frame,
		unsigned int local_time_tics,
		unsigned int local_time_seconds){
	neighbor_state *n;
	unsigned int w_time 	= packetbuf_attr(PACKETBUF_ATTR_NODE_TIMESTAMP);
	unsigned int t_sec 		= packetbuf_attr(PACKETBUF_ATTR_NODE_CLOCK_TIME);
	unsigned int last_generated = packetbuf_attr(PACKETBUF_ATTR_NODE_RAND_SEED);
	unsigned int c_t_sec 	= packetbuf_attr(PACKETBUF_ATTR_NODE_CLOCK_TIME_SENT);
	uint8_t last_ch 		= packetbuf_attr(PACKETBUF_ATTR_NODE_BLACKLIST);
	unsigned int t_tic		= packetbuf_attr(PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP);

	if (recieved_frame.src_addr[7]!=0 && recieved_frame.src_addr[7]<100){
		//					printf("Inside if\n");
		/* Update the state information for the node that sent the ACK */
		for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
			if(linkaddr_cmp((linkaddr_t *)&recieved_frame.src_addr, &n->node_link_addr)) {
				int i;
				//printf("stXt1:");
				//				for (i=0; i<8;i++){
				//					printf("%u", n->node_link_addr.u8[i]);
				//				}
//				printf("%u %04x %04x %04x %04x %04x(%d) %04x(%d) %u %u\n",
//						n->node_link_addr.u8[7],
//						n->wake_time_tics,
//						n->wake_time_seconds,
//						n->last_seed,
//						n->last_channel,
//						n->d_tics,
//						n->d_tics,
//						n->d_secs,
//						n->d_secs,
//						local_time_tics,
//						t_tic);
				n->wake_time_tics=w_time;
				n->wake_time_seconds=t_sec;
				n->last_seed=last_generated;
				n->last_channel=last_ch;
				n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
				n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
				n->consecutive_failed_tx=0;
//				printf("stXt2:");
//				printf("%u %04x %04x %04x %04x %04x(%d) %04x(%d) %u %u\n",
//						n->node_link_addr.u8[7],
//						n->wake_time_tics,
//						n->wake_time_seconds,
//						n->last_seed,
//						n->last_channel,
//						n->d_tics,
//						n->d_tics,
//						n->d_secs,
//						n->d_secs,
//						local_time_seconds,
//						c_t_sec);
				//printf("Neighbor found and added\n");
				//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
				//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->d_tics, local_time_seconds, c_t_sec, n->d_secs);
				break;
			}
		}
		/* If no matching encounter was found, we try to allocate a new one. */
		if(n == NULL) {
			n = memb_alloc(&neighbor_memb);
			if(n != NULL) { 	// We could not allocate memory for this encounter, so we just drop it.
				linkaddr_copy(&n->node_link_addr, (linkaddr_t *)&recieved_frame.src_addr);
				n->wake_time_tics=w_time;
				n->wake_time_seconds=t_sec;
				n->last_seed=last_generated;
				n->last_channel=last_ch;
				n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
				n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
				n->consecutive_failed_tx=0;
				list_add(Neighbors, n);
			}
		}
		//		int i;
		//		printf("i:");
		//		for (i=0; i<8;i++){
		//			printf("%u", sender_addr.u8[i]);
		//		}
		//		printf(" %u %u %u %u %u %u %u\n",
		//				beacon_buf[2],
		//				n->wake_time_tics,
		//				n->wake_time_seconds,
		//				n->d_tics,
		//				n->d_secs,
		//				n->last_channel,
		//				n->last_seed);
	}
}

/*Apaga el radio si se pasa 0 como parametro*/
static int
off(int keep_radio_on)
{
	//printf("f2\n");
	if(keep_radio_on) {
		return NETSTACK_RADIO.on();
	} else {
		return NETSTACK_RADIO.off();
	}
}
/******************************************************************************/
/*Devuelve 1 si el radio esta encendido, 0 si esta apagado*/
static int
check_if_radio_on(void)
{
	//printf("f3\n");
	int x;
	NETSTACK_RADIO.get_value(RADIO_PARAM_POWER_MODE,&x);
	if(x==RADIO_POWER_MODE_ON)
		return 1;
	else
		return 0;
}
/********************************************************************************/
/*Enciende el radio si y solo si se encuentra apagado*/
static int
on(void)
{
	//printf("f4\n");
	if(!check_if_radio_on()){
		return NETSTACK_RADIO.on();
	}
}
/****************************************************************/
static unsigned int
get_w_up_advance(unsigned short failed_tx)
{
	//printf("f5\n");
	unsigned short i;
	unsigned int temp=RTIMER_SECOND/50;
	if(failed_tx==0 || failed_tx==1){
		return RTIMER_SECOND/50;}
	if(failed_tx>=6){
		return (RTIMER_SECOND/136)*100;
	}

	for(i=2;i<=failed_tx;i++){
		temp=temp*2;}
	return temp;
}
/********************************************************************************/
/*Proceso de descubrimiento de vecinos que se lleva a cabo en la funcion init()*/
/* TODO Creo que los vecinos deberían mandar la info desde ahora  porque ya el powercycle está corriendo y esa info existe */
static void
neighbor_discovery(void)
{
	//printf("f6\n");
	uint8_t beacon_buf[BEACON_LENGTH];
	static  linkaddr_t addr; 		// This one is used to store the sender of the Beacons
	static linkaddr_t sender_addr; 	// This one is used to store the sender of the ACKs
	neighbor_discovery_flag=1;
	unsigned int wt;
	neighbor_state *n;
	uint8_t ackdata[ACK_LENGTH] = {0};
	unsigned int t_seconds;
	unsigned int w_time;
	unsigned int t_sec;
	unsigned int last_generated;
	unsigned int c_t_sec;
	unsigned int t_tic;
	unsigned int local_time_tics;
	unsigned int local_time_seconds;
	/*Para establecer el tiempo que durara el proceso de descubrimiento de vecinos se usa el timer.h por encuesta.
	 * Lo ideal seria usar el rtimer pero no se puede debido a que solo puede existir una unica instancia del mismo
	 * El uso de la unica instancia permitida del rtimer se reserva para el powercycle*/

	timer_set(&w,((CLOCK_SECOND*5)));
	while(neighbor_discovery_flag){
		transmitting=0;
		/*while(NETSTACK_RADIO.receiving_packet()) {
			//printf("RXing PKT\n");
		}*/
		if(NETSTACK_RADIO.pending_packet()) {
			//printf("PENDING PACKET!");
			int len=NETSTACK_RADIO.read(beacon_buf, 31);
			local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
			local_time_seconds = clock_seconds();
			if((beacon_buf[0] & 7) == FRAME802154_BEACONFRAME && beacon_buf[2] == 128){
				/*				printf("ND ACK/BEACON rx %u\n", len);
				//				printf("EB_RX:");
				//				int i;
				//				for (i=0; i<len; i++){
				//					printf("%d ",beacon_buf[i]);
				//				}
				//				printf("\n");*/
				parse_eb_ack(&beacon_buf,
						FRAME802154_BEACONFRAME,
						local_time_tics,
						local_time_seconds,
						&n);
			}
			/*else {
				int i;
				printf("Unknown rx:");
				for (i=0; i < ack_len; i++){
					printf("%02x ", ackdata[i]);
				}
				printf("\n");
				printf("Unknown type: %02x\n", (beacon_buf[0] & 7));
			}*/
		}
		/*Si el temporizador ha expirado hacemos neighbor_discovery_flag=0 luego de lo cual salimos del proceso
		 * de descubrimiento de vecinos*/
		if(timer_expired(&w)){
			neighbor_discovery_flag=0;
		}
	}
	/************ Print the neighbor list after the neighbor discovery. (Debug purposes only)   *********/
	neighbor_state *test=list_head(Neighbors);
	printf("Neighbor List after Neighbor Discovery: \n");
	while(test != NULL) {
		printf("%d. wake_time_tics: %u, d_secs: %d last_seed: %u, wake_time_seconds: %u, n: %d\n",
				test->node_link_addr.u8[7], test->wake_time_tics, test->d_secs,
				test->last_seed, test->wake_time_seconds, test->d_tics);
		test = list_item_next(test);
	}
	/************ end of neighbor list printing **********************************************************/
}
/******************************************************************************/
/*La siguiente funcion se usa para controlar el ciclo util de radio*/
static char
reception_powercycle(void)
{
	//printf("f6rp\n");
	PT_BEGIN(&pt);
	while (1){
		if(!neighbor_discovery_flag && (transmitting == waiting_to_transmit) && !syncing){  // TODO If the radio is not sending a packet, the radio should stay ON
			off(0);
			//printf("_off\n",initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2, initial_rand_seed_temp);//}
			NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
		}
		initial_rand_seed_temp=(15213*initial_rand_seed)+11237;

		rtimer_set(&reciever_powercycle_timer,(w_up_time+(initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2)), 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		/*if (transmitting!=0){
			leds_blink();
		}*/
		PT_YIELD(&pt);

		/* Turn on the radio interface */
		if ((!neighbor_discovery_flag || syncing) && (transmitting == waiting_to_transmit)/*transmitting==0*/){
			NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, list_of_channels[current_channel]);
		}
		if(check_if_radio_on()==0 && (transmitting == waiting_to_transmit)){
			on();
		}
		/* Save the timestamps (in tics and in seconds) of the last wake-up,
		 * which will be transmitted when the state is requested through an ACK */
		w_up_time=RTIMER_NOW();
		time_in_seconds = clock_seconds();
		initial_rand_seed = initial_rand_seed_temp;
		w_up_ch = list_of_channels[current_channel];
		/* TODO: Get the blacklist and embed it into the beacon. Now, the value for the blacklist is fixed to 100 */
		packetbuf_set_attr(PACKETBUF_ATTR_NODE_BLACKLIST, 0x00F0);
		blacklist=packetbuf_attr(PACKETBUF_ATTR_NODE_BLACKLIST);

		/* Create beacon and transmit it... */
		uint8_t beacon_data[BEACON_LENGTH] = {0};
		//		uint8_t beacon_data_test[BEACON_LENGTH] = {0};
//		printf("stXcsrp:%u\n", clock_seconds());
		packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, FRAME802154_BEACONFRAME);
		int pos = frame_emmac_create_eb_ack(&beacon_data, FRAME802154_BEACONFRAME,
				packetbuf_attr(PACKETBUF_ATTR_PENDING),
				128,
				w_up_time,
				time_in_seconds,
				initial_rand_seed,
				w_up_ch,
				waiting_to_transmit);
		if (transmitting == waiting_to_transmit || syncing){
			//beacon_data[1]=waiting_to_transmit;
			while(NETSTACK_RADIO.channel_clear() == 0){}
			packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1); //Se activa el timestamp a nivel fisico
			NETSTACK_RADIO.send(beacon_data, BEACON_LENGTH);
			packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);/*Se desactiva el timestamp a nivel fisico*/
			//printf("PC BCN tx\n");
			if (syncing){
				NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 26);
			}
		}
		/*		printf("TX_info: %u %u %u %u %u %u\n", pos, w_up_time, time_in_seconds,
		//				initial_rand_seed, t_seconds, w_up_ch);
		//		printf("TX_byte_info: %u %u %u %u %u %u %u %u %u\n",
		//				w_up_time%RTIMER_SECOND & 0xff,
		//				(w_up_time%RTIMER_SECOND>> 8) & 0xff,
		//				time_in_seconds & 0xff,
		//				(time_in_seconds>> 8) & 0xff,
		//				initial_rand_seed & 0xff,
		//				(initial_rand_seed>>8) & 0xff,
		//				t_seconds & 0xff,
		//				(t_seconds>>8) & 0xff,
		//				w_up_ch);
		//
		//		int i;
		//		printf("EB_TX:");
		//		for (i=0; i<=pos; i++){
		//			printf("%d ",beacon_data[i]);
		//		}
		//		printf("\n");*/
		if (transmitting==0){
			// The time spent awake is the maximum time required for a node to send a data packet from the moment it receives the beacon
			receiving=1;
			wt4powercycle = RTIMER_NOW();
			time_to_wait_awake=RTIMER_SECOND/100;
			/* This IF prevents the node to go back to sleep if RTIMER_NOW() is greater than (2^16-RTIMER_SECOND/82) because,
			 * in that case, (RTIMER_NOW() > (wt4powercycle + time_to_wait_awake)); and the node goes to sleep earlier than it should.
			 */
			if ((wt4powercycle+time_to_wait_awake) <= RTIMER_SECOND/82){
				offset=RTIMER_SECOND/82;
			}else {
				offset=0;
			}
			pkts_rxed_w_tx=0;
			while((RTIMER_NOW()+offset) < (wt4powercycle + time_to_wait_awake + offset) && !neighbor_discovery_flag && transmitting==0){
				// Check if a packet was received
				if (NETSTACK_RADIO.pending_packet() || incoming_packet==1 ){
					rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ RTIMER_SECOND/218, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
					time_to_wait_awake=RTIMER_SECOND/82 + ((RTIMER_NOW()-wt4powercycle));
					PT_YIELD(&pt);
					if ((wt4powercycle+time_to_wait_awake) <= RTIMER_SECOND/82){
						offset=RTIMER_SECOND/82;
					}else {
						offset=0;
					}
				}
			}
		}else{
			if (!syncing && waiting_to_transmit){
				off(0);
			}
		}
		current_channel=(current_channel+1)%15;

		/*if (clock_seconds()%900 < 2){ // 15mins
			printf("succ:%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
					succ_beacon, beacon_failed_sync+beacon_failed_tx, beacon_failed_sync, beacon_failed_tx,
					successful, failed, failed_COL, fail_ACK, failed_DEF, failed_ERR, failed_try_again, successful_after_try_again);
			printf("succ_B_PDR=%u  validB_PDR=%u  PDR=%u\n"
					"succ_b:%u b_fail:%u b_fail_s:%u b_fail_tx:%u\n"
					"succ_:%u fail:%u f_COL:%u f_ACK:%u f_DEF:%u f_ERR:%u,%u,%u\n",
					(succ_beacon*100)/(succ_beacon+beacon_failed_sync), (succ_beacon*100)/(succ_beacon+beacon_failed_sync+beacon_failed_tx), (successful*100)/(successful+failed),
					succ_beacon, beacon_failed_sync+beacon_failed_tx, beacon_failed_sync, beacon_failed_tx,
					successful, failed, failed_COL, fail_ACK, failed_DEF, failed_ERR, failed_try_again, successful_after_try_again);
		}*/
		receiving=0;
		rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ RTIMER_SECOND/10000, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		PT_YIELD(&pt);
	}
	PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
static int
resync_neighbor(mac_callback_t sent, linkaddr_t receiver, void *ptr, struct rdc_buf_list *buf_list){
	//printf("f7\n");
	syncing=1;
	timer_set(&w,((CLOCK_SECOND*45))); // 48s = 2 * 15channels * (TmaxInterval==1.5s)
	uint8_t beacon_buf[BEACON_LENGTH];
	rtimer_clock_t wt;
	uint8_t ret=0;
	neighbor_state *n;
	neighbor_discovery_flag=1;
	int found_it=0;
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 26);
	on();
	NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
	while (found_it==0 && !timer_expired(&w)){
		if(NETSTACK_RADIO.pending_packet()) {
			int len=NETSTACK_RADIO.read(beacon_buf, BEACON_LENGTH);
			if ((beacon_buf[0] & 7) == FRAME802154_BEACONFRAME){
				//printf("sbrx\n");
				unsigned int local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
				unsigned int local_time_seconds = clock_seconds();
				linkaddr_t sender_addr = parse_eb_ack(&beacon_buf,
						FRAME802154_BEACONFRAME,
						local_time_tics,
						local_time_seconds,
						&n);
				if(linkaddr_cmp(&sender_addr, &receiver)){
					//printf("sbrx\n");
					queuebuf_to_packetbuf(buf_list->buf);
					while(RTIMER_NOW() < wt+10){}  // 10 tics allow the receiver to switch the radio to rx mode
					packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
					packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK,1);
					packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 1);
					NETSTACK_FRAMER.create();
					uint8_t dsn;
					dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;
					NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());
					if(!NETSTACK_RADIO.channel_clear()) {
						ret=1;
					}
					else {
						//printf("sbrx2\n");
						switch(NETSTACK_RADIO.transmit(packetbuf_totlen())) {
						case RADIO_TX_OK:
							// Check for ACK
							wt = RTIMER_NOW();
							ret = MAC_TX_NOACK;
							// Flush the RXFIFO to avoid that a node interprets as his own an ACK sent to another node with the same packet sequence number.
							NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);
							// Wait for the ACK
							while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + ACK_WAIT_TIME)) {
								/*Este mecanismo es similar al explicado en neighbor discovery*/
								if(NETSTACK_RADIO.receiving_packet() ||				//Ninguna de estas condiciones se cumple cuando se envía un paquete de datos.
										NETSTACK_RADIO.pending_packet() ||
										NETSTACK_RADIO.channel_clear() == 0) {
									//printf("pkt pending 1\n");
									int len;
									uint8_t ackbuf[ACK_LENGTH];
									wt = RTIMER_NOW();
									while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {
										if(NETSTACK_RADIO.pending_packet()) {
											//printf("pkt pending 2:");
											len = NETSTACK_RADIO.read(ackbuf, ACK_LENGTH);
											//printf("%u\n", len);
											if(len == ACK_LENGTH && ackbuf[2] == dsn) {
												// ACK received
												//printf("ack check\n");
												if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){
													//printf("ack check 1\n");
													local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
													local_time_seconds = clock_seconds();
													sender_addr = parse_eb_ack(&ackbuf,
															FRAME802154_ACKFRAME,
															local_time_tics,
															local_time_seconds,
															&n);
												}
												ret = MAC_TX_OK;
												found_it=1;
												mac_call_sent_callback(sent, ptr, 0, 1);
												break;		// This breaks the While after an ACK has been received and processed.
											}
										}
									}
									if (ret==MAC_TX_OK){
										break;}
								}
							}
							break;
						case RADIO_TX_COLLISION:
							break;
						default:
							break;
						}
					}
				}
			}
		}
	}
	syncing=0;
	off(0);
	neighbor_discovery_flag=0;
	//PRINTF("syncOK\n");
	return found_it;
}
/*---------------------------------------------------------------------------*/
static int
send_packet(mac_callback_t sent, void *ptr, linkaddr_t receiver, struct rdc_buf_list *buf_list)
{
	//printf("here8!\n");
	uint8_t neighbor_channel;
	uint8_t done=0;
	time_in_advance=RTIMER_SECOND/50;
	unsigned int neighbor_wake;
	neighbor_state receiver_state;
	receiver_state=get_neighbor_state(Neighbors, receiver);
	if(receiver_state.last_seed==NULL){
		//printf("sync lost\n");
		done=resync_neighbor(sent, receiver, ptr, buf_list);
		if (!done){
			mac_call_sent_callback(sent, ptr, 1, 1);
		}
		return done;
	}
	time_in_advance=get_w_up_advance(receiver_state.consecutive_failed_tx);
	iteration_out[0]=0;
	iteration_out[1]=0;
	iteration_out[2]=0;
	iteration_out[3]=0;
	iteration_out[4]=0;
	iteration_out[5]=0;
	neighbor_wake = get_neighbor_wake_up_time(get_neighbor_state(Neighbors, receiver), &neighbor_channel, time_in_advance, &iteration_out);

	if (neighbor_wake > (unsigned int)(reciever_powercycle_timer.time)-RTIMER_SECOND/327 &&
			neighbor_wake < (unsigned int)(reciever_powercycle_timer.time)+RTIMER_SECOND/327){
		neighbor_wake+=RTIMER_SECOND/654;
	}
	while(neighbor_wake - (unsigned int)(RTIMER_NOW()) > 2){
		if ((unsigned int)(RTIMER_NOW())>neighbor_wake &&
				(unsigned int)(RTIMER_NOW())-neighbor_wake < RTIMER_SECOND/327){
			break;
		}
	}
	waiting_to_transmit=0;
	//printf("out:%u %u\n",neighbor_wake, RTIMER_NOW());
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, neighbor_channel);
	int result=1;
	int ret;
	sent_times=0;
	ack_restart:	//do{
	sent_times++;
	queuebuf_to_packetbuf(buf_list->buf); // If this line is not present, the last node in the neighbor list receives a packet with part of the header repeated. Don't know why!
	/*---------------------------------------------------------------------------------*/
	//		result=send_one_packet(sent, ptr, receiver, result);

	//printf("tx_pkt\n");
	neighbor_state *n;
	rtimer_clock_t wt;
	unsigned int w_time;
	unsigned int t_sec;
	unsigned int t_tic;
	unsigned int time_to_send;
	int is_broadcast;
	is_broadcast = packetbuf_holds_broadcast();
	while(neighbor_discovery_flag || check_if_radio_on()==0) {
		on();
	}
	/******************** Beacon detection before sending the packet ****************/
	int beacon_received = 0;
	int good_beacon=0;
	uint8_t beacon_buf[BEACON_LENGTH];
	NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
	wt = RTIMER_NOW();
	int wait_for_neighbor=2*time_in_advance;
	if (result==3){
		wait_for_neighbor=RTIMER_SECOND/82;  //If the nodes did rendezvous nut there was a collision, then we wait a shorter time for the ACK
	}
	while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + wait_for_neighbor) && beacon_received==0) {  //It should be "wt + Time_from_Chasing_algorithm but for now we just use 40ms (remember in Cooja the time is twice the real time)
		if(NETSTACK_RADIO.pending_packet()) {
			//printf("pdng_pkt_1\n");
			int len = NETSTACK_RADIO.read(beacon_buf, BEACON_LENGTH);
			//				printf("len:%u %u\n", len, (beacon_buf[0] & 7));
			if ((len == BEACON_LENGTH || len == ACK_LENGTH) && (beacon_buf[0] & 7) == FRAME802154_BEACONFRAME){
				unsigned int local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
				unsigned int local_time_seconds = clock_seconds();
				//printf("HERE!\n");
				linkaddr_t sender_addr = parse_eb_ack(&beacon_buf,
						FRAME802154_BEACONFRAME,
						local_time_tics,
						local_time_seconds,
						&n);
				if(linkaddr_cmp(&sender_addr, &receiver)){
					//						printf("OK\n");
					good_beacon=1;
//					printf("beacon_buf[26]=%04x %u\n", beacon_buf[26], len);
					if ((len == BEACON_LENGTH) && (beacon_buf[26]==1)){
						//printf("RES=%d_%d RxBusy\n_r_wait:%d,%u,%u(t)\n", 1, receiver.u8[7], receiver.u8[7], RTIMER_NOW()- wt, iteration_out[0]);
						beacon_failed_tx++;
						//printf("rx2\n");
						break;
					} else {
						//printf("_r_wait:%d,%u,%u\n", receiver.u8[7],RTIMER_NOW()- wt, iteration_out[0]);
						//printf("rx3\n");
						beacon_received=1;
					}
				}
			}
		}
		else{
			//printf("bcn_rxd_3\n");
		}
	}
	for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
		if(linkaddr_cmp(&receiver, &n->node_link_addr)) {
			if(good_beacon==0)
			{
				n->consecutive_failed_tx++;
				//printf("_r_fail:%d\n", n->consecutive_failed_tx);
				if(n->consecutive_failed_tx==9){
					n->blacklist=NULL;
					n->last_channel=NULL;
					n->last_seed=NULL;
					n->d_secs=NULL;
					n->d_tics=NULL;
					n->wake_time_seconds=NULL;
					n->wake_time_tics=NULL;
					//printf("_r_fail:delete\n");
				}
			}
			else{
				n->consecutive_failed_tx=0;
			}

		}
	}
	if (beacon_received==0){
		off(0);
		if (good_beacon==0){
			//				printf("RES=%d_%u RxNotFound _s_ ", 1, receiver.u8[7]);
			if (result!=3){
				beacon_failed_sync++;
			}
			//				else {
			//					printf(" _c_ ");
			//				}
			//				printf("\n");
			//				printf("\n_s_wait:%d,(s),%u %u %u %u %u %u %u %u\n",
			//						receiver.u8[7], iteration_out[0], iteration_out[1], iteration_out[2],
			//						iteration_out[3], iteration_out[4],
			//						RTIMER_NOW(), clock_seconds(), iteration_out[5]);
		}
		transmitting=0;
		//leds_off(7);
		failed++;
		failed_COL++;
		if(is_broadcast){
			mac_call_sent_callback(sent, ptr, MAC_TX_OK, 1);
		}else{
			mac_call_sent_callback(sent, ptr, MAC_TX_COLLISION, 1);
		}
//		printf("is_broadcast or BR= %u\n", beacon_received);
		return is_broadcast;
	}else{
		succ_beacon++;
		/* Backoff based on the node ID:
		 * RTIMER_SECOND/400: is the time required to transmit a message.*/
		wt=RTIMER_NOW();
		time_to_send = (unsigned int)(10 + random_rand()%(unsigned int)(RTIMER_SECOND/489)); // 77 tics is the time required to transmit a maximum length frame. We create a mninimum of 10 tics before ttransmission.
		//			printf("%u\n",time_to_send);
		while(RTIMER_NOW() < wt+time_to_send){}
	}
	/***************************************************************************************/

	int last_sent_ok = 0;
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, FRAME802154_DATAFRAME); //
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK,1);
	packetbuf_set_attr(PACKETBUF_ATTR_NODE_STATE_FLAG, 1);
	packetbuf_set_attr(PACKETBUF_ATTR_NODE_TIMESTAMP, w_up_time);
	packetbuf_set_attr(PACKETBUF_ATTR_NODE_CLOCK_TIME, time_in_seconds);
	packetbuf_set_attr(PACKETBUF_ATTR_NODE_RAND_SEED, initial_rand_seed);
	packetbuf_set_attr(PACKETBUF_ATTR_NODE_BLACKLIST, w_up_ch);
	//		printf("stateTX: %04x %04x %04x %04x\n",
	//				packetbuf_attr(PACKETBUF_ATTR_NODE_TIMESTAMP),
	//				packetbuf_attr(PACKETBUF_ATTR_NODE_CLOCK_TIME),
	//				packetbuf_attr(PACKETBUF_ATTR_NODE_RAND_SEED),
	//				packetbuf_attr(PACKETBUF_ATTR_NODE_BLACKLIST));

	if(NETSTACK_FRAMER.create() < 0) {
		// Failed to allocate space for headers
		//PRINTF("emmac: send failed, too large header\n");
		ret = MAC_TX_ERR_FATAL;
	} else {
		uint8_t dsn;
		dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

		NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());

		if(!NETSTACK_RADIO.channel_clear()) {
			/* Currently receiving a packet over air or the radio has
	         already received a packet that needs to be read before
	         sending with auto ack. */
			//leds_on(LEDS_GREEN);
			//				printf("RES=3_%u CCA\n", receiver.u8[7]);
			failed_try_again++;
			result=3;
			goto ack_restart;
			//				return 3;
		}else {
			//printf("pkt sent\n");
			switch(NETSTACK_RADIO.transmit(packetbuf_totlen())) {
			case RADIO_TX_OK:
				// Check for ACK
				wt = RTIMER_NOW();
				ret = MAC_TX_NOACK;
				// Flush the RXFIFO to avoid that a node interprets as his own an ACK sent to another node with the same packet sequence number.
				NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);
				// Wait for the ACK
				while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + ACK_WAIT_TIME)) {
					/*Este mecanismo es similar al explicado en neighbor discovery*/
					if(NETSTACK_RADIO.receiving_packet() ||				//Ninguna de estas condiciones se cumple cuando se envía un paquete de datos.
							NETSTACK_RADIO.pending_packet() ||
							NETSTACK_RADIO.channel_clear() == 0) {
						//printf("pkt pending 1\n");
						int len;
						uint8_t ackbuf[ACK_LENGTH];
						wt = RTIMER_NOW();
						while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {
							if(NETSTACK_RADIO.pending_packet()) {
								//printf("pkt pending 2:");
								len = NETSTACK_RADIO.read(ackbuf, ACK_LENGTH);
								//printf("%u\n", len);
								if(len == ACK_LENGTH && ackbuf[2] == dsn){
									// ACK received
									//printf("ack check\n");
									unsigned int local_time_seconds;
									unsigned int local_time_tics;
									if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){
										local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
										local_time_seconds = clock_seconds();
										linkaddr_t sender_addr = parse_eb_ack(&ackbuf,
												FRAME802154_ACKFRAME,
												local_time_tics,
												local_time_seconds,
												&n);
									}
									ret = MAC_TX_OK;
									break;		// This breaks the While after an ACK has been received and processed.
								}
							}
						}
						if (ret==MAC_TX_OK){
							break;}
					}
				}
				break;
			case RADIO_TX_COLLISION:
				//printf("RES=3_%u COL\n", receiver.u8[7]);
				off(0);
				failed_try_again++;
				return 3;
			default:
				ret = MAC_TX_ERR;
				break;
			}
		}
	}
	switch(ret){
	case MAC_TX_OK:
		last_sent_ok = 1;
		successful_after_try_again++;
		successful++;
		break;
	case MAC_TX_COLLISION:
		failed_COL++;
		failed++;
		break;
	case MAC_TX_NOACK:
		//printf("emmac tx noack\n");
		fail_ACK++;
		failed++;
		break;
	case MAC_TX_DEFERRED:
		failed_DEF++;
		failed++;
		break;
	default:
		failed_ERR++;
		failed++;
		break;
	}
	//printf("RES=%u_%d\n",ret, receiver.u8[7]);
	if(is_broadcast){
		mac_call_sent_callback(sent, ptr, MAC_TX_OK, 1);
	}else{
		mac_call_sent_callback(sent, ptr, ret, 1);
	}
	//printf("aqui%u\n", ret);
	off(0);
	transmitting = 0;
	//leds_off(7);
	//leds_blink();
	NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
	//		return last_sent_ok;
	//	} while (result==3);
	return ret;
}
/*---------------------------------------------------------------------------*/
static int
qsend_packet(mac_callback_t sent_callback, void *ptr){
	//printf("f8\n");
	struct neighbor_queue *n=ptr;
	struct rdc_buf_list *buf_list=list_head(n->queued_packet_list);
	transmitting = 1;
	waiting_to_transmit = 1;
	// We backup the next pointer, as it may be nullified by
	// mac_call_sent_callback()
	int last_sent_ok=1;
	queuebuf_to_packetbuf(buf_list->buf);
	static  linkaddr_t addr_receiver;
	linkaddr_copy(&addr_receiver,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	if (!packetbuf_holds_broadcast()){
		//		int counter_deferred = 0;
		//		do{
		last_sent_ok = send_packet(sent_callback, ptr, addr_receiver, buf_list);
		//		printf("DEF:%u\n", last_sent_ok);
		//		if (last_sent_ok == 3){
		//			printf("DEF = 3\n");
		//		}
	}else {
		neighbor_state *neighbor_broadcast_to_unicast=list_head(Neighbors);
		while(neighbor_broadcast_to_unicast != NULL && last_sent_ok) {
			transmitting = 1;
			waiting_to_transmit = 1;
			last_sent_ok = send_packet(sent_callback, ptr, neighbor_broadcast_to_unicast->node_link_addr, buf_list);
			neighbor_broadcast_to_unicast = list_item_next(neighbor_broadcast_to_unicast);
		}
	}
	return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
delay_packet(void){
	//printf("f9\n");
	int i=0;
	while (i<pkts_rxed_w_tx){
		//printf("Txcall_%d\n", i);
		send_list(delay_arg[i].sent, delay_arg[i].ptr, delay_arg[i].buf_list);
		i++;
	}
}
/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
	//printf("f10\n");
	if(receiving){
		delay_arg[pkts_rxed_w_tx].buf_list=buf_list;
		delay_arg[pkts_rxed_w_tx].ptr=ptr;
		delay_arg[pkts_rxed_w_tx].sent=sent;
		if (pkts_rxed_w_tx==0){
			ctimer_set(&delay_tx_timer, 5, (void (*))delay_packet, NULL);
		} else {
			ctimer_restart(&delay_tx_timer);
		}
		pkts_rxed_w_tx++;
		//printf("Tx_Rx_%d\n", pkts_rxed_w_tx);
		return;
	}
	while(buf_list != NULL) {
		transmitting = 1;
		waiting_to_transmit = 1;
		/* We backup the next pointer, as it may be nullified by
		 * mac_call_sent_callback() */
		struct rdc_buf_list *next = buf_list->next;
		int last_sent_ok;
		queuebuf_to_packetbuf(buf_list->buf);
		last_sent_ok = qsend_packet(sent, ptr);
		/* If packet transmission was not successful, we should back off and let
		 * upper layers retransmit, rather than potentially sending out-of-order
		 * packet fragments. */
		if(!last_sent_ok) {
			return;
		}
		buf_list = next;
	}
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
	//printf("f11\n");
	//printf("packet_input\n");
	//leds_on(2);
	incoming_packet=1;
	int original_datalen;
	uint8_t *original_dataptr;
	/*original_datalen y original_dataptr are necessary for making the ACK*/
	original_datalen = packetbuf_datalen();
	original_dataptr = packetbuf_dataptr();
	unsigned int local_time_tics = RTIMER_NOW();
	unsigned int local_time_seconds = clock_seconds();

	if(NETSTACK_FRAMER.parse() < 0) {
		PRINTF("emmac: failed to parse %u\n", packetbuf_datalen());
	} else {
		int duplicate = 0;
		// Check for duplicate packet
		duplicate = mac_sequence_is_duplicate();
		if(duplicate && original_dataptr[0]!=0) {
			// Drop the duplicates
			/*printf("emmac: drop duplicate link layer packet %u\n",
					packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));*/
		} else {
			mac_sequence_register_seqno();
		}

		if(!duplicate) {
			frame802154_t recieved_frame;
			frame_emmac_parse(original_dataptr, original_datalen, &recieved_frame);
			//			printf("dft:%u\n", recieved_frame.fcf.frame_type);
			if(recieved_frame.fcf.frame_type == FRAME802154_DATAFRAME &&
					(linkaddr_cmp((linkaddr_t *)&recieved_frame.dest_addr,
							&linkaddr_node_addr) || packetbuf_holds_broadcast())) {/*Si la trama recibida es de datos y si se requiere ACK y si la direccion del nodo es igual a la direccion de destino de la trama recibida*/
				uint8_t ackdata[ACK_LENGTH] = {0};
				//				uint8_t ackdata_test[ACK_LENGTH] = {0};
				packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, FRAME802154_ACKFRAME);
				int pos = frame_emmac_create_eb_ack(&ackdata, FRAME802154_ACKFRAME,
						packetbuf_attr(PACKETBUF_ATTR_PENDING),
						recieved_frame.seq,
						w_up_time,
						time_in_seconds,
						initial_rand_seed,
						w_up_ch,
						waiting_to_transmit);
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1); //Se activa el timestamp a nivel fisico
				NETSTACK_RADIO.send(ackdata, ACK_LENGTH);
				//printf("ACK sent %u\n", result);
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);/*Se desactiva el timestamp a nivel fisico*/
//				printf("stXtt:%u\n", packetbuf_attr(PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP));
//				update_neighbor_from_data_pkt(recieved_frame,
//						local_time_tics,
//						local_time_seconds);
			}
			//			printf("stXr:%04x %04x %04x %04x %04x %04x %04x\n",
			//					packetbuf_attr(PACKETBUF_ATTR_FRAME_TYPE),
			//					packetbuf_attr(PACKETBUF_ATTR_NODE_TIMESTAMP),
			//					packetbuf_attr(PACKETBUF_ATTR_NODE_CLOCK_TIME),
			//					packetbuf_attr(PACKETBUF_ATTR_NODE_RAND_SEED),
			//					packetbuf_attr(PACKETBUF_ATTR_NODE_BLACKLIST),
			//					packetbuf_attr(PACKETBUF_ATTR_NODE_CLOCK_TIME_SENT),
			//					packetbuf_attr(PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP));
			NETSTACK_MAC.input();
		}
	}
	incoming_packet=0;
	rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ 5, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
	//leds_off(2);
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
	//printf("f12\n");
	/* Channel Check Interval function. Without this, CSMA doesn't retransmit packets */
	return CLOCK_SECOND;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
	watchdog_stop();
	/*Se inicializan la lista y la herramienta para la reserva de memoria*/
	list_init(Neighbors);
	memb_init(&neighbor_memb);
	random_init(linkaddr_node_addr.u8[7]);
	initial_rand_seed=linkaddr_node_addr.u8[7];
	off(0);
	transmitting = 0;
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 26);
	generate_ch_list(&list_of_channels, linkaddr_node_addr.u8[7], 15);
	/*printf("Channel List: ");
	int i=0;
	for (i=0; i < 15; i++){
		printf("%u  ", list_of_channels[i]);
	}
	printf("\n");*/
	neighbor_discovery_flag=1;
	reception_powercycle();
	neighbor_discovery();
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver emmac_driver = {
		"emmac",
		init,
		qsend_packet,
		send_list,
		packet_input,
		on,
		off,
		channel_check_interval,
};
/*---------------------------------------------------------------------------*/
