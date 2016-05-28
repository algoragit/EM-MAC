/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * Copyright (c) 2009, Vrije Universiteit Brussel.
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
 */

/**
 * \file
 *         A power saving MAC protocol based on McMac, Y-MAC and X-MAC
 *         Used X-MAC code as a base for the duty cycle and message preamble routines
 *         (and you can still find bits and pieces here and there)
 *
 * \author
 *
 *         original X-MAC:
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 *         MuChMAC:
 *         Joris Borms <jborms@etro.vub.ac.be>
 */

/**
 *  > warning <  the implementation of some methods assumes the
 *  						 broadcast rime addr is 0.0
 *
 *               requires timesynch which only works with the CC2420 radio (for now)
 *
 */


/* TODO check if it's possible to use cxmac instead of xmac powercycle to improve message
 * sending routine and reduce complexity of the code... */
/* TODO implement use of a framer to create/parse packets */
/* TODO remove packetbuf packet train attribute and use data stream type instead */
/* TODO use one of the existing packetbuf attributes for powerlevel? */

#include "sys/pt.h"
#include "net/mac/MuChMAC/muchmac.h"
#include "sys/rtimer.h"
#include "dev/leds.h"
#include "net/rime/rime.h"
#include "net/mac/MuChMAC/timesynch.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "lib/random.h"
#include "lib/memb.h"
#include "net/netstack.h"

#include "sys/compower.h"
#include "sys/process.h"

#include "dev/cc2420/cc2420.h"

#include "contiki-conf.h"

#include <string.h>
#include <stdio.h>
#include "neighbor-attr.h"

/*
 * best choose FRAME_TIME, SLOT_TIME and SLOTS_IN_FRAME as a power of two,
 * so the compiler can optimize %, * and / operations to &, << and >>
 * XXX can the compiler do this automatically ??
 */

/* untested flags - set to 1 at your own risk! */
#define XMAC_CONF_COMPOWER            0
#define WITH_CHANNEL_CHECK            0

/* - some settings for performance tweaking - */
#define WITH_SLOTFLAG_CACHE           1
#define SEND_UC_DURING_BC_SLOT        0
/* - ------------------------------------- - */

PROCESS(send_packet_process, "send packet");
struct timing_data {
	rtimer_clock_t  time;
	uint16_t        frame;
	uint16_t        slot;
};
/* - ------ MuChMAC packet types --------- - */
#define TYPE_STROBE        0
#define TYPE_DATA          1
#define TYPE_DATA_TRAIN    2   //#define TYPE_ANNOUNCEMENT  2
#define TYPE_STROBE_ACK    3
#define TYPE_SYNCH_REQ     4
#define TYPE_SYNCH_ACK     5
/* - ------------------------------------- - */
struct muchmac_hdr {
	uint16_t type;
	linkaddr_t sender;
	linkaddr_t receiver;
};

/* --- definitions for channel rotation and timimg --- */
#define SLOTS_IN_FRAME        32
#define BC_SLOTS_IN_FRAME      1
#define UC_SLOTS_IN_FRAME     (SLOTS_IN_FRAME-BC_SLOTS_IN_FRAME)

#if SLOTS_IN_FRAME <= 16
typedef uint16_t slotflag_t;
#elif SLOTS_IN_FRAME <= 32
typedef uint32_t slotflag_t;
#else
#error "unsupported #slots per frame"
#endif

/* RTIMER_SECOND is signed by default! */
#define URTIMER_SECOND ((unsigned int) RTIMER_SECOND)

#define DEFAULT_ON_TIME      (URTIMER_SECOND / 200)
#define DEFAULT_OFF_TIME     (URTIMER_SECOND / 2 - DEFAULT_ON_TIME)
#define MESSAGE_WAIT_WINDOW  (DEFAULT_ON_TIME * 2)
#define FRAME_TIME           (URTIMER_SECOND)
#define SLOT_TIME            (FRAME_TIME / SLOTS_IN_FRAME)
/* timer is 16-bit, so max is 0xFFFF */
#define FRAMES               (0xFFFF / FRAME_TIME)


#define DEFAULT_STROBE_WAIT_TIME (7 * DEFAULT_ON_TIME / 8)

/*
struct xmac_config xmac_config = {
		DEFAULT_ON_TIME,
		DEFAULT_OFF_TIME,
		20 * DEFAULT_ON_TIME + 2 * DEFAULT_OFF_TIME,
		DEFAULT_STROBE_WAIT_TIME
}; */



static struct rtimer slot_timer;
static struct ctimer ct; /* synch sheduler */
static struct pt pt;

static volatile unsigned char muchmac_is_on = 1;

static rtimer_clock_t last_train;
static volatile unsigned char packet_train = 0;
static volatile unsigned char waiting_for_packet = 0;
static volatile unsigned char someone_is_sending = 0;
static volatile unsigned char synch_scheduled = 0;
static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;

