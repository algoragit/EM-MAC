

#include "net/mac/simplerdc/simplerdc.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include <stdio.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTADDR(addr) PRINTF("%d.%d", addr->u8[0], addr->u8[1])
#else
#define PRINTF(...)
#define PRINTADDR(addr)
#endif

 static struct rtimer r_timer;
 static int is_syncronized=0;
 static int pow_cycl_timestamp=0;
 static int node_identity;
 /***************************************************************************/
 static int off(int keep_radio_on)
 {
   if(keep_radio_on) {
     return NETSTACK_RADIO.on();
   } else {
     return NETSTACK_RADIO.off();
   }
 }
 /***************************************************************************/
 static int  on(void)
 {
   return NETSTACK_RADIO.on();

 }
 /****************************************************************************/

 static int check_if_radio_on(void)
 {
 	int x;
 	NETSTACK_RADIO.get_value(RADIO_PARAM_POWER_MODE,&x);
 	if(x==RADIO_POWER_MODE_ON)
 		return 1;
 	else
 		return 0;
 }
 /****************************************************************************/

 static void Powercycle_Manager(void)
 {
	 	 if(check_if_radio_on()==1)
	 	 {
	 		 off(0);
	 		 rtimer_set(&r_timer,(RTIMER_NOW()+(RTIMER_SECOND/2)), 0,(void (*)(struct rtimer *, void *))Powercycle_Manager,NULL);
	 	 }
	 	 else
	 	 {
	 		 on();
	 		pow_cycl_timestamp=RTIMER_NOW();
	 		rtimer_set(&r_timer,(RTIMER_NOW()+(RTIMER_SECOND/16)), 0,(void (*)(struct rtimer *, void *))Powercycle_Manager,NULL);
	 	 }
	 	return;
 }
/*****************************************************************************/

static int
send_one_packet(mac_callback_t sent, void *ptr)
{
  int ret;
  int last_sent_ok = 0;
  while(check_if_radio_on()==0){}
  //packetbuf_set_attr(PACKETBUF_ATTR_NODE_TIMESTAMP,60000);
 packetbuf_set_attr(PACKETBUF_ATTR_NODE_TIMESTAMP,(unsigned int)(pow_cycl_timestamp+(RTIMER_SECOND/16)-RTIMER_NOW()));
  //printf("Rece:  %u",packetbuf_attr(PACKETBUF_ATTR_NODE_TIMESTAMP));

  if(node_identity==1 || is_syncronized==1)
    {
	//unsigned int const num=pow_cycl_timestamp+(RTIMER_SECOND/16)-RTIMER_NOW();
  	//packetbuf_set_attr(PACKETBUF_ATTR_NODE_TIMESTAMP,num);
  	printf("Number is: %u",(pow_cycl_timestamp+(RTIMER_SECOND/16)-RTIMER_NOW()));
  	packetbuf_set_attr(PACKETBUF_ATTR_NODE_ID,1);
  	packetbuf_set_attr(PACKETBUF_ATTR_MAX,30);
  	 printf("Rece:  %u",packetbuf_attr(PACKETBUF_ATTR_MAX));
    }
    else
    {
    packetbuf_set_attr(PACKETBUF_ATTR_NODE_ID,node_identity);
    }

  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
  const linkaddr_t *laddr=packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
      printf("Direccion de destino del paquete transmitido:%d:%d:%d:%d:%d:%d:%d:%d\n",laddr->u8[0],laddr->u8[1],laddr->u8[2],laddr->u8[3],laddr->u8[4],laddr->u8[5],laddr->u8[6],laddr->u8[7]);
     // set_hdrptr(0);
      printf("Numero de bytes de encabezado: %u,  %u",packetbuf_datalen(),packetbuf_totlen());
      if(NETSTACK_FRAMER.create_and_secure() < 0) {
    /* Failed to allocate space for headers */
    PRINTF("simplerdc: send failed, too large header\n");
    ret = MAC_TX_ERR_FATAL;
  } else {

    switch(NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen())) {
    case RADIO_TX_OK:
      ret = MAC_TX_OK;
      break;
    case RADIO_TX_COLLISION:
      ret = MAC_TX_COLLISION;
      break;
    case RADIO_TX_NOACK:
      ret = MAC_TX_NOACK;
      break;
    default:
      ret = MAC_TX_ERR;
      break;
    }

  }
  if(ret == MAC_TX_OK) {
    last_sent_ok = 1;
  }
  mac_call_sent_callback(sent, ptr, ret, 1);
  return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
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
	packetbuf_set_attr(PACKETBUF_ATTR_MAX,30);
	const linkaddr_t *laddr;
	if(NETSTACK_FRAMER.parse() < 0) {
      printf("nullrdc: failed to parse %u\n", packetbuf_datalen());
    } else if(!linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                           &linkaddr_node_addr) &&
              !packetbuf_holds_broadcast()) {
      PRINTF("simplerdc: packet not for us\n");
    } else {
    	laddr=packetbuf_addr(PACKETBUF_ADDR_SENDER);
    	printf("Direccion de procedencia del paquete recibido:%d:%d:%d:%d:%d:%d:%d:%d\n",laddr->u8[0],laddr->u8[1],laddr->u8[2],laddr->u8[3],laddr->u8[4],laddr->u8[5],laddr->u8[6],laddr->u8[7]);
    	printf("Numbersssss:  %u",packetbuf_attr(PACKETBUF_ATTR_MAX));
    	//if(packetbuf_attr(PACKETBUF_ATTR_NODE_ID)==1 && is_syncronized==0 )
    	//{
    		//is_syncronized=1;
    		printf("Receiversssssssss:  %u",packetbuf_attr(PACKETBUF_ATTR_NODE_TIMESTAMP));
    		//rtimer_set(&r_timer,(RTIMER_NOW()+packetbuf_attr(PACKETBUF_ATTR_NODE_TIMESTAMP)), 0,(void (*)(struct rtimer *, void *))Powercycle_Manager,NULL);

    	//}
    	NETSTACK_MAC.input();
    }
  }


/*---------------------------------------------------------------------------*/
static unsigned short channel_check_interval(void)
{
  return 8;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
	//set_hdrptr(0);
  static radio_value_t chann_min;
  static radio_value_t chann_max;
  if(NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN,&chann_min)==RADIO_RESULT_OK &&
		   NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX,&chann_max)==RADIO_RESULT_OK ){
  printf("Radio channel interval: %d-%d\n",chann_min,chann_max);
  }

  node_identity=linkaddr_node_addr.u8[7];
  on();
  if(node_identity==1)
  {
	  Powercycle_Manager();
  }
  return;

}
/*---------------------------------------------------------------------------*/
const struct rdc_driver simplerdc_driver = {
  "simplerdc",
  init,
  send_packet,
  send_list,
  packet_input,
  on,
  off,
  channel_check_interval,
};
/*---------------------------------------------------------------------------*/
