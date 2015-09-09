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

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define ACK_WAIT_TIME                      RTIMER_SECOND / 150/*Podria ser hasta RTIMER/900, para valores mayores que 900 el tx no recibe el ACK*/
#define AFTER_ACK_DETECTED_WAIT_TIME       RTIMER_SECOND / 1500
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
#define MEMB_SIZE 4
MEMB(neighbor_memb, neighbor_state, MEMB_SIZE);
LIST(Neighbors);

/*---------------------------------------------------------------------------*/

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
static void neighbor_discovery(void)
{
	uint8_t beacon_buf[12];
	static  linkaddr_t addr;
	neighbor_discovery_flag=1;
	unsigned int wt;
	neighbor_state *n;
	uint8_t ackdata[13] = {0};
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
		leds_on(2);
		if(NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet() || NETSTACK_RADIO.channel_clear() == 0){
			wt=RTIMER_NOW();
			/*Si se cumple que estamos recibiendo un paquete o hay un paquete recibido pendiente a ser leido o hay un paquete
			 * en el aire entonces esperamos un tiempo AFTER_ACK_DETECTED_WAIT_TIME luego del cual leemos el paquete*/
			while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {}
			if(NETSTACK_RADIO.pending_packet()) {
				NETSTACK_RADIO.read(beacon_buf, 13);
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

					if (addr.u8[7]!=0){
						/* If we have an entry for this neighbor already, we break and send the ACK. */
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
					ack_len=13;
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
					ackdata[11] = 0;/*Se deja vacio para colocar el tiempo actual en tics a nivel fisico*/
					ackdata[12] = 0;
					//printf("Wake-time secs: %u  ",time_in_seconds);
					//printf("Wake-time tics %u  initial_rand_seed: %u\n",w_up_time, initial_rand_seed);
					packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 1);/*Se activa el timestamp a nivel fisico*/
					NETSTACK_RADIO.send(ackdata, ack_len);
					packetbuf_set_attr( PACKETBUF_ATTR_NODE_RADIO_TIMESTAMP_FLAG, 0);/*Se desactiva el timestamp a nivel fisico*/
				}
				if(beacon_buf[0]==FRAME802154_ACKFRAME){
					/*Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales*/
					w_time= beacon_buf[3]+ (beacon_buf[4] << 8); // Tiempo de wake-up en tics
					t_sec=beacon_buf[5]+ (beacon_buf[6] << 8);  // Tiempo de wake-up en seg
					last_generated=beacon_buf[7]+ (beacon_buf[8] << 8);  // Última semilla del generador
					c_t_sec = beacon_buf[9]+ (beacon_buf[10] << 8); //tiempo actual en seconds
					t_tic=beacon_buf[11]+ (beacon_buf[12] << 8); //tiempo actual en tics
					if (addr.u8[7]!=0){
						/* Update the state information for the node that sent the ACK */
						for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
							if(linkaddr_cmp(&addr, &n->node_link_addr)) {
								n->wake_time_tics=w_time;
								n->wake_time_seconds=t_sec;
								n->last_seed=last_generated;
								n->n=(long int)((long int)(local_time_tics)-(long int)(t_tic));
								n->m=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
								break;
							}
						}
						/* If no matching encounter was found, we try to allocate a new one. */
						if(n == NULL) {
							n = memb_alloc(&neighbor_memb);
							if(n == NULL) {
								break; // We could not allocate memory for this encounter, so we just drop it.
							}
							linkaddr_copy(&n->node_link_addr, &addr);
							n->wake_time_tics=w_time;
							n->wake_time_seconds=t_sec;
							n->last_seed=last_generated;
							n->n=(long int)((long int)(local_time_tics)-(long int)(t_tic));
							n->m=(int)((long int)(local_time_seconds)-(long int)(c_t_sec));
							list_add(Neighbors, n);
						}
					}
				}
			}
		}
		/* Once the timer has expired, neighbor discovery is finished */
		if(timer_expired(&w)){
			neighbor_discovery_flag=0;
			leds_off(2);
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
		if(!neighbor_discovery_flag  &&  linkaddr_node_addr.u8[7]==1){
			off(0);
			leds_off(7);
		}
		initial_rand_seed_temp=(15213*initial_rand_seed)+11237;
		/********** Makes node 1 to print it's sleeping period for the next cycle and the original seed to calculate it ********/
		//if (linkaddr_node_addr.u8[7]==1){
		printf("Sleep:%u (SEED:%u)\n",initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2, initial_rand_seed_temp);//}

		/********** Makes node 1 to follow it's own powercycle schedule ant node 2 to follow the node 1's powercycle schedule **********/
		if (linkaddr_node_addr.u8[7]==1){
			rtimer_set(&reciever_powercycle_timer,(w_up_time+(initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2)), 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		} else {
			neighbor_state *test=list_head(Neighbors);
			if (test->wake_time_seconds!=NULL){
				rtimer_set(&reciever_powercycle_timer,(get_neighbor_wake_up_time(get_neighbor_state(Neighbors, test->node_link_addr))), 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
				leds_off(7);
			} else {
				rtimer_set(&reciever_powercycle_timer,(w_up_time+(initial_rand_seed_temp%RTIMER_SECOND + RTIMER_SECOND/2)), 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
			}
		}
		/******* The only remaining line of this part that will pass to the final code is the one following the if() sentence that ends before this comment *****/
		PT_YIELD(&pt);

		/* Turn on the radio interface */
		if(check_if_radio_on()==0){
			on();}
		leds_on(7);
		rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+1000, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		/* Save the timestamps (in tics and in seconds) of the last wake-up,
		 * which will be transmitted when the state is requested through an ACK */
		w_up_time=RTIMER_NOW();
		time_in_seconds=clock_seconds();
		initial_rand_seed=initial_rand_seed_temp;
		/* TODO: Get the blacklist and embed it into the beacon. Now, the value for the blacklist is fixed to 100 */
		packetbuf_set_attr(PACKETBUF_ATTR_NODE_BLACKLIST, 0x00F0);
		blacklist=packetbuf_attr(PACKETBUF_ATTR_NODE_BLACKLIST);
		/****** I think I included this section but it proved not to be useful. I think it can be deleted but I'm not sure. ********/
		sequence_number++;
		packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, sequence_number);
		/*******************/
		/********** Makes node 1 to print the time he woke-up in secs and tics. For debuging purposes only ********/
		//if (linkaddr_node_addr.u8[7]==1){
		printf("secs:%u ",time_in_seconds);
		printf("tics:%u ",w_up_time);//}
		/*********************************************************************************************************/
		/* Create beacon and transmit it */
		uint8_t beacon_data[12] = {0,0,0,0,0,0,0,0,0,0,0};
		beacon_data[0]=FRAME802154_BEACONFRAME;
		beacon_data[1]=0;
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
		NETSTACK_RADIO.send(beacon_data, 12);
		/* After a beacon is sent, wait for DATA_PACKET_WAITING_TIME period */
		/************* Making node 2 follow the powercycle schedule of node 1 using the time prediction mechanism  ********************/
		/*if (linkaddr_node_addr.u8[7]==2){
			neighbor_state *test=list_head(Neighbors);
			if (test->wake_time_seconds!=NULL){
				get_neighbor_wake_up_time(get_neighbor_state(Neighbors, test->node_link_addr));}}*/
		/***********************************/
		wt = RTIMER_NOW();
		while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + DATA_PACKET_WAIT_TIME)) {}
		/* Check if a packet was received */
		while(NETSTACK_RADIO.receiving_packet() ||
				NETSTACK_RADIO.channel_clear() == 0){}
		/* TODO: What happens if a packet was actually received? This while is plain empty! It should call the function that passes the packet to the MAC layer, I think */
		/************ TODO Why is this rtimer_set useful? It can be removed with the PT_YIELD and just follow the while loop, I think. */
		//rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+1000, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
		//printf("Timer spent awake: %u\n", RTIMER_NOW()-w_up_time);
		/******************************************************************************************************************************/
		PT_YIELD(&pt);
	}
	PT_END(&pt);
}
/*****************************************************************************/
static int
send_one_packet(mac_callback_t sent, void *ptr)
{
	//printf("send_one_packet()\n");
	neighbor_state *n;
	/*n_state se usa para el establecimiento de la bandera de peticion de estado*/
	neighbor_state n_state;
	/* La funcion de neighbor_state v es  probrar el codigo*/
	neighbor_state v;
	rtimer_clock_t wt;
	unsigned int w_time;
	unsigned int t_sec;
	unsigned int t_tic;
	int exist;
	int is_broadcast;
	is_broadcast = packetbuf_holds_broadcast();
	static  linkaddr_t addr_receiver;
	linkaddr_copy(&addr_receiver,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	exist=check_if_neighbor_exist( Neighbors,addr_receiver);
	/*neighbor_state *test;
	printf("Neighbor List: ");
	for(test = list_head(Neighbors); test != NULL; test = list_item_next(test)) {
		printf("%d ", test->node_link_addr.u8[7]);
	}
	printf("\n addr_receiver=%d  exist=%d\n", addr_receiver.u8[7], exist);*/
	/*TODO Wait some time to wake up in the right moment*/
	/*if( !check_if_radio_on() && !neighbor_discovery_flag ){
		on();
	}*/
	while(neighbor_discovery_flag || check_if_radio_on()==0) {}

	/******************** Beacon detection before sending the packet ****************/
	int beacon_received = 0;
	int i=0;
	uint8_t beaconbuf[12];
	wt = RTIMER_NOW();
	while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + RTIMER_SECOND) && beacon_received==0) {  //It should be "wt + Time_from_Chasing_algorithm but for now just use the maximum
		if(NETSTACK_RADIO.pending_packet()) {
			//printf("PendingPacket and ");
			i++;
			//printf("i: %d\n", i);
			int len=NETSTACK_RADIO.read(beaconbuf, 12);
			//printf("Length of the message: %d\n", len);
			if (beaconbuf[0]==FRAME802154_BEACONFRAME){
				//printf("It's a Beacon from node %d and I'm waiting for node %d\n", beaconbuf[11], addr_receiver.u8[7]);
				if (is_broadcast || addr_receiver.u8[7]==beaconbuf[11]){
					//printf("addr_receiver.u8[7]=%d\n", addr_receiver.u8[7]);
					beacon_received=1;
				}
			}
			else {
				//printf("Beacon not received yet, instead: %d\n", beaconbuf[0]);
				continue;
			}
		}
	}
	/* TODO Handle what happens when the beacon is not received */
	if (beacon_received==0 && beaconbuf[0]==FRAME802154_BEACONFRAME){
		//printf("No beacon from (%d), last pkt type: %d\n", addr_receiver.u8[7], beaconbuf[0]);
		//return MAC_TX_DEFERRED;
	}else{
		//printf("beacon received: %d Type: %d FRAME802154_BEACONFRAME: %d Comparison: %d\n", beacon_received, beaconbuf[0], FRAME802154_BEACONFRAME, beaconbuf[0]==FRAME802154_BEACONFRAME);
	}
	/***************************************************************************************/

	int ret;
	int last_sent_ok = 0;
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
	packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 0);
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK,1);

	if((!exist && !is_broadcast) || (n_state.m==NULL && !is_broadcast)){/*Si el vecino no existe y el paquete no es de broadcast o no se ha completado el estado del vecinoy el paquete no es de broadcast */
		packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 1);
		printf("ACK_STATE_RQ\n");
	}
	ack_len=3;
	if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){
		ack_len=13;
	}

	if(NETSTACK_FRAMER.create() < 0) {/*Se llama al framer para crea la trama*/
		/* Failed to allocate space for headers */
		PRINTF("emmac: send failed, too large header\n");
		ret = MAC_TX_ERR_FATAL;
	} else {


		uint8_t dsn;
		dsn = ((uint8_t *)packetbuf_hdrptr())[2] & 0xff;

		NETSTACK_RADIO.prepare(packetbuf_hdrptr(), packetbuf_totlen());



		if(NETSTACK_RADIO.receiving_packet() ||
				(!is_broadcast && NETSTACK_RADIO.pending_packet())) {

			/* Currently receiving a packet over air or the radio has
         already received a packet that needs to be read before
         sending with auto ack. */
			ret = MAC_TX_COLLISION;
		}
		else {
			switch(NETSTACK_RADIO.transmit(packetbuf_totlen())) {
			case RADIO_TX_OK:
				if(is_broadcast) {
					/*Si el paquete es de broadcast no necitamos esperar por un ACK y por lo tanto devolvemos MAC_TX_OK*/
					ret = MAC_TX_OK;
				} else {

					/* Check for ack */
					wt = RTIMER_NOW();
					ret = MAC_TX_NOACK;
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
									//printf("pending_packet(%d))\n", NETSTACK_RADIO.pending_packet());
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
											t_tic=ackbuf[11]+ (ackbuf[12] << 8); //tiempo actual en tics
											/*Sobreescribimos o escribimos la informacion de estado correspondiente al vecino en cuestion*/
											for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
												if(linkaddr_cmp(&ack_addr_receiver, &n->node_link_addr)) {
													n->wake_time_tics=w_time;
													n->wake_time_seconds=t_sec;
													n->last_seed=last_generated;
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
								else {
									PRINTF("emmac tx noack\n");
								}
							}
							break;
						}
					}
				}
				break;
			case RADIO_TX_COLLISION:
				ret = MAC_TX_COLLISION;
				break;
			default:
				ret = MAC_TX_ERR;
				break;
			}
		}
	}
	if(ret == MAC_TX_OK) {
		last_sent_ok = 1;
	}
	//printf("ret: %u\n",ret);
	mac_call_sent_callback(sent, ptr, ret, 1);
	//off(0);
	return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
	/*send_packet es la funcion llamada por las capas superiores para transmitir un paquete*/
	send_one_packet(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
	//printf("send_list()\n");
	while(buf_list != NULL) {
		/* We backup the next pointer, as it may be nullified by
		 * mac_call_sent_callback() */
		struct rdc_buf_list *next = buf_list->next;
		int last_sent_ok;

		queuebuf_to_packetbuf(buf_list->buf);
		last_sent_ok = send_one_packet(sent, ptr);

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
	//printf("packet_input()\n");
	//printf("DENTRO DE PACKET_INPUT\n");
	unsigned int wake_up_time;
	int original_datalen;
	uint8_t *original_dataptr;
	/*original_datalen y original_dataptr se utlizan para la conformacion del ACK*/
	original_datalen = packetbuf_datalen();
	original_dataptr = packetbuf_dataptr();

	/*if(packetbuf_datalen() == 12 || packetbuf_datalen() == 5) {
		/* Ignore ACK packets
		PRINTF("emmac: ignored ack\n");
	}*/if (0){}
	else{
		if(NETSTACK_FRAMER.parse() < 0) {
			PRINTF("emmac: failed to parse %u\n", packetbuf_datalen());
		} else if(!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
				&linkaddr_node_addr) &&
				!packetbuf_holds_broadcast()) {
			/*Si se entra aqui es que el paquete no era para nosotros*/
			PRINTF("emmac: not for us\n");
		} else {
			int duplicate = 0;
			/* Check for duplicate packet. */
			duplicate = mac_sequence_is_duplicate();
			if(duplicate) {
				// Si el paquete recibido es un duplicado entonces lo desechamos
				PRINTF("emmac: drop duplicate link layer packet %u\n",
						packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));
			} else {
				mac_sequence_register_seqno();
			}

			if(!duplicate) {
				frame802154_t recieved_frame;
				frame_emmac_parse(original_dataptr, original_datalen, &recieved_frame);
				if(recieved_frame.fcf.frame_type == FRAME802154_DATAFRAME &&
						recieved_frame.fcf.ack_required != 0 &&
						linkaddr_cmp((linkaddr_t *)&recieved_frame.dest_addr,
								&linkaddr_node_addr)) {/*Si la trama recibida es de datos y si se requiere ACK y si la direccion del nodo es igual a la direccion de destino de la trama recibida*/
					uint8_t ackdata[13] = {0};
					unsigned int t_seconds = (unsigned int)(clock_seconds());
					if(recieved_frame.fcf.state_flag){
						/*Entramos aqui si el ACK requiere la informacion de estado del nodo*/
						ack_len=13;

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
						ackdata[11] = 0;/*Se deja vacio para colocar el tiempo actual en tics a nivel fisico*/
						ackdata[12] = 0;
						printf("\nState_SENT\n");
						//printf("Wake-time tics %u  initial_rand_seed: %u\n",w_up_time, initial_rand_seed);
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
				NETSTACK_MAC.input();
			}
		}
	}
}

/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
	/*no esta hecho*/
	return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
	/*Se inicializan la lista y la herramienta para la reserva de memoria*/
	list_init(Neighbors);
	memb_init(&neighbor_memb);
	initial_rand_seed=linkaddr_node_addr.u8[7];
	off(0);
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