static const struct radio_driver *radio;

struct rtimer* rt;

#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE

#define LEDS_ON(x) leds_on(x)
#define LEDS_OFF(x) leds_off(x)
#define LEDS_TOGGLE(x) leds_toggle(x)
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE
#define LEDS_ON(x)
#define LEDS_OFF(x)
#define LEDS_TOGGLE(x)
#define PRINTF(...)
#endif

static void (* receiver_callback)(const struct mac_driver *);

#if XMAC_CONF_COMPOWER
static struct compower_activity current_packet;
#endif /* XMAC_CONF_COMPOWER */

/*---------------------------------------------------------------------------*/
static void
set_receive_function(void (* recv)(const struct mac_driver *))
{
	receiver_callback = recv;
}
/*---------------------------------------------------------------------------*/
static void
on(void)
{
	if(muchmac_is_on && radio_is_on == 0) {
		radio_is_on = 1;
		NETSTACK_RADIO.on();
		LEDS_ON(LEDS_RED);
	}
}
/*---------------------------------------------------------------------------*/
static void
off(void)
{

	if(muchmac_is_on && radio_is_on != 0) {
		radio_is_on = 0;
		NETSTACK_RADIO.off();
		LEDS_OFF(LEDS_RED);
	}
}
/*---------------------------------------------------------------------------*/
/*------------------ POWERLEVEL TABLE MANAGMENT -----------------------------*/
/*---------------------------------------------------------------------------*/
/* move this section to a different file? powerlevel.(h|c) ? */
/* XXX this requires any of the upper layers to use PACKETBUF_ATTR_POWERLEVEL
       as a rime channel attribute */
uint8_t node_powerlevel;
NEIGHBOR_ATTRIBUTE_NONSTATIC(uint8_t, powerlevel, NULL);
static void
level_input(void)
{
	uint8_t level = packetbuf_attr(PACKETBUF_ATTR_POWERLEVEL);
	linkaddr_t* from = packetbuf_addr(PACKETBUF_ADDR_SENDER);
	neighbor_attr_set_data(&powerlevel, from, &level);
}

static void
level_output(void)
{
	packetbuf_set_attr(PACKETBUF_ATTR_POWERLEVEL, node_powerlevel);
}

