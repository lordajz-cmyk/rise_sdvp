// Main file for RTCM forwarder
#include "ch.h"
#include "hal.h"
#include "lwipthread.h"
#include "rtcm_forward.h"
#include "chprintf.h"
#include "usbconf.h"

static BaseSequentialStream *debug_stream = NULL;

int main(void) {
    // System initializations
    halInit();
    chSysInit();

    // Setup UART for debug output (via USB CDC)
    sduStart(&SDU1, &serusbcfg); // USB-CDC Serial
    debug_stream = (BaseSequentialStream*)&SDU1;

    // Setup UART for RTCM forwarding (to F9P)
    static const SerialConfig uart_cfg = {
        115200, // Baud rate
        0,
        0,
        0,
    };
    sdStart(&SD2, &uart_cfg);  // UART2 connected to u-blox

    // Start USB
    usbDisconnectBus(serusbcfg.usbp); 
    chThdSleepMilliseconds(1500); // USB Reset
    usbStart(serusbcfg.usbp, &serusbcfg);
    usbConnectBus(serusbcfg.usbp);

    // Start network thread (LwIP)
    chThdCreateStatic(wa_lwip_thread, sizeof(wa_lwip_thread),
                      NORMALPRIO + 2, lwip_thread, NULL);

    chprintf(debug_stream, "\nRTCM Forwarder starting...\n");

    // Start RTCM forwarder thread (TCP client -> UART2)
    rtcm_forward_start(debug_stream, (BaseAsynchronousChannel*)&SD2);

    // Main thread does nothing else
    while (true) {
        chThdSleepMilliseconds(1000);
    }
}
