/**
 * \file
 *         Unicast sending test program
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Tested and modified on September 8, 2012.
 *         Send address is directly read from rime.
 *
 *         Added latency functions on August, 2014 by Maite Bezunartea
 */

// imported libraries.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "contiki.h"
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h"   //To use RSSI and LQI variables
#include "sys/energest.h" // Module to measure ON time of hardware component
#include "dev/leds.h"
#include "dev/relay-phidget.h" // to access ADC7 pin on JP1A connector.

/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;	//133

// sender mote id
uint8_t sender;          // set automatically

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// receiver mote id
uint8_t receiver = 1; //let's put ID for the actual node in cooja

// message counters
uint16_t send = 0 ;

/* The sender will send a message every second
 * the timer of the temperature sensor is misused for this purpose
 * because it exists and does what is needed
 */
#define MAX_SEND_INTERVAL CLOCK_SECOND  * 2
#define SEND_INTERVAL ((MAX_SEND_INTERVAL +random_rand()) % MAX_SEND_INTERVAL)
//#define SEND_INTERVAL (CLOCK_SECOND)
#define CONF_SFD_TIMESTAMPS	1

/* Data structure of messages sent from sender
 *
 */
struct testmsg {		//FORMAT OF THE PACKET SEND
uint16_t seqno;
uint8_t power;
uint32_t clock;
uint32_t cpu;
uint32_t lpm;
uint32_t transmit;
uint32_t listen;
uint8_t padding[13];
uint16_t rtimer_ticks;
uint16_t timestamp;
};

uint16_t seqno=0;


PROCESS(temp_process, "Sending numbered messages");
AUTOSTART_PROCESSES(&temp_process);

/*--------------------------------------------------------------------------------
 * Receiver function which seems necessary to operate the sender with a normal MAC
 *-------------------------------------------------------------------------------*/

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	printf("unicast message received from %d.%d\n",
			from->u8[0], from->u8[1]);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;

/*---------------------------------------------------------------------------*/
void
packet_send(void)
{

	static unsigned long last_cpu, last_lpm, last_transmit, last_listen;

	linkaddr_t destination;
	seqno++;
	// define the destination address
	destination.u8[0] = 1;
	destination.u8[1] = 0;


	/*Set sender's info*/
	energest_flush();

	/*input format:seqno|power|clock|cpu|lpm|transmit|listen|padding|rtimer_ticks|timestamp*/
    struct testmsg msg;
    msg.seqno=seqno;

	/*Set general info*/

        msg.seqno;
        msg.power=power;
        msg.clock= clock_time();
        msg.cpu=energest_type_time(ENERGEST_TYPE_CPU) - last_cpu;
        msg.lpm = energest_type_time(ENERGEST_TYPE_LPM) - last_lpm;
        msg.transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - last_transmit;
        msg.listen = energest_type_time(ENERGEST_TYPE_LISTEN) - last_listen;
        msg.padding[0]=0;

        msg.padding[1]=0;
        msg.padding[2]=0;
        msg.padding[3]=0;
        msg.padding[4]=0;
        msg.padding[5]=0;
        msg.padding[6]=0;
        msg.padding[7]=0;
        msg.padding[8]=0;
        msg.padding[9]=0;
        msg.padding[10]=0;

        msg.padding[11]=5;
        msg.padding[12]=7;
        msg.rtimer_ticks = RTIMER_NOW();
        msg.timestamp = 0;


        /* Make sure that the values are within 16 bits. If they are larger,
             we scale them down to fit into 16 bits. */
        	while(msg.cpu >= 65536ul || msg.lpm >= 65536ul ||
        			msg.transmit >= 65536ul || msg.listen >= 65536ul) {
        		msg.cpu /= 2;
        		msg.lpm /= 2;
        		msg.transmit /= 2;
        		msg.listen /= 2;
        	}
        	last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
        	last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
        	last_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
        	last_listen = energest_type_time(ENERGEST_TYPE_LISTEN);


	/*Avoid sending to itself*/
	if(!linkaddr_cmp(&destination, &linkaddr_node_addr))
	{
		packetbuf_copyfrom(&msg, sizeof(msg));
	    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
				PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);
        //printf(" Packet sent is %s\n",payload);
        //printf(" size of payload %u\n",sizeof(msg));
        unsigned long rtime=clock_time();
        rtimer_clock_t now = RTIMER_NOW();
        //printf("Now:%u, rtime:%lu, msg.seqno:%u, msg.power:%u, msg.clock:%lu,"
        	//		" msg.timestamp:%u, msg.rtimer_ticks:%u, msg.timestamp-msg.rtimer_ticks:%u\n",
        		//			now, rtime,
        			//		msg.seqno,msg.power,msg.clock,msg.timestamp,msg.rtimer_ticks,(msg.timestamp-msg.rtimer_ticks));
		unicast_send(&uc, &destination);
	}


}
/*-------------------------------------------------------------------------------
 * Send packets with all needed statistic information
 *-------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------*/
static struct etimer et;

// Sending thread

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();


	// adjust power
	cc2420_set_txpower(power);

	tmp102_init();
	unicast_open(&uc, channel, &unicast_callbacks);
	//relay_enable(7);
	//relay_on();       // The pulse shaper circuit is active low.


	// infinite loop
	while(1)
	{
		//    wait for the SEND_INTERVAL time
		etimer_set(&et, SEND_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		//    switch off P6.7 to mark the initial send time in external power recording
		//relay_off();
		/*leds_on(LEDS_BLUE);*/

		packet_send();

		//    switch on P6.7 to end the pulse corresponding to sending in external power recording
		//relay_on();
		/*leds_off(LEDS_BLUE);*/

	}
	PROCESS_END();
}
