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

#define ACK_WAIT_TIME                      RTIMER_SECOND / 150/*Podria ser hasta RTIMER/900, para valores mayores que 900 el tx no recibe el ACK*/
#define AFTER_ACK_DETECTED_WAIT_TIME       RTIMER_SECOND / 250
#define DATA_PACKET_WAIT_TIME               RTIMER_SECOND / 500
//Values of the reception state

static struct pt pt;
static struct rtimer reciever_powercycle_timer;
static struct timer w;
static unsigned int initial_rand_seed;
static unsigned int initial_rand_seed_temp;
static unsigned int blacklist;
static unsigned short ack_len;
static unsigned short neighbor_discovery_flag;
static unsigned int w_up_time;
static unsigned time_in_seconds;
static unsigned int sequence_number=0;
uint8_t dummy_buf_to_flush_rxfifo[1];
uint8_t current_channel=0;
uint8_t w_up_ch = 26;
int transmitting=0;
int waiting_to_transmit=0;
unsigned int list_of_channels[16]={0};
static unsigned int succ_beacon=0;
static unsigned int successful=0;
static unsigned int failed=0;
static unsigned int beacon_failed_sync=0;
static unsigned int beacon_failed_tx=0;
static unsigned int failed_COL=0;
static unsigned int fail_ACK=0;
static unsigned int failed_DEF=0;
static unsigned int failed_ERR=0;
#define MEMB_SIZE 4
MEMB(neighbor_memb, neighbor_state, MEMB_SIZE);
LIST(Neighbors);

