#include <pebble.h>

// Methods
void qtp_setup();
void qtp_app_deinit();

void qtp_show();
void qtp_hide();
void qtp_timeout();

void qtp_tap_handler(AccelAxisType axis, int32_t direction);

void qtp_update_time(bool mark_dirty);

void qtp_update_battery_status(bool mark_dirty);

void qtp_init();
void qtp_deinit();
