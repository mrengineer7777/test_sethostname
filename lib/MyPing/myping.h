#pragma once

#include "stdbool.h"

//Possible replacement C:\Users\David\.platformio\packages\framework-espidf\examples\protocols\icmp_echo\main\
//https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/icmp_echo.html

extern bool PingIP_Start(uint32_t ip_addr, uint32_t ping_count, uint32_t ping_timeout, uint32_t ping_delay, esp_ping_found_fn cbf);