/*---------------------------------------------------------------------------*/
static int 	off					(int keep_radio_on);
static int 	check_if_radio_on	(void);
static void	neighbor_discovery	(void);
static char	reception_powercycle(void);
static int 	send_one_packet		(mac_callback_t sent, void *ptr, linkaddr_t receiver);
static int 	send_packet			(mac_callback_t sent, void *ptr, linkaddr_t receiver);
static void	send_list			(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list);
static void	packet_input		(void);
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
/********************************************************************************/
/*Proceso de descubrimiento de vecinos que se lleva a cabo en la funcion init()*/
/* TODO Creo que los vecinos deberían mandar la info desde ahora  porque ya el powercycle está corriendo y esa info existe */
static void neighbor_discovery(void)
{
	uint8_t beacon_buf[22];
	static  linkaddr_t addr; 		// This one is used to store the sender of the Beacons
	static linkaddr_t sender_addr; 	// This one is used to store the sender of the ACKs
	neighbor_discovery_flag=1;
	unsigned int wt;
	neighbor_state *n;
	uint8_t ackdata[22] = {0};
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
		/*if(NETSTACK_RADIO.receiving_packet() ||
				NETSTACK_RADIO.pending_packet() ||
				NETSTACK_RADIO.channel_clear() == 0){

			wt=RTIMER_NOW();

			/*Si se cumple que estamos recibiendo un paquete o hay un paquete recibido pendiente a ser leido o hay un paquete
		 * en el aire entonces esperamos un tiempo AFTER_ACK_DETECTED_WAIT_TIME luego del cual leemos el paquete*/

		//while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {} /*Mientras que RTIMER_NOW() sea menor que wt + AFTER_ACK_DETECTED_WAIT_TIME. */
		if(NETSTACK_RADIO.pending_packet()) {
			NETSTACK_RADIO.read(beacon_buf, 22);
			local_time_tics = RTIMER_NOW();
			local_time_seconds = clock_seconds();
			if(beacon_buf[0]==FRAME802154_BEACONFRAME)
			{
				/*Copiamos la direccion del beacon recibido dentro del buffer addr*/
				addr.u8[0]=beacon_buf[4];
				addr.u8[1]=beacon_buf[5];
				addr.u8[2]=beacon_buf[6];
				addr.u8[3]=beacon_buf[7];
				addr.u8[4]=beacon_buf[8];
				addr.u8[5]=beacon_buf[9];
				addr.u8[6]=beacon_buf[10];
				addr.u8[7]=beacon_buf[11];

				/* If we have an entry for this neighbor already, we renew it. */   //WHY?!?!!?!?!?!?!?!?!??***************************
				if (addr.u8[7]!=0){
					for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
						if(linkaddr_cmp(&addr, &n->node_link_addr)) {
							break;
						}
					}
					/* No matching encounter was found, so we allocate a new one. */
					if(n == NULL) {
						n = memb_alloc(&neighbor_memb);
						if(n == NULL) {
							/* We could not allocate memory for this encounter, so we just drop it. */
							return;
						}
						linkaddr_copy(&n->node_link_addr, &addr);
						list_add(Neighbors, n);
					}
				}
				/* The purpose of this while is to schedule neighbor transmissions according to the neighbor ID
				 * to avoid collisions of ACK packets from different neighbors *******************************/
				unsigned int time=RTIMER_NOW();
				while((RTIMER_NOW() < (time + (linkaddr_node_addr.u8[7]-1)*(RTIMER_SECOND/1500)))){}
				/***********/
				ack_len=22;
				t_seconds=clock_seconds();
				ackdata[0] = FRAME802154_ACKFRAME;
				ackdata[1] = 0;
				ackdata[2] = 0;  // Here goes the sequence number but it shouldn't be needed in neighbor discovery
				ackdata[3] = w_up_time & 0xff; /*w_up_time es el tiempo en tics del ultimo wake-up*/
				ackdata[4] = (w_up_time>> 8) & 0xff;
				ackdata[5] = time_in_seconds & 0xff;/*time_in_seconds es el tiempo en secs del ultimo wake-up*/
				ackdata[6] = (time_in_seconds>> 8) & 0xff;
				ackdata[7] = initial_rand_seed & 0xff;
				ackdata[8] = (initial_rand_seed>>8) & 0xff;
				ackdata[9] = t_seconds & 0xff;
				ackdata[10] = (t_seconds>>8) & 0xff;
				ackdata[11] = w_up_ch;
				ackdata[12] = linkaddr_node_addr.u8[0];
				ackdata[13] = linkaddr_node_addr.u8[1];
				ackdata[14] = linkaddr_node_addr.u8[2];
				ackdata[15] = linkaddr_node_addr.u8[3];
				ackdata[16] = linkaddr_node_addr.u8[4];
				ackdata[17] = linkaddr_node_addr.u8[5];
				ackdata[18] = linkaddr_node_addr.u8[6];
				ackdata[19] = linkaddr_node_addr.u8[7];
				ackdata[20] = 0;/*Se deja vacio para colocar el tiempo actual en tics a nivel fisico*/
				ackdata[21] = 0;
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1);/*Se activa el timestamp a nivel fisico*/
				NETSTACK_RADIO.send(ackdata, ack_len);
				packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);/*Se desactiva el timestamp a nivel fisico*/
				//printf("ch snt: %d\n", w_up_ch);
			}
			/********************/
			if(beacon_buf[0]==FRAME802154_ACKFRAME){
				sender_addr.u8[0] = beacon_buf[12];
				sender_addr.u8[1] = beacon_buf[13];
				sender_addr.u8[2] = beacon_buf[14];
				sender_addr.u8[3] = beacon_buf[15];
				sender_addr.u8[4] = beacon_buf[16];
				sender_addr.u8[5] = beacon_buf[17];
				sender_addr.u8[6] = beacon_buf[18];
				sender_addr.u8[7] = beacon_buf[19];
				/*Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales*/
				w_time= beacon_buf[3]+ (beacon_buf[4] << 8); 		// Tiempo de wake-up en tics
				t_sec=beacon_buf[5]+ (beacon_buf[6] << 8);  		// Tiempo de wake-up en seg
				last_generated=beacon_buf[7]+ (beacon_buf[8] << 8);	// Última semilla del generador
				c_t_sec = beacon_buf[9]+ (beacon_buf[10] << 8); 	//tiempo actual en seconds
				uint8_t last_ch = beacon_buf[11];					// last visited channel
				t_tic=beacon_buf[20]+ (beacon_buf[21] << 8); 		//tiempo actual en tics
				//printf("ch rcv: %d\n", last_ch);
				if (sender_addr.u8[7]!=0){
					/* Update the state information for the node that sent the ACK */
					for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
						if(linkaddr_cmp(&sender_addr, &n->node_link_addr)) {
							n->wake_time_tics=w_time;
							n->wake_time_seconds=t_sec;
							n->last_seed=last_generated;
							n->last_channel=last_ch;
							n->n=(long int)((long int)(local_time_tics)-(long int)(t_tic));
							n->m=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
							if (abs(n->n) < RTIMER_SECOND/2 && n->m != 0){
								n->m = (abs(n->m)-1) * (n->m/(abs(n->m)));
							}
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
						n->n=(long int)((long int)(local_time_tics)-(long int)(t_tic));
						n->m=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
						if (abs(n->n) < RTIMER_SECOND/2 && n->m != 0){
							n->m = (abs(n->m)-1) * (n->m/(abs(n->m)));
						}
						list_add(Neighbors, n);
					}
				}
			}
			/*************************/
		}
		//}
		/*Si el temporizador ha expirado hacemos neighbor_discovery_flag=0 luego de lo cual salimos del proceso
		 * de descubrimiento de vecinos*/
		if(timer_expired(&w)){
			neighbor_discovery_flag=0;
			//current_channel=0;
		}
	}
	/************ Print the neighbor list after the neighbor discovery. (Debug purposes only)   *********/
	neighbor_state *test=list_head(Neighbors);
	printf("Neighbor List after Neighbor Discovery: \n");
	while(test != NULL) {
		printf("%d. wake_time_tics: %u, m: %d last_seed: %u, wake_time_seconds: %u, n: %ld\n",
				test->node_link_addr.u8[7], test->wake_time_tics, test->m, test->last_seed, test->wake_time_seconds, test->n);
		test = list_item_next(test);
	}
	/************ end of neighbor list printing **********************************************************/
}
/******************************************************************************/
/*La siguiente funcion se usa para controlar el ciclo util de radio*/
static char reception_powercycle(void)
{
	rtimer_clock_t wt;
	PT_BEGIN(&pt);
	while (1){
		if(!neighbor_discovery_flag && (transmitting == waiting_to_transmit)){  // TODO If the radio is not sending a packet, the radio should stay ON
			off(0);
		}
		initial_rand_seed_temp=(15213*initial_rand_seed)+11237;
		//printf("Sleep:%u (SEED:%u)\n",initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2, initial_rand_seed_temp);//}

		rtimer_set(&reciever_powercycle_timer,(w_up_time+(initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2)), 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		//printf("pc_off:%u\npc_              %u  %u\n", RTIMER_NOW(), initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2, w_up_time+(initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2));
		if (transmitting!=0 /*|| neighbor_discovery_flag*/){
			printf("Tx & PwCy sleep.\n");
			leds_blink();
		}
		PT_YIELD(&pt);

		/* Turn on the radio interface */
		if (!neighbor_discovery_flag && (transmitting == waiting_to_transmit)/*transmitting==0*/){
			NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, list_of_channels[current_channel]);
		}
		if(check_if_radio_on()==0 && (transmitting == waiting_to_transmit)/*transmitting==0*/){
			on();
		}
		/* Save the timestamps (in tics and in seconds) of the last wake-up,
		 * which will be transmitted when the state is requested through an ACK */
		w_up_time=RTIMER_NOW();
		time_in_seconds=clock_seconds();
		//printf("pc_on:%u\n", w_up_time);
		initial_rand_seed=initial_rand_seed_temp;
		w_up_ch = list_of_channels[current_channel];
		/* TODO: Get the blacklist and embed it into the beacon. Now, the value for the blacklist is fixed to 100 */
		packetbuf_set_attr(PACKETBUF_ATTR_NODE_BLACKLIST, 0x00F0);
		blacklist=packetbuf_attr(PACKETBUF_ATTR_NODE_BLACKLIST);
		/********** Makes node 1 to print the time he woke-up in secs and tics. For debuging purposes only ********/
		//printf("secs:%u ",time_in_seconds);
		//printf("tics:%u ",w_up_time);//}
		/*********************************************************************************************************/
		/* Create beacon and transmit it... */

		uint8_t beacon_data[12] = {0,0,0,0,0,0,0,0,0,0,0};
		beacon_data[0]=FRAME802154_BEACONFRAME;
		beacon_data[2]=blacklist & 0xff;
		beacon_data[3]=(blacklist>> 8) & 0xff;
		beacon_data[4]=linkaddr_node_addr.u8[0];
		beacon_data[5]=linkaddr_node_addr.u8[1];
		beacon_data[6]=linkaddr_node_addr.u8[2];
		beacon_data[7]=linkaddr_node_addr.u8[3];
		beacon_data[8]=linkaddr_node_addr.u8[4];
		beacon_data[9]=linkaddr_node_addr.u8[5];
		beacon_data[10]=linkaddr_node_addr.u8[6];
		beacon_data[11]=linkaddr_node_addr.u8[7];
		if (transmitting == waiting_to_transmit){
			beacon_data[1]=waiting_to_transmit;
			//printf("bd[1]=%d\n", beacon_data[1]);
			while(NETSTACK_RADIO.channel_clear() == 0){}
			NETSTACK_RADIO.send(beacon_data, 12);
		}
		rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ 330, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		if (transmitting==0){
			/* The time spent awake is the maximum time required for a node to send a data packet from the moment it receives the beacon */
			wt = RTIMER_NOW();
			int time_to_wait_awake=300;
			while(RTIMER_NOW() < (wt + time_to_wait_awake) && !neighbor_discovery_flag){
				// Check if a packet was received
				/*while(NETSTACK_RADIO.receiving_packet() ||
						NETSTACK_RADIO.channel_clear() == 0){}*/
				if (NETSTACK_RADIO.pending_packet()){
					//printf("%u\n", RTIMER_NOW());
					//rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ 100, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
					PT_YIELD(&pt);
					//printf("%u\n", RTIMER_NOW());
				}
			}
			//printf("*\n", RTIMER_NOW());
		}else {
			printf("Tx & PwCy wake-up.\n");
			leds_blink();
			if (waiting_to_transmit==1){
				off(0);
			}
		}
		current_channel=(current_channel+1)%16;
		rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+ 10, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);

		if (clock_seconds()%150 < 2){
			printf("succ_b:%u b_fail:%u b_fail_s:%u b_fail_tx:%u\n"
					"succ_B_PDR=%u  validB_PDR=%u  PDR=%u\n"
					"succ_:%u fail:%u f_COL:%u f_ACK:%u f_DEF:%u f_ERR:%u\n",
					succ_beacon, beacon_failed_sync+beacon_failed_tx, beacon_failed_sync, beacon_failed_tx,
					(succ_beacon*100)/(succ_beacon+beacon_failed_sync), (succ_beacon*100)/(succ_beacon+beacon_failed_sync+beacon_failed_tx), (successful*100)/(successful+failed),
					successful, failed, failed_COL, fail_ACK, failed_DEF, failed_ERR);
		}
		/* After a beacon is sent, wait for DATA_PACKET_WAITING_TIME period */
		/************* Making node 2 follow the powercycle schedule of node 1 using the time prediction mechanism  ********************/
		/*if (linkaddr_node_addr.u8[7]==2){
			neighbor_state *test=list_head(Neighbors);
			if (test->wake_time_seconds!=NULL){
				get_neighbor_wake_up_time(get_neighbor_state(Neighbors, test->node_link_addr));}}*/
		/***********************************/
		/*wt = RTIMER_NOW();
		while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + 1000)) {
			// Check if a packet was received
			while(NETSTACK_RADIO.receiving_packet() ||
					NETSTACK_RADIO.channel_clear() == 0){}
			if (NETSTACK_RADIO.pending_packet()){
				break;
			}
		}*/
		/* TODO: What happens if a packet was actually received? */
		/* TODO The ACK works as a beacon, so sending an ACK should reset the waiting time of the node for a data packet. */
		//printf("Timer spent awake: %u\n", RTIMER_NOW()-w_up_time);
		/******************************************************************************************************************************/
		//}
		PT_YIELD(&pt);
	}
	PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
