// Kör CMD_SET_MAIN_CONFIG-blocket ur commands.c och huvudtrådens reglerloopar
// (state_control_update) på datorn, med AddressSanitizer.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "datatypes.h"
#include "buffer.h"
#include "conf_general.h"
#include "state_control.h"
#include "sensor_control.h"
#include "motor_control.h"

MAIN_CONFIG main_config;
static unsigned char eeprom[sizeof(MAIN_CONFIG)];
static int eeprom_valid = 0;
float last_sensorvalue, frontangle, io_board_as5047_angle;
static uint8_t m_send_buffer[1024];
static int vesc_calls = 0;

void conf_general_read_main_conf(MAIN_CONFIG *conf) { memcpy(conf, eeprom, sizeof(MAIN_CONFIG)); }
bool conf_general_store_main_config(MAIN_CONFIG *conf) { memcpy(eeprom, conf, sizeof(MAIN_CONFIG)); return true; }
void motor_set_vesc_value(int id, float value, motor_control_mode mode) { (void)id;(void)value;(void)mode; vesc_calls++; }
void commands_printf(const char* f, ...) { (void)f; }
void timeout_reset(void) {}
void commands_set_send_func(void *f) { (void)f; }
void commands_send_packet(unsigned char *d, unsigned int l) { (void)d;(void)l; }
void log_set_rate(int r) {(void)r;}  void log_set_enabled(bool e) {(void)e;}
void log_set_name(char *n) {(void)n;} void log_set_ext(int m, int b) {(void)m;(void)b;}
void motor_sim_set_running(bool r) {(void)r;}
void autopilot_set_active(bool a) {(void)a;}
void motor_stop(void) {}
void timeout_configure(unsigned long t, float b) {(void)t;(void)b;}
unsigned long timeout_heartbeat_ms(float s) { return s > 0 ? (unsigned long)(s*1000) : 0; }
float timeout_get_brake_current(void) { return 0; }
static int seeding = 0;
#include "extra.c"
static void maybe_sanitize(MAIN_CONFIG *c) {
#ifdef NY
	if (!seeding) conf_general_sanitize_main_config(c);
#else
	(void)c;
#endif
}   /* sanitize (ny kod) och motor_get_actuators_by_activity, urklippta */

static void set_main_config(unsigned char *data, unsigned int len) {
	void *func = 0; int id_ret = 5; int packet_id = CMD_SET_MAIN_CONFIG;
	switch (packet_id) {
#include "set_block.c"
	default: break;
	}
}

static int rd(const char *fn, unsigned char *buf, int max) {
	FILE *f = fopen(fn, "rb"); int n = fread(buf, 1, max, f); fclose(f); return n;
}

int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	// Kortet startar: EEPROM som avläst 2026-09-28 14:58 (390-byte-svaret är samma
	// serialisering, men EEPROM innehåller structen; här läggs structen via Write
	// av hela 388-byte-bilden, som är det som låg sparat).
	unsigned char full[1024]; memset(full, 0, sizeof(full));
	int nfull = rd("eeprom.bin", full, sizeof(full));
	// Bygg en EEPROM-bild med skräpet genom att tolka hela bilden med "gammal" semantik:
	// hela 388 byte finns i paketet, så alla fält läses (inga vakter slår till).
	memset(&main_config, 0, sizeof(main_config));
	seeding = 1; set_main_config(full, nfull); seeding = 0;
	conf_general_store_main_config(&main_config);
	printf("Start-EEPROM: sensors=%u state_controls=%u heartbeat=%g\n",
	       main_config.vehicle.sensors, main_config.vehicle.state_controls, main_config.vehicle.heartbeat_maxtime);
	// Ny firmware saniterar vid start, gammal gör det inte:
	conf_general_read_main_conf(&main_config);
#ifdef NY
	conf_general_sanitize_main_config(&main_config);
#endif
	printf("Efter start:  sensors=%u state_controls=%u\n", main_config.vehicle.sensors, main_config.vehicle.state_controls);

	for (int w = 1; w <= 3; w++) {
		unsigned char rx[1024];
		rd("rx_old.bin", rx, sizeof(rx));               // gammalt innehåll
		int n = rd(getenv("PAKET") ? getenv("PAKET") : "rcs.bin", rx, sizeof(rx));           // RControlStations paket överst
		set_main_config(rx, n);
		for (int t = 0; t < 1000; t++) state_control_update(); // huvudtråden, 10 s
		printf("Write %d OK: sensors=%u state_controls=%u heartbeat=%g actuators=%u vesc-anrop=%d\n", w,
		       main_config.vehicle.sensors, main_config.vehicle.state_controls,
		       main_config.vehicle.heartbeat_maxtime, main_config.vehicle.actuators, vesc_calls);
	}
	printf("KLART utan krasch\n");
	return 0;
}
