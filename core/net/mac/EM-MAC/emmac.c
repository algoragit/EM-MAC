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
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 */
#include "contiki-conf.h"
#include "net/mac/mac-sequence.h"
#include "net/mac/EM-MAC/emmac.h"
#include "net/mac/EM-MAC/frame_EM-MAC.h"
#include "net/mac/EM-MAC/framer-EM-MAC.h"
#include "net/mac/EM-MAC/LCG.h"
#include "net/mac/EM-MAC/Neighbors_list.h"
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

#define ACK_WAIT_TIME                      RTIMER_SECOND / 625 /*Podria ser hasta RTIMER/900, para valores mayores que 900 el tx no recibe el ACK*/
#define AFTER_ACK_DETECTED_WAIT_TIME       RTIMER_SECOND / 1500
#define DATA_PACKET_WAIT_TIME               RTIMER_SECOND / 625
//Values of the reception state

enum{
	Reciever_mode,
	Transmiter_mode,
};

static struct rtimer reciever_powercycle_timer;
static struct timer w;
static unsigned int initial_rand_seed;
static unsigned int blacklist;
static unsigned short ack_len;
static unsigned short radio_mode;
static unsigned short neighbor_discovery_flag;
static unsigned int cycle_num;
static unsigned int w_up_time;
static unsigned time_in_seconds;
static unsigned short waiting_to_transmit;
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
		return NETSTACK_RADIO.on();}
}
/******************************************************************************/
/*Proceso de descubrimiento de vecinos que se lleva a cabo en la funcion init()*/
static void neighbor_discovery(void)
{
	uint8_t beacon_buf[12];
	static  linkaddr_t addr;
	neighbor_discovery_flag=1;
	unsigned int wt;
	neighbor_state *n;
	/*Para establecer el tiempo que durara el proceso de descubrimiento de vecinos se usa el timer.h por encuesta.
	 * Lo ideal seria usar el rtimer pero no se puede debido a que solo puede existir una unica instancia del mismo
	 * El uso de la unica instancia permitida del rtimer se reserva para el powercycle*/

	timer_set(&w,((CLOCK_SECOND*3)/4));
	while(neighbor_discovery_flag){

		if(NETSTACK_RADIO.receiving_packet() ||
				NETSTACK_RADIO.pending_packet() ||
				NETSTACK_RADIO.channel_clear() == 0){

			wt=RTIMER_NOW();

			/*Si se cumple que estamos recibiendo un poaquete o hay un paquete recibido pendiente a ser leido o hay un paquete
			 * en el aire entonces esperamos un tiempo AFTER_ACK_DETECTED_WAIT_TIME luego del cual leemos el paquete*/

			while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {} /*Mientras que RTIMER_NOW() sea menor que wt + AFTER_ACK_DETECTED_WAIT_TIME. */
			if(NETSTACK_RADIO.pending_packet()) {
				NETSTACK_RADIO.read(beacon_buf, 12);
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

					/* If we have an entry for this neighbor already, we renew it. */
					for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
						if(linkaddr_cmp(&addr, &n->node_link_addr)) {
							n->blacklist=NULL;
							n->wake_time_tics=NULL;
							n->m=NULL;
							n->wake_time_seconds=NULL;
							n->n=NULL;
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
						n->blacklist=NULL;
						n->wake_time_tics=NULL;
						n->m=NULL;
						n->wake_time_seconds=NULL;
						n->n=NULL;
						list_add(Neighbors, n);
					}
				}
			}
		}
		/*Si el temporizador ha expirado hacemos neighbor_discovery_flag=0 luego de lo cual salimos del proceso
		 * de descubrimiento de vecinos*/

		if(timer_expired(&w)){
			neighbor_discovery_flag=0;


		}
	}

}
/******************************************************************************/
/*La siguiente funcion se usa para controlar el ciclo util de radio*/
static void reception_powercycle(void)
{
	rtimer_clock_t wt;
	if(radio_mode==Reciever_mode)
	{

		radio_mode=Transmiter_mode;
		cycle_num++;
		rtimer_set(&reciever_powercycle_timer,(w_up_time+get_rand_wake_up_time(initial_rand_seed, cycle_num)), 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
	}
	else
	{
		radio_mode=Reciever_mode;
		if(check_if_radio_on()==0){
			on();}
		/*Aqui el radio se despierta para recibir y por lo tanto se establecen los valores de wake up time en tics y en segundos.
		 * Dichos valores seran transmitidos en los ACK cuando se reciba un paquete de datos con la bandera de estado en 1*/

		w_up_time=RTIMER_NOW();
		time_in_seconds=clock_seconds();

		packetbuf_set_attr(PACKETBUF_ATTR_NODE_BLACKLIST, 100);

		blacklist=packetbuf_attr(PACKETBUF_ATTR_NODE_BLACKLIST);
		/*Se crea el beacon y se transmite*/
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

		wt = RTIMER_NOW();
		/*Se espera un corto periodo de tiempo para esperar por una posible transmision
		 * Si en dicho periodo no se recibe ningun paquete el radio se apaga*/

		while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + DATA_PACKET_WAIT_TIME)) {}
		while(NETSTACK_RADIO.receiving_packet() ||
				NETSTACK_RADIO.channel_clear() == 0){}
		if((!neighbor_discovery_flag) ){
			off(0);}
		rtimer_set(&reciever_powercycle_timer,RTIMER_NOW()+1, 0,(void (*)(struct rtimer *, void *))reception_powercycle,NULL);
	}
	return;
}
/*****************************************************************************/
static int
send_one_packet(mac_callback_t sent, void *ptr)
{


	neighbor_state *n;
	/*n_state se usa para el establecimiento de la bandera de peticion de estado*/
	neighbor_state n_state;
	/* La funcion de neighbor_state v es  probrar el codigo*/
	neighbor_state v;
	unsigned int w_time;
	unsigned int t_sec;
	unsigned int t_tic;
	int exist;
	int is_broadcast;
	is_broadcast = packetbuf_holds_broadcast();
	static  linkaddr_t addr;
	linkaddr_copy(&addr,packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	/*Si el vecino existe exist se hace igual a uno*/
	exist=check_if_neighbor_exist( Neighbors,addr);

	/*TODO Wait some time to wake up in the right moment*/
	if( radio_mode==Transmiter_mode && !neighbor_discovery_flag )
		on();
	/*TODO Beacon detection before sending the packet*/
	while(radio_mode==Reciever_mode || neighbor_discovery_flag || check_if_radio_on()==0) {}

	int ret;
	int last_sent_ok = 0;
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);
	packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 0);

	if((!exist && !is_broadcast) || (n_state.m==NULL && !is_broadcast)){/*Si el vecino no existe y el paquete no es de broadcast o no se ha completado el estado del vecinoy el paquete no es de broadcast */
		packetbuf_set_attr( PACKETBUF_ATTR_NODE_STATE_FLAG, 1);}
	ack_len=3;
	if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){
		ack_len=9;
	}


	if(NETSTACK_FRAMER.create() < 0) {/*Se llama al framer para crea la trama*/
		/* Failed to allocate space for headers */
		PRINTF("nullrdc: send failed, too large header\n");
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

					rtimer_clock_t wt;

					/* Check for ack */
					wt = RTIMER_NOW();
					while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + ACK_WAIT_TIME)) {}

					ret = MAC_TX_NOACK;
					/*Este mecanismo es similar al explicado en neighbor discovery*/
					if(NETSTACK_RADIO.receiving_packet() ||
							NETSTACK_RADIO.pending_packet() ||
							NETSTACK_RADIO.channel_clear() == 0) {

						int len;
						uint8_t ackbuf[ack_len];
						wt = RTIMER_NOW();

						while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {}

						if(NETSTACK_RADIO.pending_packet()) {
							len = NETSTACK_RADIO.read(ackbuf, ack_len);

							if(len == ack_len && ackbuf[2] == dsn) {
								/* Ack received */
								if( packetbuf_attr(PACKETBUF_ATTR_NODE_STATE_FLAG)){
									/*Si el ACK lleva informacion de estado, esta se lee y se almacen en variables temporales*/
									w_time= ackbuf[3]+ (ackbuf[4] << 8); // Tiempo de wake-up en tics
									t_sec=ackbuf[5]+ (ackbuf[6] << 8);  // Tiempo de wake-up en seg
									t_tic=ackbuf[7]+ (ackbuf[8] << 8); //tiempo actual en tics
									/*Sobreescribimos o escribimos la informacion de estado correspondiente al vecino en cuestion*/
									for(n = list_head(Neighbors); n != NULL; n = list_item_next(n)) {
										if(linkaddr_cmp(&addr, &n->node_link_addr)) {
											n->wake_time_tics=w_time;
											n->wake_time_seconds=t_sec;
											if(n->n==NULL){
												n->n=t_tic;}
											else{
												n->m=t_tic;}
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
										linkaddr_copy(&n->node_link_addr, &addr);

										n->wake_time_tics=w_time;
										n->wake_time_seconds=t_sec;
										if(n->n==NULL){
											n->n=t_tic;}
										else{
											n->m=t_tic;}
										list_add(Neighbors, n);
									}
									/*Desde aqui hasta la etiqueta prueba no forma parte del codigo. Se usa pra probar las funcionalidades.*/


									addr.u8[0]=193;
									addr.u8[1]=12;
									addr.u8[2]=0;
									addr.u8[3]=0;
									addr.u8[4]=0;
									addr.u8[5]=0;
									addr.u8[6]=0;
									addr.u8[7]=1;
									v= get_neighbor_state (Neighbors, addr);
									printf("Wake-time secs: %u\n",v.wake_time_seconds);
									printf("n: %u\n",v.n);
									printf("m: %u\n",v.m);
									printf("Wake-time tics %u\n",v.wake_time_tics);
									/*PRUEBA*/
								}
								ret = MAC_TX_OK;
							} else {
								/* Not an ack or ack not for us: collision */
								ret = MAC_TX_COLLISION;
							}
						}
					} else {
						PRINTF("nullrdc tx noack\n");
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
	printf("Resultado: %u\n",ret);
	mac_call_sent_callback(sent, ptr, ret, 1);
	off(0);
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
	unsigned int wake_up_time;
	int original_datalen;
	uint8_t *original_dataptr;
	/*original_datalen y original_dataptr se utlizan para la conformacion del ACK*/
	original_datalen = packetbuf_datalen();
	original_dataptr = packetbuf_dataptr();

	if(packetbuf_datalen() == 9 ||packetbuf_datalen() == 3) {
		/* Ignore ack packets */
		PRINTF("nullrdc: ignored ack\n");

	} else{

		if(NETSTACK_FRAMER.parse() < 0) {
			PRINTF("nullrdc: failed to parse %u\n", packetbuf_datalen());

		} else if(!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
				&linkaddr_node_addr) &&
				!packetbuf_holds_broadcast()) {
			/*Si se entra aqui es que el paquete no era para nosotros*/
			PRINTF("nullrdc: not for us\n");

		} else {
			int duplicate = 0;

			/* Check for duplicate packet. */
			duplicate = mac_sequence_is_duplicate();
			if(duplicate) {
				/* Si el paquete recibido es un duplicado entonces lo desechamos*/
				PRINTF("nullrdc: drop duplicate link layer packet %u\n",
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

					uint8_t ackdata[8] = {0,0,0,0,0,0,0,0,0};
					if(recieved_frame.fcf.state_flag){
						/*Entramos aqui si el ACK requiere la informacion de estado del nodo*/
						ack_len=9;

						ackdata[0] = FRAME802154_ACKFRAME;
						ackdata[1] = 0;
						ackdata[2] = recieved_frame.seq;
						ackdata[3] = w_up_time & 0xff; /*w_up_time es el tiempo en tics del ultimo wake-up*/
						ackdata[4] = (w_up_time>> 8) & 0xff;
						ackdata[5] = time_in_seconds & 0xff;/*time_in_seconds es el tiempo en secs del ultimo wake-up*/
						ackdata[6] = (time_in_seconds>> 8) & 0xff;
						ackdata[7] = 0;/*Se deja vacio para colocar el tiempo actual en tics a nivel fisico*/
						ackdata[8] = 0;

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
	radio_mode=Transmiter_mode;
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