static int
send_one_packet(mac_callback_t sent, void *ptr, linkaddr_t receiver)
{
	//printf("send_one_packet()\n");
	neighbor_state *n;
	/*n_state se usa para el establecimiento de la bandera de peticion de estado*/
	neighbor_state n_state;
	rtimer_clock_t wt;
	unsigned int w_time;
	unsigned int t_sec;
	unsigned int t_tic;
	unsigned int time_to_send;
	int exist;
	int is_broadcast;
	is_broadcast = packetbuf_holds_broadcast();
	/*static  linkaddr_t addr_receiver;
	linkaddr_copy(&addr_receiver,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));*/
	exist=check_if_neighbor_exist( Neighbors,receiver);
	/*neighbor_state *test;
	printf("Neighbor List: ");
	for(test = list_head(Neighbors); test != NULL; test = list_item_next(test)) {
		printf("%d ", test->node_link_addr.u8[7]);
	}
	printf("\n addr_receiver=%d  exist=%d\n", addr_receiver.u8[7], exist);*/
	/*if( !check_if_radio_on() && !neighbor_discovery_flag ){
		on();
	}*/
	while(neighbor_discovery_flag || check_if_radio_on()==0) {
		on();
	}

	/******************** Beacon detection before sending the packet ****************/
	int beacon_received = 0;
	int good_beacon=0;
	uint8_t beaconbuf[14];
	NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);	// This is done to remove any packet remaining in the Reception FIFO
	wt = RTIMER_NOW();
	while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + 1310) && beacon_received==0) {  //It should be "wt + Time_from_Chasing_algorithm but for now we just use 40ms (remember in Cooja the time is twice the real time)
		/*if( !check_if_radio_on() && !neighbor_discovery_flag ){
			on();
		}*/
		if(NETSTACK_RADIO.pending_packet()) {
			int len=NETSTACK_RADIO.read(beaconbuf, 14);
			//printf("Length of the message: %d\n", len);
			if (beaconbuf[0]==FRAME802154_BEACONFRAME){
				linkaddr_t beacon_src_addr;
				beacon_src_addr.u8[0] = beaconbuf[4];
				beacon_src_addr.u8[1] = beaconbuf[5];
				beacon_src_addr.u8[2] = beaconbuf[6];
				beacon_src_addr.u8[3] = beaconbuf[7];
				beacon_src_addr.u8[4] = beaconbuf[8];
				beacon_src_addr.u8[5] = beaconbuf[9];
				beacon_src_addr.u8[6] = beaconbuf[10];
				beacon_src_addr.u8[7] = beaconbuf[11];
				if(linkaddr_cmp(&beacon_src_addr, &receiver)){
					// printf("It's a Beacon from node %d and I'm waiting for node %d\n", beaconbuf[11], receiver.u8[7]);
					//printf("B %d\n", beacon_src_addr.u8[7]);
					//printf("bb[1]=%d\n", beaconbuf[1]);
					good_beacon=1;
					if (beaconbuf[1]==1){
						printf("RES=%d_%d RxBusy\n", 1, receiver.u8[7]);
						beacon_failed_tx++;
						break;
					} else {
						printf("wait:%d-%u\n", receiver.u8[7],RTIMER_NOW()- wt);
						beacon_received=1;
					}
				}
			}
		}
	}
	/* TODO Handle what happens when the beacon is not received */
	if (beacon_received==0 /*&& beaconbuf[0]==FRAME802154_BEACONFRAME*/){
		//printf("RES=%d_%d ", 1, receiver.u8[7]);
		off(0);
		if (good_beacon==0){
			printf("RES=%d_%d RxNotFound\n", 1, receiver.u8[7]);
			beacon_failed_sync++;
		}
		transmitting=0;
		leds_off(7);
		failed++;
		failed_COL++;
		//return MAC_TX_COLLISION;
		mac_call_sent_callback(sent, ptr, 1, 1);
		return 0;
	}else{
		succ_beacon++;
		/* Backoff based on the node ID:
		 * RTIMER_SECOND/400: is the time required to transmit a message.
		 * 82 tics is the time lapse between the end of a transmission of a DATA packet and the end of its corresponding ACK */
		wt=RTIMER_NOW();
		/*printf("Random_rand(): %u   CCA: ", (random_rand()*linkaddr_node_addr.u8[7])%(unsigned short)(50) + 10);*/
		time_to_send = (unsigned int)(random_rand()%(unsigned int)(77)); // 77 tics is the time required to transmit a maximum length frame
		printf("%u\n",time_to_send);
		while(RTIMER_NOW() < wt+time_to_send){}
		//printf("%d\n", time_to_send);
		/*leds_blink();
		printf("%d \n", NETSTACK_RADIO.channel_clear());*/
		//while(RTIMER_NOW() < (wt + (linkaddr_node_addr.u8[7] - 1)*(RTIMER_SECOND/400 + 82))){}
	}
	/***************************************************************************************/

	int ret;
	int last_sent_ok = 0;
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
	packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 0);
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK,1);

	//if((!exist /*&& !is_broadcast*/) || (n_state.m==NULL /*&& !is_broadcast*/)){/*Si el vecino no existe y el paquete no es de broadcast o no se ha completado el estado del vecinoy el paquete no es de broadcast */
		packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 1);
		//printf("ACK_STATE_RQ\n");
	/*}
	ack_len=3;
	if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){*/
		ack_len=14;
	//}

	if(NETSTACK_FRAMER.create() < 0) {/*Se llama al framer para crea la trama*/
		/* Failed to allocate space for headers */
		PRINTF("emmac: send failed, too large header\n");
		ret = MAC_TX_ERR_FATAL;
	} else {


		uint8_t dsn;
		dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

		NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());

		if(!NETSTACK_RADIO.channel_clear()/*NETSTACK_RADIO.receiving_packet() ||
				(!is_broadcast && NETSTACK_RADIO.pending_packet())*/) {

			/* Currently receiving a packet over air or the radio has
         already received a packet that needs to be read before
         sending with auto ack. */
			leds_on(LEDS_GREEN);
			ret = MAC_TX_COLLISION;
			printf("sent Collision before sending\n");
			//return 3;
		}
		else {
			switch(NETSTACK_RADIO.transmit(packetbuf_totlen())) {
			case RADIO_TX_OK:
				/*if(is_broadcast) {
					// BROADCAST packets doesn't require ACK, so we automatically return with successful transmission result
					ret = MAC_TX_OK;
				} else {*/
					// Check for ACK
					wt = RTIMER_NOW();
					ret = MAC_TX_NOACK;
					// Flush the RXFIFO to avoid that a node interprets as yours an ACK sent to another node with the same packet sequence number.
					NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);
					// Wait for the ACK
					while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + ACK_WAIT_TIME)) {
						/*Este mecanismo es similar al explicado en neighbor discovery*/
						if(NETSTACK_RADIO.receiving_packet() ||				//Ninguna de estas condiciones se cumple cuando se envía un paquete de datos.
								NETSTACK_RADIO.pending_packet() ||
								NETSTACK_RADIO.channel_clear() == 0) {
							int len;
							uint8_t ackbuf[ack_len];
							wt = RTIMER_NOW();
							while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {
								if(NETSTACK_RADIO.pending_packet()) {
									//printf("PP\n");
									len = NETSTACK_RADIO.read(ackbuf, ack_len);
									unsigned int local_time_tics = RTIMER_NOW();
									unsigned int local_time_seconds = clock_seconds();
									linkaddr_t ack_addr_receiver;
									linkaddr_copy(&ack_addr_receiver,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));

									if(len == ack_len && ackbuf[2] == dsn) {
										/* Ack received */
										if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){
											/*Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales*/
											w_time= ackbuf[3]+ (ackbuf[4] << 8); // Tiempo de wake-up en tics
											t_sec=ackbuf[5]+ (ackbuf[6] << 8);  // Tiempo de wake-up en seg
											unsigned int last_generated=ackbuf[7]+ (ackbuf[8] << 8);  // Última semilla del generador
											unsigned int c_t_sec = ackbuf[9]+ (ackbuf[10] << 8); //tiempo actual en seconds
											uint8_t last_ch = ackbuf[11];			// last visited channel
											t_tic=ackbuf[12]+ (ackbuf[13] << 8); //tiempo actual en tics
											/*Sobreescribimos o escribimos la informacion de estado correspondiente al vecino en cuestion*/
											//printf("list of Neighbors: ");
											for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
												if(linkaddr_cmp(&ack_addr_receiver, &n->node_link_addr)) {
													n->wake_time_tics=w_time;
													n->wake_time_seconds=t_sec;
													n->last_seed=last_generated;
													n->last_channel=last_ch;
													if(n->n==NULL){
														n->n=(long int)((long int)(local_time_tics)-(long int)(t_tic));}
													else{
														n->m=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));}
													/* If the values in ticks exchanged by the nodes are not distant from each other
													but they are at both sides of a second frontier (e.g. 32768 and 0 for the Zolertia Z1),
													then we should not take into account that last second because it would introduce errors
													in the time prediction mechanism. The same applies for a new neighbor. */
													if (abs(n->n) < RTIMER_SECOND/2 && n->m != 0){
														n->m = (abs(n->m)-1) * (n->m/(abs(n->m)));
													}
													break;
												}
											}
											/* No matching encounter was found, so we allocate a new one. */
											if(n == NULL) {
												n = memb_alloc(&neighbor_memb);
												if(n == NULL) {
													/* We could not allocate memory for this encounter, so we just drop it. */
													break;
												}
												linkaddr_copy(&n->node_link_addr, &ack_addr_receiver);

												n->wake_time_tics=w_time;
												n->wake_time_seconds=t_sec;
												n->last_seed=last_generated;
												n->last_channel=last_ch;
												if(n->n==NULL){
													n->n=(long int)((long int)(local_time_tics)-(long int)(t_tic));}
												else{
													n->m=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));}
												if (abs(n->n) < RTIMER_SECOND/2 && n->m != 0){
													n->m = (abs(n->m)-1) * (n->m/(abs(n->m)));
												}
												list_add(Neighbors, n);
											}
										}
										ret = MAC_TX_OK;
										break;		// This breaks the While after an ACK has been received and processed.
									} else {
										/* Not an ACK or ACK not for us: collision */
										ret = MAC_TX_COLLISION;
									}
								}
								/*else {
									printf("emmac tx noack\n");
								}*/
							}
							if (ret==MAC_TX_OK){
							break;}
						}
					}
				//}
				break;
			case RADIO_TX_COLLISION:
				printf("sent Collision after TX\n");
				ret = MAC_TX_COLLISION;
				//return 3;
				break;
			default:

				ret = MAC_TX_ERR;
				break;
			}
		}
	}
	switch(ret){
	case MAC_TX_OK:
		last_sent_ok = 1;
		successful++;
		break;
	case MAC_TX_COLLISION:
		failed_COL++;
		failed++;
		break;
	case MAC_TX_NOACK:
		printf("emmac tx noack\n");
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
	printf("RES=%u_%d\n",ret, receiver.u8[7]);
	mac_call_sent_callback(sent, ptr, ret, 1);
	off(0);
	transmitting = 0;
	leds_off(7);
	leds_blink();
	NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);
	return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static int
