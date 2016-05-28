/**
 * \addtogroup timesynch
 * @{
 */


/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
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
 * $Id: timesynch.c,v 1.9 2009/12/09 18:08:26 adamdunkels Exp $
 */

/**
 * \file
 *         A simple time synchronization mechanism
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Joris Borms  <jborms@etro.vub.ac.be> added drift estimation
 */

#include "net/rime/timesynch.h"
#include "net/packetbuf.h"
#include "net/rime/rime.h"
#include "dev/cc2420/cc2420.h"

#define abs(a) (((signed int)a) < 0 ? -(a) : (a))

//#if TIMESYNCH_CONF_ENABLED
static int authority_level;
static rtimer_clock_t offset;
static unsigned long last_correction;

static short authority_timeouts;
static struct ctimer authority_timer;
#define AUTHORITY_BASE_TIMEOUT (120 * CLOCK_SECOND)

/*---------------------------------------------------------------------------*/
int
timesynch_authority_level(void)
{
  return authority_level;
}
/*---------------------------------------------------------------------------*/
void
timesynch_set_authority_level(int level)
{
  authority_level = level;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_time(void)
{
  return RTIMER_NOW() + offset;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_time_to_rtimer(rtimer_clock_t synched_time)
{
  return synched_time - offset;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_rtimer_to_time(rtimer_clock_t rtimer_time)
{
  return rtimer_time + offset;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_offset(void)
{
  return offset;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_drift(int who)
{
//#if TIMESYNCH_CONF_ENABLED
  /*
   for nodes with authority 0 this is 0 since we consider these a "master clock"

   since rtimer is too fine-grained (resolution of only a few seconds) we need to use
   clock_seconds() to calculate drifting period and convert to rtimer_clock_t

   drift is +/- 40ppm = 80ppm worst case between two drifting clocks
   at 80ppm drift * 4096 rtimer ticks / sec => 0.32768 rtimer ticks drift / sec
    we approximate
   drift (rtimer) = seconds / 3 = 0.3333 rtimer ticks drift / sec

   if drift can get bigger than maximum rtimer resolution, another method must
   be used to represent drifting!
  */
  if (authority_level == 0 && who == 0){
    return 0;
  } else {
  }
}
/*---------------------------------------------------------------------------*/
static int
adjust_offset(rtimer_clock_t authoritative_time, rtimer_clock_t local_time)
{
  if (cc2420_authority_level_of_sender < authority_level){
  /* if the difference is too big, there's a high probability that
   * "authorative time" is incorrect - for example derived from a corrupted
   * message - in this case, ignore it. */
    rtimer_clock_t new_offset = offset + authoritative_time - local_time;
    rtimer_clock_t diff = new_offset - offset;
    if (abs(diff) < timesynch_drift(0) || last_correction == 0){
      offset = new_offset;
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
void
timesynch_incoming_packet(void)
{
  if(packetbuf_totlen() != 0) {
    /* We check the authority level of the sender of the incoming
       packet. If the sending node has a lower authority level than we
       have, we synchronize to the time of the sending node and set our
       own authority level to be one more than the sending node. */
    if(adjust_offset(cc2420_time_of_departure, cc2420_time_of_arrival)) {
      last_correction = clock_seconds();
      authority_timeouts = 0;
      if(cc2420_authority_level_of_sender + 1 != authority_level) {
        authority_level = cc2420_authority_level_of_sender + 1;
      }
    } else if (authority_level == 0){
      last_correction = clock_seconds();
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
periodic_authority_increase(void *ptr)
{
  /* XXX the authority level should be increased over time except
     for the sink node (which has authority 0). */
  if (authority_level){
    if (authority_timeouts){
      authority_level++;
    }

    authority_timeouts++;
    ctimer_set(&authority_timer, authority_timeouts * AUTHORITY_BASE_TIMEOUT,
          periodic_authority_increase, NULL);

  }
}
/*---------------------------------------------------------------------------*/
RIME_SNIFFER(sniffer, timesynch_incoming_packet, NULL);
/*---------------------------------------------------------------------------*/
void
timesynch_init(void)
{
  rime_sniffer_add(&sniffer);
  last_correction = 0;
  authority_timeouts = 0;
  ctimer_set(&authority_timer, AUTHORITY_BASE_TIMEOUT,
      periodic_authority_increase, NULL);

}
/*---------------------------------------------------------------------------*/
 /* TIMESYNCH_CONF_ENABLED */
/** @} */
