#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "timers.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// #include "usbd_core.h"
#include "bflb_mtimer.h"
#include <bflb_core.h>
#include "board.h"
#include <shell.h>

#include <assert.h>
#include <stdio.h>

//#include "usb.h"
#include "utils.h"

#include "bflb_mtimer.h"
#include "bflb_emac.h"

// #include "usbd_core.h"
// #include "usbd_cdc.h"
// #include "usbd_cdc_ecm.h"
#include "lwip/netif.h"
// #include "eth_phy.h"
// #include "ephy_general.h"

#include <wifi_pkt_hooks.h>
#include <net_pkt_filter.h>
#include <net_al.h>
#include <fhost.h>

#include <nxspi.h>
#include <nxspi_net.h>

#define NXSPI_NET_STA_RX_RDY (1 << 0)
#define NXSPI_NET_AP_RX_RDY (1 << 1)

#define NXSPI_NET_LOG(...) //printf(__VA_ARGS__)

spinet_t g_spinet;
char     s_buf[NXBD_DNLD_ITEMS*TX_PBUF_PAYLOAD_LEN+31] __attribute__((section("SHAREDRAM")));
nettrans_desc_t s_dnmsg_desc[NXBD_DNLD_ITEMS] __attribute__((section(".nocache_ram")));// up+dn

static void custom_free(struct pbuf *p)
{
    nettrans_desc_t *msg = (nettrans_desc_t *)p;

    //printf("++++++++++++++++++++custom_free:%p\r\n", msg);
    if (msg) {
        while (xQueueSend(g_spinet.dnfq, &msg, portMAX_DELAY) != pdPASS);
        NXSPI_NET_LOG("[%d] dnfq free:%p\r\n", __LINE__, msg);
    }
}

int portwifi_eth_tx(nettrans_desc_t *msg, bool is_sta)
{
    struct pbuf *p = (struct pbuf *)msg;
    net_al_if_t *net_if;

    if (!p) {
        printf("alloc error.\r\n");
        pbuf_free(p);
    }

    if (is_sta) {
        net_if = fhost_env.vif[0].net_if;
    } else {
        net_if = fhost_env.vif[1].net_if;
    }
    if (!net_if) {
    	return -1;
    }

    // Push the buffer and verify the status
    if (netif_is_up((struct netif *)net_if) && fhost_tx_start(net_if, p, NULL, NULL) == 0) {
        return 0;
    } else {
        // Failed to push message to TX task, call pbuf_free only to decrease ref count
        printf("tx error.\r\n");
        pbuf_free(p);
    }
    return -1;
}

static inline int spinet_rx_process(uint8_t is_sta)
{
    struct pbuf *p;
    int result;
    // get buf
    NXSPI_NET_LOG("dnfq count:%d\r\n", uxQueueMessagesWaiting(g_spinet.dnfq));
    xQueueReceive(g_spinet.dnfq, &g_spinet.dnmsg, portMAX_DELAY);
    NXSPI_NET_LOG("[%d] dnfq pop:%p\r\n", __LINE__, g_spinet.dnmsg);

    // init pbuf
    g_spinet.dnmsg->pbuf.custom_free_function = custom_free;
    p = pbuf_alloced_custom(PBUF_RAW_TX, TX_PBUF_FRAME_LEN,
            (PBUF_ALLOC_FLAG_DATA_CONTIGUOUS | PBUF_TYPE_ALLOC_SRC_MASK_STD_HEAP), 
            &g_spinet.dnmsg->pbuf, g_spinet.dnmsg->payload_buf, TX_PBUF_PAYLOAD_LEN);

    // recv buf
    g_spinet.dbg_dntask_mode = 2;

    // recv from spinet
    result = nxspi_read((is_sta) ? NXSPI_TYPE_NET_STA : NXSPI_TYPE_NET_AP, (uint8_t *)g_spinet.dnmsg->pbuf.pbuf.payload, (TX_PBUF_FRAME_LEN), 0);
    NXSPI_NET_LOG("net result:%d\r\n", result);
    if (0 > result) {
        while (xQueueSend(g_spinet.dnfq, &g_spinet.dnmsg, portMAX_DELAY) != pdPASS);
        NXSPI_NET_LOG("[%d] dnfq push:%p\r\n", __LINE__, g_spinet.dnmsg);
        return -1;
    }

    g_spinet.dbg_dntask_mode = 3;

    g_spinet.read_cnt++;
    g_spinet.read_bytes += result;

    p->len = (u16_t)result;
    p->tot_len = (u16_t)result;

    if (g_spinet.netstream == SPINET_NETSTREAM_TO_LOCAL) {
        pbuf_free(p);
        return result;
    }
    // update pbuf
    portwifi_eth_tx(g_spinet.dnmsg, is_sta);
    g_spinet.dnmsg = NULL;
    return result;
}

static void nxspi_net_sta_notify(void)
{
    xTaskNotify(g_spinet.dntask_hdl, NXSPI_NET_STA_RX_RDY, eSetBits);
}

static void nxspi_net_ap_notify(void)
{
    xTaskNotify(g_spinet.dntask_hdl, NXSPI_NET_AP_RX_RDY, eSetBits);
}