send_packet(mac_callback_t sent, void *ptr, linkaddr_t receiver)
{
	uint8_t neighbor_channel;
	uint8_t done=0;
	unsigned int time_in_advance=655;
	unsigned int neighbor_wake;
	// The purpose of the while is to avoid neighbor wake-ups closer to the current time than time_in_advance
	while (done==0){
		neighbor_wake = get_neighbor_wake_up_time(get_neighbor_state(Neighbors, receiver), &neighbor_channel);
		unsigned int rt_now=RTIMER_NOW();
		if (rt_now>time_in_advance && rt_now<(unsigned int)(0-time_in_advance)){
			if (neighbor_wake<(rt_now-time_in_advance) || neighbor_wake>(rt_now+time_in_advance)) 	{done=1;}
		}else{
			if(neighbor_wake<(rt_now-time_in_advance) && neighbor_wake>(rt_now+time_in_advance))	{done=1;}
		}
	}
	printf("Nw:%u %u \n", neighbor_wake, RTIMER_NOW());
	if ((neighbor_wake >= time_in_advance) && (neighbor_wake < (unsigned int)(0-time_in_advance))){  //

		/* TODO Solve the issue of not processing receiving packets while waiting for the neighbor to wake-up */
		while(!((RTIMER_NOW() >= (neighbor_wake-time_in_advance)) && (RTIMER_NOW() <= (neighbor_wake+time_in_advance)))){
			// Avoid bug when RTIMER overflows and starts over
			while(RTIMER_NOW()<5 || RTIMER_NOW()>(unsigned int)(0-5)){}
		}
	} else if (neighbor_wake < time_in_advance){
		while(!(RTIMER_NOW() >= (neighbor_wake - time_in_advance - 5))){}
	} else{
		while(!(RTIMER_NOW() > (neighbor_wake - time_in_advance))){}
	}
	/*if ((neighbor_wake < time_in_advance) || (neighbor_wake > 64881)){
		//printf("Nw:%u %u %u %u\n", neighbor_wake, neighbor_wake-time_in_advance, neighbor_wake+time_in_advance, RTIMER_NOW());
	}*/
	printf("Nw2:%u\n", RTIMER_NOW());
	if (!packetbuf_holds_broadcast()){
		leds_off(7);
		leds_on(4);
	}else {
		leds_off(7);
		leds_on(2);
	}
	waiting_to_transmit=0;
	//printf("n_ch: %d\n", neighbor_channel);
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, neighbor_channel);
	int result=3;
	/*int sent_times=0;
	while (result==3){
		printf("sent %d times\n", ++sent_times);*/
		result=send_one_packet(sent, ptr, receiver);
	//}
	return result;
}
/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
	while(buf_list != NULL) {
		transmitting = 1;
		waiting_to_transmit = 1;
		/* We backup the next pointer, as it may be nullified by
		 * mac_call_sent_callback() */
		struct rdc_buf_list *next = buf_list->next;
		int last_sent_ok;
		queuebuf_to_packetbuf(buf_list->buf);
		static  linkaddr_t addr_receiver;
		linkaddr_copy(&addr_receiver,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
		if (!packetbuf_holds_broadcast()){
			printf("UNICAST to: %d\n", addr_receiver.u8[7]);
			last_sent_ok = send_packet(sent, ptr, addr_receiver);
		}else {
			printf("BROADCAST\n");
			neighbor_state *neighbor_broadcast_to_unicast=list_head(Neighbors);
			while(neighbor_broadcast_to_unicast != NULL && last_sent_ok) {
				transmitting = 1;
				waiting_to_transmit = 1;
				queuebuf_to_packetbuf(buf_list->buf); // If this line is not present, the las node in the neighbor list receives a packet with part of the header repeated. Don't know why!
				//packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &(neighbor_broadcast_to_unicast->node_link_addr));
				last_sent_ok = send_packet(sent, ptr, neighbor_broadcast_to_unicast->node_link_addr);
				printf("Broadcast sent to: %d \n", neighbor_broadcast_to_unicast->node_link_addr.u8[7]);
				neighbor_broadcast_to_unicast = list_item_next(neighbor_broadcast_to_unicast);
			}
		}
		/* If packet transmission was not successful, we should back off and let
		 * upper layers retransmit, rather than potentially sending out-of-order
		 * packet fragments. */
		if(!last_sent_ok) {
			//printf("Last sent went wrong!\n");
			return;
		}
		buf_list = next;
	}
	//transmitting = 0;
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
	leds_on(LEDS_BLUE);
	//printf("DENTRO DE PACKET_INPUT\n");
	unsigned int wake_up_time;
	int original_datalen;
	uint8_t *original_dataptr;
	/*original_datalen y original_dataptr se utlizan para la conformacion del ACK*/
	original_datalen = packetbuf_datalen();
	original_dataptr = packetbuf_dataptr();

	/*if(packetbuf_datalen() == 16 || packetbuf_datalen() == 5) {
		// Ignore ACK packets
		printf("emmac: ignored ack\n");
	}*/if(0){}
	else{
		if(NETSTACK_FRAMER.parse() < 0) {
			printf("emmac: failed to parse %u\n", packetbuf_datalen());
		} else /*if(!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
				&linkaddr_node_addr) &&
				!packetbuf_holds_broadcast()) {

			//Si se entra aqui es que el paquete no era para nosotros
			PRINTF("emmac: not for us\n");

		} else */{
			printf("RX:%d\n", original_dataptr[2]);
			int duplicate = 0;
			/* Check for duplicate packet. */
			duplicate = mac_sequence_is_duplicate();
			if(duplicate && original_dataptr[0]!=0) {
				// Si el paquete recibido es un duplicado entonces lo desechamos
				printf("emmac: drop duplicate link layer packet %u\n",
						packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));
			} else {
				mac_sequence_register_seqno();
			}

			if(!duplicate) {
				frame802154_t recieved_frame;
				frame_emmac_parse(original_dataptr, original_datalen, &recieved_frame);
				//printf("sent for me? %d\n", recieved_frame.fcf.ack_required != 0);
				if(recieved_frame.fcf.frame_type == FRAME802154_DATAFRAME &&
						/*recieved_frame.fcf.ack_required != 0 &&*/
						(linkaddr_cmp((linkaddr_t *)&recieved_frame.dest_addr,
								&linkaddr_node_addr) || packetbuf_holds_broadcast())) {/*Si la trama recibida es de datos y si se requiere ACK y si la direccion del nodo es igual a la direccion de destino de la trama recibida*/
					uint8_t ackdata[14] = {0};
					unsigned int t_seconds = (unsigned int)(clock_seconds());
					if(recieved_frame.fcf.state_flag){
						/*Entramos aqui si el ACK requiere la informacion de estado del nodo*/
						ack_len=14;

						ackdata[0] = FRAME802154_ACKFRAME;
						ackdata[1] = 0;
						ackdata[2] = recieved_frame.seq;
						ackdata[3] = w_up_time & 0xff; /*w_up_time es el tiempo en tics del ultimo wake-up*/
						ackdata[4] = (w_up_time>> 8) & 0xff;
						ackdata[5] = time_in_seconds & 0xff;/*time_in_seconds es el tiempo en secs del ultimo wake-up*/
						ackdata[6] = (time_in_seconds>> 8) & 0xff;
						ackdata[7] = initial_rand_seed & 0xff;
						ackdata[8] = (initial_rand_seed>>8) & 0xff;
						ackdata[9] = t_seconds & 0xff;
						ackdata[10] = (t_seconds>>8) & 0xff;
						ackdata[11] = w_up_ch;
						ackdata[12] = 0;/*Se deja vacio para colocar el tiempo actual en tics a nivel fisico*/
						ackdata[13] = 0;
						linkaddr_t test;
						linkaddr_copy(&test,(linkaddr_t *)&recieved_frame.src_addr);
						printf("ACK:%u %d\n", test.u8[7], recieved_frame.seq);
						while (!NETSTACK_RADIO.channel_clear()){}
						packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1);/*Se activa el timestamp a nivel fisico*/
						NETSTACK_RADIO.send(ackdata, ack_len);
						packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);/*Se desactiva el timestamp a nivel fisico*/
					}
					else{
						/*Entramos aqui si el ACK no requiere la informacion de estado del nodo*/
						ack_len=3;
						ackdata[0] = FRAME802154_ACKFRAME;
						ackdata[1] = 0;
						ackdata[2] = recieved_frame.seq;
						NETSTACK_RADIO.send(ackdata, ack_len);
					}

				}
				/*TODO The ACK works as a beacon, so sending an ACK should reset the waiting time of the node for a data packet. */
				NETSTACK_MAC.input();
			}
		}
	}
	//NETSTACK_RADIO.read(dummy_buf_to_flush_rxfifo,1);
	leds_off(LEDS_BLUE);
}

/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
	/* Channel Check Interval function. Without this, CSMA doesn't retransmit packets */
	return CLOCK_SECOND;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
	/*Se inicializan la lista y la herramienta para la reserva de memoria*/
	list_init(Neighbors);
	memb_init(&neighbor_memb);
	random_init(linkaddr_node_addr.u8[7]);
	initial_rand_seed=linkaddr_node_addr.u8[7];
	off(0);
	transmitting = 0;
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 26);
	generate_ch_list(&list_of_channels, linkaddr_node_addr.u8[7], 16);
	printf("ch list: ");
	int i=0;
	for (i=0; i < 16; i++){
		printf("%u  ", list_of_channels[i]);
	}
	printf("\n");
	neighbor_discovery_flag=1;
	reception_powercycle();
	neighbor_discovery();
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver emmac_driver = {
		"emmac",
		init,
		send_packet,
		send_list,
		packet_input,
		on,
		off,
		channel_check_interval,
};
/*---------------------------------------------------------------------------*/
