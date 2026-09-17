// TCP to UART forwarder implementation
#include "ch.h"
#include "hal.h"
#include "rtcm_forward.h"
#include "chprintf.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define SERVER_IP   "192.168.7.1"  // Jetson IP over USB
#define SERVER_PORT 1234           // Port where str2str TCP server listens

#define BUFFER_SIZE 512

static THD_WORKING_AREA(waRTCMForwarder, 2048);

static BaseSequentialStream *debug_stream = NULL;
static BaseAsynchronousChannel *uart_stream = NULL;

static THD_FUNCTION(RTCMForwarder, arg) {
    (void)arg;
    chRegSetThreadName("RTCMForwarder");

    struct sockaddr_in server_addr;
    int sock;
    uint8_t buffer[BUFFER_SIZE];
    int bytes_read;

    while (true) {
        // Create socket
        sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            chprintf(debug_stream, "Socket creation failed\n");
            chThdSleepMilliseconds(5000);
            continue;
        }

        // Setup server address
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = PP_HTONS(SERVER_PORT);
        server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

        chprintf(debug_stream, "Connecting to %s:%d...\n",
                 SERVER_IP, SERVER_PORT);

        if (lwip_connect(sock, (struct sockaddr*)&server_addr,
                         sizeof(server_addr)) < 0) {
            chprintf(debug_stream, "Connection failed\n");
            lwip_close(sock);
            chThdSleepMilliseconds(5000);
            continue;
        }

        chprintf(debug_stream, "Connected!\n");

        // Read-Forward loop
        while (true) {
            bytes_read = lwip_recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0) {
                chprintf(debug_stream, "Connection lost\n");
                lwip_close(sock);
                break;
            }

            // Forward to UART
            for (int i = 0; i < bytes_read; i++) {
                chSequentialStreamPut((BaseSequentialStream*)uart_stream, buffer[i]);
            }
        }

        // Wait before reconnect
        chThdSleepMilliseconds(2000);
    }
}

void rtcm_forward_start(BaseSequentialStream *dbg,
                         BaseAsynchronousChannel *uart_out) {
    debug_stream = dbg;
    uart_stream = uart_out;
    chThdCreateStatic(waRTCMForwarder, sizeof(waRTCMForwarder),
                      NORMALPRIO + 1, RTCMForwarder, NULL);
}

