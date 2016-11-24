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
#define BEACON_LENGTH						18
#define ACK_LENGTH							26
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
static int 	send_one_packet		(mac_callback_t sent, void *ptr, linkaddr_t receiver, int result);
static int 	qsend_packet			(mac_callback_t sent, void *ptr);//, linkaddr_t receiver, struct rdc_buf_list *buf_list);
static int	process_packet		(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list);
static void	delay_packet		(void);
static void	send_list			(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list);
static void	packet_input		(void);
static int	resync_neighbor		(mac_callback_t sent, linkaddr_t receiver, void *ptr, struct rdc_buf_list *buf_list);
static unsigned short channel_check_interval(void);
static void	init				(void);



/*---------------------------------------------------------------------------*/
/*Apaga el radio si se pasa 0 como parametro*/
static int
off(int keep_radio_on)
{
	if(keep_radio_on) {
		return NETSTACK_RADIO.on();
	} else {
		return NETSTACK_RADIO.off();
	}
}
/******************************************************************************/
/*Devuelve 1 si el radio esta encendido, 0 si esta apagado*/
static int check_if_radio_on(void)
{
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
	if(!check_if_radio_on()){
		return NETSTACK_RADIO.on();
	}
}
/****************************************************************/
static unsigned int get_w_up_advance(unsigned short failed_tx)
{
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
static void neighbor_discovery(void)
{
	uint8_t beacon_buf[ACK_LENGTH];
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

	timer_set(&w,((CLOCK_SECOND*15)));
	while(neighbor_discovery_flag){
		transmitting=0;
		/*while(NETSTACK_RADIO.receiving_packet()) {
			//printf("RXing PKT\n");
		}*/
		if(NETSTACK_RADIO.pending_packet()) {
			//printf("PENDING PACKET!\n");
			NETSTACK_RADIO.read(beacon_buf, ACK_LENGTH);
			local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
			local_time_seconds = clock_seconds();
			if((beacon_buf[0] & 7) == FRAME802154_BEACONFRAME && beacon_buf[2] == 64){
				//printf("ND BCN rx: %u\n", beacon_buf[2]);
				//Copiamos la direccion del beacon recibido dentro del buffer addr
				addr.u8[0]=beacon_buf[16];
				addr.u8[1]=beacon_buf[15];
				addr.u8[2]=beacon_buf[14];
				addr.u8[3]=beacon_buf[13];
				addr.u8[4]=beacon_buf[12];
				addr.u8[5]=beacon_buf[11];
				addr.u8[6]=beacon_buf[10];
				addr.u8[7]=beacon_buf[9];
				/*int i;
				for (i=0; i<7; i++){
					printf("%02x ", addr.u8[i]);
				}
				printf("\n");*/

				// The purpose of this while is to schedule neighbor transmissions according to the neighbor ID to avoid collisions of ACK packets from different neighbors
				unsigned int time=RTIMER_NOW();
				while((RTIMER_NOW() < (time + (linkaddr_node_addr.u8[7]-1)*(RTIMER_SECOND/1000)))){}
				/***********/
				frame802154_t ackframe;
				ackframe.fcf.frame_type = FRAME802154_BEACONFRAME;
				ackframe.fcf.frame_pending = packetbuf_attr(PACKETBUF_ATTR_PENDING);
				ackframe.fcf.timestamp_flag= 0;
				ackframe.fcf.rand_seed_flag = 0;
				ackframe.fcf.state_flag = 0;
				ackframe.fcf.ack_required = 0;
				ackframe.fcf.panid_compression = 0;
				ackframe.fcf.frame_version = FRAME802154_IEEE802154_2006;
				ackframe.fcf.security_enabled = 0;
				ackframe.fcf.src_addr_mode = FRAME802154_LONGADDRMODE;
				ackframe.fcf.dest_addr_mode = FRAME802154_SHORTADDRMODE;
				ackframe.dest_addr[0] = 0xFF;
				ackframe.dest_addr[1] = 0xFF;
				ackframe.dest_pid = IEEE802154_PANID;
				ackframe.src_pid = IEEE802154_PANID;
				linkaddr_copy((linkaddr_t *)&ackframe.src_addr, &linkaddr_node_addr);
				linkaddr_copy((linkaddr_t *)&ackframe.dest_addr, &addr);

				ackdata[0] = (ackframe.fcf.frame_type & 7) |
						((ackframe.fcf.security_enabled & 1) << 3) |
						((ackframe.fcf.frame_pending & 1) << 4) |
						((ackframe.fcf.ack_required & 1) << 5) |
						((ackframe.fcf.panid_compression & 1) << 6) |
						((ackframe.fcf.timestamp_flag & 1) << 7);
				ackdata[1] =((ackframe.fcf.rand_seed_flag & 1) << 0)|
						((ackframe.fcf.state_flag & 1) << 1) |
						((ackframe.fcf.dest_addr_mode & 3) << 2) |
						((ackframe.fcf.frame_version & 3) << 4) |
						((ackframe.fcf.src_addr_mode & 3) << 6);
				/* sequence number */
				ackframe.seq = 128;
				ackdata[2] = ackframe.seq;
				int ack_len = 3;
				/* Destination PAN ID */
				ackdata[ack_len++] = ackframe.dest_pid & 0xff;
				ackdata[ack_len++] = (ackframe.dest_pid >> 8) & 0xff;
				/* Destination address */
				int c;
				/* Source PAN ID */
				ackdata[ack_len++] = ackframe.src_pid & 0xff;
				ackdata[ack_len++] = (ackframe.src_pid >> 8) & 0xff;
				/* Source address */
				for(c = 8; c > 0; c--) {
					ackdata[ack_len++] = ackframe.src_addr[c - 1];
				}
				ackdata[ack_len++] = w_up_time%RTIMER_SECOND & 0xff; //w_up_time es el tiempo en tics del ultimo wake-up
				ackdata[ack_len++] = (w_up_time%RTIMER_SECOND>> 8) & 0xff;
				ackdata[ack_len++] = time_in_seconds & 0xff; //time_in_seconds es el tiempo en secs del ultimo wake-up
				ackdata[ack_len++] = (time_in_seconds>> 8) & 0xff;
				ackdata[ack_len++] = initial_rand_seed & 0xff;
				ackdata[ack_len++] = (initial_rand_seed>>8) & 0xff;
				t_seconds = clock_seconds();
				ackdata[ack_len++] = t_seconds & 0xff;
				ackdata[ack_len++] = (t_seconds>>8) & 0xff;
				ackdata[ack_len++] = w_up_ch;
				ackdata[ack_len++] = 0;  //Se deja vacio para colocar el tiempo actual en tics a nivel fisico
				ackdata[ack_len++] = 0;

				//printf("ack_len=%u\n", ack_len);
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1); //Se activa el timestamp a nivel fisico
				NETSTACK_RADIO.send(ackdata, ACK_LENGTH);
				//printf("ACK sent %u\n", result);
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);/*Se desactiva el timestamp a nivel fisico*/
			}
			else if((beacon_buf[0] & 7) == FRAME802154_BEACONFRAME && beacon_buf[2] == 128){
				//printf("ND ACK rx\n");
				sender_addr.u8[0] = beacon_buf[14];
				sender_addr.u8[1] = beacon_buf[13];
				sender_addr.u8[2] = beacon_buf[12];
				sender_addr.u8[3] = beacon_buf[11];
				sender_addr.u8[4] = beacon_buf[10];
				sender_addr.u8[5] = beacon_buf[9];
				sender_addr.u8[6] = beacon_buf[8];
				sender_addr.u8[7] = beacon_buf[7];
				/*Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales*/
				w_time= beacon_buf[15]+ (beacon_buf[16] << 8); 		// Tiempo de wake-up en tics
				t_sec=beacon_buf[17]+ (beacon_buf[18] << 8);  		// Tiempo de wake-up en seg
				last_generated=beacon_buf[19]+ (beacon_buf[20] << 8);	// Última semilla del generador
				c_t_sec = beacon_buf[21]+ (beacon_buf[22] << 8); 	//tiempo actual en seconds
				uint8_t last_ch = beacon_buf[23];					// last visited channel
				t_tic=(beacon_buf[24]+ (beacon_buf[25] << 8))%RTIMER_SECOND; 		//tiempo actual en tics
				if (sender_addr.u8[7]!=0 && sender_addr.u8[7]<100){
					//printf("Inside if\n");
					/* Update the state information for the node that sent the ACK */
					for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
						if(linkaddr_cmp(&sender_addr, &n->node_link_addr)) {
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
						if(n == NULL) {
							break; 									// We could not allocate memory for this encounter, so we just drop it.
						}
						linkaddr_copy(&n->node_link_addr, &sender_addr);
						n->wake_time_tics=w_time;
						n->wake_time_seconds=t_sec;
						n->last_seed=last_generated;
						n->last_channel=last_ch;
						n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
						n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
						n->consecutive_failed_tx=0;
						list_add(Neighbors, n);
						//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
						//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->d_tics, local_time_seconds, c_t_sec, n->d_secs);
					}
				}
				else{
					//printf("Outside if: %02x %u\n", sender_addr.u8[7], sender_addr.u8[7]);
				}
			}
			else {
				/*int i;
				printf("Unknown rx:");
				for (i=0; i < ack_len; i++){
					printf("%02x ", ackdata[i]);
				}
				printf("\n");*/
				//printf("Unknown type: %02x\n", (beacon_buf[0] & 7));
			}
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
static char reception_powercycle(void)
{
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
		frame802154_t beaconframe;
		beaconframe.fcf.frame_type = FRAME802154_BEACONFRAME;
		beaconframe.fcf.frame_pending = packetbuf_attr(PACKETBUF_ATTR_PENDING);
		beaconframe.fcf.timestamp_flag= packetbuf_attr( PACKETBUF_ATTR_NODE_TIMESTAMP_FLAG);
		beaconframe.fcf.rand_seed_flag = packetbuf_attr( PACKETBUF_ATTR_NODE_RAND_SEED_FLAG);
		beaconframe.fcf.state_flag =packetbuf_attr( PACKETBUF_ATTR_NODE_STATE_FLAG);
		beaconframe.fcf.ack_required = 1;
		beaconframe.fcf.panid_compression = 0;
		beaconframe.fcf.frame_version = FRAME802154_IEEE802154_2006;
		beaconframe.fcf.security_enabled = 0;
		beaconframe.fcf.src_addr_mode = FRAME802154_LONGADDRMODE;
		beaconframe.fcf.dest_addr_mode = FRAME802154_SHORTADDRMODE;
		beaconframe.dest_addr[0] = 0xFF;
		beaconframe.dest_addr[1] = 0xFF;
		beaconframe.dest_pid = IEEE802154_PANID;
		beaconframe.src_pid = IEEE802154_PANID;
		linkaddr_copy((linkaddr_t *)&beaconframe.src_addr, &linkaddr_node_addr);

		beacon_data[0] = (beaconframe.fcf.frame_type & 7) |
				((beaconframe.fcf.security_enabled & 1) << 3) |
				((beaconframe.fcf.frame_pending & 1) << 4) |
				((beaconframe.fcf.ack_required & 1) << 5) |
				((beaconframe.fcf.panid_compression & 1) << 6) |
				((beaconframe.fcf.timestamp_flag & 1) << 7);
		beacon_data[1] =((beaconframe.fcf.rand_seed_flag & 1) << 0)|
				((beaconframe.fcf.state_flag & 1) << 1) |
				((beaconframe.fcf.dest_addr_mode & 3) << 2) |
				((beaconframe.fcf.frame_version & 3) << 4) |
				((beaconframe.fcf.src_addr_mode & 3) << 6);
		/* sequence number */
		beaconframe.seq = 64;
		beacon_data[2] = beaconframe.seq;
		int pos = 3;

		/* Destination PAN ID */
		beacon_data[pos++] = beaconframe.dest_pid & 0xff;
		beacon_data[pos++] = (beaconframe.dest_pid >> 8) & 0xff;

		/* Destination address */
		int c;
		for(c = 2; c > 0; c--) {
			beacon_data[pos++] = beaconframe.dest_addr[c - 1];
		}

		/* Source PAN ID */
		beacon_data[pos++] = beaconframe.src_pid & 0xff;
		beacon_data[pos++] = (beaconframe.src_pid >> 8) & 0xff;

		//printf("pos=%u\n", pos);
		/* Source address */
		for(c = 8; c > 0; c--) {
			beacon_data[pos++] = beaconframe.src_addr[c - 1];
		}
		//printf("pos=%u\n", pos);
		beacon_data[pos++] = waiting_to_transmit;


		if (transmitting == waiting_to_transmit || syncing){
			//beacon_data[1]=waiting_to_transmit;
			while(NETSTACK_RADIO.channel_clear() == 0){}
			NETSTACK_RADIO.send(beacon_data, pos);
			//printf("PC BCN tx\n");
			if (syncing){
				NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 26);
			}
		}
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

		if (clock_seconds()%900 < 2){ // 15mins
			/*printf("succ:%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
					succ_beacon, beacon_failed_sync+beacon_failed_tx, beacon_failed_sync, beacon_failed_tx,
					successful, failed, failed_COL, fail_ACK, failed_DEF, failed_ERR, failed_try_again, successful_after_try_again);
			printf("succ_B_PDR=%u  validB_PDR=%u  PDR=%u\n"
					"succ_b:%u b_fail:%u b_fail_s:%u b_fail_tx:%u\n"
					"succ_:%u fail:%u f_COL:%u f_ACK:%u f_DEF:%u f_ERR:%u,%u,%u\n",
					(succ_beacon*100)/(succ_beacon+beacon_failed_sync), (succ_beacon*100)/(succ_beacon+beacon_failed_sync+beacon_failed_tx), (successful*100)/(successful+failed),
					succ_beacon, beacon_failed_sync+beacon_failed_tx, beacon_failed_sync, beacon_failed_tx,
					successful, failed, failed_COL, fail_ACK, failed_DEF, failed_ERR, failed_try_again, successful_after_try_again);*/
		}
		receiving=0;
		rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ RTIMER_SECOND/10000, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		PT_YIELD(&pt);
	}
	PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
static int send_one_packet(mac_callback_t sent, void *ptr, linkaddr_t receiver, int result)
{
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
	uint8_t beaconbuf[ACK_LENGTH];
	NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
	wt = RTIMER_NOW();
	int wait_for_neighbor=2*time_in_advance;
	if (result==3){
		wait_for_neighbor=RTIMER_SECOND/82;  //If the nodes did rendezvous nut there was a collision, then we wait a shorter time for the ACK
	}
	while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + wait_for_neighbor) && beacon_received==0) {  //It should be "wt + Time_from_Chasing_algorithm but for now we just use 40ms (remember in Cooja the time is twice the real time)
		if(NETSTACK_RADIO.pending_packet()) {
			//printf("pdng_pkt_1\n");
			int len = NETSTACK_RADIO.read(beaconbuf, ACK_LENGTH);
			//printf("len:%u %u\n", len, (beaconbuf[0] & 7));
			if ((len == BEACON_LENGTH || len == ACK_LENGTH) && ((beaconbuf[0] & 7) == FRAME802154_BEACONFRAME || (beaconbuf[0] & 7) == FRAME802154_ACKFRAME)){
				int addr_start_byte = 16;

				if (((beaconbuf[0] & 7) == FRAME802154_BEACONFRAME) && len == ACK_LENGTH){
					//printf("ACKFRAME\n");
					addr_start_byte = 14;
					beacon_received = 1;
					//break;
				}
				linkaddr_t beacon_src_addr;
				beacon_src_addr.u8[0] = beaconbuf[addr_start_byte--];
				beacon_src_addr.u8[1] = beaconbuf[addr_start_byte--];
				beacon_src_addr.u8[2] = beaconbuf[addr_start_byte--];
				beacon_src_addr.u8[3] = beaconbuf[addr_start_byte--];
				beacon_src_addr.u8[4] = beaconbuf[addr_start_byte--];
				beacon_src_addr.u8[5] = beaconbuf[addr_start_byte--];
				beacon_src_addr.u8[6] = beaconbuf[addr_start_byte--];
				beacon_src_addr.u8[7] = beaconbuf[addr_start_byte--];
				if(linkaddr_cmp(&beacon_src_addr, &receiver)){
					good_beacon=1;
					//printf("rx1\n");
					//printf("beaconbuf[1]=%u\n", beaconbuf[1]);
					if ((len == BEACON_LENGTH) && (beaconbuf[17]==1)){
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
			//printf("RES=%d_%u RxNotFound _s_ ", 1, receiver.u8[7]);
			if (result==3){
				//printf(" _c_ ");
			} else {
				beacon_failed_sync++;
			}
			//printf("\n");
			/*printf("\n_s_wait:%d,(s),%u %u %u %u %u %u %u %u\n",
					receiver.u8[7], iteration_out[0], iteration_out[1], iteration_out[2],
					iteration_out[3], iteration_out[4],
					RTIMER_NOW(), clock_seconds(), iteration_out[5]);*/
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
		//printf("is_broadcast or BR= %u\n", beacon_received);
		return is_broadcast;
	}else{
		succ_beacon++;
		/* Backoff based on the node ID:
		 * RTIMER_SECOND/400: is the time required to transmit a message.*/
		wt=RTIMER_NOW();
		time_to_send = (unsigned int)(10 + random_rand()%(unsigned int)(RTIMER_SECOND/489)); // 77 tics is the time required to transmit a maximum length frame. We create a mninimum of 10 tics before ttransmission.
		//printf("%u\n",time_to_send);
		while(RTIMER_NOW() < wt+time_to_send){}
	}
	/***************************************************************************************/

	int ret;
	int last_sent_ok = 0;
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK,1);
	packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 1);
	//ack_len=ACK_LENGTH;

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
			//printf("RES=3_%u\n", receiver.u8[7]);
			failed_try_again++;
			return 3;
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
										//printf("ack check 1\n");
										//Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales
										local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
										local_time_seconds = clock_seconds();
										w_time= ackbuf[15]+ (ackbuf[16] << 8); // Tiempo de wake-up en tics
										t_sec=ackbuf[17]+ (ackbuf[18] << 8);  // Tiempo de wake-up en seg
										unsigned int last_generated=ackbuf[19]+ (ackbuf[20] << 8);  // Última semilla del generador
										unsigned int c_t_sec = ackbuf[21]+ (ackbuf[22] << 8); //tiempo actual en seconds
										uint8_t last_ch = ackbuf[23];			// last visited channel
										t_tic=(ackbuf[24]+ (ackbuf[25] << 8))%RTIMER_SECOND; //tiempo actual en tics
										//Sobreescribimos o escribimos la informacion de estado correspondiente al vecino en cuestion
										//printf("list of Neighbors: ");
										for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
											if(linkaddr_cmp(&receiver, &n->node_link_addr)) {
												n->wake_time_tics=w_time;
												n->wake_time_seconds=t_sec;
												n->last_seed=last_generated;
												n->last_channel=last_ch;
												n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
												n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
												n->consecutive_failed_tx=0;
												//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
												//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->n, local_time_seconds, c_t_sec, n->m);
												break;
											}
										}
										// No matching encounter was found, so we allocate a new one.
										if(n == NULL) {
											//printf("Neigh not found. ");
											n = memb_alloc(&neighbor_memb);
											if(n == NULL) {
												// We could not allocate memory for this encounter, so we just drop it.
												break;
											}
											//printf("Neigh added %d\n");
											linkaddr_copy(&n->node_link_addr, &receiver);

											n->wake_time_tics=w_time;
											n->wake_time_seconds=t_sec;
											n->last_seed=last_generated;
											n->last_channel=last_ch;
											n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
											n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
											n->consecutive_failed_tx=0;
											list_add(Neighbors, n);
											//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
											//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->n, local_time_seconds, c_t_sec, n->m);
										}
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
				//printf("RES=3_%u\n", receiver.u8[7]);
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
	off(0);
	transmitting = 0;
	//leds_off(7);
	//leds_blink();
	return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static int resync_neighbor(mac_callback_t sent, linkaddr_t receiver, void *ptr, struct rdc_buf_list *buf_list){
	syncing=1;
	timer_set(&w,((CLOCK_SECOND*45))); // 48s = 2 * 15channels * (TmaxInterval==1.5s)
	uint8_t beaconbuf[BEACON_LENGTH];
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
			int len=NETSTACK_RADIO.read(beaconbuf, BEACON_LENGTH);
			if ((beaconbuf[0] & 7) == FRAME802154_BEACONFRAME){
				//printf("sbrx\n");
				linkaddr_t beacon_src_addr;
				beacon_src_addr.u8[0] = beaconbuf[16];
				beacon_src_addr.u8[1] = beaconbuf[15];
				beacon_src_addr.u8[2] = beaconbuf[14];
				beacon_src_addr.u8[3] = beaconbuf[13];
				beacon_src_addr.u8[4] = beaconbuf[12];
				beacon_src_addr.u8[5] = beaconbuf[11];
				beacon_src_addr.u8[6] = beaconbuf[10];
				beacon_src_addr.u8[7] = beaconbuf[9];
				if(linkaddr_cmp(&beacon_src_addr, &receiver)){
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
											unsigned int local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
											unsigned int local_time_seconds = clock_seconds();
											if(len == ACK_LENGTH && ackbuf[2] == dsn) {
												// ACK received
												//printf("ack check\n");
												if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){
													//printf("ack check 1\n");
													//Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales
													unsigned int w_time= ackbuf[15]+ (ackbuf[16] << 8); // Tiempo de wake-up en tics
													unsigned int t_sec=ackbuf[17]+ (ackbuf[18] << 8);  // Tiempo de wake-up en seg
													unsigned int last_generated=ackbuf[19]+ (ackbuf[20] << 8);  // Última semilla del generador
													unsigned int c_t_sec = ackbuf[21]+ (ackbuf[22] << 8); //tiempo actual en seconds
													uint8_t last_ch = ackbuf[23];			// last visited channel
													unsigned int t_tic=(ackbuf[24]+ (ackbuf[25] << 8))%RTIMER_SECOND; //tiempo actual en tics
													//Sobreescribimos o escribimos la informacion de estado correspondiente al vecino en cuestion
													//printf("list of Neighbors: ");
													for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
														if(linkaddr_cmp(&receiver, &n->node_link_addr)) {
															n->wake_time_tics=w_time;
															n->wake_time_seconds=t_sec;
															n->last_seed=last_generated;
															n->last_channel=last_ch;
															n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
															n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
															n->consecutive_failed_tx=0;
															//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
															//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->n, local_time_seconds, c_t_sec, n->m);
															break;
														}
													}
													// No matching encounter was found, so we allocate a new one.
													if(n == NULL) {
														//printf("Neigh not found. ");
														n = memb_alloc(&neighbor_memb);
														if(n == NULL) {
															// We could not allocate memory for this encounter, so we just drop it.
															break;
														}
														//printf("Neigh added %d\n");
														linkaddr_copy(&n->node_link_addr, &receiver);

														n->wake_time_tics=w_time;
														n->wake_time_seconds=t_sec;
														n->last_seed=last_generated;
														n->last_channel=last_ch;
														n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
														n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
														n->consecutive_failed_tx=0;
														list_add(Neighbors, n);
														//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
														//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->n, local_time_seconds, c_t_sec, n->m);
													}
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
static int send_packet(mac_callback_t sent, void *ptr, linkaddr_t receiver, struct rdc_buf_list *buf_list)
{
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
	sent_times=0;
	do{
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
		uint8_t beaconbuf[ACK_LENGTH];
		NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
		wt = RTIMER_NOW();
		int wait_for_neighbor=2*time_in_advance;
		if (result==3){
			wait_for_neighbor=RTIMER_SECOND/82;  //If the nodes did rendezvous nut there was a collision, then we wait a shorter time for the ACK
		}
		while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + wait_for_neighbor) && beacon_received==0) {  //It should be "wt + Time_from_Chasing_algorithm but for now we just use 40ms (remember in Cooja the time is twice the real time)
			if(NETSTACK_RADIO.pending_packet()) {
				//printf("pdng_pkt_1\n");
				int len = NETSTACK_RADIO.read(beaconbuf, ACK_LENGTH);
				//printf("len:%u %u\n", len, (beaconbuf[0] & 7));
				if ((len == BEACON_LENGTH || len == ACK_LENGTH) && ((beaconbuf[0] & 7) == FRAME802154_BEACONFRAME || (beaconbuf[0] & 7) == FRAME802154_ACKFRAME)){
					int addr_start_byte = 16;

					if (((beaconbuf[0] & 7) == FRAME802154_BEACONFRAME) && len == ACK_LENGTH){
						//printf("ACKFRAME\n");
						addr_start_byte = 14;
						beacon_received = 1;
						//break;
					}
					linkaddr_t beacon_src_addr;
					beacon_src_addr.u8[0] = beaconbuf[addr_start_byte--];
					beacon_src_addr.u8[1] = beaconbuf[addr_start_byte--];
					beacon_src_addr.u8[2] = beaconbuf[addr_start_byte--];
					beacon_src_addr.u8[3] = beaconbuf[addr_start_byte--];
					beacon_src_addr.u8[4] = beaconbuf[addr_start_byte--];
					beacon_src_addr.u8[5] = beaconbuf[addr_start_byte--];
					beacon_src_addr.u8[6] = beaconbuf[addr_start_byte--];
					beacon_src_addr.u8[7] = beaconbuf[addr_start_byte--];
					if(linkaddr_cmp(&beacon_src_addr, &receiver)){
						good_beacon=1;
						//printf("rx1\n");
						//printf("beaconbuf[1]=%u\n", beaconbuf[1]);
						if ((len == BEACON_LENGTH) && (beaconbuf[17]==1)){
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
				//printf("RES=%d_%u RxNotFound _s_ ", 1, receiver.u8[7]);
				if (result==3){
					//printf(" _c_ ");
				} else {
					beacon_failed_sync++;
				}
				//printf("\n");
				/*printf("\n_s_wait:%d,(s),%u %u %u %u %u %u %u %u\n",
						receiver.u8[7], iteration_out[0], iteration_out[1], iteration_out[2],
						iteration_out[3], iteration_out[4],
						RTIMER_NOW(), clock_seconds(), iteration_out[5]);*/
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
			//printf("is_broadcast or BR= %u\n", beacon_received);
			return is_broadcast;
		}else{
			succ_beacon++;
			/* Backoff based on the node ID:
			 * RTIMER_SECOND/400: is the time required to transmit a message.*/
			wt=RTIMER_NOW();
			time_to_send = (unsigned int)(10 + random_rand()%(unsigned int)(RTIMER_SECOND/489)); // 77 tics is the time required to transmit a maximum length frame. We create a mninimum of 10 tics before ttransmission.
			//printf("%u\n",time_to_send);
			while(RTIMER_NOW() < wt+time_to_send){}
		}
		/***************************************************************************************/

		int ret;
		int last_sent_ok = 0;
		packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
		packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK,1);
		packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 1);
		//ack_len=ACK_LENGTH;

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
				//printf("RES=3_%u CCA\n", receiver.u8[7]);
				failed_try_again++;
				return 3;
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
											//printf("ack check 1\n");
											//Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales
											local_time_tics = RTIMER_NOW()%RTIMER_SECOND;
											local_time_seconds = clock_seconds();
											w_time= ackbuf[15]+ (ackbuf[16] << 8); // Tiempo de wake-up en tics
											t_sec=ackbuf[17]+ (ackbuf[18] << 8);  // Tiempo de wake-up en seg
											unsigned int last_generated=ackbuf[19]+ (ackbuf[20] << 8);  // Última semilla del generador
											unsigned int c_t_sec = ackbuf[21]+ (ackbuf[22] << 8); //tiempo actual en seconds
											uint8_t last_ch = ackbuf[23];			// last visited channel
											t_tic=(ackbuf[24]+ (ackbuf[25] << 8))%RTIMER_SECOND; //tiempo actual en tics
											//Sobreescribimos o escribimos la informacion de estado correspondiente al vecino en cuestion
											//printf("list of Neighbors: ");
											for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
												if(linkaddr_cmp(&receiver, &n->node_link_addr)) {
													n->wake_time_tics=w_time;
													n->wake_time_seconds=t_sec;
													n->last_seed=last_generated;
													n->last_channel=last_ch;
													n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
													n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
													n->consecutive_failed_tx=0;
													//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
													//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->n, local_time_seconds, c_t_sec, n->m);
													break;
												}
											}
											// No matching encounter was found, so we allocate a new one.
											if(n == NULL) {
												//printf("Neigh not found. ");
												n = memb_alloc(&neighbor_memb);
												if(n == NULL) {
													// We could not allocate memory for this encounter, so we just drop it.
													break;
												}
												//printf("Neigh added %d\n");
												linkaddr_copy(&n->node_link_addr, &receiver);

												n->wake_time_tics=w_time;
												n->wake_time_seconds=t_sec;
												n->last_seed=last_generated;
												n->last_channel=last_ch;
												n->d_tics=(int)((int)(local_time_tics)-(int)(t_tic));
												n->d_secs=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
												n->consecutive_failed_tx=0;
												list_add(Neighbors, n);
												//printf("w_t:%u w_s:%u l_t:%u t_t:%u d_t:%d l_s:%u t_s:%u d_s:%d\n",
												//		n->wake_time_tics, n->wake_time_seconds, local_time_tics, t_tic, n->n, local_time_seconds, c_t_sec, n->m);
											}
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
		off(0);
		transmitting = 0;
		//leds_off(7);
		//leds_blink();
		return last_sent_ok;
		NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
	} while (result==3);
	return result;
}
/*---------------------------------------------------------------------------*/
/*static int process_packet(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
	transmitting = 1;
	waiting_to_transmit = 1;
	// We backup the next pointer, as it may be nullified by
	// mac_call_sent_callback()
	int last_sent_ok=1;
	queuebuf_to_packetbuf(buf_list->buf);
	static  linkaddr_t addr_receiver;
	linkaddr_copy(&addr_receiver,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	if (!packetbuf_holds_broadcast()){
		last_sent_ok = send_packet(sent, ptr, addr_receiver, buf_list);
	}else {
		neighbor_state *neighbor_broadcast_to_unicast=list_head(Neighbors);
		while(neighbor_broadcast_to_unicast != NULL && last_sent_ok) {
			transmitting = 1;
			waiting_to_transmit = 1;
			last_sent_ok = send_packet(sent, ptr, neighbor_broadcast_to_unicast->node_link_addr, buf_list);
			neighbor_broadcast_to_unicast = list_item_next(neighbor_broadcast_to_unicast);
		}
	}
	return last_sent_ok;
}*/
/*---------------------------------------------------------------------------*/
static int qsend_packet(mac_callback_t sent_callback, void *ptr){
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
		last_sent_ok = send_packet(sent_callback, ptr, addr_receiver, buf_list);
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
static void delay_packet(void){
	int i=0;
	while (i<pkts_rxed_w_tx){
		//printf("Txcall_%d\n", i);
		send_list(delay_arg[i].sent, delay_arg[i].ptr, delay_arg[i].buf_list);
		i++;
	}
}
/*---------------------------------------------------------------------------*/
static void send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
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
static void packet_input(void)
{
	//printf("packet_input\n");
	//leds_on(2);
	incoming_packet=1;
	unsigned int wake_up_time;
	unsigned int t_seconds;
	int original_datalen;
	uint8_t *original_dataptr;
	/*original_datalen y original_dataptr are necessary for making the ACK*/
	original_datalen = packetbuf_datalen();
	original_dataptr = packetbuf_dataptr();

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
			if(recieved_frame.fcf.frame_type == FRAME802154_DATAFRAME &&
					(linkaddr_cmp((linkaddr_t *)&recieved_frame.dest_addr,
							&linkaddr_node_addr) || packetbuf_holds_broadcast())) {/*Si la trama recibida es de datos y si se requiere ACK y si la direccion del nodo es igual a la direccion de destino de la trama recibida*/
				uint8_t ackdata[ACK_LENGTH] = {0};
				frame802154_t ackframe;
				ackframe.fcf.frame_type = FRAME802154_BEACONFRAME;
				ackframe.fcf.frame_pending = packetbuf_attr(PACKETBUF_ATTR_PENDING);
				ackframe.fcf.timestamp_flag= 1;
				ackframe.fcf.rand_seed_flag = 1;
				ackframe.fcf.state_flag = 1;
				ackframe.fcf.ack_required = 0;
				ackframe.fcf.panid_compression = 0;
				ackframe.fcf.frame_version = FRAME802154_IEEE802154_2006;
				ackframe.fcf.security_enabled = 0;
				ackframe.fcf.src_addr_mode = FRAME802154_LONGADDRMODE;
				ackframe.fcf.dest_addr_mode = FRAME802154_SHORTADDRMODE;
				ackframe.dest_addr[0] = 0xFF;
				ackframe.dest_addr[1] = 0xFF;
				ackframe.dest_pid = IEEE802154_PANID;
				ackframe.src_pid = IEEE802154_PANID;
				linkaddr_copy((linkaddr_t *)&ackframe.src_addr, &linkaddr_node_addr);
				//linkaddr_copy((linkaddr_t *)&ackframe.dest_addr, &addr);

				ackdata[0] = (ackframe.fcf.frame_type & 7) |
						((ackframe.fcf.security_enabled & 1) << 3) |
						((ackframe.fcf.frame_pending & 1) << 4) |
						((ackframe.fcf.ack_required & 1) << 5) |
						((ackframe.fcf.panid_compression & 1) << 6) |
						((ackframe.fcf.timestamp_flag & 1) << 7);
				ackdata[1] =((ackframe.fcf.rand_seed_flag & 1) << 0)|
						((ackframe.fcf.state_flag & 1) << 1) |
						((ackframe.fcf.dest_addr_mode & 3) << 2) |
						((ackframe.fcf.frame_version & 3) << 4) |
						((ackframe.fcf.src_addr_mode & 3) << 6);
				/* sequence number */
				ackframe.seq = recieved_frame.seq;
				ackdata[2] = ackframe.seq;
				int ack_len = 3;
				/* Destination PAN ID */
				ackdata[ack_len++] = ackframe.dest_pid & 0xff;
				ackdata[ack_len++] = (ackframe.dest_pid >> 8) & 0xff;
				/* Destination address */
				int c;
				/* Source PAN ID */
				ackdata[ack_len++] = ackframe.src_pid & 0xff;
				ackdata[ack_len++] = (ackframe.src_pid >> 8) & 0xff;
				/* Source address */
				for(c = 8; c > 0; c--) {
					ackdata[ack_len++] = ackframe.src_addr[c - 1];
				}
				ackdata[ack_len++] = w_up_time%RTIMER_SECOND & 0xff; //w_up_time es el tiempo en tics del ultimo wake-up
				ackdata[ack_len++] = (w_up_time%RTIMER_SECOND>> 8) & 0xff;
				ackdata[ack_len++] = time_in_seconds & 0xff; //time_in_seconds es el tiempo en secs del ultimo wake-up
				ackdata[ack_len++] = (time_in_seconds>> 8) & 0xff;
				ackdata[ack_len++] = initial_rand_seed & 0xff;
				ackdata[ack_len++] = (initial_rand_seed>>8) & 0xff;
				t_seconds = (unsigned int)(clock_seconds());
				ackdata[ack_len++] = t_seconds & 0xff;
				ackdata[ack_len++] = (t_seconds>>8) & 0xff;
				ackdata[ack_len++] = w_up_ch;
				ackdata[ack_len++] = 0;  //Se deja vacio para colocar el tiempo actual en tics a nivel fisico
				ackdata[ack_len++] = 0;

				//printf("ack_len=%u\n", ack_len);
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1); //Se activa el timestamp a nivel fisico
				NETSTACK_RADIO.send(ackdata, ACK_LENGTH);
				//printf("ACK sent %u\n", result);
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);/*Se desactiva el timestamp a nivel fisico*/
				//printf("RX:%d\n", recieved_frame.seq);
				/*if(recieved_frame.fcf.state_flag){
					//Entramos aqui si el ACK requiere la informacion de estado del nodo
					ack_len=14;

					ackdata[0] = FRAME802154_ACKFRAME;
					ackdata[1] = 0;
					ackdata[2] = recieved_frame.seq;
					ackdata[3] = w_up_time%RTIMER_SECOND & 0xff; //w_up_time es el tiempo en tics del ultimo wake-up
					ackdata[4] = (w_up_time%RTIMER_SECOND>> 8) & 0xff;
					ackdata[5] = time_in_seconds & 0xff;//time_in_seconds es el tiempo en secs del ultimo wake-up
					ackdata[6] = (time_in_seconds>> 8) & 0xff;
					ackdata[7] = initial_rand_seed & 0xff;
					ackdata[8] = (initial_rand_seed>>8) & 0xff;
					ackdata[9] = t_seconds & 0xff;
					ackdata[10] = (t_seconds>>8) & 0xff;
					ackdata[11] = w_up_ch;
					ackdata[12] = 0;//Se deja vacio para colocar el tiempo actual en tics a nivel fisico
					ackdata[13] = 0;
					linkaddr_t test;
					linkaddr_copy(&test,(linkaddr_t *)&recieved_frame.src_addr);
					while (!NETSTACK_RADIO.channel_clear()){}
					packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1);//Se activa el timestamp a nivel fisico
					int resack = NETSTACK_RADIO.send(ackdata, ack_len);
					printf("resack:%u\n", resack);
					packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);//Se desactiva el timestamp a nivel fisico
				}
				else{
					//Entramos aqui si el ACK no requiere la informacion de estado del nodo
					ack_len=3;
					ackdata[0] = FRAME802154_ACKFRAME;
					ackdata[1] = 0;
					ackdata[2] = recieved_frame.seq;
					while (!NETSTACK_RADIO.channel_clear()){}
					NETSTACK_RADIO.send(ackdata, ack_len);
				}*/

			}
			NETSTACK_MAC.input();
		}
	}
	incoming_packet=0;
	rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ 5, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
	//leds_off(2);
}
/*---------------------------------------------------------------------------*/
static unsigned short channel_check_interval(void)
{
	/* Channel Check Interval function. Without this, CSMA doesn't retransmit packets */
	return CLOCK_SECOND;
}
/*---------------------------------------------------------------------------*/
static void init(void)
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
