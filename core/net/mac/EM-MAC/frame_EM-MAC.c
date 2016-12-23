/*
 *
 *  Copyright (c) 2008, Swedish Institute of Computer Science
 *  All rights reserved.
 *
 *  Additional fixes for AVR contributed by:
 *
 *      Colin O'Flynn coflynn@newae.com
 *      Eric Gnoske egnoske@gmail.com
 *      Blake Leverett bleverett@gmail.com
 *      Mike Vidales mavida404@gmail.com
 *      Kevin Brown kbrown3@uccs.edu
 *      Nate Bohlmann nate@elfwerks.com
 *
 *  Additional fixes for MSP430 contributed by:
 *        Joakim Eriksson
 *        Niclas Finne
 *        Nicolas Tsiftes
 *
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of the copyright holders nor the names of
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 *  \brief This file is where the main functions that relate to frame
 *  manipulation will reside.
 */

/**
 *  \file
 *  \brief 802.15.4 frame creation and parsing functions
 *
 *  This file converts to and from a structure to a packed 802.15.4
 *  frame.
 */

/**
 *   \addtogroup frame802154
 *   @{
 */

#include "sys/cc.h"
#include "net/mac/EM-MAC/frame_EM-MAC.h"
#include "net/llsec/llsec802154.h"
#include "net/linkaddr.h"
#include <string.h>

/**
 *  \brief Structure that contains the lengths of the various addressing and security fields
 *  in the 802.15.4 header.  This structure is used in \ref frame_emmac_create()
 */
typedef struct {
	uint8_t dest_pid_len;    /**<  Length (in bytes) of destination PAN ID field */
	uint8_t dest_addr_len;   /**<  Length (in bytes) of destination address field */
	uint8_t src_pid_len;     /**<  Length (in bytes) of source PAN ID field */
	uint8_t src_addr_len;    /**<  Length (in bytes) of source address field */
	uint8_t timestamp_len;   /**<  Length (in bytes) of timestamp field */
	uint8_t clock_time_len;  /**<  Length (in bytes) of clock_time field */
	uint8_t random_seed_len; /**<  Length (in bytes) of random seed field */
	uint8_t blacklist_len;   /**<  Lenght (in bytes) of blacklist field*/
	uint8_t clock_time_sent_len;   /**<  Lenght (in bytes) of blacklist field*/
	uint8_t radio_timestamp_len;   /**<  Lenght (in bytes) of blacklist field*/
	uint8_t aux_sec_len;     /**<  Length (in bytes) of aux security header field */
} field_length_t;