uint8_t
level_lookup2(const linkaddr_t* addr)
{
	if (linkaddr_cmp(addr,&linkaddr_node_addr)){
		return node_powerlevel;
	}
	uint8_t* level = neighbor_attr_get_data(&powerlevel, addr);
	if (level != NULL && (*level) != 0){
		return *level;
	} else {
		return 1;
	}
}
/*---------------------------------------------------------------------------*/
/*-------------------------- SLOT STRUCTURE ---------------------------------*/
/*---------------------------------------------------------------------------*/
#define is_broadcast_slot(slot_index)  ((slot_index) >= UC_SLOTS_IN_FRAME)
/*---------------------------------------------------------------------------*/
/* linear feedback shift register */
static inline uint16_t
nextseed(uint16_t seed){
	uint16_t bit = ((seed >> 0) ^ (seed >> 12) ^ (seed >> 13) ^ (seed >> 15)) ;
	seed = (seed >> 1) | (bit << 15);
	return seed;
}
/*---------------------------------------------------------------------------*/
static slotflag_t
get_slotflags(unsigned short frame_index, const linkaddr_t* id, uint8_t n)
{

	static slotflag_t result;

#if WITH_SLOTFLAG_CACHE
	/* this avoids calculating the same slotflags several times,
	 * which should result in computational speedup if node_powerlevel > 1 */

	static struct {
		unsigned short index;
		uint8_t n;
	} cache;

	if (linkaddr_cmp(id, &linkaddr_node_addr)){
		if (cache.index == frame_index && cache.n == n){
			return result;
		} else {
			cache.index = frame_index;
			cache.n = n;
		}
	} else {
		cache.n = -1;
	}

#endif

	result = 0;

	uint16_t i, seed;
	/* last slots are broadcast slots, slotflag bits are in time-reversed order */
	for (i = 0; i < BC_SLOTS_IN_FRAME; i++){
		result |= (((slotflag_t)1) << ((SLOTS_IN_FRAME-1)-i));
	}

	seed = *(uint16_t*)id;

	for (i = 0; i < frame_index ; i++){
		seed = nextseed(seed);
	}

	for (i = 0; i < n ; i++){
		seed = nextseed(seed);

		uint16_t slot = seed % (UC_SLOTS_IN_FRAME);
		while (result & (((slotflag_t)1) << slot)){
			slot = (slot + 5) % UC_SLOTS_IN_FRAME;
		}
		result = result | (((slotflag_t)1) << (slot));

	}

	return result;

}
/*---------------------------------------------------------------------------*/
/*---------------------- CHANNELS AND TIMING --------------------------------*/
/*---------------------------------------------------------------------------*/
static unsigned short
get_tx_channel_at(unsigned short frame_index,
		unsigned short slot_index, const linkaddr_t* target)
{
	/* static random channel array to speed up random channel selection */
	static uint8_t channels[8][8] = {\
			{	12,	26,	24,	22,	18,	16,	14,	20	},
			{	20,	14,	26,	24,	22,	18,	16,	12	},
			{	14,	12,	16,	26,	24,	20,	18,	22	},
			{	22,	16,	12,	18,	26,	24,	20,	14	},
			{	24,	18,	14,	12,	20,	26,	22,	16	},
			{	16,	20,	18,	14,	12,	22,	26,	24	},
			{	26,	22,	20,	16,	14,	12,	24,	18	},
			{	18,	24,	22,	20,	16,	14,	12,	26	},
	};
	// check broadcast
	if(linkaddr_cmp(target,&linkaddr_null) ||
			is_broadcast_slot(slot_index)){
		return 4;//channels[frame_index%8][slot_index%8];
	} else {
		return 5;//channels[(target->u8[0]+frame_index)%8][slot_index%8];
	}


}
/*---------------------------------------------------------------------------*/
static unsigned short
get_tx_channel(rtimer_clock_t rnow, const linkaddr_t* target)
{
	rtimer_clock_t now = timesynch_rtimer_to_time(rnow);
	unsigned short frame_index = (now / FRAME_TIME);
	unsigned short slot_index = (now % FRAME_TIME) / SLOT_TIME;
	return get_tx_channel_at(frame_index, slot_index, target);
}
/*---------------------------------------------------------------------------*/
static void
set_radio_channel(int channel)
{
	if (cc2420_get_channel() != channel){
		cc2420_set_channel(channel);
	}
}
/*---------------------------------------------------------------------------*/
static rtimer_clock_t
next_on_time(rtimer_clock_t rnow, const linkaddr_t* addr)
{
	rtimer_clock_t now;
	for (now = timesynch_rtimer_to_time(rnow) ; ; now += FRAME_TIME){
		unsigned short frame_index = (now / FRAME_TIME);
		rtimer_clock_t last_frame_start = frame_index * FRAME_TIME;

		slotflag_t slotflags = get_slotflags(frame_index,
				addr, level_lookup2(addr));

		//printf("slotflags %lu \n", slotflags);

		uint8_t i;
		for (i = 0; i < SLOTS_IN_FRAME ; i++){
			if (slotflags & (((slotflag_t)1) << i)){
				rtimer_clock_t on_time = timesynch_time_to_rtimer(
						last_frame_start + i * SLOT_TIME);
				/* only return a time in the future */
				if (!RTIMER_CLOCK_LT(on_time, rnow + URTIMER_SECOND / 200)){
					return on_time;
				}
			}
		}
	}
}
/*---------------------------------------------------------------------------*/
static struct timing_data
next_send_time(const linkaddr_t* addr)
{
	uint8_t level = level_lookup2(addr);
	rtimer_clock_t drift = timesynch_drift(1);

	struct timing_data send;
	rtimer_clock_t now;
	send.time = RTIMER_NOW() - 1;

	/* slot indices that we will check */
	uint8_t from, to;
	if (linkaddr_cmp(addr,&linkaddr_null)){
		from = UC_SLOTS_IN_FRAME;
		to = SLOTS_IN_FRAME;
	} else {
		from = 0;
		/* XXX "to = SLOTS_IN_FRAME" when SEND_UC_DURING_BC_SLOT is true? */
		to = UC_SLOTS_IN_FRAME;
	}

	for (now = timesynch_time()	;	; now += FRAME_TIME)	{
		send.frame = now / FRAME_TIME;

		rtimer_clock_t frame_start = send.frame * FRAME_TIME;
		slotflag_t slotflags = get_slotflags(send.frame, addr, level);

		uint8_t i;
		for (i = from; i < to ; i++){
			if (slotflags & (((slotflag_t)1) << i)){
				rtimer_clock_t slot_start = frame_start + i * SLOT_TIME;
				send.time = timesynch_time_to_rtimer(slot_start	- drift
						- URTIMER_SECOND / 200 );
				if (!RTIMER_CLOCK_LT(send.time, RTIMER_NOW() + URTIMER_SECOND / 200)
				){//&& !RTIMER_CLOCK_LT(send.time, RTIMER_TIME(&slot_timer))){
					send.slot = i;
					return send;
				}
			}
		}

	}
	return send;
}
/*---------------------------------------------------------------------------*/
/*------------ POWERCYCLE ---------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static struct queuebuf     *queued_packet;
static struct timing_data   queued_send_time;

#if 1
static void synchronize(void* data);
/*---------------------------------------------------------------------------*/
static char powercycle(struct rtimer *t, void *ptr);
static inline void
schedule_powercycle(struct rtimer *t, rtimer_clock_t time2wake)
{
	int r;
	if(muchmac_is_on) {
		r = rtimer_set(t, time2wake, 0,
				(void (*)(struct rtimer *, void *))powercycle, NULL);
		printf("\n\nFUNDIOS!!! \n\n");
		if(r) {
			PRINTF("schedule_powercycle: could not set rtimer\n");
		}
	}
}
static inline void
powercycle_turn_radio_off(void)
{
	if(we_are_sending == 0 &&
			waiting_for_packet == 0) {
		off();
	}
#if XMAC_CONF_COMPOWER
	compower_accumulate(&compower_idle_activity);
#endif /* XMAC_CONF_COMPOWER */
}
static inline void
powercycle_turn_radio_on(struct rtimer *t)
{
	if(we_are_sending == 0 &&
			waiting_for_packet == 0) {
		set_radio_channel(get_tx_channel(RTIMER_TIME(t), &linkaddr_node_addr));
		on();
	}
}
static char
powercycle(struct rtimer *t, void *ptr)
{

	PT_BEGIN(&pt);
	printf("\n\n DEBUG: PT ON \n");
	while(1) {
		printf("\n\n DEBUG: PC ON \n");
		/* Only wait for some cycles to pass for someone to start sending */
		if(someone_is_sending > 0) {
			someone_is_sending--;
		}

		if ((timesynch_drift(0) > (SLOT_TIME / 2)) && !synch_scheduled){
			synch_scheduled = 1;
			ctimer_set(&ct, 0, synchronize, &slot_timer);
		}

		/* If there were a strobe in the air, turn radio on */
		powercycle_turn_radio_on(t);
		schedule_powercycle(t, RTIMER_TIME(t) + DEFAULT_ON_TIME);
		printf("\n\n DEBUG: Radio ON \n");
		printf("\nDEFAULT_OFF_TIME: %u \n", DEFAULT_OFF_TIME);
		PT_YIELD(&pt);

		if(DEFAULT_OFF_TIME > 0) {
			printf("\n\n DEBUG \n");
			powercycle_turn_radio_off();
			if(waiting_for_packet != 0) {
				waiting_for_packet++;
				if(waiting_for_packet > 2) {
					/* We should not be awake for more than two consecutive
	     power cycles without having heard a packet, so we turn off
	     the radio. */
					waiting_for_packet = 0;
					powercycle_turn_radio_off();
				}
			}
			//printf("next_on_time: %u\n",on_time);
			schedule_powercycle(t, next_on_time(RTIMER_TIME(t), &linkaddr_node_addr));
			printf("\n\n DEBUG: Radio OFF \n");
			PT_YIELD(&pt);
		}
	}

	PT_END(&pt);
}

