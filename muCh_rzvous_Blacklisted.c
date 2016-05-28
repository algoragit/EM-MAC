/*Multichannel Rendezvous with Blacklisted Channels*/
/*Detecting Channel Conditions*/


#define CCA_CHECK_TIME                     RTIMER_ARCH_SECOND/ 
8192
#define threshold_Cbad  15

int canal;
NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX,&canal);
static unsigned int badness_metric[canal+1]= {0};
int wake_up_channel;
unsigned int blacklist = 0x0000;
unsigned short count = 0;
static rtimer_clock_t t0;
uint8_t collisions;
//blacklist as bitmap 2 bytes 0x0000


/*Based on the channel badness metrics, a node in EM-MAC 
selects the set of channels it switches among. Every node 
maintains its own channel blacklist that identifies the 
channels the node regards as “bad” channels.*/

static void
multichannel_rendezvous(void)
{
/* if the next chosen channel is on the node’s channel 
blacklist, the node stays on its current channel */

/*If all channels have a badness metric beyond Cbad, the 
“least bad” channel is removed from the blacklist.*/
if (blacklist == pow(2, canal+1)) // It should be (canal-1), I think, because, for example, if canal=1 is blacklisted and you check it against the blacklist vector, it should be equal to 0x0001, which would be pow(2,0)
{
	int i,least_bad_channel;
	least_bad_channel = 0;

	for(i=0;i<=canal;i++)
	{
		if(badness_metric[least_bad_channel] > badness_metric[i])
			least_bad_channel = i;

	}

}

/* This function doesn't return any value. What should it return?
 * No channel is removed from the blacklist.
 * I think this function is incomplete.
 */


/*A node R expires and removes from its channel blacklist the
channels that have been on the blacklist for more than Tblack 
time and resets the badness metric of such channel to 0. */


/*To enable potential senders to learn its blacklisted 
channels, a node R represents its blacklisted channels using 
a bitmap and embeds it in its wake-up beacons. */

/*If R changes its channel blacklist, a sender S learns the 
updated blacklist of R after receiving a wake-up beacon from 
R. */



}



/*Detecta las condiciones del canal y lo añade o elimina de la blacklist*/
static int 
channel_conditions()
{
	NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL,&
wake_up_channel);

	if(NETSTACK_RADIO.channel_clear()!= 0 )
	{

		count = 0;
		/*Channel idle*/
		/*Send the packet and channel's badness metric -1
(metric >=0). After a node sends a wake-up beacon: if CCA
==0 and not valid packet --> retransmit packet. If collision
resolution fails --> channel's badness metric +2*/

		if (badness_metric[wake_up_channel] > 0)
		{
			//If badness_metric<=15 remove channel from blacklist
			if (badness_metric[wake_up_channel] == threshold_Cbad+1)
				blacklist ^= (1<<(wake_up_channel)); //XOR  //I think you should only remove the channel from the blacklist once it has passed T_black.
			/* A node R expires and removes from its channel blacklist the channels that have been on the blacklist for more than Tblack time and resets
			 * the badness metric of such channel to 0. */
			--badness_metric[wake_up_channel];
		}
			
		return 1;

	}
else if (NETSTACK_RADIO.channel_clear()== 0 )
	{
		count++;

		if(count < 3)
		{
			t0 = RTIMER_NOW();
			/*From ContikiMAC*/
			while(RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + 
CCA_CHECK_TIME)) {}		/* Then, after waiting this time, it should check again, doesn't it? */
		}
		/*If channel still busy after three such CCA checks:
channel's badness metric +2 and sleep*/
		else
		{
			badness_metric[wake_up_channel]+= 2;
			//If badness_metric>15 include channel in blacklist
			if (badness_metric[wake_up_channel] > threshold_Cbad)
				blacklist |= (1<<(wake_up_channel));
			//Sleep
			return 0;
		}
	}
}
