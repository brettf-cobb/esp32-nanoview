#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "nanoview_uart.h"
#include "nanoview_main.h"

static const int RX_BUF_SIZE = 128;

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_18)
#define LED (GPIO_NUM_2)

enum state {
    WAIT_START,
    GOT_START,
    IN_PACKET
};



void nv_init_uart() {
    const uart_config_t uart_config = {
        .baud_rate = 2400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config);
    ESP_LOGW("RX_TASK", "uart_set_pin");
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    ESP_LOGW("RX_TASK", "uart_driver_install");
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

void nv_rx_task(struct task_config *tc)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    //uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    struct nv_packet p;
    uint8_t start, to_read = 0;
    uint8_t offset = 0;
    int rx_bytes = 0;
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    while (true) {
        do {
            start = 0;
            uart_read_bytes(UART_NUM_1, (uint8_t *)&start, 1, 1000 / portTICK_PERIOD_MS);
        } while (start != 0xAA);
        gpio_set_level(LED, 1);
        // Got start
        read_type:
            if (uart_read_bytes(UART_NUM_1, &(p.type), 1, 1000 / portTICK_PERIOD_MS) < 1)
                goto read_type;

        switch (p.type) {
            case 0x10:
                to_read = 36;
                break;
            case 0x11:
                to_read = 66;
                break;
            case 0x12:
                to_read = 3;
                break;
            default:
                ESP_LOGW(RX_TASK_TAG, "Unknown packet typde: 0x%x", p.type);
                continue; // Uknown packet type
        }

        while (to_read > 0) {
            rx_bytes = uart_read_bytes(UART_NUM_1, &(p.data[offset]), to_read, 1000 / portTICK_PERIOD_MS);
            to_read -= rx_bytes;
            offset += rx_bytes;
        }
        // Should have whole packet now
        ESP_LOGD(RX_TASK_TAG, "Packet Type %02x: Read %d bytes", p.type, offset);
        ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, p.data, offset, ESP_LOG_DEBUG);
        uint16_t packet_crc = *((uint16_t *) (&(p.data[offset-2])));
        ESP_LOGD(RX_TASK_TAG, "CRC: %04x == %04x [%s]", packet_crc, nv_crc(p.data, offset-2), ((packet_crc == nv_crc(p.data, offset-2)) ? "OK" : "BAD"));
        if (packet_crc == nv_crc(p.data, offset-2)) {
            if (xQueueSend(tc->xNvMessageQueue, (void *)&p, (TickType_t)(250 / portTICK_PERIOD_MS)) != pdPASS) {
                ESP_LOGE(RX_TASK_TAG, "Error placing NV packet on queue");
            }
        }
        offset = 0;
        gpio_set_level(LED, 0);
    }
    //free(data);
}

uint16_t nv_crc(uint8_t data[], uint8_t len) {
    uint8_t crc1 = 0, crc2 = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc1 += data[i];
        crc2 ^= crc1;
    }
    return ((crc2 << 8) | crc1);
}