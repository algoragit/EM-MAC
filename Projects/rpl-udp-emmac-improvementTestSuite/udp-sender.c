/*
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

#include "contiki.h"
#include "node-id.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#ifdef WITH_COMPOWER
#include "powertrace.h"
#endif
#include "contiki-lib.h"
#include "contiki-net.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <string.h>
#include "net/rpl/rpl.h"
#include "simstats.h"

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define UDP_EXAMPLE_ID  190

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#ifndef PERIOD
#define PERIOD 5
#endif

//#define START_INTERVAL		(15 * CLOCK_SECOND)
#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % SEND_INTERVAL)
#define MAX_PAYLOAD_LEN		127

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;

static uint16_t total_pkts;
static unsigned int transmited_packets=0;
static unsigned int app_bytes;
static unsigned int app_bytesl2=0;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
	//char *str;

	if(uip_newdata()) {
		/*str = uip_appdata;
    str[uip_datalen()] = '\0';
    printf("DATA recv '%s'\n", str);*/
	}
}
/*---------------------------------------------------------------------------*/
static void
send_packet(void *ptr)
{
	static int seq_id;
	//char buf[MAX_PAYLOAD_LEN];
	struct {
		uint8_t seqno;
	} msg;

	seq_id++;
	total_pkts++;
	if(seq_id == 256) {
		/* Wrap to 128 to identify restarts */
		seq_id = 128;
	}
	msg.seqno = seq_id;
	/*PRINTF("DATA send to %d 'Hello %d'\n",
         server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1], seq_id);*/
	//sprintf(buf, "%d from the client", seq_id);
	uip_udp_packet_sendto(client_conn, &msg, sizeof(msg),
			&server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));

	printf("Out;%u;%u\n",seq_id,total_pkts);
	transmited_packets++;
	static unsigned int temp;
	temp=app_bytes;
	app_bytes+=sizeof(msg);
	if(app_bytes<temp)
	{app_bytesl2++;}
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
	int i;
	uint8_t state;

	PRINTF("Client IPv6 addresses: ");
	for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
		state = uip_ds6_if.addr_list[i].state;
		if(uip_ds6_if.addr_list[i].isused &&
				(state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
			PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
			PRINTF("\n");
			/* hack to make address "final" */
			if (state == ADDR_TENTATIVE) {
				uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
			}
		}
	}
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
	uip_ipaddr_t ipaddr;

	uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
	uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
	uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

	/* The choice of server address determines its 6LoPAN header compression.
	 * (Our address will be compressed Mode 3 since it is derived from our link-local address)
	 * Obviously the choice made here must also be selected in udp-server.c.
	 *
	 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the 6LowPAN protocol preferences,
	 * e.g. set Context 0 to aaaa::.  At present Wireshark copies Context/128 and then overwrites it.
	 * (Setting Context 0 to aaaa::1111:2222:3333:4444 will report a 16 bit compressed address of aaaa::1111:22ff:fe33:xxxx)
	 *
	 * Note the IPCMV6 checksum verification depends on the correct uncompressed addresses.
	 */

#if 0
	/* Mode 1 - 64 bits inline */
	uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
#elif 1
	/* Mode 2 - 16 bits inline */
	uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
	/* Mode 3 - derived from server link-local (MAC) address */
	uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0x0250, 0xc2ff, 0xfea8, 0xcd1a); //redbee-econotag
#endif
}
/*---------------------------------------------------------------------------*/



PROCESS_THREAD(node_process, ev, data)
{
  //static struct etimer et;
  PROCESS_BEGIN();

  static int is_coordinator = 0;
  
  /* Set node with ID == 1 as coordinator, convenient in Cooja. */
  if(node_id == 1) {
	  is_coordinator=1;
  }

  uip_ipaddr_t *br_prefix=NULL;
  if(is_coordinator) {
    uip_ipaddr_t prefix;
    uip_ip6addr(&prefix, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    br_prefix=&prefix;
  } else {
	  br_prefix=NULL;
  }
  
  uip_ipaddr_t global_ipaddr;

   if(br_prefix) { /* We are RPL root. Will be set automatically
                      as TSCH pan coordinator via the tsch-rpl module */
     memcpy(&global_ipaddr, br_prefix, 16);
     uip_ds6_set_addr_iid(&global_ipaddr, &uip_lladdr);
     uip_ds6_addr_add(&global_ipaddr, 0, ADDR_AUTOCONF);
     rpl_set_root(RPL_DEFAULT_INSTANCE, &global_ipaddr);
     rpl_set_prefix(rpl_get_any_dag(), br_prefix, 64);
     rpl_repair_root(RPL_DEFAULT_INSTANCE);
   }



  /*etimer_set(&et, CLOCK_SECOND * 300);
  while(1) {

      PROCESS_YIELD_UNTIL(etimer_expired(&et));
       print_network_status();
      etimer_reset(&et);
    }*/

  PROCESS_END();
}



PROCESS_THREAD(udp_client_process, ev, data)
{
	static struct etimer periodic;
	static struct ctimer backoff_timer;
#if WITH_COMPOWER
	static int print = 0;
#endif
	static struct etimer et;
	PROCESS_BEGIN();

	etimer_set(&et, 1700 * CLOCK_SECOND);

	PROCESS_PAUSE();

	set_global_address();

	PRINTF("UDP client process started\n");

	print_local_addresses();

	/* new connection with remote host */
	client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
	if(client_conn == NULL) {
		PRINTF("No UDP connection available, exiting the process!\n");
		PROCESS_EXIT();
	}
	udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

	PRINTF("Created a connection with the server ");
	PRINT6ADDR(&client_conn->ripaddr);
	PRINTF(" local/remote port %u/%u\n",
			UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

#if WITH_COMPOWER
	powertrace_sniff(POWERTRACE_ON);
#endif

	etimer_set(&periodic, SEND_INTERVAL);
	while(1) {
		PROCESS_YIELD();
		if(ev == tcpip_event) {
			tcpip_handler();
		}

		if(etimer_expired(&periodic)) {
			etimer_reset(&periodic);
			ctimer_set(&backoff_timer, SEND_TIME, send_packet, NULL);

#if WITH_COMPOWER
			if (print == 0) {
				powertrace_print("#P");
			}
			if (++print == 3) {
				print = 0;
			}
#endif

		}
		else if(etimer_expired(&et)) {
			rpl_dag_t *dag;
			dag = rpl_get_any_dag();
			PRINTF("%lu; %lu; %lu; %lu; %lu",
					//dag_parent.u8[LINKADDR_SIZE - 2],
					SIMSTATS_GET(pkttx),
					energest_type_time(ENERGEST_TYPE_LISTEN),
					energest_type_time(ENERGEST_TYPE_TRANSMIT),
					energest_type_time(ENERGEST_TYPE_LPM),
					energest_type_time(ENERGEST_TYPE_CPU));
			printf(";%u;%u\n",
					app_bytes,app_bytesl2);   //- Bytes transmitidos a nivel de aplicacion
		}
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
