/****************************************************************************
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include "wifi_mgmr_ext.h"

#include "bflb_gpio.h"
#include "bflb_irq.h"
#include "bflb_uart.h"

#include "rfparam_adapter.h"
#include "async_event.h"
#include "board.h"
#include "shell.h"


#include "bluetooth.h"
#include "conn.h"
#include "conn_internal.h"
#if defined(BL702)
#include "ble_lib_api.h"
#elif defined(BL602)
#include "ble_lib_api.h"
#include "bl602_glb.h"
#include "rfparam_adapter.h"
#elif defined(BL616)
#include "btble_lib_api.h"
#include "bl616_glb.h"
#include "rfparam_adapter.h"
#elif defined(BL616CL)
#include "btble_lib_api.h"
#include "bl616cl_glb.h"
#include "rfparam_adapter.h"
#elif defined(BL618DG)
#include "btble_lib_api.h"
#include "bl618dg_glb.h"
#include "rfparam_adapter.h"
#endif

#include "ble_cli_cmds.h"
#include "hci_driver.h"
#include "hci_core.h"
#if defined(CONFIG_BT_SETTINGS)
#include "bflb_mtd.h"
#include "easyflash.h"
#endif

#define DBG_TAG "MAIN"
#include "log.h"
#include "fhost_api.h"
#include "wifi_mgmr.h"
#include "mm.h"

struct bflb_device_s *gpio;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct bflb_device_s *uart0;

extern void shell_init_with_task(struct bflb_device_s *shell);

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Functions
 ****************************************************************************/


void wifi_event_handler(async_input_event_t ev, void *priv)
{
    uint32_t code = ev->code;
    switch (code) {
        case CODE_WIFI_ON_INIT_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_INIT_DONE\r\n", __func__);
            wifi_mgmr_task_start();
        } break;
        case CODE_WIFI_ON_MGMR_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_MGMR_DONE\r\n", __func__);
        } break;
        case CODE_WIFI_ON_SCAN_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_SCAN_DONE\r\n", __func__);
            wifi_mgmr_sta_scanlist();
        } break;
        case CODE_WIFI_ON_CONNECTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_CONNECTED\r\n", __func__);
            void mm_sec_keydump();
            mm_sec_keydump();
        } break;
        #ifdef CODE_WIFI_ON_GOT_IP_ABORT
        case CODE_WIFI_ON_GOT_IP_ABORT: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_GOT_IP_ABORT\r\n", __func__);
        } break;
        #endif
        #ifdef CODE_WIFI_ON_GOT_IP_TIMEOUT
        case CODE_WIFI_ON_GOT_IP_TIMEOUT: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_GOT_IP_TIMEOUT\r\n", __func__);
        } break;
        #endif
        case CODE_WIFI_ON_GOT_IP: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_GOT_IP\r\n", __func__);
            LOG_I("[SYS] Memory left is %d Bytes\r\n", kfree_size(0));
        } break;
        case CODE_WIFI_ON_DISCONNECT: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_DISCONNECT\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STARTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STARTED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STOPPED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STOPPED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STA_ADD: {
            LOG_I("[APP] [EVT] [AP] [ADD] %lld\r\n", xTaskGetTickCount());
        } break;
        case CODE_WIFI_ON_AP_STA_DEL: {
            LOG_I("[APP] [EVT] [AP] [DEL] %lld\r\n", xTaskGetTickCount());
        } break;
        default: {
            LOG_I("[APP] [EVT] Unknown code %u \r\n", code);
        }
    }
}

void wifi_start_firmware_task(void *param)
{
    LOG_I("Starting wifi ...\r\n");

    async_register_event_filter(EV_WIFI, wifi_event_handler, NULL);

    wifi_task_create();

    LOG_I("Starting fhost ...\r\n");
    fhost_init();

    vTaskDelete(NULL);
}

static void ble_connected(struct bt_conn *conn, uint8_t err)
{
    if (err || conn->type != BT_CONN_TYPE_LE) {
        return;
    }

    printf("%s", __func__);
}

static void ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
    int ret;

    if (conn->type != BT_CONN_TYPE_LE) {
        return;
    }

    printf("%s", __func__);

    ret = set_adv_enable(true);
    if (ret) {
        printf("Restart adv fail.\r\n");
    }
}

static struct bt_conn_cb ble_conn_callbacks = {
    .connected = ble_connected,
    .disconnected = ble_disconnected,
};

void bt_enable_cb(int err)
{
    if (!err) {
        bt_addr_le_t bt_addr;

        bt_get_local_public_address(&bt_addr);
        printf("BD_ADDR:(MSB)%02x:%02x:%02x:%02x:%02x:%02x(LSB)\r\n",
               bt_addr.a.val[5], bt_addr.a.val[4], bt_addr.a.val[3], bt_addr.a.val[2], bt_addr.a.val[1], bt_addr.a.val[0]);

        bt_set_name("COEX-BT");
        bt_conn_cb_register(&ble_conn_callbacks);
        ble_cli_register();

#if defined(CONFIG_BT_BREDR)
        extern int bredr_cli_register(void);
        bredr_cli_register();
#endif
    }
}

static TaskHandle_t bluetooth_start_handle;

static void bluetooth_start_task(void *pvParameters)
{
#if defined(BL618DG)
    bt_addr_le_t addr;
#endif

    // Initialize BLE controller
    #if defined(BL702) || defined(BL602)
    ble_controller_init(configMAX_PRIORITIES - 1);
    #else
    btble_controller_init(configMAX_PRIORITIES - 1);
    #endif
    // Initialize BLE Host stack
    hci_driver_init();
    bt_enable(bt_enable_cb);

#if defined(BL618DG)
    if (bt_addr_le_create_static(&addr) == 0) {
        bt_id_create(&addr, NULL);
    }
#endif

    vTaskDelete(NULL);
}


int main(void)
{
    board_init();

    uart0 = bflb_device_get_by_name("uart0");
    gpio = bflb_device_get_by_name("gpio");
    shell_init_with_task(uart0);
       
    #if defined(CONFIG_BT_SETTINGS)
        bflb_mtd_init();
        /* ble stack need easyflash kv */
        easyflash_init();
    #endif

    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }

    LOG_I("PHY RF init success!\r\n");

    tcpip_init(NULL, NULL);

    xTaskCreate(wifi_start_firmware_task, "wifi init", 1024, NULL, 10, NULL);
    xTaskCreate(bluetooth_start_task, (char *)"bluetooth_start", 1024, NULL, 10, &bluetooth_start_handle);
    vTaskStartScheduler();

    while (1) {
    }
}

#if defined(BL618DG)
int cmd_setup_bt_path(int argc, char **argv)
{
    extern void cmd_set_btble_standalone(int argc, char **argv);

    cmd_set_btble_standalone(0, NULL);
    return 0;
}

int cmd_spdt_pin(int argc, char **argv)
{
    int pin;

    if (argc != 2) {
        printf("usage: spdt_pin <pin>\r\n");
        return -1;
    }

    pin = atoi(argv[1]);
    if (pin < 0 || pin > 31 || gpio == NULL) {
        printf("invalid SPDT pin %d\r\n", pin);
        return -1;
    }

    printf("set pin %d to function %sSPDT\r\n", pin, pin % 2 ? "~" : "");
    bflb_gpio_init(gpio, pin, (25 << GPIO_FUNC_SHIFT) | GPIO_ALTERNATE);
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(cmd_setup_bt_path, setup_bt_path, setup bt path);
SHELL_CMD_EXPORT_ALIAS(cmd_spdt_pin, spdt_pin, set pin to spdt function);
#endif
