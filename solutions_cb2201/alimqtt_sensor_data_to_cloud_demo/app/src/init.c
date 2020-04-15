#include <stdbool.h>
#include "pin_name.h"
#include "pin.h"
#include "app_main.h"
#include <devices/devicelist.h>
#include <devices/esp8266.h>
#include <yoc/partition.h>
#include <yoc/init.h>

#ifndef CONSOLE_IDX
#define CONSOLE_IDX 0
#endif

#define TAG "init"

extern void ioreuse_initial(void);
extern int32_t drv_pin_config_mode(port_name_e port, uint8_t offset, int pin_mode);
static void board_pinmux_config(void)
{
    //console
    drv_pinmux_config(CONSOLE_TXD, CONSOLE_TXD_FUNC);
    drv_pinmux_config(CONSOLE_RXD, CONSOLE_RXD_FUNC);

    //uart1
    drv_pinmux_config(AT_PIN_USART_TX, AT_TXD_FUNC);
    drv_pinmux_config(AT_PIN_USART_RX, AT_RXD_FUNC);
    drv_pin_config_mode(PORTA, 15, 1);
    drv_pin_config_mode(PORTA, 16, 1);

    //uart2
    drv_pinmux_config(NETM_PIN_USART_TX, NETM_PIN_USART_TX_FUNC);
    drv_pinmux_config(NETM_PIN_USART_RX, NETM_PIN_USART_RX_FUNC);

    //enc28j60
    drv_pinmux_config(ENC28J60_ETH_SPI_MISO, ENC28J60_ETH_SPI_MISO_FUNC);
    drv_pinmux_config(ENC28J60_ETH_SPI_MOSI, ENC28J60_ETH_SPI_MOSI_FUNC);
    drv_pinmux_config(ENC28J60_ETH_SPI_CS, ENC28J60_ETH_SPI_CS_FUNC);
    drv_pinmux_config(ENC28J60_ETH_SPI_SCK, ENC28J60_ETH_SPI_SCK_FUNC);
    drv_pinmux_config(ENC28J60_ETH_PIN_RST, ENC28J60_ETH_PIN_RST_FUNC);
    drv_pinmux_config(ENC28J60_ETH_PIN_INT, ENC28J60_ETH_PIN_INT_FUNC);

    //iic
    drv_pinmux_config(EXAMPLE_PIN_IIC_SDA, EXAMPLE_PIN_IIC_SDA_FUNC);
    drv_pinmux_config(EXAMPLE_PIN_IIC_SCL, EXAMPLE_PIN_IIC_SCL_FUNC);

    //adc
    drv_pinmux_config(EXAMPLE_ADC_CH12, EXAMPLE_ADC_CH12_FUNC);

    //pwm
    drv_pinmux_config(EXAMPLE_PWM_CH, EXAMPLE_PWM_CH_FUNC);

    //dht11
    drv_pinmux_config(EXAMPLE_DHT11_PIN, EXAMPLE_DHT11_PIN_FUNC);

    //led
    drv_pinmux_config(EXAMPLE_LED1_PIN1, EXAMPLE_LED1_PIN1_FUNC);
    drv_pinmux_config(EXAMPLE_LED1_PIN2, EXAMPLE_LED1_PIN2_FUNC);
    drv_pinmux_config(EXAMPLE_LED2_PIN1, EXAMPLE_LED2_PIN1_FUNC);
    drv_pinmux_config(EXAMPLE_LED2_PIN2, EXAMPLE_LED2_PIN2_FUNC);

    //push button
    drv_pinmux_config(EXAMPLE_PUSH_BUTTON_PIN, EXAMPLE_PUSH_BUTTON_PIN_FUNC);
}

netmgr_hdl_t app_netmgr_hdl;

static void network_init()
{
    // network init

    /* kv config check */
    
    aos_kv_setint("wifi_en", 1);
    aos_kv_setint("gprs_en", 0);
    aos_kv_setint("eth_en", 0);

    esp_wifi_param_t esp_param;

    esp_param.device_name    = "uart2";
    esp_param.baud           = 115200;
    esp_param.buf_size       = 4096;
    esp_param.enable_flowctl = 0;
    esp_param.reset_pin      = WIFI_ESP8266_RESET;
    esp_param.smartcfg_pin   = WIFI_ESP8266_SMARTCFG;

    wifi_esp8266_register(NULL, &esp_param);
    app_netmgr_hdl = netmgr_dev_wifi_init();

    if (app_netmgr_hdl) {
        utask_t *task = utask_new("netmgr", 1 * 1024, QUEUE_MSG_COUNT, AOS_DEFAULT_APP_PRI);
        netmgr_service_init(task);
        netmgr_start(app_netmgr_hdl);
    }
}
void board_init(void)
{
    ioreuse_initial();

    board_pinmux_config();

}

void board_yoc_init(void)
{
    event_service_init(NULL);
    uart_csky_register(CONSOLE_IDX);
    uart_csky_register(2);
    flash_csky_register(0);

    console_init(CONSOLE_IDX, 115200, 512);

    int ret = partition_init();
    if (ret <= 0) {
        LOGE(TAG, "partition init failed");
    } else {
        LOGI(TAG, "find %d partitions", ret);
    }

    aos_kv_init("kv");
    network_init();

    utask_t *task = utask_new("at&cli", 2 * 1024, QUEUE_MSG_COUNT, AOS_DEFAULT_APP_PRI);
    board_cli_init(task);
}