#else
static void synchronize(void* data);
/*---------------------------------------------------------------------------*/
static char
powercycle(struct rtimer *t, void *ptr)
{
	int r;

	static unsigned char send_next;

	PT_BEGIN(&pt);

	while(1) {

		/* Only wait for some cycles to pass for someone to start sending */
		if(someone_is_sending > 0) {
			someone_is_sending--;
		}

		if(waiting_for_packet == 0) {
			if(we_are_sending == 0) {
				off();
#if XMAC_CONF_COMPOWER
				compower_accumulate(&compower_idle_activity);
#endif /* XMAC_CONF_COMPOWER */
			}
		} else {
			waiting_for_packet++;
			if(waiting_for_packet > 2) {
				/* We should not be awake for more than two consecutive
	     power cycles without having heard a packet, so we turn off
	     the radio. */
				waiting_for_packet = 0;
				if(we_are_sending == 0) {
					off();
				}
#if XMAC_CONF_COMPOWER
				compower_accumulate(&compower_idle_activity);
#endif /* XMAC_CONF_COMPOWER */
			} else {
				rtimer_set(t, RTIMER_TIME(t) + MESSAGE_WAIT_WINDOW, 1,
						(void (*)(struct rtimer *, void *)) powercycle , ptr);
				PT_YIELD(&pt);
				continue; // repeat "off()" checks
			}
		}

		// first check if we are not drifting too far

		if ((timesynch_drift(0) > (SLOT_TIME / 2)) && !synch_scheduled){

			synch_scheduled = 1;
			ctimer_set(&ct, 0, synchronize, &slot_timer);

		} else {
			rtimer_clock_t on_time = next_on_time(RTIMER_TIME(t), &linkaddr_node_addr);
			r = rtimer_set(t, on_time, 1,
					(void (*)(struct rtimer *, void *)) powercycle , ptr);

			if(r) {
				PRINTF("MuChmac: 1 could not set rtimer %d\n", r);
			}

			PT_YIELD(&pt);

			if (!we_are_sending && !waiting_for_packet){
				set_radio_channel(get_tx_channel(RTIMER_TIME(t), &linkaddr_node_addr));
				on();
			}

			if(muchmac_is_on) {
				r = rtimer_set(t, RTIMER_TIME(t) + DEFAULT_ON_TIME, 1,
						(void (*)(struct rtimer *, void *))powercycle, ptr);

				if(r) {
					PRINTF("MuChmac: 3 could not set rtimer %d\n", r);
				}

				PT_YIELD(&pt);
			}

		}

	}

	PT_END(&pt);
}
#endif
/*---------------------------------------------------------------------------*/
/*------------ SEND/RECEIVE PACKETS -----------------------------------------*/
/*---------------------------------------------------------------------------*/
process_event_t EVENT_SEND_NOW, EVENT_SEND_FROM_QUEUE;
PROCESS_THREAD(send_packet_process, ev, data)
{
	//  {ev}
	//   |  EVENT_SEND_NOW        : send immediatly from packetbuf
	//   |  EVENT_SEND_FROM_QUEUE : send from queue with preambles, unless it is
	//                              a train packet

	PROCESS_BEGIN();
	static struct etimer et;

	while (1){

		if (queued_packet == NULL){
			PROCESS_YIELD_UNTIL(ev == EVENT_SEND_NOW || ev == EVENT_SEND_FROM_QUEUE);
		}

		/* if (ev == EVENT_SEND_NOW) we should send from packetbuf immediatly,
		   else we whould wait until the scheduled queued send time
		   we use a "busy loop" since scheduling two separate rtimers doesn't work :( */
		while (!((ev == EVENT_SEND_NOW) ||
				RTIMER_CLOCK_LT(queued_send_time.time, RTIMER_NOW()))) {
			etimer_set(&et, 1);
			PROCESS_YIELD();
		}

		unsigned short channel = 0;
		// check if we are sending from queue
		struct queuebuf* backup = NULL;
		if (ev != EVENT_SEND_NOW){
			// store away current packetbuf content
			backup = queuebuf_new_from_packetbuf();
			queuebuf_to_packetbuf(queued_packet);
			queuebuf_free(queued_packet);
			queued_packet = NULL;
			channel = get_tx_channel_at(queued_send_time.frame, queued_send_time.slot,
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
		}

		ev = PROCESS_EVENT_NONE; // process event has been handled

		rtimer_clock_t t0;
		rtimer_clock_t t;
		int strobes;
		struct muchmac_hdr hdr;
		int got_strobe_ack = 0;
		struct {
			struct muchmac_hdr hdr;
		} strobe, strobe_recv;
		int len;
		int is_broadcast = 0;


#if WITH_CHANNEL_CHECK
		/* Check if there are other strobes in the air. */
		waiting_for_packet = 1;
		on();
		t0 = RTIMER_NOW();
		while(RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + xmac_config.strobe_wait_time * 2)) {
			len = NETSTACK_RADIO.read(&strobe.hdr, sizeof(strobe.hdr));
			if(len > 0) {
				someone_is_sending = 1;
			}
		}
		waiting_for_packet = 0;

		while(someone_is_sending);

#endif /* WITH_CHANNEL_CHECK */

		//off();

		/* Create the X-MAC header for the data packet. We cannot do this
     in-place in the packet buffer, because we cannot be sure of the
     alignment of the header in the packet buffer. */
		hdr.type = (packetbuf_attr(PACKETBUF_ATTR_PACKET_TRAIN) == 0) ?
				TYPE_DATA : TYPE_DATA_TRAIN;
		linkaddr_copy(&hdr.sender, &linkaddr_node_addr);
		linkaddr_copy(&hdr.receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));

		if(linkaddr_cmp(&hdr.receiver, &linkaddr_null)) {
			is_broadcast = 1;
		}

		/* Copy the X-MAC header to the header portion of the packet
     buffer. */
		packetbuf_hdralloc(sizeof(struct muchmac_hdr));
		memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct muchmac_hdr));
		packetbuf_compact();


		t0 = RTIMER_NOW();
		strobes = 0;

		LEDS_ON(LEDS_BLUE);

		/* Send a train of strobes until the receiver answers with an ACK. */

		/* Turn on the radio to listen for the strobe ACK. */
		if(!is_broadcast) {
			on();
		}

		rtimer_clock_t strobe_time = (timesynch_drift(1) + DEFAULT_ON_TIME) * 2;

		strobe.hdr.type = TYPE_STROBE;
		linkaddr_copy(&strobe.hdr.sender, &linkaddr_node_addr);
		linkaddr_copy(&strobe.hdr.receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));

		if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) == PACKETBUF_ATTR_PACKET_TYPE_ACK){
			PRINTF("sending ack on channel %u to %u.%u\n",
					get_tx_channel(packetbuf_addr(PACKETBUF_ADDR_RECEIVER)),
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
			//channel = get_tx_channel(&linkaddr_node_addr);
			//set_radio_channel(channel);
			NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
			goto endofsend; // message send loop
		} else if (packet_train){
			//channel = get_tx_channel(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
			//set_radio_channel(channel);
			NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
			goto endofsend; // message send loop
		} else {
			if (channel == 0){
				channel = get_tx_channel(RTIMER_NOW(), packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
			}
			set_radio_channel(channel);
			PRINTF("sending message on channel %u to %u.%u\n",
					channel,
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
		}

		/* Send the strobe packet. */

		/* By setting we_are_sending to one, we ensure that the rtimer
     powercycle interrupt do not interfere with us sending the packet. */
		we_are_sending = 1;

		watchdog_stop();

		got_strobe_ack = 0;

		for(strobes = 0;
				got_strobe_ack == 0 &&
						RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + strobe_time);
				strobes++) {

			t = RTIMER_NOW();

			if (!is_broadcast){
				NETSTACK_RADIO.send((const uint8_t *)&strobe, sizeof(struct muchmac_hdr));
			} else {
				NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
			}

			while(got_strobe_ack == 0 &&
					RTIMER_CLOCK_LT(RTIMER_NOW(), t + DEFAULT_STROBE_WAIT_TIME)) {
				/* See if we got an ACK */
				if(!is_broadcast) {
					len = NETSTACK_RADIO.read((void*)(&strobe_recv), sizeof(struct muchmac_hdr));
					if(len > 0) {
						if(linkaddr_cmp(&strobe_recv.hdr.sender, &linkaddr_node_addr) &&
								linkaddr_cmp(&strobe_recv.hdr.receiver, &linkaddr_node_addr)) {
							/* We got an ACK from the receiver, so we can immediately send
	       the packet. */
							got_strobe_ack = 1;
						} else {
							/* we received something else, so stop sending and shedule for
						next cycle */
							got_strobe_ack = -1;
						}
					}
				}
			}

		}

		/* Send the data packet. */
		if(is_broadcast || got_strobe_ack == 1) {
			NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
		} else { // if (got_strobe_ack == -1){
			//qsend_packet();
			off();
		}

		watchdog_start();

		endofsend:

		if(packetbuf_attr(PACKETBUF_ATTR_RELIABLE) &&
				!is_broadcast && (packet_train || got_strobe_ack == 1)){
			/* packet needs upper layer ACK */
			waiting_for_packet = 1;
		}

		/* expect to send more packets?
		 * - check PACKETBUF_ATTR_PACKET_TRAIN flag
		 * - for first packet of a train: don't train packets if tx failed
		 * - don't train broadcast */
		packet_train = !is_broadcast && (packet_train || (got_strobe_ack == 1))
											&& packetbuf_attr(PACKETBUF_ATTR_PACKET_TRAIN);

		if (packet_train){
			last_train = RTIMER_NOW();
		}

		if (backup != NULL){
			// restore packetbuf
			queuebuf_to_packetbuf(backup);
			queuebuf_free(backup);
		}

		we_are_sending = packet_train;

		PRINTF("MuChmac: send (strobes=%u,len=%u,%s), done\n", strobes,
				packetbuf_totlen(), got_strobe_ack ? "ack" : "no ack");

#if XMAC_CONF_COMPOWER
		/* Accumulate the power consumption for the packet transmission. */
		compower_accumulate(&current_packet);

		/* Convert the accumulated power consumption for the transmitted
     packet to packet attributes so that the higher levels can keep
     track of the amount of energy spent on transmitting the
     packet. */
		compower_attrconv(&current_packet);

		/* Clear the accumulated power consumption so that it is ready for
     the next packet. */
		compower_clear(&current_packet);
#endif /* XMAC_CONF_COMPOWER */

		LEDS_OFF(LEDS_BLUE);


	}
	PROCESS_END();

}
/*---------------------------------------------------------------------------*/
static int
qsend_packet(void)
{

	if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) == PACKETBUF_ATTR_PACKET_TYPE_ACK
			&& we_are_sending == 0){
		process_post_synch(&send_packet_process, EVENT_SEND_NOW, NULL);
		if (!waiting_for_packet){
			/* take a bit of margin before switching off the radio (kludge??) */
			rtimer_clock_t t0 = RTIMER_NOW();
			while (RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + URTIMER_SECOND / 400));
			off();
		}
		return 1;
	}

	if (packet_train){
		if(RTIMER_CLOCK_LT(RTIMER_NOW(), last_train + MESSAGE_WAIT_WINDOW)){
			process_post_synch(&send_packet_process, EVENT_SEND_NOW, NULL);
			return 1;
		} else {
			/* packet train was interrupted, resume normal sending schedule */
			we_are_sending = packet_train = 0;
			off();
		}
	}

	if (queued_packet == NULL){
		queued_packet = queuebuf_new_from_packetbuf();
		if (queued_packet != NULL){
			queued_send_time = next_send_time(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
			process_post_synch(&send_packet_process, EVENT_SEND_FROM_QUEUE, NULL);
			//if (RTIMER_CLOCK_LT(queued_send_time.time, RTIMER_TIME(&slot_timer))){
			//	rtimer_run_next();
			//}
			PRINTF("scheduling message to %u.%u\n",
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
			PRINTF("send %u to %u.%u now %u timer %u \n", queued_send_time,
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
					RTIMER_NOW(),RTIMER_TIME(&slot_timer));
			return 1;
		} else {
			printf("too many queued packets! cannot shedule MuChMAC \n");
			return 0;
		}
	} else {
		PRINTF("MuChMAC: packet already enqueued for %u (now %u), dropping \n",
				queued_send_time, RTIMER_NOW());
		RIMESTATS_ADD(sendingdrop);
		return 0;
	}

	return 0;
}
/*---------------------------------------------------------------------------*/
static void
input_packet(const struct radio_driver *d)
{
	if(receiver_callback) {
		receiver_callback(&muchmac_driver);
	}
}
/*---------------------------------------------------------------------------*/
static int
read_packet(void)
{
	struct muchmac_hdr *hdr;
	uint8_t len;

	packetbuf_clear();

	len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);

	if(len > 0) {
		packetbuf_set_datalen(len);
		hdr = packetbuf_dataptr();

		packetbuf_hdrreduce(sizeof(struct muchmac_hdr));

		if(hdr->type == TYPE_STROBE) {
			/* There is no data in the packet so it has to be a strobe. */
			someone_is_sending = 2;

			if(linkaddr_cmp(&hdr->receiver, &linkaddr_node_addr)) {
				/* This is a strobe packet for us. */

				if(linkaddr_cmp(&hdr->sender, &linkaddr_node_addr)) {
					/* If the sender address is our node address, the strobe is
	     a stray strobe ACK to us, which we ignore unless we are
	     currently sending a packet.  */
					someone_is_sending = 0;
				} else {
					struct muchmac_hdr msg;
					/* If the sender address is someone else, we should
	     acknowledge the strobe and wait for the packet. By using
	     the same address as both sender and receiver, we flag the
	     message is a strobe ack. */
					msg.type = TYPE_STROBE_ACK;
					linkaddr_copy(&msg.receiver, &hdr->sender);
					linkaddr_copy(&msg.sender, &hdr->sender);
					/* We turn on the radio in anticipation of the incoming
	     packet. */
					someone_is_sending = 1;
					waiting_for_packet = 1;
					on();
					NETSTACK_RADIO.send((const uint8_t *)&msg, sizeof(struct muchmac_hdr));
				}
			} else if(linkaddr_cmp(&hdr->receiver, &linkaddr_null)) {
				/* If the receiver address is null, the strobe is sent to
	   prepare for an incoming broadcast packet. If this is the
	   case, we turn on the radio and wait for the incoming
	   broadcast packet. */
				waiting_for_packet = 1;
				on();
			}
			/* We are done processing the strobe and we therefore return
	 to the caller. */
			return RIME_OK;
		} else if(hdr->type == TYPE_DATA || hdr->type == TYPE_DATA_TRAIN) {
			someone_is_sending = 0;
			if(linkaddr_cmp(&hdr->receiver, &linkaddr_node_addr) ||
					linkaddr_cmp(&hdr->receiver, &linkaddr_null)) {
				/* This is a regular packet that is destined to us or to the
	   broadcast address. */


#if XMAC_CONF_COMPOWER
				/* Accumulate the power consumption for the packet reception. */
				compower_accumulate(&current_packet);
				/* Convert the accumulated power consumption for the received
	   packet to packet attributes so that the higher levels can
	   keep track of the amount of energy spent on receiving the
	   packet. */
				compower_attrconv(&current_packet);

				/* Clear the accumulated power consumption so that it is ready
	   for the next packet. */
				compower_clear(&current_packet);
#endif /* XMAC_CONF_COMPOWER */


				waiting_for_packet = (hdr->type == TYPE_DATA_TRAIN);
				if (!waiting_for_packet){
					/* We have received the final packet, so we can go back to being
			   asleep. */
					off();
				}
				return packetbuf_totlen();
			}
		} else if(hdr->type == TYPE_SYNCH_REQ) {
			if (timesynch_authority_level() < cc2420_authority_level_of_sender){
				/* immediatly send back a packet
				the radio will timestamp this message so the requesting node can synch on it */
				linkaddr_copy(&hdr->receiver,&hdr->sender);
				linkaddr_copy(&hdr->sender,&linkaddr_node_addr);
				hdr->type = TYPE_SYNCH_ACK;
				NETSTACK_RADIO.send(hdr, sizeof(struct muchmac_hdr));
				PRINTF("replied synch time %u to %u.%u\n",timesynch_time(),
						hdr->receiver.u8[0],hdr->receiver.u8[1]);
			}
			return RIME_OK;
		}
	}
	return 0;
}
/*---------------------------------------------------------------------------*/
/*------------ SYNCHRONIZE AT STARTUP ---------------------------------------*/
/*---------------------------------------------------------------------------*/
static void
synchronize(void* data)
{
	rt = (struct rtimer*) data;

	if (timesynch_authority_level() > 0){

		we_are_sending = 1;

		unsigned short channel;
		if (rt->func == NULL){
			// if we aren't powercycling yet, pick a random channel
			channel = (((unsigned short) linkaddr_node_addr.u8[0]) % 8) * 2 + 12;
		} else {
			channel = get_tx_channel(RTIMER_NOW(), &linkaddr_null);
		}


		struct muchmac_hdr synchprobe;

		synchprobe.type = TYPE_SYNCH_REQ;
		linkaddr_copy(&synchprobe.receiver, &linkaddr_null);
		linkaddr_copy(&synchprobe.sender, &linkaddr_node_addr);

		int len = 0;
		int synched = 0;

		//rtimer_clock_t t = RTIMER_NOW();
		rtimer_clock_t t0;

		watchdog_stop();

		on();

		//set_radio_channel(channel);

		while (!synched){ // &&
			//(rt->func == NULL) || RTIMER_CLOCK_LT((t0 = RTIMER_NOW()), t + xmac_config.strobe_time))) {
			set_radio_channel(channel);

			//printf("requesting synch... \n");
			t0 = RTIMER_NOW();
			NETSTACK_RADIO.send(&synchprobe, sizeof(struct muchmac_hdr));


			while (!synched &&
					RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + DEFAULT_STROBE_WAIT_TIME)) {
				rtimer_clock_t trecv = timesynch_time();
				len = NETSTACK_RADIO.read(packetbuf_dataptr(), sizeof(struct muchmac_hdr));
				if (len == sizeof(struct muchmac_hdr)){
					if (((struct muchmac_hdr*)packetbuf_dataptr())->type == TYPE_SYNCH_ACK) {
						packetbuf_set_datalen(len);
						//cc2420_time_of_arrival = trecv;
						timesynch_incoming_packet();
						synched = 1;
					} else {
						// someone else is probably on this channel
						channel = (((unsigned int) rand()) % 8) * 2 + 12;
					}
				}
			}

		}

		watchdog_start();

		PRINTF("synched! authority was %i, now %i, offset = %u, time = %u ...", auth ,
				timesynch_authority_level(), timesynch_offset(), timesynch_time());
		//printf("departure %u, arrival %u \n", cc2420_time_of_departure, cc2420_time_of_arrival);

		we_are_sending = 0;
		synch_scheduled = 0;

	} else {
		PRINTF("I'm already synched because I'm the master clock \n");
	}
	rt->time = RTIMER_NOW();
	powercycle(rt, NULL);
}
/*---------------------------------------------------------------------------*/
/*------------ INITIALISATION -----------------------------------------------*/
/*---------------------------------------------------------------------------*/
const struct mac_driver *
muchmac_init(const struct radio_driver *d)
{
	radio_is_on = 0;
	waiting_for_packet = 0;
	process_start(&send_packet_process, NULL);
	PT_INIT(&pt);

	slot_timer.func = NULL;
	ctimer_set(&ct, CLOCK_SECOND * 2, synchronize, &slot_timer);

	node_powerlevel = 1;
	neighbor_attr_register(&powerlevel);
	//static struct abc_sniffer sniff_powerlevel = {NULL, level_input, level_output};
	//abc_sniffer_add(&sniff_powerlevel);

	EVENT_SEND_FROM_QUEUE = process_alloc_event();
	EVENT_SEND_NOW = process_alloc_event();

	muchmac_is_on = 1;
	radio = d;
	//NETSTACK_RADIO.set_receive_function(input_packet);
	set_receive_function(input_packet);

	queued_packet = NULL;
	synchronize(rt);

	return &muchmac_driver;
}
/*---------------------------------------------------------------------------*/
static int
turn_on(void)
{
	return 1;
}
/*---------------------------------------------------------------------------*/
static int
turn_off(int keep_radio_on)
{
	return 0;
}
/*---------------------------------------------------------------------------*/
unsigned short
channel_check_interval(void){
	return FRAME_TIME / node_powerlevel;
}
/*---------------------------------------------------------------------------*/
const struct mac_driver muchmac_driver =
{
		"MuChMAC",
		muchmac_init,
		qsend_packet,
		read_packet,
		set_receive_function,
		turn_on,
		turn_off,
		channel_check_interval
};
