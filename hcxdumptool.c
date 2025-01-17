#include "include/hcxdumptool.h"
#include "include/cJSON.h"

static bool deauthenticationflag = true;
static bool proberequestflag = true;
static bool associationflag = true;
static bool reassociationflag = true;
static bool activemonitorflag = true;

static u8 wanteventflag = 0;
static u8 exiteapolpmkidflag = 0;
static u8 exiteapolm2flag = 0;
static u8 exiteapolm3flag = 0;

static pid_t hcxpid = 0;
static bool clearScreen;

static unsigned int seed = 7;

static int fd_socket_nl = 0; // netlink socket
static int fd_socket_rt = 0; // netlink route socket
static int fd_socket_unix = 0;
static int fd_socket_rx = 0;
static int fd_socket_tx = 0;
static int fd_timer1 = 0;
static int fd_pcapng = 0;

static size_t pcap_buffer_size = 0;
static u8 *pcap_buffer;

static u8 rdsort = 0;
static long int totalcapturedcount = 0;
static long int wecbcount = 0;
static long int wepbcount = 0;
static long int widbcount = 0;
static long int wshbcount = 0;

static struct sock_fprog bpf = {0};

static int ifaktindex = 0;
static u8 ifaktstatus = 0;
static u8 ifakttype = 0;

static frequencylist_t *ifaktfrequencylist = NULL;
static char ifaktname[IF_NAMESIZE] = {0};
static u8 ifakthwmac[ETH_ALEN] = {0};

static u16 nlfamily = 0;
static u32 nlseqcounter = 1;

static size_t ifpresentlistcounter = 0;

static size_t scanlistindex = 0;
static frequencylist_t *scanlist = NULL;

static interface_t *ifpresentlist;

static aplist_t *aplist = NULL;			// AP List (For AP Interaction)
static aprglist_t *aprglist = NULL;		// Rogue AP List (For Client Interaction)
static clientlist_t *clientlist = NULL; // Client List (For AP / Client Interaction)
static maclist_t *maclist = NULL;
static u64 lifetime = 0;
static u32 ouiaprg = 0;
static u32 nicaprg = 0;
static u32 ouiclientrg = 0;
static u32 nicclientrg = 0;
static u64 replaycountrg = 0;

static struct timespec tspecakt = {0};
static u64 tsakt = 0;
static u64 tsfirst = 0;
static u64 tshold = 0;
static u64 tottime = 0;
static u64 timehold = TIMEHOLD;
static int timerwaitnd = TIMER_EPWAITND;

static u64 errorcountmax = ERROR_MAX;
static u64 errorcount = 0;
static u32 watchdogcountmax = WATCHDOG_MAX;
static u32 attemptapmax = ATTEMPTAP_MAX;
static u32 attemptclientmax = ATTEMPTCLIENT_MAX;

static u64 packetcount = 1;

static size_t beaconindex = 0;
static size_t proberesponseindex = 0;

static u32 beacontxmax = BEACONTX_MAX;
static u32 proberesponsetxmax = PROBERESPONSETX_MAX;

static u64 beacontimestamp = 1;

static rth_t *rth = NULL;
static ssize_t packetlen = 0;
static u8 *packetptr = NULL;
static u16 ieee82011len = 0;
static u8 *ieee82011ptr = NULL;
static u16 payloadlen = 0;
static u8 *payloadptr = NULL;
static ieee80211_mac_t *macfrx = NULL;
static u8 *llcptr = NULL;
static ieee80211_llc_t *llc = NULL;
static u16 eapauthlen = 0;
static ieee80211_eapauth_t *eapauth;
static u16 eapauthpllen = 0;
static u8 *eapauthplptr = NULL;
static u16 eapolpllen = 0;
static u8 *eapolplptr = NULL;
static ieee80211_wpakey_t *wpakey;
static ieee80211_pmkid_t *pmkid;
static u16 keyinfo = 0;
static u8 kdv = 0;

static enhanced_packet_block_t *epbhdr = NULL;

static ieee80211_mac_t *macftx = NULL;
static u16 seqcounter1 = 1; /* deauthentication / disassociation */
static u16 seqcounter2 = 1; /* proberequest authentication association */
static u16 seqcounter3 = 1; /* probereresponse authentication response 3 */
static u16 seqcounter4 = 1; /* beacon */

