#include <math.h>
#include <float.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "lwip/err.h"
#include "ping/ping.h"
#include "esp_ping.h"
#include "myping.h"

bool PingIP_Start(uint32_t ip_addr, uint32_t ping_count, uint32_t ping_timeout, uint32_t ping_delay, esp_ping_found_fn cbf) {
    int32_t result = 0;

    esp_ping_set_target(PING_TARGET_IP_ADDRESS_COUNT, &ping_count, sizeof(ping_count));
    esp_ping_set_target(PING_TARGET_RCV_TIMEO, &ping_timeout, sizeof(ping_timeout));
    esp_ping_set_target(PING_TARGET_DELAY_TIME, &ping_delay, sizeof(ping_delay));
    esp_ping_set_target(PING_TARGET_IP_ADDRESS, &ip_addr, sizeof(ip_addr) );
    esp_ping_set_target(PING_TARGET_RES_FN, cbf, sizeof(cbf));
    result = ping_init();
    switch(result)
    {
        case ERR_INPROGRESS:
            printf("PingIP_Start() ping in progress\r\n");
            ping_deinit();                                                          //Ping should be done by now.  Stop previous ping and return failure.
            return false;
            break;
        case ERR_MEM:
            return false;                                                           //Out of memory error. Return failure.
            break;
    }

    return true;
}