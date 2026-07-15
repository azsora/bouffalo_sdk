#include "bflb_dma.h"
#include "bflb_l1c.h"
#include "bflb_mtimer.h"
#include "bflb_pec_v2_instance.h"
#include "board.h"
#include "image_sensor.h"
#include "sensor/common.h"

#define CAM_FRAME_BUFFER_BYTES  (1280 * 720 * 2)
#define CAM_DMA_LLI_COUNT       ((CAM_FRAME_BUFFER_BYTES / 4064) + 2)

static struct bflb_device_s *i2cx;
static struct bflb_device_s *pec_cam;
static struct bflb_device_s *dma0_ch0;
static struct bflb_pec_dvp_cam_s *pec_cam_cfg;
static uint32_t cam_frame_bytes;

static ATTR_NOINIT_PSRAM_SECTION __attribute((aligned(BFLB_CACHE_LINE_SIZE))) uint8_t cam_frame[CAM_FRAME_BUFFER_BYTES];
static struct bflb_dma_channel_lli_transfer_s transfers[1];
static struct bflb_dma_channel_lli_pool_s lli[CAM_DMA_LLI_COUNT];

static void dma0_ch0_isr(void *arg)
{
    static uint32_t dma_tc_flag0 = 0;
    printf("tc done, flag = %u\r\n", dma_tc_flag0++);
    if ((dma_tc_flag0 % 50) == 0) {
        bflb_pec_dvp_cam_stop(pec_cam);
        bflb_l1c_dcache_invalidate_range(cam_frame, sizeof(cam_frame));
        for (int i = 0; i < cam_frame_bytes; i++) {
            if (i % 16 == 0) {
                printf("\r\n");
            }
            printf("%02X ", cam_frame[i]);
        }
        bflb_pec_dvp_cam_start(pec_cam);
    }
}

int main(void)
{
    struct image_sensor_config_s *sensor_config;
    struct bflb_dma_channel_config_s dma_config;
    int used_count;

    board_init();
    board_pec_dvp_cam_gpio_init();

    i2cx = bflb_device_get_by_name("i2c0");
    pec_cam = bflb_device_get_by_name("pec_sm0");
    dma0_ch0 = bflb_device_get_by_name("dma0_ch0");

    if (image_sensor_scan(i2cx, &sensor_config)) {
        printf("sensor name: %s\r\n", sensor_config->name);
        printf("sensor resolution: %u x %u\r\n", sensor_config->resolution_x, sensor_config->resolution_y);
    } else {
        printf("error: can't identify sensor\r\n");
        while (1) {
            bflb_mtimer_delay_ms(1000);
        }
    }
    pec_cam_cfg = bflb_pec_dvp_cam_get_cfg(sensor_config->name);
    if (pec_cam_cfg == NULL) {
        printf("error: no matched pec dvp cam config for this sensor\r\n");
        while (1) {
            bflb_mtimer_delay_ms(1000);
        }
    }
    pec_cam_cfg->resolution_x = sensor_config->resolution_x;
    pec_cam_cfg->resolution_y = sensor_config->resolution_y;
    cam_frame_bytes = pec_cam_cfg->resolution_x * pec_cam_cfg->resolution_y * pec_cam_cfg->pixel_bits / 8;

    dma_config.direction = DMA_PERIPH_TO_MEMORY;
    dma_config.src_req = DMA_REQUEST_PEC_SM0_RX;
    dma_config.dst_req = DMA_REQUEST_NONE;
    dma_config.src_addr_inc = DMA_ADDR_INCREMENT_DISABLE;
    dma_config.dst_addr_inc = DMA_ADDR_INCREMENT_ENABLE;
    if (pec_cam_cfg->fifo_threshold == (1 - 1)) {
        dma_config.src_burst_count = DMA_BURST_INCR1;
        dma_config.dst_burst_count = DMA_BURST_INCR1;
    } else if (pec_cam_cfg->fifo_threshold == (4 - 1)) {
        dma_config.src_burst_count = DMA_BURST_INCR4;
        dma_config.dst_burst_count = DMA_BURST_INCR4;
    } else if (pec_cam_cfg->fifo_threshold == (8 - 1)) {
        dma_config.src_burst_count = DMA_BURST_INCR8;
        dma_config.dst_burst_count = DMA_BURST_INCR8;
    } else if (pec_cam_cfg->fifo_threshold == (16 - 1)) {
        dma_config.src_burst_count = DMA_BURST_INCR16;
        dma_config.dst_burst_count = DMA_BURST_INCR16;
    } else {
        printf("error: not support this fifo threshold: %d\r\n", pec_cam_cfg->fifo_threshold);
        while (1) {
            bflb_mtimer_delay_ms(1000);
        }
    }
    if (pec_cam_cfg->bits_every_push == 8) {
        dma_config.src_width = DMA_DATA_WIDTH_8BIT;
        dma_config.dst_width = DMA_DATA_WIDTH_8BIT;
    } else if (pec_cam_cfg->bits_every_push == 16) {
        dma_config.src_width = DMA_DATA_WIDTH_16BIT;
        dma_config.dst_width = DMA_DATA_WIDTH_16BIT;
    } else if (pec_cam_cfg->bits_every_push == 32) {
        dma_config.src_width = DMA_DATA_WIDTH_32BIT;
        dma_config.dst_width = DMA_DATA_WIDTH_32BIT;
    } else {
        printf("error: not support this bits_every_push: %d\r\n", pec_cam_cfg->bits_every_push);
        while (1) {
            bflb_mtimer_delay_ms(1000);
        }
    }
    bflb_dma_channel_init(dma0_ch0, &dma_config);
    bflb_dma_channel_irq_attach(dma0_ch0, dma0_ch0_isr, NULL);

    transfers[0].src_addr = (uint32_t)DMA_ADDR_PEC_SM0_RDR;
    transfers[0].dst_addr = (uint32_t)(uintptr_t)cam_frame;
    transfers[0].nbytes = cam_frame_bytes;
    used_count = bflb_dma_channel_lli_reload(dma0_ch0, lli, CAM_DMA_LLI_COUNT, transfers, 1);
    bflb_dma_channel_lli_link_head(dma0_ch0, lli, used_count);

    if (bflb_pec_dvp_cam_init(pec_cam, pec_cam_cfg) != 0) {
        printf("error: pec dvp cam init failed\r\n");
        while (1) {
            bflb_mtimer_delay_ms(1000);
        }
    }

    bflb_dma_channel_start(dma0_ch0);
    bflb_mtimer_delay_us(pec_cam_cfg->delay_first_us);
    bflb_pec_dvp_cam_start(pec_cam);

    while (1) {
        bflb_mtimer_delay_ms(1000);
    }
}
