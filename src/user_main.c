#include "osapi.h"
#include "user_interface.h"

static os_timer_t ptimer;

void ICACHE_FLASH_ATTR user_set_station_config(void) 
{         
    char ssid[32] = "grodansparadis";          
    char password[64] = "brattbergavagen17!";          
    struct station_config stationConf;          
    stationConf.bssid_set = 0;      
    //need not check MAC address of AP         
    os_memcpy(&stationConf.ssid, ssid, 32);          
    os_memcpy(&stationConf.password, password, 64);          

    if ( wifi_station_set_config(&stationConf) ) {
        os_printf("Station config OK!\n");
    }
    else {
        os_printf("Station config failed!\n");
    }
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABBBCDDD
 *                A : rf cal
 *                B : at parameters
 *                C : rf init data
 *                D : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }
    return rf_cal_sec;
}

void blinky(void *arg)
{
	static uint8_t state = 0;

	if (state) {
		GPIO_OUTPUT_SET(2, 1);
	} else {
		GPIO_OUTPUT_SET(2, 0);
	}
	state ^= 1;
}

void ICACHE_FLASH_ATTR user_init(void)
{
    gpio_init();

    uart_init(115200, 115200);
    os_printf("\n\n\n\n");
    os_printf("--------------------------------------\n");
    os_printf("Hello World!\n");
    os_printf("SDK version:%s\n", system_get_sdk_version());

    uint8_t mac[6];
    if ( wifi_get_macaddr(0,mac) ) {            
        os_printf("VSCP GUID: :FF:FF:FF:FF:FF:FF:FF:FE:%02X:%02X:%02X:%02X:%02X:%02X:00:00\n", 
                    mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);    
    }

    // Disable WiFi
    wifi_set_opmode(STATIONAP_MODE);
    user_set_station_config();

    // Connected here

    // Structure holding the TCP connection information.
    struct espconn *espconn;

    // TCP specific protocol structure.
    //LOCAL esp_tcp tcp_proto;


    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    os_timer_disarm(&ptimer);
    os_timer_setfn(&ptimer, (os_timer_func_t *)blinky, NULL);
    os_timer_arm(&ptimer, 1000, 1);
}