/*----------------------------------------------------------------------------*/
CC_INLINE static uint8_t
addr_len(uint8_t mode)
{
	switch(mode) {
	case FRAME802154_SHORTADDRMODE:  /* 16-bit address */
		return 2;
	case FRAME802154_LONGADDRMODE:   /* 64-bit address */
		return 8;
	default:
		return 0;
	}
}
/*----------------------------------------------------------------------------*/
#if LLSEC802154_USES_EXPLICIT_KEYS
static uint8_t
get_key_id_len(uint8_t key_id_mode)
{
	switch(key_id_mode) {
	case FRAME802154_1_BYTE_KEY_ID_MODE:
		return 1;
	case FRAME802154_5_BYTE_KEY_ID_MODE:
		return 5;
	case FRAME802154_9_BYTE_KEY_ID_MODE:
		return 9;
	default:
		return 0;
	}
}
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
/*----------------------------------------------------------------------------*/
static void
field_len(frame802154_t *p, field_length_t *flen)
{
	/* init flen to zeros */
	memset(flen, 0, sizeof(field_length_t));

	/* Determine lengths of each field based on fcf and other args */
	if(p->fcf.dest_addr_mode & 3) {
		flen->dest_pid_len = 2;
	}
	if(p->fcf.src_addr_mode & 3) {
		flen->src_pid_len = 2;
	}

	/* Set PAN ID compression bit if src pan id matches dest pan id. */
	if((p->fcf.dest_addr_mode & 3) && (p->fcf.src_addr_mode & 3) &&
			p->src_pid == p->dest_pid) {
		p->fcf.panid_compression = 1;

		/* compressed header, only do dest pid */
		flen->src_pid_len = 0;
	} else {
		p->fcf.panid_compression = 0;
	}

	/* determine address lengths */
	flen->dest_addr_len = addr_len(p->fcf.dest_addr_mode & 3);
	flen->src_addr_len = addr_len(p->fcf.src_addr_mode & 3);

	/* determine timestamp lenght*/
//	if(p->fcf.state_flag ){
//		flen->timestamp_len=2;
//		flen->clock_time_len=2;}
//	else{
//		flen->timestamp_len=0;
//		flen->clock_time_len=0;}
//
//	/* determine random seed lenght */
//	if(p->fcf.state_flag ){
//		flen->random_seed_len=2;}
//	else{
//		flen->random_seed_len=0;}
//
//	/* determine blacklist lenght */
//	if(p->fcf.frame_type == FRAME802154_BEACONFRAME || p->fcf.frame_type == FRAME802154_DATAFRAME){
//		flen->blacklist_len=2;}
//	else{
//		flen->blacklist_len=0;}
//
//	if(p->fcf.state_flag ){
//		flen->clock_time_sent = 2;
//	}

	if(p->fcf.state_flag ){
		flen->timestamp_len = 2;
		flen->clock_time_len = 2;
		flen->random_seed_len = 2;
		flen->blacklist_len = 2;
		flen->clock_time_sent_len = 2;
		flen->radio_timestamp_len = 2;
	}
	else{
		flen->timestamp_len = 0;
		flen->clock_time_len = 0;
		flen->random_seed_len = 0;
		flen->blacklist_len = 0;
		flen->clock_time_sent_len = 0;
		flen->radio_timestamp_len = 0;
	}


#if LLSEC802154_SECURITY_LEVEL
	/* Aux security header */
	if(p->fcf.security_enabled & 1) {
		flen->aux_sec_len = 5
#if LLSEC802154_USES_EXPLICIT_KEYS
				+ get_key_id_len(p->aux_hdr.security_control.key_id_mode);
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
		;
	}
#endif /* LLSEC802154_SECURITY_LEVEL */
}
/*----------------------------------------------------------------------------*/
/**
 *   \brief Calculates the length of the frame header.  This function is
 *   meant to be called by a higher level function, that interfaces to a MAC.
 *
 *   \param p Pointer to frame802154_t_t struct, which specifies the
 *   frame to send.
 *
 *   \return The length of the frame header.
 */
int
frame_emmac_hdrlen(frame802154_t *p)
{
	field_length_t flen;
	field_len(p, &flen);
	return 3 +
			flen.dest_pid_len +
			flen.dest_addr_len +
			flen.src_pid_len +
			flen.src_addr_len +
			flen.timestamp_len +
			flen.clock_time_len +
			flen.random_seed_len +
			flen.blacklist_len +
			flen.clock_time_sent_len +
			flen.radio_timestamp_len +
			flen.aux_sec_len;
}
/*----------------------------------------------------------------------------*/
/**
 *   \brief Creates a frame for transmission over the air.  This function is
 *   meant to be called by a higher level function, that interfaces to a MAC.
 *
 *   \param p Pointer to frame802154_t struct, which specifies the
 *   frame to send.
 *
 *   \param buf Pointer to the buffer to use for the frame.
 *
 *   \return The length of the frame header
 */
