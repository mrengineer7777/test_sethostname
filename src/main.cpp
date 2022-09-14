#include <stdio.h>
#include "lwip/netdb.h"
#include "ping/ping_sock.h"
#include "IPAddress.h"

extern "C" {
  #include "esp32-hal.h"
  #include "lwip\sockets.h"
}

#include <Arduino.h>

//-----Prototypes-----
ip_addr_t _url_to_ipaddr(const char *ip_cstr);
//-----Prototypes-----

//Convert passed URL or IP cstr into ip address
//Uses new ip_addr_t format that combines IP4 and IP6 addresses
//Based on https://github.com/espressif/esp-idf/blob/master/examples/protocols/icmp_echo/main/echo_example_main.c
ip_addr_t _url_to_ipaddr(const char *ip_cstr) {
    struct sockaddr_in6 sock_addr6;
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));

    if (inet_pton(AF_INET6, ip_cstr, &sock_addr6.sin6_addr) == 1) {
        /* convert ip6 string to ip6 address */
        ipaddr_aton(ip_cstr, &target_addr);
    } else {
        struct addrinfo hint;
        struct addrinfo *res = NULL;
        memset(&hint, 0, sizeof(hint));
        /* convert ip4 string or hostname to ip4 or ip6 address */
        if (getaddrinfo(ip_cstr, NULL, &hint, &res) != 0) {
            log_v("Unknown host %s\n", ip_cstr);
            return target_addr;
        }
        if (res->ai_family == AF_INET) {
            struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
            inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
        } else {
            struct in6_addr addr6 = ((struct sockaddr_in6 *) (res->ai_addr))->sin6_addr;
            inet6_addr_to_ip6addr(ip_2_ip6(&target_addr), &addr6);
        }
        freeaddrinfo(res);
    }
    return target_addr;
}

void setup() {
    char buf[64];
    
    Serial.begin(115200);
    delay(2000);
    
    sprintf(buf, "%s", "www.google.com");
    ip_addr_t ip = _url_to_ipaddr(buf);

    log_v("%s = %s", buf, IPAddress(ip.u_addr.ip4.addr).toString().c_str());
}

void loop() {
    delay(1000);                        // Allow other tasks to run
}