int spinet_dn_task(void *arg)
{
    int ret;
    uint32_t event = 0;

    while (1) {

        NXSPI_NET_LOG("xTaskNotifyWait...\r\n");
        xTaskNotifyWait(0, NXSPI_NET_AP_RX_RDY | NXSPI_NET_STA_RX_RDY, &event, portMAX_DELAY);
        NXSPI_NET_LOG("Wait event:%d\n", event);

        g_spinet.dbg_dntask_mode = 1;

        do {

            if (event & NXSPI_NET_STA_RX_RDY) {
                ret = spinet_rx_process(1);
                if (ret <= 0) {
                    event &= ~NXSPI_NET_STA_RX_RDY;
                }
            }

            if (event & NXSPI_NET_AP_RX_RDY) {
                ret = spinet_rx_process(0);
                if (ret <= 0) {
                    event &= ~NXSPI_NET_AP_RX_RDY;
                }
            }
        } while(event);
    }
}

static int spinet_queue_init(void)
{
    int i;

    // reset global
    memset(&g_spinet, 0, sizeof(spinet_t));

    // init queue sem
    g_spinet.dnfq = xQueueCreate(NXBD_DNLD_ITEMS + 1, sizeof(nettrans_desc_t *));
    if (!g_spinet.dnfq) {
        printf("failed to create queue\r\n");
        return -1;
    }

    // init sem
    for (i = 0; i < NXBD_DNLD_ITEMS; i++) {
        nettrans_desc_t *msg;
#if 0
        struct pbuf_custom *p;

        p = &(dnmsg_desc[i].pbuf);
        p->custom_free_function = custom_free;
        p = pbuf_alloced_custom(PBUF_RAW_TX, TX_PBUF_FRAME_LEN, 
                (PBUF_ALLOC_FLAG_DATA_CONTIGUOUS | PBUF_TYPE_ALLOC_SRC_MASK_STD_HEAP), 
                &p->pbuf, p->payload_buf, TX_PBUF_PAYLOAD_LEN);
#endif
        s_dnmsg_desc[i].payload_buf = s_buf + TX_PBUF_PAYLOAD_LEN*i;
        msg = &(s_dnmsg_desc[i]);
        xQueueSend(g_spinet.dnfq, &msg, portMAX_DELAY);
    }
    for (i = 0; i < NXBD_UPLD_ITEMS; i++) {
    }

    nxspi_rxd_callback_register(nxspi_net_sta_notify, NXSPI_TYPE_NET_STA);
    nxspi_rxd_callback_register(nxspi_net_ap_notify, NXSPI_TYPE_NET_AP);

    return 0;
}

static int dual_stack_peer_input(struct pbuf *p, bool is_sta)
{
    nxspi_write((is_sta) ? NXSPI_TYPE_NET_STA : NXSPI_TYPE_NET_AP, (uint8_t *)(uintptr_t)p->payload, p->len, -1);
    g_spinet.write_cnt++;
    g_spinet.write_bytes += g_spinet.upmsg->len;

    pbuf_free(p);

    return 0;
}

int dual_stack_input(struct pbuf *p, bool is_sta)
{
    nxspi_write((is_sta) ? NXSPI_TYPE_NET_STA : NXSPI_TYPE_NET_AP, (uint8_t *)(uintptr_t)p->payload, p->len, -1);
    g_spinet.write_cnt++;
    g_spinet.write_bytes += g_spinet.upmsg->len;

    return 0;
}

static void *eth_input_hook(bool is_sta, void *pkt, void *arg)
{
    struct pbuf *p = (struct pbuf *)pkt;

    if (npf_is_8021X(p)) {
        // The packet is an 802.1X protocol packet.
        return p;
    }

    if (g_spinet.netstream == SPINET_NETSTREAM_TO_LOCAL) {
        g_spinet.local_pktcnt++;
        //printf("local handle:%d\r\n", g_spinet.local_pktcnt);
        return p;
    } else if (g_spinet.netstream == SPINET_NETSTREAM_TO_HOST) {
        g_spinet.host_pktcnt++;
        //NXSPI_NET_LOG("host  handle:%d\r\n", g_spinet.host_pktcnt);
        // XXX distinguish STA/AP
        NXSPI_NET_LOG("dual_stack_peer_input:%d\r\n", is_sta);
        int ret = dual_stack_peer_input(p, is_sta);
        if (ret) {
            pbuf_free(p);
            return NULL;
        }
    } else {
        g_spinet.oth_pktcnt++;
        printf("oth    handle:%d\r\n", g_spinet.oth_pktcnt);
        pbuf_free(p);
    }

    return NULL;
}

static int spinet_wifi_init(void)
{
    bflb_pkt_eth_input_hook_register(eth_input_hook, NULL);//&ctx->rx_env);
    return 0;
}

void spinet_init(void)
{
    // init 0
    memset(&g_spinet, 0, sizeof(spinet_t));

    spinet_queue_init();
    // spinet_usb_init();
    spinet_wifi_init();

    xTaskCreate(spinet_dn_task, (char *)"nxspidn", 512, &g_spinet, 25, &g_spinet.dntask_hdl);

    return;
}