int
frame_emmac_create(frame802154_t *p, uint8_t *buf)
{

	int c;
	field_length_t flen;
	uint8_t pos;
#if LLSEC802154_USES_EXPLICIT_KEYS
	uint8_t key_id_mode;
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */

	field_len(p, &flen);
	/* OK, now we have field lengths.  Time to actually construct */
	/* the outgoing frame, and store it in buf */
	buf[0] = (p->fcf.frame_type & 7) |
			((p->fcf.security_enabled & 1) << 3) |
			((p->fcf.frame_pending & 1) << 4) |
			((p->fcf.ack_required & 1) << 5) |
			((p->fcf.panid_compression & 1) << 6);
//			|
//			((p->fcf.timestamp_flag & 1) << 7);
	buf[1] =//((p->fcf.rand_seed_flag & 1) << 0)|
			((p->fcf.state_flag & 1) << 1) |
			((p->fcf.dest_addr_mode & 3) << 2) |
			((p->fcf.frame_version & 3) << 4) |
			((p->fcf.src_addr_mode & 3) << 6);

	/* sequence number */
	buf[2] = p->seq;
	pos = 3;

	/* Destination PAN ID */
	if(flen.dest_pid_len == 2) {
		buf[pos++] = p->dest_pid & 0xff;
		buf[pos++] = (p->dest_pid >> 8) & 0xff;
	}

	/* Destination address */
	for(c = flen.dest_addr_len; c > 0; c--) {
		buf[pos++] = p->dest_addr[c - 1];
	}

	/* Source PAN ID */
	if(flen.src_pid_len == 2) {
		buf[pos++] = p->src_pid & 0xff;
		buf[pos++] = (p->src_pid >> 8) & 0xff;
	}

	/* Source address */
	for(c = flen.src_addr_len; c > 0; c--) {
		buf[pos++] = p->src_addr[c - 1];
	}

	if (p->fcf.state_flag == 1){
		buf[pos++] = p->timestamp & 0xff;
		buf[pos++] = (p->timestamp>> 8) & 0xff;
		buf[pos++] = p->clock_time & 0xff;
		buf[pos++] = (p->clock_time>> 8) & 0xff;
		buf[pos++] = p->random_seed & 0xff;
		buf[pos++] = (p->random_seed>> 8) & 0xff;
		buf[pos++] = p->blacklist & 0xff;
		buf[pos++] = (p->blacklist>> 8) & 0xff;
		buf[pos++] = p->clock_time_sent & 0xff;
		buf[pos++] = (p->clock_time_sent>> 8) & 0xff;
		buf[pos++] = p->radio_timestamp & 0xff;
		buf[pos++] = (p->radio_timestamp>> 8) & 0xff;
	}

//	printf("stXt:%04x %04x %04x %04x %04x %04x %04x\n",
//			p->fcf.frame_type,
//			p->timestamp,
//			p->clock_time,
//			p->random_seed,
//			p->blacklist,
//			p->clock_time_sent,
//			p->radio_timestamp
//			);

#if LLSEC802154_SECURITY_LEVEL
	/* Aux header */
	if(flen.aux_sec_len) {
		buf[pos++] = p->aux_hdr.security_control.security_level
#if LLSEC802154_USES_EXPLICIT_KEYS
				| (p->aux_hdr.security_control.key_id_mode << 3)
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
				;
		memcpy(buf + pos, p->aux_hdr.frame_counter.u8, 4);
		pos += 4;

#if LLSEC802154_USES_EXPLICIT_KEYS
		key_id_mode = p->aux_hdr.security_control.key_id_mode;
		if(key_id_mode) {
			c = (key_id_mode - 1) * 4;
			memcpy(buf + pos, p->aux_hdr.key_source.u8, c);
			pos += c;
			buf[pos++] = p->aux_hdr.key_index;
		}
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
	}
#endif /* LLSEC802154_SECURITY_LEVEL */

	return (int)pos;

}
/*----------------------------------------------------------------------------*/
/**
 *   \brief Parses an input frame.  Scans the input frame to find each
 *   section, and stores the information of each section in a
 *   frame802154_t structure.
 *
 *   \param data The input data from the radio chip.
 *   \param len The size of the input data
 *   \param pf The frame802154_t struct to store the parsed frame information.
 */
