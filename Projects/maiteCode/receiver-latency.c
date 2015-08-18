/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Tested and modified on September 8, 2012. Includes a useless but necessary timer!
 *         Writes with a fixed format in the serialdump to facilitate transfer of data to excel.
 *         Ack power is 0dBm, regardless of the sender settings.
 *			Added latency functions on August, 2014 by Maite Bezunartea
 *
 */

// imported libraries.

#include <stdio.h>
#include "contiki.h"
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include "dev/cc2420/cc2420.h"        // To use RSSI and LQI variables
#include "sys/energest.h"      // Module to measure ON time of hardware component
#include "dev/leds.h"
#include "dev/relay-phidget.h" // to access ADC7 pin on JP1A connector.


/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 

 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// message counters
static uint16_t received = 0 ;

//variables for channel quality
int8_t lqi_val ;
int16_t rssi_val ;

/* Altough it doesn't have anything to do every 10 seconds this timer is needed in order 
 * for the receiver to work ???
 */
#define TMP102_READ_INTERVAL (CLOCK_SECOND*10)
static struct etimer et;

#define CONF_SFD_TIMESTAMPS	1

// Writes a title on the console

PROCESS(temp_process, "receiving numbered messages");
AUTOSTART_PROCESSES(&temp_process);

struct testmsg {
	uint16_t seqno;
	uint8_t power;
	uint32_t clock;
	uint32_t cpu;
	uint32_t lpm;
	uint32_t transmit;
	uint32_t listen;
	uint8_t padding[13];
	uint16_t rtimer_ticks;
	//uint8_t rtimer_ticks2;
	uint16_t timestamp;
	//uint8_t timestamp2;

};


// This is the receiver function

recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	unsigned long rtime; // received time (long)
	  rtimer_clock_t now = RTIMER_NOW();		//received time (for the latency)
	  struct testmsg msg;
	  memcpy(&msg, packetbuf_dataptr(), sizeof(msg));


	//    switch 0ff P6.7 to mark the initial send time in external power recording
	//relay_off();
	/*leds_on(LEDS_BLUE);*/

	//  get the rssi and lqi values for the received packet
	rssi_val = ( cc2420_last_rssi ) - 45;
	lqi_val = cc2420_last_correlation;

	received ++;
	rtime=clock_time();
	uint16_t timestamp = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP);
	uint16_t rx_timestamp=packetbuf_attr(PACKETBUF_ATTR_NODE_TIMESTAMP);
	/*output format:rtimer-receiver|timestamp-receiver|received_time|sender|received|seqno|tx_power|sent_time|cpu|lpm|transmit|listen|timestamp-sender|latency-sender|RSSI|LQI*/
	/*printf("%u,%u,%lu,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%u,%u,%u,%5d,%5d\n",
				now, timestamp,rtime,from->u8[0], received,
				msg.seqno,msg.power,msg.clock,msg.cpu,msg.lpm,msg.transmit,msg.listen,msg.timestamp,msg.rtimer_ticks,(msg.timestamp-msg.rtimer_ticks),
				rssi_val,lqi_val);*/
	printf("Now:%u,timestamp:%u,rtime:%lu,msg.seqno:%u,rx_timestamp: %u\n", now, timestamp,rtime,msg.seqno,rx_timestamp);

	//    switch on P6.7 to mark the end of receiving time in external power recording
	//relay_on();
	/*leds_off(LEDS_BLUE);*/
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};

static struct unicast_conn uc;

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

	// Initialisation of the useless but needed timer ???

	tmp102_init();

	unicast_open(&uc, channel, &unicast_callbacks);

	// adjust power
	cc2420_set_txpower(power);

	// Enables output pin 7

	//relay_enable(7);
	//relay_on();         // Active low detector!


	PROCESS_END();
}