/*---------------------------------------------------------------------------*/
static const char *macaprgfirst = "internet";
/*---------------------------------------------------------------------------*/
static const u8 beacondata[] =
	{
		/* Tag: Supported Rates 1(B), 2(B), 5.5(B), 11(B), 6(B), 9, 12(B), 18, [Mbit/sec] */
		0x01,
		0x08,
		0x82,
		0x84,
		0x8b,
		0x96,
		0x8c,
		0x12,
		0x98,
		0x24,
		/* Tag: DS Parameter set: Current Channel: 1 */
		0x03,
		0x01,
		0x01,
		/* Tag: TIM Information */
		0x05,
		0x04,
		0x00,
		0x01,
		0x00,
		0x00,
		/* Tag: Extended Supported Rates 24(B), 36, 48, 54, [Mbit/sec] */
		0x32,
		0x04,
		0xb0,
		0x48,
		0x60,
		0x6c,
		/* Tag: RSN Information CCM CCM PSK */
		0x30,
		0x14,
		0x01,
		0x00,
		0x00,
		0x0f,
		0xac,
		0x04,
		0x01,
		0x00,
		0x00,
		0x0f,
		0xac,
		0x04,
		0x01,
		0x00,
		0x00,
		0x0f,
		0xac,
		0x02,
		0x00,
		0x00,
};
#define BEACONDATA_SIZE sizeof(beacondata)
/*---------------------------------------------------------------------------*/
static const u8 proberesponsedata[] =
	{
		/* Tag: Supported Rates 1(B), 2(B), 5.5(B), 11(B), 6(B), 9, 12(B), 18, [Mbit/sec] */
		0x01,
		0x08,
		0x82,
		0x84,
		0x8b,
		0x96,
		0x8c,
		0x12,
		0x98,
		0x24,
		/* Tag: DS Parameter set: Current Channel: 1 */
		0x03,
		0x01,
		0x01,
		/* Tag: Extended Supported Rates 24(B), 36, 48, 54, [Mbit/sec] */
		0x32,
		0x04,
		0xb0,
		0x48,
		0x60,
		0x6c,
		/* Tag: RSN Information CCM CCM PSK */
		0x30,
		0x14,
		0x01,
		0x00,
		0x00,
		0x0f,
		0xac,
		0x04,
		0x01,
		0x00,
		0x00,
		0x0f,
		0xac,
		0x04,
		0x01,
		0x00,
		0x00,
		0x0f,
		0xac,
		0x02,
		0x00,
		0x00,
};
#define PROBERESPONSEDATA_SIZE sizeof(proberesponsedata)
/*---------------------------------------------------------------------------*/
static const u8 proberequest_undirected_data[] =
	{
		/* Tag: Wildcard */
		0x00, 0x00,
		/* Tag: Supported Rates 1(B), 2(B), 5.5(B), 11(B), 6(B), 9, 12(B), 18, [Mbit/sec] */
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x12, 0x98, 0x24,
		/* Tag: Extended Supported Rates 24(B), 36, 48, 54, [Mbit/sec] */
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c};
#define PROBEREQUEST_UNDIRECTED_SIZE sizeof(proberequest_undirected_data)
/*---------------------------------------------------------------------------*/
static const u8 authenticationrequestdata[] =
	{
		0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
#define AUTHENTICATIONREQUEST_SIZE sizeof(authenticationrequestdata)
/*---------------------------------------------------------------------------*/
static const u8 authenticationresponsedata[] =
	{
		0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
#define AUTHENTICATIONRESPONSE_SIZE sizeof(authenticationresponsedata)
/*---------------------------------------------------------------------------*/
static const u8 reassociationrequestdata[] =
	{
		/* Tag: Supported Rates 1(B), 2(B), 5.5(B), 11(B), 6(B), 9, 12(B), 18, [Mbit/sec] */
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x12, 0x98, 0x24,
		/* Tag: Extended Supported Rates 24(B), 36, 48, 54, [Mbit/sec] */
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c,
		/* RSN information AES PSK (WPA2) */
		0x30, 0x14, 0x01, 0x00,
		0x00, 0x0f, 0xac, 0x04, /* group cipher */
		0x01, 0x00,				/* count */
		0x00, 0x0f, 0xac, 0x04, /* pairwise cipher */
		0x01, 0x00,				/* count */
		0x00, 0x0f, 0xac, 0x02, /* AKM */
		0x80, 0x00,
		/* RM Enabled Capabilities */
		0x46, 0x05, 0x7b, 0x00, 0x02, 0x00, 0x00,
		/* Supported Operating Classes */
		0x3b, 0x04, 0x51, 0x51, 0x53, 0x54};
#define REASSOCIATIONREQUEST_SIZE sizeof(reassociationrequestdata)
/*---------------------------------------------------------------------------*/
static const u8 associationrequestcapa[] =
	{
		0x31, 0x04, 0x05, 0x00};
#define ASSOCIATIONREQUESTCAPA_SIZE sizeof(associationrequestcapa)
/*---------------------------------------------------------------------------*/
static const u8 associationrequestdata[] =
	{
		/* Tag: Supported Rates 1(B), 2(B), 5.5(B), 11(B), 6(B), 9, 12(B), 18, [Mbit/sec] */
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x12, 0x98, 0x24,
		/* Tag: Extended Supported Rates 24(B), 36, 48, 54, [Mbit/sec] */
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c,
		/* RSN information AES PSK (WPA2) */
		0x30, 0x14, 0x01, 0x00,
		0x00, 0x0f, 0xac, 0x04, /* group cipher */
		0x01, 0x00,				/* count */
		0x00, 0x0f, 0xac, 0x04, /* pairwise cipher */
		0x01, 0x00,				/* count */
		0x00, 0x0f, 0xac, 0x02, /* AKM */
		0x80, 0x00,
		/* RM Enabled Capabilities */
		0x46, 0x05, 0x7b, 0x00, 0x02, 0x00, 0x00,
		/* Supported Operating Classes */
		0x3b, 0x04, 0x51, 0x51, 0x53, 0x54};
#define ASSOCIATIONREQUEST_SIZE sizeof(associationrequestdata)
/*---------------------------------------------------------------------------*/
static const u8 associationresponsedata[] =
	{
		/* Tag: Supported Rates 1(B), 2(B), 5.5(B), 11(B), 6(B), 9, 12(B), 18, [Mbit/sec] */
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x12, 0x98, 0x24,
		/* Tag: Extended Supported Rates 24(B), 36, 48, 54, [Mbit/sec] */
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c,
		/* Tag: Extended Capabilities (8 octets) */
		// 0x7f, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40
};
#define ASSOCIATIONRESPONSEDATA_SIZE sizeof(associationresponsedata)
/*---------------------------------------------------------------------------*/
static u8 eapolm1data[] =
	{
		/* LLC */
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e,
		/* M1 WPA2 */
		0x02,
		0x03,
		0x00, 0x5f,
		0x02,
		0x00, 0x8a,
		0x00, 0x10,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00};
#define EAPOLM1DATA_SIZE sizeof(eapolm1data)
/*---------------------------------------------------------------------------*/
static const u8 eaprequestiddata[] =
	{
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e,
		0x01, 0x00, 0x00, 0x05, 0x01, 0x01, 0x00, 0x05, 0x01};
#define EAPREQUESTID_SIZE sizeof(eaprequestiddata)
/*---------------------------------------------------------------------------*/
/* interface bit rate */
static const u8 legacy241mbdata[] =
	{
		0x10, 0x00,
		0x5a, 0x80,
		0x0c, 0x00,
		0x01, 0x80,
		0x05, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00};
#define LEGACYXXXMB_SIZE sizeof(legacy241mbdata)
/*---------------------------------------------------------------------------*/
static const u8 legacy56mbdata[] =
	{
		0x10, 0x00,
		0x5a, 0x80,
		0x0c, 0x00,
		0x01, 0x80,
		0x05, 0x00, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00};
/*---------------------------------------------------------------------------*/
static u8 macaprg[ETH_ALEN] = {0};
static u8 macclientrg[ETH_ALEN + 2] = {0};
static u8 anoncerg[32] = {0};
static u8 snoncerg[32] = {0};
static char weakcandidate[PSK_MAX];
static char timestring1[TIMESTRING_LEN];

static char country[3];

static authseqakt_t authseqakt = {0};

static u8 nltxbuffer[NLTX_SIZE] = {0};
static u8 nlrxbuffer[NLRX_SIZE] = {0};

static u8 epb[PCAPNG_SNAPLEN * 2] = {0};
static u8 epbown[WLTXBUFFER] = {0};
static u8 wltxbuffer[WLTXBUFFER] = {0};
static u8 wltxnoackbuffer[WLTXBUFFER] = {0};

static char rtb[RTD_LEN] = {0};

/*===========================================================================*/

/*---------------------------------------------------------------------------*/

/*===========================================================================*/
/* frequency handling */
/*---------------------------------------------------------------------------*/
static u32 channel_to_frequency(u16 channel, u16 band)
{
	if (channel <= 0)
		return 0;
	switch (band)
	{
	case NL80211_BAND_2GHZ:
		if (channel == 14)
			return 2484;
		else if (channel < 14)
			return 2407 + (channel * 5);
		break;

	case NL80211_BAND_5GHZ:
		if (channel >= 182 && channel <= 196)
			return 4000 + (channel * 5);
		else
			return 5000 + channel * 5;
		break;

	case NL80211_BAND_6GHZ:
		if (channel == 2)
			return 5935;
		if (channel <= 233)
			return 5950 + (channel * 5);
		break;

	case NL80211_BAND_60GHZ:
		if (channel < 7)
			return 56160 + (channel * 2160);
		break;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	case NL80211_BAND_S1GHZ:
		return 902000 + (channel * 500);
#endif
	}
	return 0;
}
/*---------------------------------------------------------------------------*/
static u16 frequency_to_channel(u32 frequency)
{
	if (frequency == 2484)
		return 14;
	else if (frequency < 2484)
		return (frequency - 2407) / 5;
	else if (frequency >= 4910 && frequency <= 4980)
		return (frequency - 4000) / 5;
	else if (frequency < 5925)
		return (frequency - 5000) / 5;
	else if (frequency == 5935)
		return 2;
	else if (frequency <= 45000)
		return (frequency - 5950) / 5;
	else if (frequency >= 58320 && frequency <= 70200)
		return (frequency - 56160) / 2160;
	else
		return 0;
}

/*===========================================================================*/
static u16 addoption(u8 *posopt, u16 optioncode, u16 optionlen, char *option)
{
	static u16 padding;
	static option_header_t *optionhdr;

	optionhdr = (option_header_t *)posopt;
	optionhdr->option_code = optioncode;
	optionhdr->option_length = optionlen;
	padding = (4 - (optionlen % 4)) % 4;
	memset(optionhdr->option_data, 0, optionlen + padding);
	memcpy(optionhdr->option_data, option, optionlen);
	return optionlen + padding + 4;
}
/*---------------------------------------------------------------------------*/

/*===========================================================================*/
static u16 addcustomoption(u8 *pospt)
{
	static u16 colen;
	static option_header_t *optionhdr;
	static optionfield64_t *of;

	optionhdr = (option_header_t *)pospt;
	optionhdr->option_code = SHB_CUSTOM_OPT;
	colen = OH_SIZE;
	memcpy(pospt + colen, &hcxmagic, 4);
	colen += 4;
	memcpy(pospt + colen, &hcxmagic, 32);
	colen += 32;
	colen += addoption(pospt + colen, OPTIONCODE_MACAP, 6, (char *)macaprg);
	of = (optionfield64_t *)(pospt + colen);
	of->option_code = OPTIONCODE_RC;
	of->option_length = 8;
	of->option_value = replaycountrg;
	colen += 12;
	colen += addoption(pospt + colen, OPTIONCODE_ANONCE, 32, (char *)anoncerg);
	colen += addoption(pospt + colen, OPTIONCODE_MACCLIENT, 6, (char *)macclientrg);
	colen += addoption(pospt + colen, OPTIONCODE_SNONCE, 32, (char *)snoncerg);
	colen += addoption(pospt + colen, OPTIONCODE_WEAKCANDIDATE, strnlen(weakcandidate, PSK_MAX), weakcandidate);
	colen += addoption(pospt + colen, 0, 0, NULL);
	optionhdr->option_length = colen - OH_SIZE;
	return colen;
}
/*===========================================================================*/
static inline void writeepbm1(void)
{
	static ssize_t epblen;
	static ssize_t ii;
	static u64 tsm1;
	static u16 padding;
	static total_length_t *totallength;

	ii = RTHTX_SIZE + EPB_SIZE;
	macftx = (ieee80211_mac_t *)&epbown[ii];
	macftx->type = IEEE80211_FTYPE_DATA;
	macftx->subtype = IEEE80211_STYPE_DATA;
	macftx->from_ds = 1;
	macftx->duration = 0x0431;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macfrx->addr1, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	ii += MAC_SIZE_NORM;
	memcpy(&epbown[ii], &eapolm1data, EAPOLM1DATA_SIZE);
	ii += EAPOLM1DATA_SIZE;
	epbhdr = (enhanced_packet_block_t *)epbown;
	epblen = EPB_SIZE;
	epbhdr->block_type = EPBID;
	epbhdr->interface_id = 0;
	epbhdr->cap_len = ii;
	epbhdr->org_len = ii;
	tsm1 = tsakt - 1;
	epbhdr->timestamp_high = tsm1 >> 32;
	epbhdr->timestamp_low = (u32)tsm1 & 0xffffffff;
	padding = (4 - (epbhdr->cap_len % 4)) % 4;
	epblen += ii;
	memset(&epbown[epblen], 0, padding);
	epblen += padding;
	epblen += addoption(epbown + epblen, SHB_EOC, 0, NULL);
	totallength = (total_length_t *)(epbown + epblen);
	epblen += TOTAL_SIZE;
	epbhdr->total_length = epblen;
	totallength->total_length = epblen;
	if (extend_and_copy_pcap(&epbown, epblen) != epblen)
		errorcount++;
	/* if (write(fd_pcapng, &epbown, epblen) != epblen)
		errorcount++; */
	wepbcount++;
	return;
}
/*===========================================================================*/
static inline void writeepb(void)
{
	static ssize_t epblen;
	static u16 padding;
	static total_length_t *totallength;

	epbhdr = (enhanced_packet_block_t *)epb;
	epblen = EPB_SIZE;
	epbhdr->block_type = EPBID;
	epbhdr->interface_id = 0;
	epbhdr->cap_len = packetlen;
	epbhdr->org_len = packetlen;
	epbhdr->timestamp_high = tsakt >> 32;
	epbhdr->timestamp_low = (u32)tsakt & 0xffffffff;
	padding = (4 - (epbhdr->cap_len % 4)) % 4;
	epblen += packetlen;
	memset(&epb[epblen], 0, padding);
	epblen += padding;
	epblen += addoption(epb + epblen, SHB_EOC, 0, NULL);
	totallength = (total_length_t *)(epb + epblen);
	epblen += TOTAL_SIZE;
	epbhdr->total_length = epblen;
	totallength->total_length = epblen;
	if (extend_and_copy_pcap(&epb, epblen) != epblen)
		errorcount++;
	/* if (write(fd_pcapng, &epb, epblen) != epblen)
		errorcount++; */
	wepbcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static bool writeshb(void)
{
	static ssize_t shblen;
	static section_header_block_t *shbhdr;
	static total_length_t *totallength;
	static struct utsname unameData;
	static char sysinfo[SHB_SYSINFO_LEN];
	static u8 shb[PCAPNG_BLOCK_SIZE];

	memset(&shb, 0, PCAPNG_BLOCK_SIZE);
	shblen = SHB_SIZE;
	shbhdr = (section_header_block_t *)shb;
	shbhdr->block_type = PCAPNGBLOCKTYPE;
	shbhdr->byte_order_magic = PCAPNGMAGICNUMBER;
	shbhdr->major_version = PCAPNG_MAJOR_VER;
	shbhdr->minor_version = PCAPNG_MINOR_VER;
	shbhdr->section_length = -1;
	if (uname(&unameData) == 0)
	{
		shblen += addoption(shb + shblen, SHB_HARDWARE, strlen(unameData.machine), unameData.machine);
		snprintf(sysinfo, SHB_SYSINFO_LEN, "%s %s", unameData.sysname, unameData.release);
		shblen += addoption(shb + shblen, SHB_OS, strlen(sysinfo), sysinfo);
		snprintf(sysinfo, SHB_SYSINFO_LEN, "net-nomad-hcx %s", VERSION_TAG);
		shblen += addoption(shb + shblen, SHB_USER_APPL, strlen(sysinfo), sysinfo);
	}
	shblen += addcustomoption(shb + shblen);
	shblen += addoption(shb + shblen, SHB_EOC, 0, NULL);
	totallength = (total_length_t *)(shb + shblen);
	shblen += TOTAL_SIZE;
	shbhdr->total_length = shblen;
	totallength->total_length = shblen;
	if (extend_and_copy_pcap(&shb, shblen) != shblen)
	{
		return false;
	}
	wshbcount++;
	return true;
}
/*---------------------------------------------------------------------------*/
static bool writeidb(void)
{
	static ssize_t idblen;
	static interface_description_block_t *idbhdr;
	static total_length_t *totallength;
	static char tr[1];
	static u8 idb[PCAPNG_BLOCK_SIZE];

	memset(&idb, 0, PCAPNG_BLOCK_SIZE);
	idblen = IDB_SIZE;
	idbhdr = (interface_description_block_t *)idb;
	idbhdr->block_type = IDBID;
	idbhdr->linktype = DLT_IEEE802_11_RADIO;
	idbhdr->reserved = 0;
	idbhdr->snaplen = PCAPNG_SNAPLEN;
	idblen += addoption(idb + idblen, IF_NAME, strnlen(ifaktname, IF_NAMESIZE), ifaktname);
	idblen += addoption(idb + idblen, IF_MACADDR, 6, (char *)ifakthwmac);
	tr[0] = TSRESOL_NSEC;
	idblen += addoption(idb + idblen, IF_TSRESOL, 1, tr);
	idblen += addoption(idb + idblen, SHB_EOC, 0, NULL);
	totallength = (total_length_t *)(idb + idblen);
	idblen += TOTAL_SIZE;
	idbhdr->total_length = idblen;
	totallength->total_length = idblen;
	if (extend_and_copy_pcap(&idb, idblen) != idblen)
	{
		return false;
	}
	widbcount++;
	return true;
}
/*---------------------------------------------------------------------------*/
static bool writecb(void)
{
	static ssize_t cblen;
	static custom_block_t *cbhdr;
	static optionfield64_t *of;
	static total_length_t *totallength;
	static u8 cb[PCAPNG_BLOCK_SIZE];

	memset(&cb, 0, PCAPNG_BLOCK_SIZE);
	cbhdr = (custom_block_t *)cb;
	cblen = CB_SIZE;
	cbhdr->block_type = CBID;
	cbhdr->total_length = CB_SIZE;
	memcpy(cbhdr->pen, &hcxmagic, 4);
	memcpy(cbhdr->hcxm, &hcxmagic, 32);
	cblen += addoption(cb + cblen, OPTIONCODE_MACAP, 6, (char *)macaprg);
	of = (optionfield64_t *)(cb + cblen);
	of->option_code = OPTIONCODE_RC;
	of->option_length = 8;
	of->option_value = replaycountrg;
	cblen += 12;
	cblen += addoption(cb + cblen, OPTIONCODE_ANONCE, 32, (char *)anoncerg);
	cblen += addoption(cb + cblen, OPTIONCODE_MACCLIENT, 6, (char *)macclientrg);
	cblen += addoption(cb + cblen, OPTIONCODE_SNONCE, 32, (char *)snoncerg);
	cblen += addoption(cb + cblen, OPTIONCODE_WEAKCANDIDATE, strnlen(weakcandidate, PSK_MAX), weakcandidate);
	cblen += addoption(cb + cblen, 0, 0, NULL);
	totallength = (total_length_t *)(cb + cblen);
	cblen += TOTAL_SIZE;
	cbhdr->total_length = cblen;
	totallength->total_length = cblen;
	if (extend_and_copy_pcap(&cb, cblen) != cblen)
	{
		return false;
	}
	wecbcount++;
	return true;
}

/*---------------------------------------------------------------------------*/

static bool setup_pcap_buffer()
{
	pcap_buffer = (u8 *)calloc(pcap_buffer_size + 1, sizeof(u8));

	if (pcap_buffer == NULL)
	{
		return false;
	}
	if (writeshb() == false)
		return false;
	if (writeidb() == false)
		return false;
	if (writecb() == false)
		return false;
	return true;
}

static ssize_t extend_and_copy_pcap(const void *__buf, ssize_t __n)
{
	ssize_t pcap_new_total = pcap_buffer_size + __n;

	u8 *newbuffer = realloc(pcap_buffer, pcap_new_total * sizeof(u8));
	if (newbuffer == NULL)
	{
		return false;
	}
	pcap_buffer = newbuffer;
	memcpy(pcap_buffer + pcap_buffer_size, __buf, __n);
	pcap_buffer_size = pcap_new_total;

	return __n;
}

/*===========================================================================*/
/*===========================================================================*/
/* TX 802.11 */
/*===========================================================================*/
static inline void send_80211_associationrequest_org(size_t i)
{
	ssize_t ii;

	ii = RTHTXNOACK_SIZE;
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[ii];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_ASSOC_REQ;
	wltxnoackbuffer[ii + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, (aplist + i)->macclient, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter2++ << 4;
	if (seqcounter1 > 4095)
		seqcounter2 = 1;
	ii += MAC_SIZE_NORM;
	memcpy(&wltxnoackbuffer[ii], &associationrequestcapa, ASSOCIATIONREQUESTCAPA_SIZE);
	ii += ASSOCIATIONREQUESTCAPA_SIZE;
	wltxnoackbuffer[ii++] = 0;
	wltxnoackbuffer[ii++] = (aplist + i)->ie.essidlen;
	memcpy(&wltxnoackbuffer[ii], (aplist + i)->ie.essid, (aplist + i)->ie.essidlen);
	ii += (aplist + i)->ie.essidlen;
	memcpy(&wltxnoackbuffer[ii], &associationrequestdata, ASSOCIATIONREQUEST_SIZE);
	if (((aplist + i)->ie.flags & APGS_CCMP) == APGS_CCMP)
		wltxnoackbuffer[ii + 0x17] = RSN_CS_CCMP;
	else if (((aplist + i)->ie.flags & APGS_TKIP) == APGS_TKIP)
		wltxnoackbuffer[ii + 0x17] = RSN_CS_TKIP;
	if (((aplist + i)->ie.flags & APCS_CCMP) == APCS_CCMP)
		wltxnoackbuffer[ii + 0x1d] = RSN_CS_CCMP;
	else if (((aplist + i)->ie.flags & APCS_TKIP) == APCS_TKIP)
		wltxnoackbuffer[ii + 0x1d] = RSN_CS_TKIP;
	ii += ASSOCIATIONREQUEST_SIZE;
	if ((write(fd_socket_tx, &wltxnoackbuffer, ii)) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_associationrequest(size_t i)
{
	ssize_t ii;

	ii = RTHTX_SIZE;
	macftx = (ieee80211_mac_t *)&wltxbuffer[ii];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_ASSOC_REQ;
	wltxbuffer[ii + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macclientrg, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter2++ << 4;
	if (seqcounter1 > 4095)
		seqcounter2 = 1;
	ii += MAC_SIZE_NORM;
	memcpy(&wltxbuffer[ii], &associationrequestcapa, ASSOCIATIONREQUESTCAPA_SIZE);
	ii += ASSOCIATIONREQUESTCAPA_SIZE;
	wltxbuffer[ii++] = 0;
	wltxbuffer[ii++] = (aplist + i)->ie.essidlen;
	memcpy(&wltxbuffer[ii], (aplist + i)->ie.essid, (aplist + i)->ie.essidlen);
	ii += (aplist + i)->ie.essidlen;
	memcpy(&wltxbuffer[ii], &associationrequestdata, ASSOCIATIONREQUEST_SIZE);
	if (((aplist + i)->ie.flags & APGS_CCMP) == APGS_CCMP)
		wltxbuffer[ii + 0x17] = RSN_CS_CCMP;
	else if (((aplist + i)->ie.flags & APGS_TKIP) == APGS_TKIP)
		wltxbuffer[ii + 0x17] = RSN_CS_TKIP;
	if (((aplist + i)->ie.flags & APCS_CCMP) == APCS_CCMP)
		wltxbuffer[ii + 0x1d] = RSN_CS_CCMP;
	else if (((aplist + i)->ie.flags & APCS_TKIP) == APCS_TKIP)
		wltxbuffer[ii + 0x1d] = RSN_CS_TKIP;
	ii += ASSOCIATIONREQUEST_SIZE;
	if ((write(fd_socket_tx, &wltxbuffer, ii)) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_eap_request_id(void)
{
	static ssize_t ii;

	ii = RTHTX_SIZE;
	macftx = (ieee80211_mac_t *)&wltxbuffer[ii];
	macftx->type = IEEE80211_FTYPE_DATA;
	macftx->subtype = IEEE80211_STYPE_DATA;
	wltxbuffer[ii + 1] = 0;
	macftx->from_ds = 1;
	macftx->duration = 0x0431;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macfrx->addr1, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	ii += MAC_SIZE_NORM;
	memcpy(&wltxbuffer[ii], &eaprequestiddata, EAPREQUESTID_SIZE);
	ii += EAPREQUESTID_SIZE;
	if (write(fd_socket_tx, wltxbuffer, ii) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_eapol_m1(void)
{
	static ssize_t ii;

	ii = RTHTX_SIZE;
	macftx = (ieee80211_mac_t *)&wltxbuffer[ii];
	macftx->type = IEEE80211_FTYPE_DATA;
	macftx->subtype = IEEE80211_STYPE_DATA;
	wltxbuffer[ii + 1] = 0;
	macftx->from_ds = 1;
	macftx->duration = 0x0431;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macfrx->addr1, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	ii += MAC_SIZE_NORM;
	memcpy(&wltxbuffer[ii], &eapolm1data, EAPOLM1DATA_SIZE);
	ii += EAPOLM1DATA_SIZE;
	if (write(fd_socket_tx, wltxbuffer, ii) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_reassociationresponse(u16 aid)
{
	static ssize_t ii;
	static ieee80211_assoc_or_reassoc_resp_t *associationresponsetx;

	ii = RTHTX_SIZE;
	macftx = (ieee80211_mac_t *)&wltxbuffer[ii];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_REASSOC_RESP;
	wltxbuffer[ii + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macfrx->addr1, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	ii += MAC_SIZE_NORM;
	associationresponsetx = (ieee80211_assoc_or_reassoc_resp_t *)&wltxbuffer[ii];
	associationresponsetx->capability = 0x431;
	associationresponsetx->status = 0;
	associationresponsetx->aid = aid;
	ii += IEEE80211_ASSOCIATIONRESPONSE_SIZE;
	memcpy(&wltxbuffer[ii], &associationresponsedata, ASSOCIATIONRESPONSEDATA_SIZE);
	ii += ASSOCIATIONRESPONSEDATA_SIZE;
	if (write(fd_socket_tx, &wltxbuffer, ii) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_associationresponse(void)
{
	static ssize_t ii;
	static ieee80211_assoc_or_reassoc_resp_t *associationresponsetx;

	ii = RTHTX_SIZE;
	macftx = (ieee80211_mac_t *)&wltxbuffer[ii];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_ASSOC_RESP;
	wltxbuffer[ii + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macfrx->addr1, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	ii += MAC_SIZE_NORM;
	associationresponsetx = (ieee80211_assoc_or_reassoc_resp_t *)&wltxbuffer[ii];
	associationresponsetx->capability = 0x431;
	associationresponsetx->status = 0;
	associationresponsetx->aid = 1;
	ii += IEEE80211_ASSOCIATIONRESPONSE_SIZE;
	memcpy(&wltxbuffer[ii], &associationresponsedata, ASSOCIATIONRESPONSEDATA_SIZE);
	ii += ASSOCIATIONRESPONSEDATA_SIZE;
	if (write(fd_socket_tx, &wltxbuffer, ii) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_authenticationresponse(void)
{
	macftx = (ieee80211_mac_t *)&wltxbuffer[RTHTX_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_AUTH;
	wltxbuffer[RTHTX_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macfrx->addr1, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	memcpy(&wltxbuffer[RTHTX_SIZE + MAC_SIZE_NORM], authenticationresponsedata, AUTHENTICATIONRESPONSE_SIZE);
	if ((write(fd_socket_tx, &wltxbuffer, RTHTX_SIZE + MAC_SIZE_NORM + AUTHENTICATIONRESPONSE_SIZE)) == RTHTX_SIZE + MAC_SIZE_NORM + AUTHENTICATIONRESPONSE_SIZE)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_reassociationrequest(size_t i)
{
	static ssize_t ii;
	static ieee80211_reassoc_req_t *reassociationrequest;

	ii = RTHTXNOACK_SIZE;
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[ii];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_REASSOC_REQ;
	wltxnoackbuffer[ii + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, (aplist + i)->macap, ETH_ALEN);
	memcpy(macftx->addr2, (aplist + i)->macclient, ETH_ALEN);
	memcpy(macftx->addr3, (aplist + i)->macap, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	ii += MAC_SIZE_NORM;
	reassociationrequest = (ieee80211_reassoc_req_t *)&wltxnoackbuffer[ii];
	reassociationrequest->capability = 0x431;
	reassociationrequest->listen_interval = 0x14;
	memcpy(reassociationrequest->current_macap, (aplist + i)->macap, ETH_ALEN);
	ii += sizeof(ieee80211_reassoc_req_t) - 1;
	wltxnoackbuffer[ii++] = 0;
	wltxnoackbuffer[ii++] = (aplist + i)->ie.essidlen;
	memcpy(&wltxnoackbuffer[ii], (aplist + i)->ie.essid, (aplist + i)->ie.essidlen);
	ii += (aplist + i)->ie.essidlen;
	memcpy(&wltxnoackbuffer[ii], &reassociationrequestdata, REASSOCIATIONREQUEST_SIZE);
	if (((aplist + i)->ie.flags & APGS_CCMP) == APGS_CCMP)
		wltxnoackbuffer[ii + 0x17] = RSN_CS_CCMP;
	else if (((aplist + i)->ie.flags & APGS_TKIP) == APGS_TKIP)
		wltxnoackbuffer[ii + 0x17] = RSN_CS_TKIP;
	if (((aplist + i)->ie.flags & APCS_CCMP) == APCS_CCMP)
		wltxnoackbuffer[ii + 0x1d] = RSN_CS_CCMP;
	else if (((aplist + i)->ie.flags & APCS_TKIP) == APCS_TKIP)
		wltxnoackbuffer[ii + 0x1d] = RSN_CS_TKIP;
	ii += REASSOCIATIONREQUEST_SIZE;
	if ((write(fd_socket_tx, &wltxnoackbuffer, ii)) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_authenticationrequestnoack(void)
{
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[RTHTXNOACK_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_AUTH;
	wltxnoackbuffer[RTHTX_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macclientrg, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter2++ << 4;
	if (seqcounter1 > 4095)
		seqcounter2 = 1;
	memcpy(&wltxnoackbuffer[RTHTXNOACK_SIZE + MAC_SIZE_NORM], authenticationrequestdata, AUTHENTICATIONREQUEST_SIZE);
	if ((write(fd_socket_tx, &wltxnoackbuffer, RTHTXNOACK_SIZE + MAC_SIZE_NORM + AUTHENTICATIONREQUEST_SIZE)) == RTHTXNOACK_SIZE + MAC_SIZE_NORM + AUTHENTICATIONREQUEST_SIZE)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_authenticationrequest(void)
{
	macftx = (ieee80211_mac_t *)&wltxbuffer[RTHTX_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_AUTH;
	wltxbuffer[RTHTX_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macfrx->addr2, ETH_ALEN);
	memcpy(macftx->addr2, macclientrg, ETH_ALEN);
	memcpy(macftx->addr3, macfrx->addr3, ETH_ALEN);
	macftx->sequence = seqcounter2++ << 4;
	if (seqcounter1 > 4095)
		seqcounter2 = 1;
	memcpy(&wltxbuffer[RTHTX_SIZE + MAC_SIZE_NORM], authenticationrequestdata, AUTHENTICATIONREQUEST_SIZE);
	if ((write(fd_socket_tx, &wltxbuffer, RTHTX_SIZE + MAC_SIZE_NORM + AUTHENTICATIONREQUEST_SIZE)) == RTHTX_SIZE + MAC_SIZE_NORM + AUTHENTICATIONREQUEST_SIZE)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_probereresponse(u8 *macclientrsp, u8 *macaprgrsp, u8 essidlenrsp, u8 *essidrsp)
{
	static ssize_t ii;
	static ieee80211_beacon_proberesponse_t *beacontx;

	ii = RTHTXNOACK_SIZE;
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[ii];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_PROBE_RESP;
	wltxnoackbuffer[ii + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macclientrsp, ETH_ALEN);
	memcpy(macftx->addr2, macaprgrsp, ETH_ALEN);
	memcpy(macftx->addr3, macaprgrsp, ETH_ALEN);
	macftx->sequence = seqcounter3++ << 4;
	if (seqcounter1 > 4095)
		seqcounter3 = 1;
	ii += MAC_SIZE_NORM;
	beacontx = (ieee80211_beacon_proberesponse_t *)&wltxnoackbuffer[ii];
	beacontx->timestamp = beacontimestamp++;
	beacontx->beacon_interval = 1024;
	beacontx->capability = 0x431;
	ii += IEEE80211_PROBERESPONSE_SIZE;
	wltxnoackbuffer[ii++] = 0;
	wltxnoackbuffer[ii++] = essidlenrsp;
	memcpy(&wltxnoackbuffer[ii], essidrsp, essidlenrsp);
	ii += essidlenrsp;
	memcpy(&wltxnoackbuffer[ii], proberesponsedata, PROBERESPONSEDATA_SIZE);
	wltxnoackbuffer[ii + 0x0c] = (u8)(scanlist + scanlistindex)->channel;
	ii += PROBERESPONSEDATA_SIZE;
	if ((write(fd_socket_tx, &wltxnoackbuffer, ii)) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_beacon(void)
{
	static ssize_t ii;
	static ieee80211_beacon_proberesponse_t *beacontx;

	beaconindex++;
	if (beaconindex >= beacontxmax)
		beaconindex = 0;
	if ((aprglist + beaconindex)->essidlen == 0)
		beaconindex = 0;
	ii = RTHTXNOACK_SIZE;
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[ii];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_BEACON;
	wltxnoackbuffer[ii + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macbc, ETH_ALEN);
	memcpy(macftx->addr2, (aprglist + beaconindex)->macaprg, ETH_ALEN);
	memcpy(macftx->addr3, (aprglist + beaconindex)->macaprg, ETH_ALEN);
	macftx->sequence = seqcounter4++ << 4;
	if (seqcounter1 > 4095)
		seqcounter4 = 1;
	ii += MAC_SIZE_NORM;
	beacontx = (ieee80211_beacon_proberesponse_t *)&wltxnoackbuffer[ii];
	beacontx->timestamp = beacontimestamp++;
	beacontx->beacon_interval = 1024;
	beacontx->capability = 0x431;
	ii += IEEE80211_BEACON_SIZE;
	wltxnoackbuffer[ii++] = 0;
	wltxnoackbuffer[ii++] = (aprglist + beaconindex)->essidlen;
	memcpy(&wltxnoackbuffer[ii], (aprglist + beaconindex)->essid, (aprglist + beaconindex)->essidlen);
	ii += (aprglist + beaconindex)->essidlen;
	memcpy(&wltxnoackbuffer[ii], beacondata, BEACONDATA_SIZE);
	wltxnoackbuffer[ii + 0x0c] = (u8)(scanlist + scanlistindex)->channel;
	ii += BEACONDATA_SIZE;
	if ((write(fd_socket_tx, &wltxnoackbuffer, ii)) == ii)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_proberequest_undirected(void)
{
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[RTHTXNOACK_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_PROBE_REQ;
	wltxnoackbuffer[RTHTXNOACK_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macbc, ETH_ALEN);
	memcpy(macftx->addr2, macclientrg, ETH_ALEN);
	memcpy(macftx->addr3, macbc, ETH_ALEN);
	macftx->sequence = seqcounter2++ << 4;
	if (seqcounter1 > 4095)
		seqcounter2 = 1;
	memcpy(&wltxnoackbuffer[RTHTXNOACK_SIZE + MAC_SIZE_NORM], proberequest_undirected_data, PROBEREQUEST_UNDIRECTED_SIZE);
	if ((write(fd_socket_tx, &wltxnoackbuffer, RTHTXNOACK_SIZE + MAC_SIZE_NORM + PROBEREQUEST_UNDIRECTED_SIZE)) == RTHTXNOACK_SIZE + MAC_SIZE_NORM + PROBEREQUEST_UNDIRECTED_SIZE)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_disassociation_fm_ap(const u8 *macclient, const u8 *macap, u8 reason)
{
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[RTHTXNOACK_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_DISASSOC;
	wltxnoackbuffer[RTHTXNOACK_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macclient, ETH_ALEN);
	memcpy(macftx->addr2, macap, ETH_ALEN);
	memcpy(macftx->addr3, macap, ETH_ALEN);
	macftx->sequence = seqcounter1++ << 4;
	if (seqcounter1 > 4095)
		seqcounter1 = 1;
	wltxnoackbuffer[RTHTXNOACK_SIZE + MAC_SIZE_NORM] = reason;
	if ((write(fd_socket_tx, &wltxnoackbuffer, RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)) == RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_disassociation_fm_client(const u8 *macclient, const u8 *macap, u8 reason)
{
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[RTHTXNOACK_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_DISASSOC;
	wltxnoackbuffer[RTHTXNOACK_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macap, ETH_ALEN);
	memcpy(macftx->addr2, macclient, ETH_ALEN);
	memcpy(macftx->addr3, macap, ETH_ALEN);
	macftx->sequence = seqcounter1++ << 4;
	if (seqcounter1 > 4095)
		seqcounter1 = 1;
	wltxnoackbuffer[RTHTXNOACK_SIZE + MAC_SIZE_NORM] = reason;
	if ((write(fd_socket_tx, &wltxnoackbuffer, RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)) == RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_deauthentication_fm_ap(const u8 *macclient, const u8 *macap, u8 reason)
{
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[RTHTXNOACK_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_DEAUTH;
	wltxnoackbuffer[RTHTXNOACK_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macclient, ETH_ALEN);
	memcpy(macftx->addr2, macap, ETH_ALEN);
	memcpy(macftx->addr3, macap, ETH_ALEN);
	macftx->sequence = seqcounter1++ << 4;
	if (seqcounter1 > 4095)
		seqcounter1 = 1;
	wltxnoackbuffer[RTHTXNOACK_SIZE + MAC_SIZE_NORM] = reason;
	if ((write(fd_socket_tx, &wltxnoackbuffer, RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)) == RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)
		return;
	errorcount++;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void send_80211_deauthentication_fm_client(const u8 *macclient, const u8 *macap, u8 reason)
{
	macftx = (ieee80211_mac_t *)&wltxnoackbuffer[RTHTXNOACK_SIZE];
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_DEAUTH;
	wltxnoackbuffer[RTHTXNOACK_SIZE + 1] = 0;
	macftx->duration = 0x013a;
	memcpy(macftx->addr1, macap, ETH_ALEN);
	memcpy(macftx->addr2, macclient, ETH_ALEN);
	memcpy(macftx->addr3, macap, ETH_ALEN);
	macftx->sequence = seqcounter1++ << 4;
	if (seqcounter1 > 4095)
		seqcounter1 = 1;
	wltxnoackbuffer[RTHTXNOACK_SIZE + MAC_SIZE_NORM] = reason;
	if ((write(fd_socket_tx, &wltxnoackbuffer, RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)) == RTHTXNOACK_SIZE + MAC_SIZE_NORM + 2)
		return;
	errorcount++;
	return;
}
/*===========================================================================*/
/*===========================================================================*/
/* RX 802.11 */
static inline __attribute__((always_inline)) void get_tagvendor(infoelement_t *infoelement, int infolen, u8 *infostart)
{
	static size_t c;
	static ieee80211_wpaie_t *iewpa;
	static ieee80211_wpasuite_t *wpasuite;
	static ieee80211_wpasuitecount_t *wpasuitecount;

	iewpa = (ieee80211_wpaie_t *)infostart;
	if (memcmp(&wpasuiteoui, iewpa->oui, 3) != 0)
		return;
	if (iewpa->type != 1)
		return;
	if (iewpa->version != 1)
		return;

	infostart += IEEE80211_WPAIE_SIZE;
	infolen -= IEEE80211_WPAIE_SIZE;
	wpasuite = (ieee80211_wpasuite_t *)infostart;
	if (memcmp(&wpasuiteoui, wpasuite->oui, 3) != 0)
		return;

	infostart += IEEE80211_WPASUITE_SIZE;
	infolen -= IEEE80211_WPASUITE_SIZE;
	wpasuitecount = (ieee80211_wpasuitecount_t *)infostart;
	infostart += IEEE80211_WPASUITECOUNT_SIZE;
	infolen -= IEEE80211_WPASUITECOUNT_SIZE;
	for (c = 0; c < wpasuitecount->count; c++)
	{
		if (infolen <= 0)
			return;
		wpasuite = (ieee80211_wpasuite_t *)infostart;
		infostart += IEEE80211_WPASUITE_SIZE;
		infolen -= IEEE80211_WPASUITE_SIZE;
	}
	wpasuitecount = (ieee80211_wpasuitecount_t *)infostart;
	infostart += IEEE80211_WPASUITECOUNT_SIZE;
	infolen -= IEEE80211_WPASUITECOUNT_SIZE;
	for (c = 0; c < wpasuitecount->count; c++)
	{
		if (infolen <= 0)
			return;
		wpasuite = (ieee80211_wpasuite_t *)infostart;
		if (memcmp(&wpasuiteoui, wpasuite->oui, 3) == 0)
		{
			if (wpasuite->type == WPA_AKM_PSK)
				infoelement->flags |= APWPAAKM_PSK;
		}
		infostart += IEEE80211_WPASUITE_SIZE;
		infolen -= IEEE80211_WPASUITE_SIZE;
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline __attribute__((always_inline)) void get_tagrsn(infoelement_t *infoelement, int infolen, u8 *infostart)
{
	static size_t c;
	static ieee80211_rsnie_t *iersn;
	static ieee80211_rnsuite_t *rsnsuite;
	static ieee80211_rsnsuitecount_t *rsnsuitecount;
	static ieee80211_rsncapability_t *rsncapability;

	iersn = (ieee80211_rsnie_t *)infostart;
	if (iersn->version != 1)
		return;
	infostart += IEEE80211_RSNIE_SIZE;
	infolen -= IEEE80211_RSNIE_SIZE;
	rsnsuite = (ieee80211_rnsuite_t *)infostart;
	if (memcmp(&rsnsuiteoui, rsnsuite->oui, 3) != 0)
		return;
	if (rsnsuite->type == RSN_CS_CCMP)
		infoelement->flags |= APGS_CCMP;
	if (rsnsuite->type == RSN_CS_TKIP)
		infoelement->flags |= APGS_TKIP;
	infostart += IEEE80211_RSNSUITE_SIZE;
	infolen -= IEEE80211_RSNSUITE_SIZE;
	rsnsuitecount = (ieee80211_rsnsuitecount_t *)infostart;
	infostart += IEEE80211_RSNSUITECOUNT_SIZE;
	infolen -= IEEE80211_RSNSUITECOUNT_SIZE;
	for (c = 0; c < rsnsuitecount->count; c++)
	{
		if (infolen <= 0)
			return;
		rsnsuite = (ieee80211_rnsuite_t *)infostart;
		if (memcmp(&rsnsuiteoui, rsnsuite->oui, 3) == 0)
		{
			if (rsnsuite->type == RSN_CS_CCMP)
				infoelement->flags |= APCS_CCMP;
			if (rsnsuite->type == RSN_CS_TKIP)
				infoelement->flags |= APCS_TKIP;
		}
		infostart += IEEE80211_RSNSUITE_SIZE;
		infolen -= IEEE80211_RSNSUITE_SIZE;
	}
	rsnsuitecount = (ieee80211_rsnsuitecount_t *)infostart;
	infostart += IEEE80211_RSNSUITECOUNT_SIZE;
	infolen -= IEEE80211_RSNSUITECOUNT_SIZE;
	for (c = 0; c < rsnsuitecount->count; c++)
	{
		if (infolen <= 0)
			return;
		rsnsuite = (ieee80211_rnsuite_t *)infostart;
		if (memcmp(&rsnsuiteoui, rsnsuite->oui, 3) == 0)
		{
			if (rsnsuite->type == RSN_AKM_PSK)
				infoelement->flags |= APRSNAKM_PSK;
			if (rsnsuite->type == RSN_AKM_PSK256)
				infoelement->flags |= APRSNAKM_PSK256;
			if (rsnsuite->type == RSN_AKM_PSKFT)
				infoelement->flags |= APRSNAKM_PSKFT;
		}
		infostart += IEEE80211_RSNSUITE_SIZE;
		infolen -= IEEE80211_RSNSUITE_SIZE;
	}
	if (infolen < 2)
		return;
	rsncapability = (ieee80211_rsncapability_t *)infostart;
	if ((rsncapability->capability & MFP_REQUIRED) == MFP_REQUIRED)
		infoelement->flags |= AP_MFP;
	return;
}
/*---------------------------------------------------------------------------*/
static inline __attribute__((always_inline)) void tagwalk_channel_essid_rsn(infoelement_t *infoelement, int infolen, u8 *infostart)
{
	static ieee80211_ietag_t *infoptr;

	while (0 < infolen)
	{
		infoptr = (ieee80211_ietag_t *)infostart;
		if (infolen < (int)(infoptr->len + IEEE80211_IETAG_SIZE))
			return;
		if (infoptr->id == TAG_SSID)
		{
			if ((infoptr->len > 0) && (infoptr->len <= ESSID_MAX))
			{
				infoelement->flags |= APIE_ESSID;
				infoelement->essidlen = infoptr->len;
				memcpy(infoelement->essid, &infoptr->ie[0], infoptr->len);
			}
		}
		else if (infoptr->id == TAG_CHAN)
		{
			if (infoptr->len == 1)
				infoelement->channel = (u16)infoptr->ie[0];
		}
		else if (infoptr->id == TAG_RSN)
		{
			if (infoptr->len >= IEEE80211_RSNIE_LEN_MIN)
				get_tagrsn(infoelement, infoptr->len, infoptr->ie);
		}
		else if (infoptr->id == TAG_VENDOR)
		{
			if (infoptr->len >= IEEE80211_WPAIE_LEN_MIN)
				get_tagvendor(infoelement, infoptr->len, infoptr->ie);
		}
		infostart += infoptr->len + IEEE80211_IETAG_SIZE;
		infolen -= infoptr->len + IEEE80211_IETAG_SIZE;
	}
	return;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static inline int get_keyinfo(u16 kyif)
{
	if (kyif & WPA_KEY_INFO_ACK)
	{
		if (kyif & WPA_KEY_INFO_INSTALL)
			return 3; /* handshake 3 */
		else
			return 1; /* handshake 1 */
	}
	else
	{
		if (kyif & WPA_KEY_INFO_SECURE)
			return 4; /* handshake 4 */
		else
			return 2; /* handshake 2 */
	}
	return 0;
}
/*---------------------------------------------------------------------------*/
static inline void process80211pspoll(void)
{
	static size_t i;

	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr1, (aplist + i)->macap, ETH_ALEN) == 0)
		{
			if ((aplist + i)->status >= AP_EAPOL_M3)
				return;
			if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) == 0)
				(aplist + i)->count = attemptapmax;
			memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
			return;
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211action(void)
{
	static size_t i;
	static ieee80211_action_t *action;

	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp((aplist + i)->macap, macfrx->addr1, ETH_ALEN) == 0)
		{
			if ((aplist + i)->status >= AP_EAPOL_M3)
				return;
			if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) == 0)
				(aplist + i)->count = attemptapmax;
			memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
			break;
		}
	}
	action = (ieee80211_action_t *)payloadptr;
	if (payloadlen < (IEEE80211_ACTION_SIZE + IEEE80211_IETAG_SIZE))
		return;
	if ((action->category == RADIO_MEASUREMENT) && (action->code == NEIGHBOR_REPORT_REQUEST))
		writeepb();
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211qosdata(void)
{
	static size_t i;

	if ((macfrx->to_ds != 1) && (macfrx->from_ds != 0))
		return;
	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		{
			if (memcmp((aplist + i)->macap, macfrx->addr1, ETH_ALEN) == 0)
			{
				if ((aplist + i)->status >= AP_EAPOL_M3)
					return;
				if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) == 0)
					(aplist + i)->count = attemptapmax;
				memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
				return;
			}
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211qosnull(void)
{
	static size_t i;

	if ((macfrx->to_ds != 1) && (macfrx->from_ds != 0))
		return;
	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		{
			if (memcmp((aplist + i)->macap, macfrx->addr1, ETH_ALEN) == 0)
			{
				if ((aplist + i)->status >= AP_EAPOL_M3)
					return;
				if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) == 0)
					(aplist + i)->count = attemptapmax;
				memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
				return;
			}
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211null(void)
{
	static size_t i;

	if ((macfrx->to_ds != 1) && (macfrx->from_ds != 0))
		return;
	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		{
			if (memcmp((aplist + i)->macap, macfrx->addr1, ETH_ALEN) == 0)
			{
				if ((aplist + i)->status >= AP_EAPOL_M3)
					return;
				if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) == 0)
					(aplist + i)->count = attemptapmax;
				memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
				return;
			}
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211blockack(void)
{
	static size_t i;

	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp((aplist + i)->macap, macfrx->addr1, ETH_ALEN) == 0)
		{
			if ((aplist + i)->status >= AP_EAPOL_M3)
				return;
			if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) == 0)
				(aplist + i)->count = attemptapmax;
			memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
			return;
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211blockackreq(void)
{
	static size_t i;

	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp((aplist + i)->macap, macfrx->addr1, ETH_ALEN) == 0)
		{
			if ((aplist + i)->status >= AP_EAPOL_M3)
				return;
			if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) == 0)
				(aplist + i)->count = attemptapmax;
			memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
			return;
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eap_start(void)
{
	static size_t i;

	for (i = 0; i < CLIENTLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr2, (clientlist + i)->macclient, ETH_ALEN) != 0)
			continue;
		if (memcmp(macfrx->addr1, (clientlist + i)->macap, ETH_ALEN) != 0)
			continue;
		(clientlist + i)->tsakt = tsakt;
		(clientlist + i)->status |= CLIENT_EAP_START;
		if ((clientlist + i)->count == 0)
			return;
		send_80211_eap_request_id();
		(clientlist + i)->count -= 1;
		return;
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eapol_m4(void)
{
	static size_t i;

	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp((aplist + i)->macap, macfrx->addr3, ETH_ALEN) == 0)
		{
			if ((aplist + i)->status >= AP_EAPOL_M3)
				return;
			if (deauthenticationflag == true)
			{
				if (((aplist + i)->ie.flags & APAKM_MASK) == 0)
				{
					send_80211_disassociation_fm_ap((aplist + i)->macclient, (aplist + i)->macap, WLAN_REASON_DISASSOC_AP_BUSY);
					send_80211_disassociation_fm_client(macfrx->addr2, (aplist + i)->macap, WLAN_REASON_DISASSOC_STA_HAS_LEFT);
				}
			}
			memcpy((aplist + i)->macclient, macfrx->addr2, ETH_ALEN);
			qsort(aplist, APLIST_MAX, APLIST_SIZE, sort_aplist_by_tsakt);
			return;
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eapol_m3(void)
{
	static size_t i;

	if ((authseqakt.status & AP_EAPOL_M2) == AP_EAPOL_M2)
	{
		if (memcmp(&authseqakt.macap, macfrx->addr2, ETH_ALEN) == 0)
		{
			if (authseqakt.replaycountm2 == (be64toh(wpakey->replaycount) - 1))
			{
				if (authseqakt.kdv2 == kdv)
				{
					if ((tsakt - tshold) < EAPOLM3TIMEOUT)
					{
						if (memcmp(&authseqakt.noncem1, &wpakey->nonce[28], 4) == 0)
						{
							for (i = 0; i < APLIST_MAX - 1; i++)
							{
								if (memcmp((aplist + i)->macap, authseqakt.macap, ETH_ALEN) == 0)
								{
									(aplist + i)->tsakt = tsakt;
									authseqakt.status = 0;
									(aplist + i)->status |= AP_EAPOL_M3;
									wanteventflag |= exiteapolm3flag;
									return;
								}
							}
						}
					}
				}
			}
		}
	}
	authseqakt.status = 0;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eapol_m2rg(void)
{
	size_t i;

	authseqakt.status = 0;
	writeepbm1();
	for (i = 0; i < CLIENTLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr2, (clientlist + i)->macclient, ETH_ALEN) != 0)
			continue;
		if (memcmp(macfrx->addr1, (clientlist + i)->macap, ETH_ALEN) != 0)
			continue;
		(clientlist + i)->tsakt = tsakt;
		(clientlist + i)->status |= CLIENT_EAPOL_M2;
		memcpy((clientlist + i)->mic, &wpakey->keymic[0], 4);
		if ((clientlist + i)->count == 0)
			return;
		if (memcmp((clientlist + i)->mic, &wpakey->keymic[0], 4) == 0)
			send_80211_disassociation_fm_ap(macfrx->addr2, macfrx->addr1, WLAN_REASON_PREV_AUTH_NOT_VALID);
		(clientlist + i)->count -= 1;
		return;
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eapol_m2(void)
{
	authseqakt.replaycountm2 = be64toh(wpakey->replaycount);
	if (replaycountrg == authseqakt.replaycountm2)
	{
		process80211eapol_m2rg();
		return;
	}
	if ((authseqakt.status & AP_EAPOL_M1) == AP_EAPOL_M1)
	{
		if (memcmp(&authseqakt.macap, macfrx->addr1, ETH_ALEN) == 0)
		{
			if (authseqakt.replaycountm1 == authseqakt.replaycountm2)
			{
				authseqakt.kdv2 = kdv;
				if (authseqakt.kdv1 == authseqakt.kdv2)
				{
					if ((tsakt - tshold) < EAPOLM2TIMEOUT)
					{
						authseqakt.status |= AP_EAPOL_M2;
						wanteventflag |= exiteapolm2flag;
					}
					else
						authseqakt.status = 0;
					return;
				}
			}
		}
	}
	authseqakt.status = 0;
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eapol_m1(void)
{
	static size_t i;

	memset(&authseqakt, 0, AUTHSEQAKT_SIZE);
	memcpy(&authseqakt.macap, macfrx->addr2, ETH_ALEN);
	authseqakt.kdv1 = kdv;
	authseqakt.replaycountm1 = be64toh(wpakey->replaycount);
	memcpy(&authseqakt.noncem1, &wpakey->nonce[28], 4);
	authseqakt.status = AP_EAPOL_M1;
	if (ntohs(wpakey->wpadatalen) == IEEE80211_PMKID_SIZE)
	{
		pmkid = (ieee80211_pmkid_t *)(eapolplptr + IEEE80211_WPAKEY_SIZE);
		if (memcmp(&rsnsuiteoui, pmkid->oui, 3) == 0)
		{
			if (pmkid->len >= 0x14)
			{
				if (pmkid->type == PMKID_KDE)
				{
					if (memcmp(pmkid->pmkid, &zeroed, PMKID_MAX) != 0)
					{
						authseqakt.status |= AP_PMKID;
						wanteventflag |= exiteapolpmkidflag;
					}
				}
			}
		}
	}
	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp((aplist + i)->macap, authseqakt.macap, ETH_ALEN) == 0)
		{
			if (((aplist + i)->status & AP_PMKID) == AP_PMKID)
				return;
			(aplist + i)->status |= authseqakt.status;
			(aplist + i)->tsakt = tsakt;
			return;
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eapol(void)
{
	eapolplptr = eapauthplptr + IEEE80211_EAPAUTH_SIZE;
	eapolpllen = eapauthpllen - IEEE80211_EAPAUTH_SIZE;
	if ((eapolpllen + IEEE80211_EAPAUTH_SIZE + IEEE80211_LLC_SIZE) > payloadlen)
		return;
	wpakey = (ieee80211_wpakey_t *)eapolplptr;
	if ((kdv = ntohs(wpakey->keyinfo) & WPA_KEY_INFO_TYPE_MASK) == 0)
		return;
	keyinfo = (get_keyinfo(ntohs(wpakey->keyinfo)));
	switch (keyinfo)
	{
	case M1:
		process80211eapol_m1();
		break;

	case M2:
		process80211eapol_m2();
		break;

	case M3:
		process80211eapol_m3();
		break;

	case M4:
		if (deauthenticationflag == true)
			process80211eapol_m4();
		break;
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211eapauthentication(void)
{
	tshold = tsakt;
	eapauthplptr = payloadptr + IEEE80211_LLC_SIZE;
	eapauthpllen = payloadlen - IEEE80211_LLC_SIZE;
	eapauth = (ieee80211_eapauth_t *)eapauthplptr;
	eapauthlen = ntohs(eapauth->len);
	if (eapauthlen > (eapauthpllen - IEEE80211_EAPAUTH_SIZE))
		return;
	if (eapauth->type == EAPOL_KEY)
		process80211eapol();
	else if (eapauth->type == EAPOL_START)
		process80211eap_start();
	writeepb();
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211reassociationresponse(void)
{
	tshold = tsakt;
	memcpy(&authseqakt.macap, macfrx->addr2, ETH_ALEN);
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211reassociationrequest(void)
{
	static size_t i;
	static ieee80211_reassoc_req_t *reassociationrequest;
	static u16 reassociationrequestlen;

	tshold = tsakt;
	reassociationrequest = (ieee80211_reassoc_req_t *)payloadptr;
	if ((reassociationrequestlen = payloadlen - IEEE80211_REASSOCIATIONREQUEST_SIZE) < IEEE80211_IETAG_SIZE)
		return;
	memcpy(&authseqakt.macap, macfrx->addr1, ETH_ALEN);
	for (i = 0; i < CLIENTLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr2, (clientlist + i)->macclient, ETH_ALEN) != 0)
			continue;
		if (memcmp(macfrx->addr1, (clientlist + i)->macap, ETH_ALEN) != 0)
			continue;
		(clientlist + i)->tsakt = tsakt;
		if ((clientlist + i)->count == 0)
			return;
		if ((tsakt - (clientlist + i)->tsassoc) > TIMEREASSOCWAIT)
		{
			tagwalk_channel_essid_rsn(&(clientlist + i)->ie, reassociationrequestlen, reassociationrequest->ie);
			if (((clientlist + i)->ie.flags & APRSNAKM_PSK) != 0)
			{
				send_80211_reassociationresponse((clientlist + i)->aid++);
				if ((clientlist + i)->aid > 0xff)
					(clientlist + i)->aid = 1;
				send_80211_eapol_m1();
				(clientlist + i)->count -= 1;
			}
			else
				(clientlist + i)->count = 0;
			writeepb();
		}
		(clientlist + i)->tsassoc = tsakt;
		return;
	}
	memset((clientlist + i), 0, CLIENTLIST_SIZE);
	(clientlist + i)->tsakt = tsakt;
	(clientlist + i)->tsassoc = tsfirst;
	(clientlist + i)->tsreassoc = tsfirst;
	(clientlist + i)->count = attemptclientmax;
	(clientlist + i)->aid = 1;
	memcpy((clientlist + i)->macclient, macfrx->addr2, ETH_ALEN);
	memcpy((clientlist + i)->macap, macfrx->addr1, ETH_ALEN);
	tagwalk_channel_essid_rsn(&(clientlist + i)->ie, reassociationrequestlen, reassociationrequest->ie);
	if ((((clientlist + i)->ie.flags & APRSNAKM_PSK) != 0) && (attemptclientmax > 0))
	{
		send_80211_reassociationresponse((clientlist + i)->aid++);
		send_80211_eapol_m1();
	}
	else
		(clientlist + i)->count = 0;
	qsort(clientlist, i + 1, CLIENTLIST_SIZE, sort_clientlist_by_tsakt);
	writeepb();
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211associationresponse(void)
{
	tshold = tsakt;
	memcpy(&authseqakt.macap, macfrx->addr2, ETH_ALEN);
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211associationrequest(void)
{
	static size_t i;
	static ieee80211_assoc_req_t *associationrequest;
	static u16 associationrequestlen;

	tshold = tsakt;
	associationrequest = (ieee80211_assoc_req_t *)payloadptr;
	if ((associationrequestlen = payloadlen - IEEE80211_ASSOCIATIONREQUEST_SIZE) < IEEE80211_IETAG_SIZE)
		return;
	memcpy(&authseqakt.macap, macfrx->addr1, ETH_ALEN);
	for (i = 0; i < CLIENTLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr2, (clientlist + i)->macclient, ETH_ALEN) != 0)
			continue;
		if (memcmp(macfrx->addr1, (clientlist + i)->macap, ETH_ALEN) != 0)
			continue;
		(clientlist + i)->tsakt = tsakt;
		if ((clientlist + i)->count == 0)
			return;
		tagwalk_channel_essid_rsn(&(clientlist + i)->ie, associationrequestlen, associationrequest->ie);
		if ((tsakt - (clientlist + i)->tsassoc) > TIMEASSOCWAIT)
		{
			if (((clientlist + i)->ie.flags & APRSNAKM_PSK) != 0)
			{
				send_80211_associationresponse();
				send_80211_eapol_m1();
				(clientlist + i)->count -= 1;
			}
			else
				(clientlist + i)->count = 0;
			writeepb();
		}
		(clientlist + i)->tsassoc = tsakt;
		return;
	}
	memset((clientlist + i), 0, CLIENTLIST_SIZE);
	(clientlist + i)->tsakt = tsakt;
	(clientlist + i)->tsassoc = tsfirst;
	(clientlist + i)->tsreassoc = tsfirst;
	(clientlist + i)->count = attemptclientmax;
	memcpy((clientlist + i)->macclient, macfrx->addr2, ETH_ALEN);
	memcpy((clientlist + i)->macap, macfrx->addr1, ETH_ALEN);
	tagwalk_channel_essid_rsn(&(clientlist + i)->ie, associationrequestlen, associationrequest->ie);
	if ((((clientlist + i)->ie.flags & APRSNAKM_PSK) != 0) && (attemptclientmax > 0))
	{
		send_80211_associationresponse();
		send_80211_eapol_m1();
	}
	else
		(clientlist + i)->count = 0;
	qsort(clientlist, i + 1, CLIENTLIST_SIZE, sort_clientlist_by_tsakt);
	writeepb();
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211authentication_fmclient(void)
{
	size_t i;

	for (i = 0; i < CLIENTLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr2, (clientlist + i)->macclient, ETH_ALEN) != 0)
			continue;
		if (memcmp(macfrx->addr1, (clientlist + i)->macap, ETH_ALEN) != 0)
			continue;
		(clientlist + i)->tsakt = tsakt;
		if ((clientlist + i)->count == 0)
			return;
		if ((tsakt - (clientlist + i)->tsauth) > TIMEAUTHWAIT)
		{
			send_80211_authenticationresponse();
			writeepb();
		}
		(clientlist + i)->tsauth = tsakt;
		return;
	}
	memset((clientlist + i), 0, CLIENTLIST_SIZE);
	(clientlist + i)->tsakt = tsakt;
	(clientlist + i)->tsauth = tsfirst;
	(clientlist + i)->tsassoc = tsfirst;
	(clientlist + i)->tsreassoc = tsfirst;
	(clientlist + i)->count = attemptclientmax;
	memcpy((clientlist + i)->macclient, macfrx->addr2, ETH_ALEN);
	memcpy((clientlist + i)->macap, macfrx->addr1, ETH_ALEN);
	if (attemptclientmax > 0)
		send_80211_authenticationresponse();
	qsort(clientlist, i + 1, CLIENTLIST_SIZE, sort_clientlist_by_tsakt);
	writeepb();
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211authentication(void)
{
	size_t i;
	static ieee80211_auth_t *auth;

	tshold = tsakt;
	auth = (ieee80211_auth_t *)payloadptr;
	if (payloadlen < IEEE80211_AUTH_SIZE)
		return;
	if (auth->algorithm == OPEN_SYSTEM)
	{
		if (auth->sequence == 1)
			process80211authentication_fmclient();
		else if (auth->sequence == 2)
		{
			if (memcmp(&macclientrg, macfrx->addr1, 3) == 0)
			{
				for (i = 0; i < APLIST_MAX - 1; i++)
				{
					if (memcmp((aplist + i)->macap, macfrx->addr2, ETH_ALEN) == 0)
					{
						(aplist + i)->tsakt = tsakt;
						(aplist + i)->status |= AP_IN_RANGE;
						if ((tsakt - (aplist + i)->tsauth) > TIMEAUTHWAIT)
						{
							if (((aplist + i)->ie.flags & APRSNAKM_PSK) != 0)
								send_80211_associationrequest(i);
							writeepb();
						}
						(aplist + i)->tsauth = tsakt;
						break;
					}
				}
			}
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static void get_tag(u8 ietag, essid_t *essid, int infolen, u8 *infostart)
{
	static ieee80211_ietag_t *infoptr;

	while (0 < infolen)
	{
		infoptr = (ieee80211_ietag_t *)infostart;
		if (infolen < (int)(infoptr->len + IEEE80211_IETAG_SIZE))
			return;
		if (infoptr->id == ietag)
		{
			essid->len = infoptr->len;
			essid->essid = (u8 *)infoptr->ie;
			return;
		}
		infostart += infoptr->len + IEEE80211_IETAG_SIZE;
		infolen -= infoptr->len + IEEE80211_IETAG_SIZE;
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211proberequest_directed(void)
{
	static size_t i;
	static ieee80211_proberequest_t *proberequest;
	static u16 proberequestlen;
	static essid_t essid;

	proberequest = (ieee80211_proberequest_t *)payloadptr;
	if ((proberequestlen = payloadlen - IEEE80211_PROBERESPONSE_SIZE) < IEEE80211_IETAG_SIZE)
		return;
	get_tag(TAG_SSID, &essid, proberequestlen, proberequest->ie);
	if (attemptclientmax > 0)
		send_80211_probereresponse(macfrx->addr2, macfrx->addr1, essid.len, essid.essid);
	for (i = 0; i < MACLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr1, (maclist + i)->mac, ETH_ALEN) != 0)
			continue;
		(maclist + i)->tsakt = tsakt;
		return;
	}
	(maclist + i)->tsakt = tsakt;
	memcpy((maclist + i)->mac, macfrx->addr1, ETH_ALEN);
	qsort(maclist, i + 1, MACLIST_SIZE, sort_maclist_by_tsakt);
	writeepb();
	return;
}
/*---------------------------------------------------------------------------*/
static inline void process80211proberequest_undirected(void)
{
	static size_t i;
	static ieee80211_proberequest_t *proberequest;
	static u16 proberequestlen;
	static essid_t essid;

	proberequest = (ieee80211_proberequest_t *)payloadptr;
	if ((proberequestlen = payloadlen - IEEE80211_PROBERESPONSE_SIZE) < IEEE80211_IETAG_SIZE)
		return;
	get_tag(TAG_SSID, &essid, proberequestlen, proberequest->ie);
	if (essid.len == 0)
	{
		if (proberesponseindex >= proberesponsetxmax)
			proberesponseindex = 0;
		if ((aprglist + proberesponseindex)->essidlen == 0)
			proberesponseindex = 0;
		if (attemptclientmax > 0)
			send_80211_probereresponse(macfrx->addr2, (aprglist + proberesponseindex)->macaprg, (aprglist + proberesponseindex)->essidlen, (aprglist + proberesponseindex)->essid);
		proberesponseindex++;
		return;
	}
	for (i = 0; i < APRGLIST_MAX - 1; i++)
	{
		if ((aprglist + i)->essidlen != essid.len)
			continue;
		if (memcmp((aprglist + i)->essid, essid.essid, essid.len) != 0)
			continue;
		(aprglist + i)->tsakt = tsakt;
		if (attemptclientmax > 0)
			send_80211_probereresponse(macfrx->addr2, (aprglist + i)->macaprg, essid.len, essid.essid);
		return;
	}
	memset((aprglist + i), 0, APRGLIST_SIZE);
	(aprglist + i)->tsakt = tsakt;
	(aprglist + i)->essidlen = essid.len;
	memcpy((aprglist + i)->essid, essid.essid, essid.len);
	(aprglist + i)->macaprg[5] = nicaprg & 0xff;
	(aprglist + i)->macaprg[4] = (nicaprg >> 8) & 0xff;
	(aprglist + i)->macaprg[3] = (nicaprg >> 16) & 0xff;
	(aprglist + i)->macaprg[2] = ouiaprg & 0xff;
	(aprglist + i)->macaprg[1] = (ouiaprg >> 8) & 0xff;
	(aprglist + i)->macaprg[0] = (ouiaprg >> 16) & 0xff;
	nicaprg++;
	if (attemptclientmax > 0)
		send_80211_probereresponse(macfrx->addr2, (aprglist + i)->macaprg, essid.len, essid.essid);
	qsort(aprglist, i + 1, APRGLIST_SIZE, sort_aprglist_by_tsakt);
	writeepb();
	return;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static inline void process80211proberesponse(void)
{
	static size_t i;
	static ieee80211_beacon_proberesponse_t *proberesponse;
	static u16 proberesponselen;

	proberesponse = (ieee80211_beacon_proberesponse_t *)payloadptr;
	if ((proberesponselen = payloadlen - IEEE80211_PROBERESPONSE_SIZE) < IEEE80211_IETAG_SIZE)
		return;
	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr3, (aplist + i)->macap, ETH_ALEN) != 0)
			continue;
		(aplist + i)->tsakt = tsakt;
		if (((aplist + i)->status & AP_PROBERESPONSE) == 0)
		{
			writeepb();
			tshold = tsakt;
			(aplist + i)->status |= AP_PROBERESPONSE;
		}
		tagwalk_channel_essid_rsn(&(aplist + i)->ie, proberesponselen, proberesponse->ie);
		if ((aplist + i)->ie.channel == 0)
			(aplist + i)->ie.channel = (scanlist + scanlistindex)->channel;
		if (((aplist + i)->ie.flags & APIE_ESSID) == APIE_ESSID)
			(aplist + i)->status |= AP_ESSID;
		return;
	}
	memset((aplist + i), 0, APLIST_SIZE);
	(aplist + i)->tsakt = tsakt;
	(aplist + i)->tshold1 = tsakt;
	(aplist + i)->tsauth = tsfirst;
	(aplist + i)->count = attemptapmax;
	memcpy((aplist + i)->macap, macfrx->addr3, ETH_ALEN);
	memcpy((aplist + i)->macclient, &macbc, ETH_ALEN);
	(aplist + i)->status |= AP_PROBERESPONSE;
	tagwalk_channel_essid_rsn(&(aplist + i)->ie, proberesponselen, proberesponse->ie);
	if ((aplist + i)->ie.channel == 0)
		(aplist + i)->ie.channel = (scanlist + scanlistindex)->channel;
	if ((aplist + i)->ie.channel != (scanlist + scanlistindex)->channel)
		return;
	if (((aplist + i)->ie.flags & APIE_ESSID) == APIE_ESSID)
		(aplist + i)->status |= AP_ESSID;
	if (deauthenticationflag == true)
	{
		if (((aplist + i)->ie.flags & AP_MFP) == 0)
		{
			if (((aplist + i)->ie.flags & APAKM_MASK) != 0)
				send_80211_deauthentication_fm_ap(macbc, (aplist + i)->macap, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
		}
	}
	if (associationflag == true)
	{
		if ((((aplist + i)->ie.flags & APRSNAKM_PSK) != 0) && (((aplist + i)->ie.flags & APIE_ESSID) == 0))
			send_80211_authenticationrequestnoack();
	}
	if (reassociationflag == true)
	{
		if (((aplist + i)->ie.flags & APRSNAKM_PSK) != 0)
			send_80211_reassociationrequest(i);
	}
	writeepb();
	qsort(aplist, i + 1, APLIST_SIZE, sort_aplist_by_tsakt);
	tshold = tsakt;
	return;
}
/*---------------------------------------------------------------------------*/
static inline __attribute__((always_inline)) void process80211beacon(void)
{
	static size_t i;
	static ieee80211_beacon_proberesponse_t *beacon;
	static u16 beaconlen;

	beacon = (ieee80211_beacon_proberesponse_t *)payloadptr;
	if ((beaconlen = payloadlen - IEEE80211_BEACON_SIZE) < IEEE80211_IETAG_SIZE)
		return;
	for (i = 0; i < APLIST_MAX - 1; i++)
	{
		if (memcmp(macfrx->addr3, (aplist + i)->macap, ETH_ALEN) != 0)
			continue;
		(aplist + i)->tsakt = tsakt;
		if (((aplist + i)->status & AP_BEACON) == 0)
		{
			writeepb();
			tshold = tsakt;
			(aplist + i)->status |= AP_BEACON;
		}
		if ((aplist + i)->status >= AP_EAPOL_M3)
			return;
		tagwalk_channel_essid_rsn(&(aplist + i)->ie, beaconlen, beacon->ie);
		if ((aplist + i)->ie.channel == 0)
			(aplist + i)->ie.channel = (scanlist + scanlistindex)->channel;
		if ((aplist + i)->ie.channel != (scanlist + scanlistindex)->channel)
			return;
		if ((aplist + i)->tsakt - (aplist + i)->tshold1 > TIMEBEACONNEW)
		{
			(aplist + i)->count = attemptapmax;
			memcpy((aplist + i)->macclient, &macbc, ETH_ALEN);
			(aplist + i)->tshold1 = tsakt;
		}
		if ((aplist + i)->count == 0)
			return;
		if (associationflag == true)
		{
			if (((aplist + i)->count % 8) == 6)
			{
				if (((aplist + i)->status & AP_EAPOL_M1) == 0)
				{
					if (((aplist + i)->status & AP_ESSID) == AP_ESSID)
					{
						if (((aplist + i)->ie.flags & APRSNAKM_PSK) != 0)
							send_80211_authenticationrequest();
					}
				}
			}
		}
		if (deauthenticationflag == true)
		{
			if (((aplist + i)->count % 8) == 4)
			{
				if (((aplist + i)->ie.flags & AP_MFP) == 0)
				{
					if (((aplist + i)->ie.flags & APAKM_MASK) != 0)
					{
						if (memcmp(&macbc, (aplist + i)->macclient, ETH_ALEN) != 0)
						{
							send_80211_deauthentication_fm_ap((aplist + i)->macclient, (aplist + i)->macap, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
							send_80211_deauthentication_fm_client((aplist + i)->macclient, (aplist + i)->macap, WLAN_REASON_DEAUTH_LEAVING);
						}
						else
							send_80211_deauthentication_fm_ap(macbc, (aplist + i)->macap, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
					}
				}
			}
		}
		if (reassociationflag == true)
		{
			if (((aplist + i)->count % 8) == 2)
			{
				if (((aplist + i)->ie.flags & APRSNAKM_PSK) != 0)
					send_80211_associationrequest_org(i);
			}
			if (((aplist + i)->count % 8) == 0)
			{
				if (((aplist + i)->ie.flags & APRSNAKM_PSK) != 0)
					send_80211_reassociationrequest(i);
			}
		}
		(aplist + i)->count -= 1;
		return;
	}
	memset((aplist + i), 0, APLIST_SIZE);
	(aplist + i)->tsakt = tsakt;
	(aplist + i)->tshold1 = tsakt;
	(aplist + i)->tsauth = tsfirst;
	(aplist + i)->count = attemptapmax;
	memcpy((aplist + i)->macap, macfrx->addr3, ETH_ALEN);
	memcpy((aplist + i)->macclient, &macbc, ETH_ALEN);
	(aplist + i)->status |= AP_BEACON;
	tagwalk_channel_essid_rsn(&(aplist + i)->ie, beaconlen, beacon->ie);
	if ((aplist + i)->ie.channel == 0)
		(aplist + i)->ie.channel = (scanlist + scanlistindex)->channel;
	if ((aplist + i)->ie.channel != (scanlist + scanlistindex)->channel)
		return;
	if (((aplist + i)->ie.flags & APIE_ESSID) == APIE_ESSID)
		(aplist + i)->status |= AP_ESSID;
	if (deauthenticationflag == true)
	{
		if (((aplist + i)->ie.flags & AP_MFP) == 0)
		{
			if (((aplist + i)->ie.flags & APAKM_MASK) != 0)
				send_80211_deauthentication_fm_ap(macbc, (aplist + i)->macap, WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
		}
	}
	if (associationflag == true)
	{
		if ((((aplist + i)->ie.flags & APRSNAKM_PSK) != 0) && (((aplist + i)->ie.flags & APIE_ESSID) == 0))
			send_80211_authenticationrequestnoack();
	}
	if (proberequestflag == true)
	{
		if (((aplist + i)->ie.flags & APIE_ESSID) == 0)
			send_80211_proberequest_undirected();
	}
	if (reassociationflag == true)
	{
		if (((aplist + i)->ie.flags & APRSNAKM_PSK) != 0)
			send_80211_associationrequest_org(i);
	}
	writeepb();
	qsort(aplist, i + 1, APLIST_SIZE, sort_aplist_by_tsakt);
	tshold = tsakt;
	return;
}
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
static inline __attribute__((always_inline)) void process_packet(void)
{
	if ((packetlen = read(fd_socket_rx, packetptr, PCAPNG_SNAPLEN)) < RTHRX_SIZE)
	{
		if (packetlen == -1)
			errorcount++;
		return;
	}
	totalcapturedcount++;
	rth = (rth_t *)packetptr;
#ifndef __LITTLE_ENDIAN__
	if ((rth->it_present & IEEE80211_RADIOTAP_DBM_ANTSIGNAL) == 0)
		return;
	if (rth->it_len > packetlen)
	{
		errorcount++;
		return;
	}
	ieee82011ptr = packetptr + rth->it_len;
	ieee82011len = packetlen - rth->it_len;
#else
	if ((le32toh(rth->it_present) & IEEE80211_RADIOTAP_DBM_ANTSIGNAL) == 0)
		return;
	if (le16toh(rth->it_len) > packetlen)
	{
		errorcount++;
		return;
	}
	ieee82011ptr = packetptr + le16toh(rth->it_len);
	ieee82011len = packetlen - le16toh(rth->it_len);
#endif
	if (ieee82011len <= MAC_SIZE_RTS)
		return;
	macfrx = (ieee80211_mac_t *)ieee82011ptr;
	if ((macfrx->from_ds == 1) && (macfrx->to_ds == 1))
	{
		payloadptr = ieee82011ptr + MAC_SIZE_LONG;
		payloadlen = ieee82011len - MAC_SIZE_LONG;
	}
	else
	{
		payloadptr = ieee82011ptr + MAC_SIZE_NORM;
		payloadlen = ieee82011len - MAC_SIZE_NORM;
	}
	clock_gettime(CLOCK_REALTIME, &tspecakt);
	tsakt = ((u64)tspecakt.tv_sec * 1000000000ULL) + tspecakt.tv_nsec;
	packetcount++;
	if (macfrx->type == IEEE80211_FTYPE_MGMT)
	{
		if (macfrx->subtype == IEEE80211_STYPE_BEACON)
			process80211beacon();
		else if (macfrx->subtype == IEEE80211_STYPE_PROBE_RESP)
			process80211proberesponse();
		else if (macfrx->subtype == IEEE80211_STYPE_PROBE_REQ)
		{
			if (memcmp(&macbc, macfrx->addr3, ETH_ALEN) == 0)
				process80211proberequest_undirected();
			else
				process80211proberequest_directed();
		}
		else if (macfrx->subtype == IEEE80211_STYPE_AUTH)
			process80211authentication();
		else if (macfrx->subtype == IEEE80211_STYPE_ASSOC_REQ)
			process80211associationrequest();
		else if (macfrx->subtype == IEEE80211_STYPE_ASSOC_RESP)
			process80211associationresponse();
		else if (macfrx->subtype == IEEE80211_STYPE_REASSOC_REQ)
			process80211reassociationrequest();
		else if (macfrx->subtype == IEEE80211_STYPE_REASSOC_RESP)
			process80211reassociationresponse();
		else if (macfrx->subtype == IEEE80211_STYPE_ACTION)
			process80211action();
	}
	else if (macfrx->type == IEEE80211_FTYPE_CTL)
	{
		if (macfrx->subtype == IEEE80211_STYPE_BACK)
			process80211blockack();
		else if (macfrx->subtype == IEEE80211_STYPE_BACK)
			process80211blockackreq();
		else if (macfrx->subtype == IEEE80211_STYPE_PSPOLL)
			process80211pspoll();
	}
	else if (macfrx->type == IEEE80211_FTYPE_DATA)
	{
		if ((macfrx->subtype & IEEE80211_STYPE_QOS_DATA) == IEEE80211_STYPE_QOS_DATA)
		{
			payloadptr += IEEE80211_QOS_SIZE;
			payloadlen -= IEEE80211_QOS_SIZE;
		}
		if (payloadlen > IEEE80211_LLC_SIZE)
		{
			llcptr = payloadptr;
			llc = (ieee80211_llc_t *)llcptr;
			if (((ntohs(llc->type)) == LLC_TYPE_AUTH) && (llc->dsap == IEEE80211_LLC_SNAP) && (llc->ssap == IEEE80211_LLC_SNAP))
				process80211eapauthentication();
		}
		if ((macfrx->subtype & IEEE80211_STYPE_QOS_NULLFUNC) == IEEE80211_STYPE_QOS_NULLFUNC)
			process80211qosnull();
		else if ((macfrx->subtype & IEEE80211_STYPE_NULLFUNC) == IEEE80211_STYPE_NULLFUNC)
			process80211null();
		else if ((macfrx->subtype & IEEE80211_STYPE_QOS_DATA) == IEEE80211_STYPE_QOS_DATA)
			process80211qosdata();
	}
	return;
}
/*===========================================================================*/
/*===========================================================================*/
/* MAIN SCAN LOOP */
static bool nl_scanloop(void)
{
	static ssize_t i;
	static int fd_epoll = 0;
	static int epi = 0;
	static int epret = 0;
	static struct epoll_event ev, events[EPOLL_EVENTS_MAX];
	static size_t packetcountlast = 0;
	static u64 timer1count;

	// Some straight Linux magic right here.
	// Create and get FD of epoll instance.
	if ((fd_epoll = epoll_create(1)) < 0)
		return false;

	// Add fd_socket_rx to epoll interest list. Register to events on the rx socket.
	ev.data.fd = fd_socket_rx;
	ev.events = EPOLLIN;
	if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_socket_rx, &ev) < 0)
		return false;
	epi++;

	// Add fd_timer1 to epoll interest list. Register to events on the timer FD.
	ev.data.fd = fd_timer1;
	ev.events = EPOLLIN;
	if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_timer1, &ev) < 0)
		return false;
	epi++;

	// wanteventflag is a u8 set of fbitlags to track the "exit state" of the program using a single byte. If any flag gets tripped, the program will exit.
	// EXIT_ON_SIGTERM 0x01			: 00000001
	// EXIT_ON_TOT 0x04				: 00000100
	// EXIT_ON_WATCHDOG 0x08		: 00001000
	// EXIT_ON_EAPOL_PMKID 0x10		: 00010000
	// EXIT_ON_EAPOL_M2 0x20		: 00100000
	// EXIT_ON_EAPOL_M3 0x40 		: 01000000
	// EXIT_ON_ERROR 0x80 			: 10000000

	while (!wanteventflag)
	{

		// Handle errorcount reaching errormaxcount
		if (errorcount > errorcountmax)
			wanteventflag |= EXIT_ON_ERROR;

		// Wait for an event from the epoll instance (from our timer or the rx socket) Timeout is 100 by default
		// epret = number of fd's ready.
		epret = epoll_pwait(fd_epoll, events, epi, timerwaitnd, NULL);
		if (epret == -1)
		{
			if (errno != EINTR)
				errorcount++;
			continue;
		}

		// iterate through FD events
		for (i = 0; i < epret; i++)
		{
			// Process FD by type
			if (events[i].data.fd == fd_socket_rx)	 // Event came from fd_socket_rx (we got a packet)
				process_packet();					 // process the packet.
			else if (events[i].data.fd == fd_timer1) // The event came from the timer.
			{
				// The timer is set to fire every 1 second, so every second the event will fire.

				// Handle error in time.
				if (read(fd_timer1, &timer1count, sizeof(u64)) == -1)
					errorcount++;

				// lifetime of program goes up by one (beacuse interval is seconds)
				lifetime++;

				// Get time from CLOCK_REALTIME and asssign to tspecakt
				clock_gettime(CLOCK_REALTIME, &tspecakt);
				// join seconds and nanoseconds to get one complete nanosecond value, store in tsakt.
				tsakt = ((u64)tspecakt.tv_sec * 1000000000ULL) + tspecakt.tv_nsec;

				// show realtime function gets called from here.
				// show_realtime();
				send_lists();

				// Handle channel switching.
				if ((tsakt - tshold) > timehold)
				{
					scanlistindex++;
					if (nl_set_frequency() == false)
						errorcount++;
					tshold = tsakt;
				}

				// Handle timeout timer.
				if ((tottime > 0) && (lifetime >= tottime))
					wanteventflag |= EXIT_ON_TOT;

				// Handle watchdog timer.
				if ((lifetime % watchdogcountmax) == 0)
				{
					if (packetcount == packetcountlast)
						wanteventflag |= EXIT_ON_WATCHDOG;
					packetcountlast = packetcount;
				}
			}
		}
		// if nothing was waiting for us, send a beacon. (I'm assuming to spice up the airwaves?)
		if (epret == 0)
			send_80211_beacon();
	}
	return true;
}
/*===========================================================================*/

/*===========================================================================*/
/* NETLINK */
static struct nlattr *nla_next(const struct nlattr *nla, int *remaining)
{
	int totlen = NLA_ALIGN(nla->nla_len);

	*remaining -= totlen;
	return (struct nlattr *)((u8 *)nla + totlen);
}
/*---------------------------------------------------------------------------*/
static int nla_ok(const struct nlattr *nla, int remaining)
{
	size_t r = remaining;

	return r >= sizeof(*nla) && nla->nla_len >= sizeof(*nla) && nla->nla_len <= r;
}
/*---------------------------------------------------------------------------*/
static int nla_datalen(const struct nlattr *nla)
{
	return nla->nla_len - NLA_HDRLEN;
}
/*---------------------------------------------------------------------------*/
static void *nla_data(const struct nlattr *nla)
{
	return (u8 *)nla + NLA_HDRLEN;
}
/*---------------------------------------------------------------------------*/
static void nl_get_supported_bands(interface_t *ipl, struct nlattr *nla)
{
	static int nlanremlen;
	static struct nlattr *nlai, *nlan;
	static frequencylist_t *freql;

	nlai = (struct nlattr *)nla_data(nla);
	nlan = (struct nlattr *)nla_data(nlai);
	if (nlan->nla_type != NL80211_BAND_ATTR_FREQS)
		return;
	nlai = (struct nlattr *)nla_data(nlan);
	nlanremlen = nlai->nla_len - sizeof(struct nlattr);
	nlan = (struct nlattr *)nla_data(nlai);
	freql = ipl->frequencylist;
	if (ipl->i > FREQUENCYLIST_MAX - 1)
		return;
	(freql + ipl->i)->frequency = 0;
	(freql + ipl->i)->pwr = 0;
	(freql + ipl->i)->status = 0;
	while (nla_ok(nlan, nlanremlen))
	{
		if (nlan->nla_type == NL80211_FREQUENCY_ATTR_FREQ)
		{
			(freql + ipl->i)->frequency = *((u32 *)nla_data(nlan));
			(freql + ipl->i)->channel = frequency_to_channel((freql + ipl->i)->frequency);
		}
		if (nlan->nla_type == NL80211_FREQUENCY_ATTR_MAX_TX_POWER)
			(freql + ipl->i)->pwr = *((u32 *)nla_data(nlan));
		if (nlan->nla_type == NL80211_FREQUENCY_ATTR_DISABLED)
			(freql + ipl->i)->status = IF_STAT_FREQ_DISABLED;
		nlan = nla_next(nlan, &nlanremlen);
	}
	if ((freql + ipl->i)->frequency != 0)
		ipl->i++;
	return;
}
/*---------------------------------------------------------------------------*/
static u8 nl_get_supported_iftypes(struct nlattr *nla)
{
	struct nlattr *pos = (struct nlattr *)nla_data(nla);
	int nestremlen = nla_datalen(nla);
	while (nla_ok(pos, nestremlen))
	{
		if (pos->nla_type == NL80211_IFTYPE_MONITOR)
			return IF_HAS_MONITOR;
		pos = nla_next(pos, &nestremlen);
	}
	return 0;
}
/*---------------------------------------------------------------------------*/
static bool nl_get_interfacelist(void)
{
	static ssize_t i;
	static size_t ii;
	static ssize_t msglen;
	static int nlremlen = 0;
	static u32 ifindex;
	static u32 wiphy;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;
	static char ifname[IF_NAMESIZE];
	static u8 vimac[ETH_ALEN];

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = nlfamily;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = NL80211_CMD_GET_INTERFACE;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return false;
	ii = 0;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				nlfamily = 0;
				return false;
			}
			glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
			if (glh->cmd != NL80211_CMD_NEW_INTERFACE)
				continue;
			nla = (struct nlattr *)((unsigned char *)NLMSG_DATA(nlh) + sizeof(struct genlmsghdr));
			nlremlen = NLMSG_PAYLOAD(nlh, 0) - 4;
			while (nla_ok(nla, nlremlen))
			{
				if (nla->nla_type == NL80211_ATTR_IFINDEX)
					ifindex = *((u32 *)nla_data(nla));
				if (nla->nla_type == NL80211_ATTR_IFNAME)
					strncpy(ifname, nla_data(nla), IF_NAMESIZE - 1);
				if (nla->nla_type == NL80211_ATTR_WIPHY)
				{
					wiphy = *((u32 *)nla_data(nla));
				}
				if (nla->nla_type == NL80211_ATTR_MAC)
				{
					if (nla->nla_len == 10)
						memcpy(vimac, nla_data(nla), ETH_ALEN);
				}
				nla = nla_next(nla, &nlremlen);
			}
			for (ii = 0; ii < INTERFACELIST_MAX; ii++)
			{
				if ((ifpresentlist + ii)->wiphy == wiphy)
				{
					(ifpresentlist + ii)->index = ifindex;
					strncpy((ifpresentlist + ii)->name, ifname, IF_NAMESIZE);
					memcpy((ifpresentlist + ii)->vimac, &vimac, ETH_ALEN);
					break;
				}
			}
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static bool nl_get_interfacestatus(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static int nlremlen = 0;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = nlfamily;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = NL80211_CMD_GET_INTERFACE;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_IFINDEX;
	*(u32 *)nla_data(nla) = ifaktindex;
	i += 8;
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				nlfamily = 0;
				return false;
			}
			glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
			if (glh->cmd != NL80211_CMD_NEW_INTERFACE)
				continue;
			nla = (struct nlattr *)((unsigned char *)NLMSG_DATA(nlh) + sizeof(struct genlmsghdr));
			nlremlen = NLMSG_PAYLOAD(nlh, 0) - 4;
			while (nla_ok(nla, nlremlen))
			{
				if (nla->nla_type == NL80211_ATTR_IFTYPE)
				{
					if (*((u32 *)nla_data(nla)) == NL80211_IFTYPE_MONITOR)
						ifaktstatus |= IF_STAT_MONITOR;
				}
				nla = nla_next(nla, &nlremlen);
			}
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static bool nl_get_regulatorydomain(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static int nlremlen = 0;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;

	country[0] = 0;
	country[1] = 0;
	country[2] = 0;
	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = nlfamily;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = NL80211_CMD_GET_REG;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				nlfamily = 0;
				return false;
			}
			glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
			if (glh->cmd != NL80211_CMD_GET_REG)
				continue;
			nla = (struct nlattr *)((unsigned char *)NLMSG_DATA(nlh) + sizeof(struct genlmsghdr));
			nlremlen = NLMSG_PAYLOAD(nlh, 0) - 4;
			while (nla_ok(nla, nlremlen))
			{
				if (nla->nla_type == NL80211_ATTR_REG_ALPHA2)
				{
					if (nla->nla_len == 7)
						memcpy(country, nla_data(nla), 2);
				}
				nla = nla_next(nla, &nlremlen);
			}
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static bool nl_get_interfacecapabilities(void)
{
	static ssize_t i;
	static ssize_t ii;
	static ssize_t msglen;
	static int nlremlen;
	static size_t dnlen;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;
	static char *drivername = NULL;
	static char driverfmt[128] = {0};
	static char driverlink[128] = {0};

	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = nlfamily;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = NL80211_CMD_GET_WIPHY;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 4;
	nla->nla_type = NL80211_ATTR_SPLIT_WIPHY_DUMP;
	*(u32 *)nla_data(nla) = ifaktindex;
	i += 4;
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return false;
	ii = 0;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				return false;
			}
			nlremlen = 0;
			glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
			if (glh->cmd != NL80211_CMD_NEW_WIPHY)
				continue;
			nla = (struct nlattr *)((unsigned char *)NLMSG_DATA(nlh) + sizeof(struct genlmsghdr));
			nlremlen = NLMSG_PAYLOAD(nlh, 0) - 4;
			while (nla_ok(nla, nlremlen))
			{
				if (nla->nla_type == NL80211_ATTR_WIPHY)
				{
					(ifpresentlist + ii)->wiphy = *((u32 *)nla_data(nla));
					snprintf(driverfmt, 64, "/sys/class/ieee80211/phy%d/device/driver", (ifpresentlist + ii)->wiphy);
					memset(&driverlink, 0, 128);
					if ((dnlen = readlink(driverfmt, driverlink, 64)) > 0)
					{
						drivername = basename(driverlink);
						if (drivername != NULL)
							strncpy((ifpresentlist + ii)->driver, drivername, DRIVERNAME_MAX - 1);
					}
				}
				if (nla->nla_type == NL80211_ATTR_SUPPORTED_IFTYPES)
				{
					(ifpresentlist + ii)->type |= nl_get_supported_iftypes(nla);
					(ifpresentlist + ii)->type |= IF_HAS_NETLINK;
				}
				if (nla->nla_type == NL80211_ATTR_WIPHY_BANDS)
					nl_get_supported_bands((ifpresentlist + ii), nla);
				if (nla->nla_type == NL80211_ATTR_FEATURE_FLAGS)
				{
					if ((*((u32 *)nla_data(nla)) & NL80211_FEATURE_ACTIVE_MONITOR) == NL80211_FEATURE_ACTIVE_MONITOR)
						(ifpresentlist + ii)->type |= IF_HAS_MONITOR_ACTIVE;
				}
				nla = nla_next(nla, &nlremlen);
			}
		}
		if (ii < INTERFACELIST_MAX)
			ii++;
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static inline bool nl_set_frequency(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;

	i = 0;
	if (((scanlist + scanlistindex)->frequency) == 0)
		scanlistindex = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = nlfamily;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = NL80211_CMD_SET_WIPHY;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_IFINDEX;
	*(u32 *)nla_data(nla) = ifaktindex;
	i += 8;
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_WIPHY_FREQ;
	*(u32 *)nla_data(nla) = (scanlist + scanlistindex)->frequency;
	i += 8;
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_CHANNEL_WIDTH;
	*(u32 *)nla_data(nla) = NL80211_CHAN_WIDTH_20_NOHT;
	i += 8;
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_WIPHY_CHANNEL_TYPE;
	*(u32 *)nla_data(nla) = NL80211_CHAN_NO_HT;
	i += 8;
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_CENTER_FREQ1;
	*(u32 *)nla_data(nla) = (scanlist + scanlistindex)->frequency;
	i += 8;
	if (((scanlist + scanlistindex)->frequency) <= 2484)
		memcpy(&nltxbuffer[i], legacy241mbdata, LEGACYXXXMB_SIZE);
	else
		memcpy(&nltxbuffer[i], legacy56mbdata, LEGACYXXXMB_SIZE);
	i += LEGACYXXXMB_SIZE;
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				return false;
			}
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static inline void nl_set_powersave_off(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = nlfamily;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = NL80211_CMD_SET_INTERFACE;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_IFINDEX;
	*(u32 *)nla_data(nla) = ifaktindex;
	i += 8;
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_PS_STATE;
	*(u32 *)nla_data(nla) = NL80211_PS_DISABLED;
	i += 8;
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return;
				errorcount++;
				return;
			}
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static inline bool nl_set_monitormode(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = nlfamily;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = NL80211_CMD_SET_INTERFACE;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_IFINDEX;
	*(u32 *)nla_data(nla) = ifaktindex;
	i += 8;
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_len = 8;
	nla->nla_type = NL80211_ATTR_IFTYPE;
	*(u32 *)nla_data(nla) = NL80211_IFTYPE_MONITOR;
	i += 8;
	if (((ifakttype & IFTYPEMONACT) == IFTYPEMONACT) && (activemonitorflag == true))
	{
		nla = (struct nlattr *)(nltxbuffer + i);
		nla->nla_len = 8;
		nla->nla_type = NL80211_ATTR_MNTR_FLAGS;
		nla = (struct nlattr *)nla_data(nla);
		nla->nla_len = 4;
		nla->nla_type = NL80211_MNTR_FLAG_ACTIVE;
		i += 8;
	}
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				return false;
			}
		}
	}
	return false;
}
/*===========================================================================*/
/* RTLINK */
static void *rta_data(const struct rtattr *rta)
{
	return (u8 *)rta + 4;
}
/*---------------------------------------------------------------------------*/
static bool rt_set_interfacemac(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct ifinfomsg *ifih;
	static struct rtattr *rta;
	static struct nlmsgerr *nle;

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	ifih = (struct ifinfomsg *)(nltxbuffer + i);
	ifih->ifi_family = 0;
	ifih->ifi_type = 0;
	ifih->ifi_index = ifaktindex;
	ifih->ifi_flags = 0;
	ifih->ifi_change = 0;
	i += sizeof(struct ifinfomsg);
	rta = (struct rtattr *)(nltxbuffer + i);
	rta->rta_len = 10;
	rta->rta_type = IFLA_ADDRESS;
	memcpy(&nltxbuffer[i + 4], &macclientrg, ETH_ALEN + 2);
	i += 12;
	nlh->nlmsg_len = i;
	if ((write(fd_socket_rt, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_rt, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return false;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				return false;
			}
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static bool rt_set_interface(u32 condition)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct ifinfomsg *ifih;
	static struct nlmsgerr *nle;

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	ifih = (struct ifinfomsg *)(nltxbuffer + i);
	ifih->ifi_family = 0;
	ifih->ifi_type = 0;
	ifih->ifi_index = ifaktindex;
	ifih->ifi_flags = condition;
	ifih->ifi_change = 1;
	i += sizeof(struct ifinfomsg);
	nlh->nlmsg_len = i;
	if ((write(fd_socket_rt, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_rt, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return false;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				return false;
			}
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static bool rt_get_interfacestatus(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct ifinfomsg *ifih;
	static struct nlmsgerr *nle;

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	ifih = (struct ifinfomsg *)(nltxbuffer + i);
	ifih->ifi_family = AF_PACKET;
	ifih->ifi_type = 0;
	ifih->ifi_index = ifaktindex;
	ifih->ifi_flags = 0;
	ifih->ifi_change = 0;
	i += sizeof(struct ifinfomsg);
	nlh->nlmsg_len = i;
	if ((write(fd_socket_rt, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_rt, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return false;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				return false;
			}
			ifih = (struct ifinfomsg *)NLMSG_DATA(nlh);
			if ((ifih->ifi_flags & IFF_UP) == IFF_UP)
				ifaktstatus |= IF_STAT_UP;
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static bool rt_get_interfacelist(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct ifinfomsg *ifih;
	static struct nlmsgerr *nle;
	static struct rtattr *rta;
	static int rtaremlen;
	static u8 hwmac[ETH_ALEN];

	i = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	ifih = (struct ifinfomsg *)(nltxbuffer + i);
	ifih->ifi_family = AF_PACKET;
	ifih->ifi_type = 0;
	ifih->ifi_index = 0;
	ifih->ifi_flags = 0;
	ifih->ifi_change = 0;
	i += sizeof(struct ifinfomsg);
	rta = (struct rtattr *)(nltxbuffer + i);
	rta->rta_type = IFLA_EXT_MASK;
	*(u32 *)rta_data(rta) = 1;
	rta->rta_len = 8;
	i += 8;
	nlh->nlmsg_len = i;
	if ((write(fd_socket_rt, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_rt, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				return false;
			}
			ifih = (struct ifinfomsg *)NLMSG_DATA(nlh);
			if ((ifih->ifi_flags & IFF_UP) == IFF_UP)
				ifaktstatus |= IF_STAT_UP;
			rta = (struct rtattr *)((unsigned char *)NLMSG_DATA(nlh) + sizeof(struct ifinfomsg));
			rtaremlen = NLMSG_PAYLOAD(nlh, 0) - sizeof(struct ifinfomsg);
			while (RTA_OK(rta, rtaremlen))
			{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
				if (rta->rta_type == IFLA_PERM_ADDRESS)
				{
					if (rta->rta_len == 10)
						memcpy(hwmac, rta_data(rta), ETH_ALEN);
				}
#else
				if (rta->rta_type == IFLA_ADDRESS)
				{
					if (rta->rta_len == 10)
						memcpy(hwmac, rta_data(rta), ETH_ALEN);
				}
#endif
				rta = RTA_NEXT(rta, rtaremlen);
			}
			for (i = 0; i < INTERFACELIST_MAX; i++)
			{
				if ((ifpresentlist + i)->index == ifih->ifi_index)
					memcpy((ifpresentlist + i)->hwmac, &hwmac, ETH_ALEN);
			}
		}
	}
	return false;
}
/*---------------------------------------------------------------------------*/
static bool nl_get_familyid(void)
{
	static ssize_t i;
	static ssize_t msglen;
	static struct nlmsghdr *nlh;
	static struct genlmsghdr *glh;
	static struct nlattr *nla;
	static struct nlmsgerr *nle;
	static int nlremlen = 0;

	i = 0;
	nlfamily = 0;
	nlh = (struct nlmsghdr *)nltxbuffer;
	nlh->nlmsg_type = GENL_ID_CTRL;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = nlseqcounter++;
	nlh->nlmsg_pid = hcxpid;
	i += sizeof(struct nlmsghdr);
	glh = (struct genlmsghdr *)(nltxbuffer + i);
	glh->cmd = CTRL_CMD_GETFAMILY;
	glh->version = 1;
	glh->reserved = 0;
	i += sizeof(struct genlmsghdr);
	nla = (struct nlattr *)(nltxbuffer + i);
	nla->nla_type = CTRL_ATTR_FAMILY_NAME;
	i += sizeof(struct nlattr);
	memcpy(nltxbuffer + i, NL80211_GENL_NAME, sizeof(NL80211_GENL_NAME));
	i += sizeof(NL80211_GENL_NAME);
	nla->nla_len = sizeof(struct nlattr) + sizeof(NL80211_GENL_NAME);
	nlh->nlmsg_len = i;
	if ((write(fd_socket_nl, nltxbuffer, i)) != i)
		return false;
	while (1)
	{
		msglen = read(fd_socket_nl, &nlrxbuffer, NLRX_SIZE);
		if (msglen == -1)
			break;
		if (msglen == 0)
			break;
		for (nlh = (struct nlmsghdr *)nlrxbuffer; NLMSG_OK(nlh, (u32)msglen); nlh = NLMSG_NEXT(nlh, msglen))
		{
			if (nlh->nlmsg_type == NLMSG_DONE)
				return true;
			if (nlh->nlmsg_type == NLMSG_ERROR)
			{
				nle = (struct nlmsgerr *)(nlrxbuffer + sizeof(struct nlmsghdr));
				if (nle->error == 0)
					return true;
				errorcount++;
				nlfamily = 0;
				return false;
			}
			glh = (struct genlmsghdr *)NLMSG_DATA(nlh);
			nla = (struct nlattr *)((unsigned char *)NLMSG_DATA(nlh) + sizeof(struct genlmsghdr));
			nlremlen = 0;
			nlremlen = NLMSG_PAYLOAD(nlh, 0) - 4;
			while (nla_ok(nla, nlremlen))
			{
				if (nla->nla_type == CTRL_ATTR_FAMILY_ID)
					nlfamily = *((u32 *)nla_data(nla));
				nla = nla_next(nla, &nlremlen);
			}
		}
	}
	nlfamily = 0;
	return false;
}
/*===========================================================================*/
static void usrfrequency_to_scanlist(u16 ufrq)
{
	size_t i;

	if (ufrq == 0)
		return;
	for (i = 0; i < (FREQUENCYLIST_MAX - 1); i++)
	{
		if ((ifaktfrequencylist + i)->status == 0)
		{
			if ((ifaktfrequencylist + i)->frequency == ufrq)
			{
				(scanlist + scanlistindex)->frequency = ufrq;
				(scanlist + scanlistindex)->channel = frequency_to_channel(ufrq);
				scanlistindex++;
				if (scanlistindex >= (FREQUENCYLIST_MAX - 1))
					return;
				return;
			}
		}
	}
	return;
}
/*---------------------------------------------------------------------------*/
static bool set_interface(bool interfacefrequencyflag, char *userfrequencylistname, char *userchannellistname)
{
	static size_t i;
	static char *ufld = NULL;
	static char *tokptr = NULL;
	static char *userband = NULL;
	static u16 uband;
	static u32 ufreq;

	if (ifaktindex == 0)
	{
		for (i = 0; i < ifpresentlistcounter; i++)
		{
			if (((ifpresentlist + i)->type & IF_HAS_NLMON) == IF_HAS_NLMON)
			{
				ifaktindex = (ifpresentlist + i)->index;
				ifakttype = (ifpresentlist + i)->type;
				memcpy(&ifaktname, (ifpresentlist + i)->name, IF_NAMESIZE);
				memcpy(&ifakthwmac, (ifpresentlist + i)->hwmac, ETH_ALEN);
				ifaktfrequencylist = (ifpresentlist + i)->frequencylist;
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < ifpresentlistcounter; i++)
		{
			if ((ifpresentlist + i)->index == ifaktindex)
			{
				if (((ifpresentlist + i)->type & IF_HAS_NLMON) == 0)
					return false;
				ifakttype = (ifpresentlist + i)->type;
				memcpy(&ifakthwmac, (ifpresentlist + i)->hwmac, ETH_ALEN);
				ifaktfrequencylist = (ifpresentlist + i)->frequencylist;
				break;
			}
		}
	}
	if (ifaktfrequencylist == NULL)
		return false;
	if (rt_set_interface(0) == false)
		return false;
	if ((ifakttype & IF_HAS_MONITOR_ACTIVE) == IF_HAS_MONITOR_ACTIVE)
	{
		if (rt_set_interfacemac() == false)
			return false;
	}
	if (nl_set_monitormode() == false)
		return false;
	if (rt_set_interface(IFF_UP) == false)
		return false;
	nl_set_powersave_off();
	if (nl_get_interfacestatus() == false)
		return false;
	if (rt_get_interfacestatus() == false)
		return false;
	scanlistindex = 0;
	if (interfacefrequencyflag == true)
	{
		for (i = 0; i < (FREQUENCYLIST_MAX - 1); i++)
		{
			if ((ifaktfrequencylist + i)->status == 0)
			{
				(scanlist + scanlistindex)->frequency = (ifaktfrequencylist + i)->frequency;
				(scanlist + scanlistindex)->channel = (ifaktfrequencylist + i)->channel;
				scanlistindex++;
				if (scanlistindex >= (FREQUENCYLIST_MAX - 1))
					break;
			}
			if ((ifaktfrequencylist + i)->frequency == 0)
				break;
		}
	}
	else if ((userfrequencylistname != NULL) || (userchannellistname != NULL))
	{
		if (userfrequencylistname != NULL)
		{
			ufld = strdup(userfrequencylistname);
			tokptr = strtok(ufld, ",");
			while ((tokptr != NULL) && (i < (SCANLIST_MAX - 1)))
			{
				usrfrequency_to_scanlist(strtol(tokptr, NULL, 10));
				tokptr = strtok(NULL, ",");
			}
			free(ufld);
		}
		if (userchannellistname != NULL)
		{
			ufld = strdup(userchannellistname);
			tokptr = strtok(ufld, ",");
			while ((tokptr != NULL) && (i < (SCANLIST_MAX - 1)))
			{
				uband = strtol(tokptr, &userband, 10);
				if (userband[0] == 'a')
					ufreq = channel_to_frequency(uband, NL80211_BAND_2GHZ);
				else if (userband[0] == 'b')
					ufreq = channel_to_frequency(uband, NL80211_BAND_5GHZ);
				else if (userband[0] == 'c')
					ufreq = channel_to_frequency(uband, NL80211_BAND_6GHZ);
				else if (userband[0] == 'd')
					ufreq = channel_to_frequency(uband, NL80211_BAND_60GHZ);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
				else if (userband[0] == 'e')
					ufreq = channel_to_frequency(uband, NL80211_BAND_S1GHZ);
#endif
				usrfrequency_to_scanlist(ufreq);
				tokptr = strtok(NULL, ",");
			}
			free(ufld);
		}
	}
	else
	{
		(scanlist + scanlistindex)->frequency = 2412;
		(scanlist + scanlistindex++)->channel = 1;
		(scanlist + scanlistindex)->frequency = 2437;
		(scanlist + scanlistindex++)->channel = 6;
		(scanlist + scanlistindex)->frequency = 2462;
		(scanlist + scanlistindex++)->channel = 11;
		(scanlist + scanlistindex)->frequency = 0;
		(scanlist + scanlistindex)->channel = 0;
	}
	scanlistindex = 0;
	if (nl_set_frequency() == false)
		return false;
	// show_interfacecapabilities2();
	return true;
}
/*===========================================================================*/

static bool get_interfacelist(void)
{
	static size_t i;

	nl_get_familyid();
	if (nlfamily == 0)
	{
		errorcount++;
		return false;
	}
	nl_get_regulatorydomain();
	if (nl_get_interfacecapabilities() == false)
		return false;
	if (nl_get_interfacelist() == false)
		return false;
	for (i = 0; i < INTERFACELIST_MAX - 1; i++)
	{
		if ((ifpresentlist + i)->index == 0)
			break;
		ifpresentlistcounter++;
	}
	if (rt_get_interfacelist() == false)
		return false;
	if (ifpresentlistcounter == 0)
		return false;
	qsort(ifpresentlist, ifpresentlistcounter, INTERFACELIST_SIZE, sort_interfacelist_by_index);
	return true;
}
/*===========================================================================*/
/* RAW PACKET SOCKET */
static bool open_socket_tx(void)
{
	static struct sockaddr_ll saddr;
	static struct packet_mreq mrq;
	static int socket_rx_flags;
	static int prioval;
	static socklen_t priolen;

	if ((fd_socket_tx = socket(PF_PACKET, SOCK_RAW | SOCK_CLOEXEC, htons(ETH_P_ALL))) < 0)
		return false;
	memset(&mrq, 0, sizeof(mrq));
	mrq.mr_ifindex = ifaktindex;
	mrq.mr_type = PACKET_MR_PROMISC;
	if (setsockopt(fd_socket_tx, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mrq, sizeof(mrq)) < 0)
		return false;
	priolen = sizeof(prioval);
	prioval = 20;
	if (setsockopt(fd_socket_rx, SOL_SOCKET, SO_PRIORITY, &prioval, priolen) < 0)
		return false;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sll_family = PF_PACKET;
	saddr.sll_ifindex = ifaktindex;
	saddr.sll_protocol = htons(ETH_P_ALL);
	saddr.sll_halen = ETH_ALEN;
	saddr.sll_pkttype = PACKET_OTHERHOST;
	if (bind(fd_socket_tx, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
		return false;
	if ((socket_rx_flags = fcntl(fd_socket_rx, F_GETFL, 0)) < 0)
		return false;
	if (fcntl(fd_socket_tx, F_SETFL, socket_rx_flags | O_NONBLOCK) < 0)
		return false;
	return true;
}
/*---------------------------------------------------------------------------*/
static bool open_socket_rx()
{
	size_t c = 10;
	static struct sockaddr_ll saddr;
	static struct packet_mreq mrq;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	static int enable = 1;
#endif
	static int socket_rx_flags;
	static int prioval;
	static socklen_t priolen;

	if ((fd_socket_rx = socket(PF_PACKET, SOCK_RAW | SOCK_CLOEXEC, htons(ETH_P_ALL))) < 0)
		return false;
	memset(&mrq, 0, sizeof(mrq));
	mrq.mr_ifindex = ifaktindex;
	mrq.mr_type = PACKET_MR_PROMISC;
	if (setsockopt(fd_socket_rx, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mrq, sizeof(mrq)) < 0)
		return false;
	priolen = sizeof(prioval);
	prioval = 20;
	if (setsockopt(fd_socket_rx, SOL_SOCKET, SO_PRIORITY, &prioval, priolen) < 0)
		return false;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	if (setsockopt(fd_socket_rx, SOL_PACKET, PACKET_IGNORE_OUTGOING, &enable, sizeof(int)) < 0)
		printError("PACKET_IGNORE_OUTGOING is not supported by kernel. Falling back to validate radiotap header length", 0);
#endif
	if (bpf.len > 0)
	{
		if (setsockopt(fd_socket_rx, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)) < 0)
			return false;
	}
	memset(&saddr, 0, sizeof(saddr));
	saddr.sll_family = PF_PACKET;
	saddr.sll_ifindex = ifaktindex;
	saddr.sll_protocol = htons(ETH_P_ALL);
	saddr.sll_halen = ETH_ALEN;
	saddr.sll_pkttype = PACKET_OTHERHOST;
	if (bind(fd_socket_rx, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
		return false;
	if ((socket_rx_flags = fcntl(fd_socket_rx, F_GETFL, 0)) < 0)
		return false;
	if (fcntl(fd_socket_rx, F_SETFL, socket_rx_flags | O_NONBLOCK) < 0)
		return false;
	while ((!wanteventflag) || (c != 0))
	{
		packetlen = read(fd_socket_rx, epb + EPB_SIZE, PCAPNG_SNAPLEN);
		if (packetlen == -1)
			break;
		c--;
	}
	return true;
}

/*===========================================================================*/
/* CONTROL SOCKETS */

static void close_sockets(void)
{
	if (fd_socket_unix != 0)
		close(fd_socket_unix);
	if (fd_socket_rt != 0)
		close(fd_socket_rt);
	if (fd_socket_nl != 0)
		close(fd_socket_nl);
	if (fd_socket_tx != 0)
		close(fd_socket_tx);
	if (bpf.filter != NULL)
	{
		if (fd_socket_rx > 0)
			setsockopt(fd_socket_rx, SOL_SOCKET, SO_DETACH_FILTER, &bpf, sizeof(bpf));
		free(bpf.filter);
	}
	if (fd_socket_rx != 0)
		close(fd_socket_rx);
	return;
}
/*---------------------------------------------------------------------------*/
static bool open_socket_unix(void)
{
	if ((fd_socket_unix = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		return false;
	return true;
}
/*---------------------------------------------------------------------------*/
static bool open_socket_rt(void)
{
	static struct sockaddr_nl saddr;
	static int nltxbuffsize = NLTX_SIZE;
	static int nlrxbuffsize = NLRX_SIZE;

	if ((fd_socket_rt = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE)) < 0)
		return false;
	if (setsockopt(fd_socket_rt, SOL_SOCKET, SO_SNDBUF, &nltxbuffsize, sizeof(nltxbuffsize)) < 0)
		return false;
	if (setsockopt(fd_socket_rt, SOL_SOCKET, SO_RCVBUF, &nlrxbuffsize, sizeof(nlrxbuffsize)) < 0)
		return false;
	memset(&saddr, 0, sizeof(saddr));
	saddr.nl_family = AF_NETLINK;
	saddr.nl_pid = getpid();
	if (bind(fd_socket_rt, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
		return false;
	return true;
}
/*---------------------------------------------------------------------------*/
static bool open_socket_nl(void)
{
	static struct sockaddr_nl saddr;
	static int nltxbuffsize = NLTX_SIZE;
	static int nlrxbuffsize = NLRX_SIZE;

	if ((fd_socket_nl = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC)) < 0)
		return false;
	if (setsockopt(fd_socket_nl, SOL_SOCKET, SO_SNDBUF, &nltxbuffsize, sizeof(nltxbuffsize)) < 0)
		return false;
	if (setsockopt(fd_socket_nl, SOL_SOCKET, SO_RCVBUF, &nlrxbuffsize, sizeof(nlrxbuffsize)) < 0)
		return false;
	if (fcntl(fd_socket_nl, F_SETFL, O_NONBLOCK) < 0)
		return false;
	memset(&saddr, 0, sizeof(saddr));
	saddr.nl_family = AF_NETLINK;
	saddr.nl_pid = hcxpid;
	if (bind(fd_socket_nl, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
		return false;
	return true;
}
/*===========================================================================*/
static bool open_control_sockets(void)
{
	if (open_socket_rt() == false)
		return false;
	if (open_socket_nl() == false)
		return false;
	if (open_socket_unix() == false)
		return false;
	return true;
}
/*===========================================================================*/

/* TIMER */
static bool set_timer(void)
{
	static struct itimerspec tval1;

	if ((fd_timer1 = timerfd_create(CLOCK_BOOTTIME, 0)) < 0)
		return false;
	tval1.it_value.tv_sec = TIMER1_VALUE_SEC;
	tval1.it_value.tv_nsec = TIMER1_VALUE_NSEC;
	tval1.it_interval.tv_sec = TIMER1_INTERVAL_SEC;
	tval1.it_interval.tv_nsec = TIMER1_INTERVAL_NSEC;
	if (timerfd_settime(fd_timer1, 0, &tval1, NULL) == -1)
		return false;
	return true;
}

/*===========================================================================*/

/* SIGNALHANDLER */
static void signal_handler(int signum)
{
	if ((signum == SIGINT) || (signum == SIGTERM) || (signum == SIGKILL) || (signum == SIGTSTP))
		wanteventflag |= EXIT_ON_SIGTERM;
	return;
}
/*---------------------------------------------------------------------------*/
static bool set_signal_handler(void)
{
	struct sigaction sa;

	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		return false;
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		return false;
	if (sigaction(SIGTSTP, &sa, NULL) < 0)
		return false;
	return true;
}
/*===========================================================================*/
static void init_values(void)
{
	static size_t i;
	static struct timespec waitfordevice;

	waitfordevice.tv_sec = 1;
	waitfordevice.tv_nsec = 0;
	clock_gettime(CLOCK_REALTIME, &tspecakt);
	tsfirst = ((u64)tspecakt.tv_sec * 1000000000ULL) + tspecakt.tv_nsec;
	nanosleep(&waitfordevice, NULL);
	clock_gettime(CLOCK_REALTIME, &tspecakt);
	tsakt = ((u64)tspecakt.tv_sec * 1000000000ULL) + tspecakt.tv_nsec;
	tshold = ((u64)tspecakt.tv_sec * 1000000000ULL) + tspecakt.tv_nsec;
	strftime(timestring1, TIMESTRING_LEN, "%Y%m%d%H%M%S", localtime(&tspecakt.tv_sec));
	seed += (unsigned int)tspecakt.tv_nsec & 0xffffffff;
	srand(seed);
	ouiaprg = (vendoraprg[rand() % ((VENDORAPRG_SIZE / sizeof(int)))]) & 0xffffff;
	nicaprg = rand() & 0xffffff;
	macaprg[5] = nicaprg & 0xff;
	macaprg[4] = (nicaprg >> 8) & 0xff;
	macaprg[3] = (nicaprg >> 16) & 0xff;
	macaprg[2] = ouiaprg & 0xff;
	macaprg[1] = (ouiaprg >> 8) & 0xff;
	macaprg[0] = (ouiaprg >> 16) & 0xff;
	aprglist->tsakt = tsakt;
	aprglist->essidlen = strnlen(macaprgfirst, ESSID_MAX);
	memcpy(aprglist->essid, macaprgfirst, strnlen(macaprgfirst, ESSID_MAX));
	memcpy(aprglist->macaprg, &macaprg, ETH_ALEN);
	nicaprg++;
	ouiclientrg = (vendorclientrg[rand() % ((VENDORCLIENTRG_SIZE / sizeof(int)))]) & 0xffffff;
	nicclientrg = rand() & 0xffffff;
	macclientrg[7] = 0;
	macclientrg[6] = 0;
	macclientrg[5] = nicclientrg & 0xff;
	macclientrg[4] = (nicclientrg >> 8) & 0xff;
	macclientrg[3] = (nicclientrg >> 16) & 0xff;
	macclientrg[2] = ouiclientrg & 0xff;
	macclientrg[1] = (ouiclientrg >> 8) & 0xff;
	macclientrg[0] = (ouiclientrg >> 16) & 0xff;
	strncpy(weakcandidate, WEAKCANDIDATEDEF, PSK_MAX);
	replaycountrg = (rand() % 0xfff) + 0xf000;
	eapolm1data[0x17] = (replaycountrg >> 8) & 0xff;
	eapolm1data[+0x18] = replaycountrg & 0xff;
	for (i = 0; i < 32; i++)
	{
		anoncerg[i] = rand() % 0xff;
		eapolm1data[i + 0x19] = anoncerg[i];
		snoncerg[i] = rand() % 0xff;
	}
	packetptr = &epb[EPB_SIZE];
	memcpy(&wltxbuffer, &rthtxdata, RTHTX_SIZE);
	memcpy(&wltxnoackbuffer, &rthtxnoackdata, RTHTXNOACK_SIZE);
	memcpy(&epbown[EPB_SIZE], &rthtxdata, RTHTX_SIZE);
	return;
}
/*---------------------------------------------------------------------------*/
static void close_lists(void)
{
	static size_t i;

	if (maclist != NULL)
		free(maclist);
	if (clientlist != NULL)
		free(clientlist);
	if (aprglist != NULL)
		free(aprglist);
	if (aplist != NULL)
		free(aplist);
	if (scanlist != NULL)
		free(scanlist);
	if (ifpresentlist != NULL)
	{
		for (i = 0; i < INTERFACELIST_MAX; i++)
		{
			if ((ifpresentlist + i)->frequencylist != NULL)
				free((ifpresentlist + i)->frequencylist);
		}
		free(ifpresentlist);
	}
	return;
}
/*---------------------------------------------------------------------------*/
static void close_fds(void)
{
	if (fd_timer1 != 0)
		close(fd_timer1);
	if (fd_pcapng != 0)
		close(fd_pcapng);
	return;
}
/*---------------------------------------------------------------------------*/
static bool init_lists(void)
{
	ssize_t i;

	if ((scanlist = (frequencylist_t *)calloc(SCANLIST_MAX, FREQUENCYLIST_SIZE)) == NULL)
		return false;
	if ((aplist = (aplist_t *)calloc(APLIST_MAX, APLIST_SIZE)) == NULL)
		return false;
	if ((aprglist = (aprglist_t *)calloc(APRGLIST_MAX, APRGLIST_SIZE)) == NULL)
		return false;
	if ((clientlist = (clientlist_t *)calloc(CLIENTLIST_MAX, CLIENTLIST_SIZE)) == NULL)
		return false;
	if ((maclist = (maclist_t *)calloc(MACLIST_MAX, MACLIST_SIZE)) == NULL)
		return false;
	if ((ifpresentlist = (interface_t *)calloc(INTERFACELIST_MAX, INTERFACELIST_SIZE)) == NULL)
		return false;
	for (i = 0; i < INTERFACELIST_MAX; i++)
	{
		if (((ifpresentlist + i)->frequencylist = (frequencylist_t *)calloc(FREQUENCYLIST_MAX, FREQUENCYLIST_SIZE)) == NULL)
			return false;
	}
	return true;
}
/*===========================================================================*/
static size_t chop(char *buffer, size_t len)
{
	char *ptr = NULL;

	ptr = buffer + len - 1;
	while (len)
	{
		if (*ptr != '\n')
			break;
		*ptr-- = 0;
		len--;
	}
	while (len)
	{
		if (*ptr != '\r')
			break;
		*ptr-- = 0;
		len--;
	}
	return len;
}
/*---------------------------------------------------------------------------*/
static int fgetline(FILE *inputstream, size_t size, char *buffer)
{
	size_t len = 0;
	char *buffptr = NULL;

	if (feof(inputstream))
		return -1;
	buffptr = fgets(buffer, size, inputstream);
	if (buffptr == NULL)
		return -1;
	len = strlen(buffptr);
	len = chop(buffptr, len);
	return len;
}

/*===========================================================================*/

bool generate_filter(char *dev, char *addr)
{

	pcap_t *handle;
	char error_buffer[PCAP_ERRBUF_SIZE];
	struct bpf_program filter;
	static struct sock_filter *bpfptr;

	char filter_exp[1024] = "";

	static size_t targets_size;
	static char *ufld = NULL;
	static char *tokptr = NULL;

	const char *targets[50];

	ufld = strdup(addr);
	tokptr = strtok(ufld, ",");
	while (tokptr != NULL)
	{
		targets[targets_size] = tokptr;
		targets_size += 1;
		tokptr = strtok(NULL, ",");
	}

	for (int i = 0; i < targets_size; i++)
	{
		char line[250];
		if (i == 0)
		{
			snprintf(line, sizeof(line), "wlan addr1 %s or wlan addr2 %s or wlan addr3 %s ", targets[i], targets[i], targets[i]);
		}
		else
		{
			snprintf(line, sizeof(line), "or wlan addr1 %s or wlan addr2 %s or wlan addr3 %s ", targets[i], targets[i], targets[i]);
		}
		strncat(filter_exp, line, sizeof(line));
	}
	strncat(filter_exp, "or wlan addr3 ff:ff:ff:ff:ff:ff", sizeof(filter_exp));

	bpf_u_int32 subnet_mask, ip;

	if (pcap_lookupnet(dev, &ip, &subnet_mask, error_buffer) == -1)
	{
		ip = 0;
		subnet_mask = 0;
	}
	handle = pcap_open_live(dev, BUFSIZ, 1, 1000, error_buffer);
	if (handle == NULL)
	{
		static char *error[300];
		snprintf(error, 299, "Could not open %s - %s", dev, error_buffer);
		printError(error, 1);
		exit(EXIT_FAILURE);
	}
	if (pcap_compile(handle, &filter, filter_exp, 0, ip) == -1)
	{
		static char *error[300];
		snprintf(error, 299, "Bad filter - %s", pcap_geterr(handle));
		printError(error, 1);
		exit(EXIT_FAILURE);
	}

	free(ufld);

	struct bpf_insn *insn;
	insn = filter.bf_insns;
	static u16 c;

	bpf.len = filter.bf_len;
	if (bpf.len == 0)
		return false;

	bpf.filter = (struct sock_filter *)calloc(bpf.len, sizeof(struct sock_filter));

	c = 0;
	bpfptr = bpf.filter;
	while (c < bpf.len)
	{
		bpfptr->code = insn->code;
		bpfptr->jt = insn->jt;
		bpfptr->jf = insn->jf;
		bpfptr->k = insn->k;
		bpfptr++;
		insn++;
		c++;
	}

	return true;
}

/*---------------------------------------------------------------------------*/
static void read_essidlist(char *listname)
{
	static size_t i;
	static int len;
	static FILE *fh_essidlist;
	static char linein[ESSID_MAX];

	if ((fh_essidlist = fopen(listname, "r")) == NULL)
	{
		static char *error[300];
		snprintf(error, 299, "Failed to open beacon list %s", listname);
		printError(error, 0);
		return;
	}
	i = 0;
	while (i < (APRGLIST_MAX - 1))
	{
		if ((len = fgetline(fh_essidlist, ESSID_MAX, linein)) == -1)
			break;
		if ((len == 0) || (len > ESSID_MAX))
			continue;
		(aprglist + i)->tsakt = tsakt - i;
		(aprglist + i)->essidlen = len;
		memcpy((aprglist + i)->essid, linein, len);
		(aprglist + i)->macaprg[5] = nicaprg & 0xff;
		(aprglist + i)->macaprg[4] = (nicaprg >> 8) & 0xff;
		(aprglist + i)->macaprg[3] = (nicaprg >> 16) & 0xff;
		(aprglist + i)->macaprg[2] = ouiaprg & 0xff;
		(aprglist + i)->macaprg[1] = (ouiaprg >> 8) & 0xff;
		(aprglist + i)->macaprg[0] = (ouiaprg >> 16) & 0xff;
		nicaprg++;
		i++;
	}
	(aprglist + i)->essidlen = 0;
	fclose(fh_essidlist);
	beaconindex = 0;
	return;
}

cJSON *aplist_jsonify(aplist_t *ap)
{
	cJSON *ap_json = cJSON_CreateObject();

	cJSON_AddItemToObject(ap_json, "tsakt", cJSON_CreateNumber(ap->tsakt / 1000000000ULL));
	cJSON_AddItemToObject(ap_json, "tshold1", cJSON_CreateNumber(ap->tshold1 / 1000000000ULL));
	cJSON_AddItemToObject(ap_json, "tsauth", cJSON_CreateNumber(ap->tsauth / 1000000000ULL));
	cJSON_AddItemToObject(ap_json, "count", cJSON_CreateNumber(ap->count));

	char mac_ap[13] = "";
	for (int i = 0; i < 6; i++)
	{
		char hex[3];
		snprintf(hex, 3, "%x", ap->macap[i]);
		strncat(mac_ap, hex, 3);
	}
	cJSON *mac_ap_json = cJSON_CreateString(mac_ap);
	cJSON_AddItemToObject(ap_json, "macap", mac_ap_json);

	char mac_client[13] = "";
	for (int i = 0; i < 6; i++)
	{
		char hex[3];
		snprintf(hex, 3, "%x", ap->macclient[i]);
		strncat(mac_client, hex, 3);
	}
	cJSON *mac_client_json = cJSON_CreateString(mac_client);
	cJSON_AddItemToObject(ap_json, "macclient", mac_client_json);

	cJSON_AddItemToObject(ap_json, "status", cJSON_CreateNumber(ap->status));

	cJSON_AddItemToObject(ap_json, "AP_IN_RANGE", cJSON_CreateBool(ap->status & AP_IN_RANGE));
	cJSON_AddItemToObject(ap_json, "ESSID_COLLECTED", cJSON_CreateBool(ap->status & AP_ESSID));
	cJSON_AddItemToObject(ap_json, "BEACON_COLLECTED", cJSON_CreateBool(ap->status & AP_BEACON));
	cJSON_AddItemToObject(ap_json, "PROBE_RESP_COLLECTED", cJSON_CreateBool(ap->status & AP_PROBERESPONSE));
	cJSON_AddItemToObject(ap_json, "EAPOL_M1_COLLECTED", cJSON_CreateBool(ap->status & AP_EAPOL_M1));
	cJSON_AddItemToObject(ap_json, "EAPOL_M2_COLLECTED", cJSON_CreateBool(ap->status & AP_EAPOL_M2));
	cJSON_AddItemToObject(ap_json, "EAPOL_M3_COLLECTED", cJSON_CreateBool(ap->status & AP_EAPOL_M3));
	cJSON_AddItemToObject(ap_json, "PMKID_COLLECTED", cJSON_CreateBool(ap->status & AP_PMKID));

	return ap_json;
}

cJSON *clientlist_jsonify(clientlist_t *client)
{
	cJSON *clientlist_json = cJSON_CreateObject();
	cJSON_AddItemToObject(clientlist_json, "tsakt", cJSON_CreateNumber(client->tsakt / 1000000000ULL));
	cJSON_AddItemToObject(clientlist_json, "tsauth", cJSON_CreateNumber(client->tsauth / 1000000000ULL));
	cJSON_AddItemToObject(clientlist_json, "tsassoc", cJSON_CreateNumber(client->tsassoc / 1000000000ULL));
	cJSON_AddItemToObject(clientlist_json, "tsreassoc", cJSON_CreateNumber(client->tsreassoc / 1000000000ULL));
	cJSON_AddItemToObject(clientlist_json, "aid", cJSON_CreateNumber(client->aid));
	cJSON_AddItemToObject(clientlist_json, "count", cJSON_CreateNumber(client->count));

	char mac_ap[13] = "";
	for (int i = 0; i < 6; i++)
	{
		char hex[3];
		snprintf(hex, 3, "%x", client->macap[i]);
		strncat(mac_ap, hex, 3);
	}
	cJSON *mac_ap_json = cJSON_CreateString(mac_ap);
	cJSON_AddItemToObject(clientlist_json, "macap", mac_ap_json);

	char mac_client[13] = "";
	for (int i = 0; i < 6; i++)
	{
		char hex[3];
		snprintf(hex, 3, "%x", client->macclient[i]);
		strncat(mac_client, hex, 3);
	}
	cJSON *mac_client_json = cJSON_CreateString(mac_client);
	cJSON_AddItemToObject(clientlist_json, "macclient", mac_client_json);

	// Create and add MIC
	cJSON *client_mic = cJSON_CreateArray();
	for (int i = 0; i < 4; i++)
	{
		cJSON_AddItemToArray(client_mic, cJSON_CreateNumber(client->mic[i]));
	}
	cJSON_AddItemToObject(clientlist_json, "mic", client_mic);

	cJSON_AddItemToObject(clientlist_json, "status", cJSON_CreateNumber(client->status));
	cJSON_AddItemToObject(clientlist_json, "EAP_START_COLLECTED", cJSON_CreateBool(client->status & CLIENT_EAP_START));
	cJSON_AddItemToObject(clientlist_json, "ROGUE_M2_COLLECTED", cJSON_CreateBool(client->status & CLIENT_EAPOL_M2));

	return clientlist_json;
}

static inline void send_lists(void)
{
	cJSON *data = cJSON_CreateObject();
	cJSON *dumptool = cJSON_CreateObject();
	cJSON *all_aps = cJSON_CreateArray();
	cJSON *all_clients = cJSON_CreateArray();
	char *string = NULL;

	if (clearScreen)
	{
		printf("\e[1;1H\e[2J");
	}

	for (int i = 0; i < 10; i++)
	{
		if ((aplist + i)->tsakt == 0)
			break; // No more APs

		// cJSON *ap;
		cJSON *ap = aplist_jsonify(aplist + i);
		cJSON_AddItemToArray(all_aps, ap);
	}
	cJSON_AddItemToObject(dumptool, "aplist", all_aps);

	for (int i = 0; i < 10; i++)
	{
		if ((clientlist + i)->tsakt == 0)
			break; // No more Clients

		// cJSON *client;
		cJSON *client = clientlist_jsonify(clientlist + i);
		cJSON_AddItemToArray(all_clients, client);
	}
	cJSON_AddItemToObject(dumptool, "clientlist", all_clients);

	cJSON_AddItemToObject(data, "dumptool", dumptool);

	if (clearScreen)
	{
		string = cJSON_Print(data);
	}
	else
	{
		string = cJSON_PrintUnformatted(data);
	}
	cJSON_Delete(data);
	if (string)
	{
		printf("%s\n", string);
	}
	cJSON_free(string);
}

static void printError(char *error, bool fatal)
{
	cJSON *data = cJSON_CreateObject();
	cJSON *dumptool = cJSON_CreateObject();
	char *string = NULL;

	cJSON_AddStringToObject(dumptool, "message", error);
	cJSON_AddBoolToObject(dumptool, "fatal", fatal);

	cJSON_AddItemToObject(data, "ERROR", dumptool);

	if (clearScreen)
	{
		string = cJSON_Print(data);
	}
	else
	{
		string = cJSON_PrintUnformatted(data);
	}
	cJSON_Delete(data);
	if (string)
	{
		printf("%s\n", string);
	}
	cJSON_free(string);
}

pcap_buffer_t *hcx(const char *iname, const char *target_mac, const char *channel_list, bool clear)
{
	// Setup options
	static u8 exiteapolflag = 0;				// Did we exit because of eapol needs being met? (damn i hope so)
	static bool interfacefrequencyflag = false; // Use interface freqs for scan... This will override a specific channel... reccomend we keep this off.
	static struct timespec tspecifo, tspeciforem;
	static char *essidlistname = NULL;		   // ESSID list approved for targeting unassociated clients (We could use this, if we can get a list of probes from a target from kismet?)
	static char *userchannellistname = NULL;   // List of user channels to scan (Likely our priority use-case because we should have the channel from Kismet)
	static char *userfrequencylistname = NULL; // List of user freqs to scan (Likely not used)

	clearScreen = clear;

	// Exit if these are met. For now, let's require M1/M2/M3 to exit (our best bet for cracking).
	exiteapolpmkidflag = false;
	exiteapolm2flag = false;
	exiteapolm3flag = false;

	// set interface name and index based on arg.
	ifaktindex = if_nametoindex(iname);
	strncpy(ifaktname, iname, IF_NAMESIZE - 1);

	// Grab channel from arg
	userchannellistname = channel_list;

	///// OLD COMMANDLINE OPTIONS WE CAN CHANGE IF WE WANT /////

	// Only for "DISABLE BEACON"
	// timerwaitnd = -1;

	// Only for disabling deauth (WHY WOULD WE WANT THIS?)
	// deauthenticationflag = false;

	// For disabling probereqs
	// proberequestflag = false;

	// For disabling association and reassociation
	// associationflag = false;
	// reassociationflag = false;

	// For max transmit of beacons... must be between 1-500.
	// Keep this default for now - I can see tuning in our future.
	// beacontxmax = 500

	// For max proberesponses must be between 1-500.
	// Keep this default for now - I can see tuning in our future.
	// proberesponsetxmax = 500

	// attemptclientmax = 0
	//  default value
	// attemptapmax = 32
	// if (attemptapmax > 0) // if not 0
	//	attemptapmax *= 8;
	// else // otherwise disable all the death/probereq/assoc/reassoc (Don't interact with AP's)
	//{
	//	deauthenticationflag = false;
	//	proberequestflag = false;
	//	associationflag = false;
	//	reassociationflag = false;
	// }

	// Handles hold time on scan.
	// Default is 1000000000ULL
	// timehold = 2;
	// timehold *= 1000000000ULL;

	// Timeout Timer
	// tottime = 10*60;

	// Watchdog Count (Default 600)
	// Also it looks like this makes sure we recieve packets, if it goes this many seconds and no new packets arrive it throws the watchdog.
	// watchdogcountmax = 600;

	// Error Max (Default 100)
	// errorcountmax = 100;

	// Display Sorting... (Default 0)
	// 0 : By time (last seen on top)
	// 1 : By Status (last EAPOL/PMKID on top)
	// rdsort = 0;

	// Monitor Mode Active Flag Status (Default True)
	// True - Allow hardware to respond to ACK destined for our NIC.
	// False - Hardware will ignore ACK destined for our NIC, allow software to handle it (which we will not).
	// activemonitorflag = false;

	// Let's get this shit going
	setbuf(stdout, NULL);
	hcxpid = getpid();

	if (set_signal_handler() == false)
	{
		errorcount++;
		printError("failed to initialize signal handler", 1);
		goto byebye;
	}
	if (init_lists() == false)
	{
		errorcount++;
		printError("failed to initialize lists", 1);
		goto byebye;
	}
	init_values();

	if (open_control_sockets() == false)
	{
		errorcount++;
		printError("failed to open control sockets", 1);
		goto byebye;
	}
	if (get_interfacelist() == false)
	{
		errorcount++;
		printError("failed to get interface list", 1);
		goto byebye;
	}

	/*---------------------------------------------------------------------------*/
	if (getuid() != 0)
	{
		errorcount++;
		printError("must be run as root", 1);
		goto byebye;
	}

	// ARM INTERFACE / SET CHANNEL AND SO ON
	// interfacefrequencyflag 1: Use Interface freqs in scanlist 0: Do not use interface frequency in scanlist
	// userfrequencylistname STR: Comma delim. list of freqs (2412,2417,5180,...)
	// userchannellistname STR: (1a,2a,36b...) default: 1a,6a,11a | important notice: channel numbers are not unique -- it is mandatory to add band information to the channel number (e.g. 12a)
	if (set_interface(interfacefrequencyflag, userfrequencylistname, userchannellistname) == false)
	{
		errorcount++;
		printError("failed to arm interface", 1);
		goto byebye;
	}
	// This is the most actual work I've had to do on this project so far
	if (generate_filter(iname, target_mac) == false)
	{
		errorcount++;
		printError("failed to generate BPF", 1);
		exit(EXIT_FAILURE);
	}
	if (essidlistname != NULL)
		read_essidlist(essidlistname);
	if (setup_pcap_buffer() == false)
	{
		errorcount++;
		printError("failed to open pcapng buffer", 1);
		goto byebye;
	}
	if (open_socket_rx() == false)
	{
		errorcount++;
		printError("failed to open raw packet socket", 1);
		goto byebye;
	}
	if (open_socket_tx() == false)
	{
		errorcount++;
		printError("failed to open transmit socket", 1);
		goto byebye;
	}
	if (set_timer() == false)
	{
		errorcount++;
		printError("failed to initialize timer", 1);
		goto byebye;
	}
	/*---------------------------------------------------------------------------*/
	tspecifo.tv_sec = 5;
	tspecifo.tv_nsec = 0;
	if (bpf.len == 0)
	{
		printError("BPF error - length 0", 1);
		exit(EXIT_FAILURE);
	}
	nanosleep(&tspecifo, &tspeciforem);

	if (nl_scanloop() == false)
	{
		errorcount++;
		printError("failed to initialize main scan loop", 1);
	}

/*---------------------------------------------------------------------------*/
byebye:
	close_fds();
	close_sockets();
	close_lists();

	static pcap_buffer_t result;
	result.len = pcap_buffer_size;
	result.result = pcap_buffer;
	return &result;
	/*---------------------------------------------------------------------------*/
}