int
frame_emmac_parse(uint8_t *data, int len, frame802154_t *pf)
{
	uint8_t *p;
	frame802154_fcf_t fcf;
	int c;
#if LLSEC802154_USES_EXPLICIT_KEYS
	uint8_t key_id_mode;
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */

	if(len < 3) {
		return 0;
	}

	p = data;

	/* decode the FCF */
	fcf.frame_type = p[0] & 7;
	fcf.security_enabled = (p[0] >> 3) & 1;
	fcf.frame_pending = (p[0] >> 4) & 1;
	fcf.ack_required = (p[0] >> 5) & 1;
	fcf.panid_compression = (p[0] >> 6) & 1;
//	fcf.timestamp_flag = (p[0] >> 7) & 1;
//
//	fcf.rand_seed_flag = (p[1] >> 0) & 1;
	fcf.state_flag = (p[1] >> 1) & 1;
	fcf.dest_addr_mode = (p[1] >> 2) & 3;
	fcf.frame_version = (p[1] >> 4) & 3;
	fcf.src_addr_mode = (p[1] >> 6) & 3;

	/* copy fcf and seqNum */
	memcpy(&pf->fcf, &fcf, sizeof(frame802154_fcf_t));
	pf->seq = p[2];
	p += 3;                             /* Skip first three bytes */

	/* Destination address, if any */
	if(fcf.dest_addr_mode) {
		/* Destination PAN */
		pf->dest_pid = p[0] + (p[1] << 8);
		p += 2;

		/* Destination address */
		/*     l = addr_len(fcf.dest_addr_mode); */
		/*     for(c = 0; c < l; c++) { */
		/*       pf->dest_addr.u8[c] = p[l - c - 1]; */
		/*     } */
		/*     p += l; */
		if(fcf.dest_addr_mode == FRAME802154_SHORTADDRMODE) {
			linkaddr_copy((linkaddr_t *)&(pf->dest_addr), &linkaddr_null);
			pf->dest_addr[0] = p[1];
			pf->dest_addr[1] = p[0];
			p += 2;
		} else if(fcf.dest_addr_mode == FRAME802154_LONGADDRMODE) {
			for(c = 0; c < 8; c++) {
				pf->dest_addr[c] = p[7 - c];
			}
			p += 8;
		}
	} else {
		linkaddr_copy((linkaddr_t *)&(pf->dest_addr), &linkaddr_null);
		pf->dest_pid = 0;
	}

	/* Source address, if any */
	if(fcf.src_addr_mode) {
		/* Source PAN */
		if(!fcf.panid_compression) {
			pf->src_pid = p[0] + (p[1] << 8);
			p += 2;
		} else {
			pf->src_pid = pf->dest_pid;
		}

		/* Source address */
		/*     l = addr_len(fcf.src_addr_mode); */
		/*     for(c = 0; c < l; c++) { */
		/*       pf->src_addr.u8[c] = p[l - c - 1]; */
		/*     } */
		/*     p += l; */
		if(fcf.src_addr_mode == FRAME802154_SHORTADDRMODE) {
			linkaddr_copy((linkaddr_t *)&(pf->src_addr), &linkaddr_null);
			pf->src_addr[0] = p[1];
			pf->src_addr[1] = p[0];
			p += 2;
		} else if(fcf.src_addr_mode == FRAME802154_LONGADDRMODE) {
			for(c = 0; c < 8; c++) {
				pf->src_addr[c] = p[7 - c];
			}
			p += 8;
		}
	} else {
		linkaddr_copy((linkaddr_t *)&(pf->src_addr), &linkaddr_null);
		pf->src_pid = 0;
	}

	if(fcf.state_flag){
		/*Timestamp and clock_time if any*/
		pf->timestamp= p[0] + (p[1] << 8);
		p += 2;
		pf->clock_time= p[0] + (p[1] << 8);
		p += 2;
		/*Random seed if any*/
		pf->random_seed= p[0]+ (p[1] << 8);
		p += 2;
		/*Blacklist if any*/
		pf->blacklist= p[0]+ (p[1] << 8);
		p += 2;
		pf->clock_time_sent= p[0]+ (p[1] << 8);
		p += 2;
		pf->radio_timestamp= p[0]+ (p[1] << 8);
		p += 2;
	}

//	printf("stXtf: %04x(%u)\n",
//			pf->radio_timestamp,pf->radio_timestamp);
//	if(pf->radio_timestamp==0){
//		int i;
//		printf("stXtf0: %u ", fcf.dest_addr_mode);
//		for (i=0; i<len; i++){
//			printf("%02x", data[i]);
//		}
//		printf("\n");
//		printf("stXtf: %04x(%u) %04x(%u) %04x(%u) %04x(%u) %04x(%u) %04x(%u) %04x(%u) %04x(%u)\n",
//				pf->timestamp,pf->timestamp,
//				pf->clock_time,pf->clock_time,
//				pf->random_seed,pf->random_seed,
//				pf->blacklist, pf->blacklist,
//				pf->clock_time_sent,pf->clock_time_sent,
//				pf->radio_timestamp,pf->radio_timestamp);
//	}
//	printf("stateRX: %04x(%u) %04x(%u) %04x(%u) %04x(%u)\n",
//			pf->timestamp,pf->timestamp,
//			pf->clock_time,pf->clock_time,
//			pf->random_seed,pf->random_seed,
//			pf->blacklist, pf->blacklist);

#if LLSEC802154_SECURITY_LEVEL
	if(fcf.security_enabled) {
		pf->aux_hdr.security_control.security_level = p[0] & 7;
#if LLSEC802154_USES_EXPLICIT_KEYS
		pf->aux_hdr.security_control.key_id_mode = (p[0] >> 3) & 3;
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
		p += 1;

		memcpy(pf->aux_hdr.frame_counter.u8, p, 4);
		p += 4;

#if LLSEC802154_USES_EXPLICIT_KEYS
		key_id_mode = pf->aux_hdr.security_control.key_id_mode;
		if(key_id_mode) {
			c = (key_id_mode - 1) * 4;
			memcpy(pf->aux_hdr.key_source.u8, p, c);
			p += c;
			pf->aux_hdr.key_index = p[0];
			p += 1;
		}
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
	}
#endif /* LLSEC802154_SECURITY_LEVEL */

//	printf("p: %04x %04x %04x %04x %04x %04x\n",
//			pf->fcf.frame_type,
//			pf->timestamp,
//			pf->clock_time,
//			pf->random_seed,
//			pf->blacklist,
//			pf->clock_time_sent
//	);

	/* header length */
	c = p - data;
	/* payload length */
	pf->payload_len = (len - c);
	/* payload */
	pf->payload = p;
	/* return header length if successful */
	return c > len ? 0 : c;
}

