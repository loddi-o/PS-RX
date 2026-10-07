//
// PS-RX - servizio USB per le app (protocollo in protocollo.h e docs/PROTOCOLLO_USB.md).
//
// Le richieste arrivano da tud_vendor_control_xfer_cb() (usb_descriptors.cpp) dentro tud_task(),
// sul core 0. Si servono con sole copie di memoria: niente flash, niente attese, niente printf
// per le letture. Le azioni lente (abbinamento, dimentica, spegnimento dei controller, RSSI,
// misura dell'heap) vanno in coda e le esegue servizio_usb_task().
//

#ifndef PSRX_SERVIZIO_USB_H
#define PSRX_SERVIZIO_USB_H

#include <cstdint>

#include "tusb.h"

bool psrx_servizio_usb(uint8_t rhport, uint8_t fase, tusb_control_request_t const *richiesta);
void servizio_usb_task();

// Eventi per le notifiche (letti dall'app con CMD_EVENTI); aggiornati da servizio_usb_task().
uint16_t servizio_usb_ultimo_evento();

#endif // PSRX_SERVIZIO_USB_H
