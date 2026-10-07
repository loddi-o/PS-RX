#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// PS-RX: lwIP serve SOLO per il Wake-on-LAN. Il WiFi si connette a una rete salvata (client DHCP)
// e manda pacchetti magici UDP in broadcast. Niente server web, niente mDNS, niente TCP, niente OTA:
// meno codice attivo sulla radio condivisa con il Bluetooth e piu' memoria libera.
//
// lwIP gira NO_SYS dentro il ciclo principale (cyw43_arch_poll + rete_task), senza OS e senza lock.
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    1600  // arena di lwIP (bss): DHCP e qualche pacchetto da 102 byte
#define MEMP_NUM_ARP_QUEUE          2
#define MEMP_NUM_UDP_PCB            2     // DHCP + Wake-on-LAN
#define MEMP_NUM_PBUF               4
#define PBUF_POOL_SIZE              4     // ricezione dal driver cyw43 (frame interi)
#define MEMP_NUM_SYS_TIMEOUT        8     // DHCP (4 timer) + ARP + margine

#define LWIP_TCP                    0
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    0
#define LWIP_UDP                    1
#define LWIP_DHCP                   1     // client DHCP: indirizzo dal router di casa
#define LWIP_DNS                    0
#define LWIP_IGMP                   0
#define LWIP_MDNS_RESPONDER         0

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

#define LWIP_STATS                  0
#define MEM_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0
#define LWIP_STATS_DISPLAY          0
#define SYS_STATS                   0

#define LWIP_CHKSUM_ALGORITHM       3

#endif /* LWIPOPTS_H */