int frame_emmac_create_eb_ack(uint8_t *beacon_data, uint8_t type,
		uint8_t pending,
		uint8_t seq,
		unsigned int w_up_time,
		unsigned int time_in_seconds,
		unsigned int initial_rand_seed,
		unsigned int w_up_ch,
		unsigned int waiting_to_transmit){
	frame802154_t beaconframe;
	beaconframe.fcf.frame_type = FRAME802154_BEACONFRAME;
	beaconframe.fcf.frame_pending = pending;
//	beaconframe.fcf.timestamp_flag= 1;
//	beaconframe.fcf.rand_seed_flag = 1;
	beaconframe.fcf.state_flag =1;
	beaconframe.fcf.ack_required = 0;
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
			((beaconframe.fcf.panid_compression & 1) << 6);
//			|
//			((beaconframe.fcf.timestamp_flag & 1) << 7);
	beacon_data[1] =//((beaconframe.fcf.rand_seed_flag & 1) << 0)|
			((beaconframe.fcf.state_flag & 1) << 1) |
			((beaconframe.fcf.dest_addr_mode & 3) << 2) |
			((beaconframe.fcf.frame_version & 3) << 4) |
			((beaconframe.fcf.src_addr_mode & 3) << 6);
	/* Sequence Number */
	beaconframe.seq = seq;
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
	beacon_data[pos++] = w_up_time%RTIMER_SECOND & 0xff; //w_up_time es el tiempo en tics del ultimo wake-up
	beacon_data[pos++] = (w_up_time%RTIMER_SECOND>> 8) & 0xff;
	beacon_data[pos++] = time_in_seconds & 0xff; //time_in_seconds es el tiempo en secs del ultimo wake-up
	beacon_data[pos++] = (time_in_seconds>> 8) & 0xff;
	beacon_data[pos++] = initial_rand_seed & 0xff;
	beacon_data[pos++] = (initial_rand_seed>>8) & 0xff;
	unsigned int t_seconds = (unsigned int)(clock_seconds());
	beacon_data[pos++] = t_seconds & 0xff;
	beacon_data[pos++] = (t_seconds>>8) & 0xff;
	beacon_data[pos++] = w_up_ch;
	if (type == FRAME802154_BEACONFRAME){
		beacon_data[pos++] = waiting_to_transmit;
//		printf("bts:%02x\n", beacon_data[pos-1]);
	}
	beacon_data[pos++] = 0;  //Se deja vacio para colocar el tiempo actual en tics a nivel fisico
	beacon_data[pos++] = 0;
//	printf("bts:%04x %04x %04x %04x %04x\n",
//			initial_rand_seed,
//			w_up_time,
//			time_in_seconds,
//			w_up_ch,
//			waiting_to_transmit);
	return pos;
}

/** \}   */
