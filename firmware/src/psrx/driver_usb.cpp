//
// PS-RX - driver USB dell'applicazione per TinyUSB: XInput (modalita' Xbox) e posti dello Steam
// Controller (modalita' Steam). TinyUSB li prova prima dei suoi (HID, audio, vendor); ognuno prende solo
// le interfacce della sua modalita', quindi in modalita' PlayStation tutto resta come in DS5-Linux-Bridge.
//

#include "device/usbd_pvt.h"
#include "tusb.h"

extern const usbd_class_driver_t psrx_driver_xinput;   // xbox.cpp
extern const usbd_class_driver_t psrx_driver_steam;    // steam_usb.cpp

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *quanti) {
    static usbd_class_driver_t driver[2];
    driver[0] = psrx_driver_xinput;
    driver[1] = psrx_driver_steam;
    *quanti = 2;
    return driver;
}
