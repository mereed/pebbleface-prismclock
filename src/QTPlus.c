#include "QTPlus.h"

GColor qtp_background_color = GColorBlack;

static AppTimer *display_timer;
int cur_day = -1;

static Window *qtp_window2;
static bool qtp_is_showing;
static TextLayer *qtp_battery_text_layer;
static TextLayer *qtp_time_layer;
static TextLayer *qtp_date_layer;
static TextLayer *qtp_day_layer;

static GBitmap *qtp_battery_images[3];
static GBitmap *qtp_battery_image;
static BitmapLayer *qtp_battery_image_layer;


static const int QTP_BATTERY_ICONS[] = {
	RESOURCE_ID_IMAGE_0,
	RESOURCE_ID_IMAGE_1,
	RESOURCE_ID_IMAGE_2
};


/* Initialize listeners to show and hide Quick Tap Plus as well as update data */
void qtp_setup() {
	qtp_is_showing = false;
	accel_tap_service_subscribe(&qtp_tap_handler);
	
	qtp_battery_images[0] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_0);
	qtp_battery_images[1] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_1);
	qtp_battery_images[2] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_2);
	
	qtp_battery_image = qtp_battery_images[0];
	
}

/* Handle taps from the hardware */
void qtp_tap_handler(AccelAxisType axis, int32_t direction) {
	if (qtp_is_showing) {
		qtp_hide();
	} else {
		qtp_show();
	}
	qtp_is_showing = !qtp_is_showing;
}


/* Update the text layer for the battery status */
void qtp_update_battery_status(bool mark_dirty) {
	BatteryChargeState charge_state = battery_state_service_peek();
	static char battery_text[] = "100%";
	snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);

	if (charge_state.charge_percent >= 70) {
		qtp_battery_image = qtp_battery_images[2];
	} else if (charge_state.charge_percent <= 30) {
		qtp_battery_image = qtp_battery_images[0];
	} else {
		qtp_battery_image = qtp_battery_images[1];
	}
	bitmap_layer_set_bitmap(qtp_battery_image_layer, qtp_battery_image);

	text_layer_set_text(qtp_battery_text_layer, battery_text);
	
	if (mark_dirty) {
		layer_mark_dirty(text_layer_get_layer(qtp_battery_text_layer));
		layer_mark_dirty(bitmap_layer_get_layer(qtp_battery_image_layer));
	}
}

/* Update the text layer for the clock */
void qtp_update_time(bool mark_dirty) {
	
	static char time_text[10];
	
	clock_copy_time_string(time_text, sizeof(time_text));
	text_layer_set_text(qtp_time_layer, time_text);
	
	if (mark_dirty) {
		layer_mark_dirty(text_layer_get_layer(qtp_time_layer));

	}
}

void qtp_update_date(struct tm *tick_time) {

	static char date_text[] = "xxxxxxxxx 00xx";
    static char wday_text[] = "xxxxxxxxx";
		
	// Only update the date when it's changed.
    int new_cur_day = tick_time->tm_year*1000 + tick_time->tm_yday;
    if (new_cur_day != cur_day) {
        cur_day = new_cur_day;
	}
        strftime(date_text, sizeof(date_text), "%B %e", tick_time);
        text_layer_set_text(qtp_date_layer, date_text);

        strftime(wday_text, sizeof(wday_text), "%A", tick_time);
        text_layer_set_text(qtp_day_layer, wday_text);

}

void force_update(void) {
    time_t now = time(NULL);
    qtp_update_date(localtime(&now));
}

/* Auto-hide the window after a certain time */
void qtp_timeout() {
	qtp_hide();
	qtp_is_showing = false;
}

/* Create the QTPlus Window and initialize the layres */
void qtp_init() {
	qtp_window2 = window_create();
	
	qtp_background_color  = GColorBlack;
    window_set_background_color(qtp_window2, qtp_background_color);

	
	GRect time_frame = GRect( 0,30,144,32 );
		qtp_time_layer = text_layer_create(time_frame);
		qtp_update_time(false);
	    text_layer_set_text_color(qtp_time_layer, GColorWhite);
        text_layer_set_background_color(qtp_time_layer, GColorClear);
		text_layer_set_text_alignment(qtp_time_layer, GTextAlignmentCenter);
		//text_layer_set_font(qtp_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
		text_layer_set_font(qtp_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIXEL_20)));
		layer_add_child(window_get_root_layer(qtp_window2), text_layer_get_layer(qtp_time_layer));

	GRect date_frame = GRect( 0,76,144,32 );
		qtp_date_layer = text_layer_create(date_frame);
	    text_layer_set_text_color(qtp_date_layer, GColorWhite);
        text_layer_set_background_color(qtp_date_layer, GColorClear);
		text_layer_set_text_alignment(qtp_date_layer, GTextAlignmentCenter);
		//text_layer_set_font(qtp_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	    text_layer_set_font(qtp_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIXEL_16)));
		layer_add_child(window_get_root_layer(qtp_window2), text_layer_get_layer(qtp_date_layer));
	
	GRect day_frame = GRect( 0,57,144,32 );
		qtp_day_layer = text_layer_create(day_frame);
	    text_layer_set_text_color(qtp_day_layer, GColorWhite);
        text_layer_set_background_color(qtp_day_layer, GColorClear);
		text_layer_set_text_alignment(qtp_day_layer, GTextAlignmentCenter);
		//text_layer_set_font(qtp_day_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	    text_layer_set_font(qtp_day_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIXEL_16)));
		layer_add_child(window_get_root_layer(qtp_window2), text_layer_get_layer(qtp_day_layer));
	
	/* Battery Logo layer */
	GRect battery_logo_frame = GRect( 30, 100, 32, 16 );
	qtp_battery_image_layer = bitmap_layer_create(battery_logo_frame);
	bitmap_layer_set_bitmap(qtp_battery_image_layer, qtp_battery_image);
	bitmap_layer_set_alignment(qtp_battery_image_layer, GAlignTopLeft);
	layer_add_child(window_get_root_layer(qtp_window2), bitmap_layer_get_layer(qtp_battery_image_layer)); 

	/* Battery Status text layer */
	GRect battery_frame = GRect( 67,99,60,32 );
	qtp_battery_text_layer =  text_layer_create(battery_frame);
    text_layer_set_text_color(qtp_battery_text_layer, GColorWhite);
    text_layer_set_background_color(qtp_battery_text_layer, GColorClear);
	//text_layer_set_font(qtp_battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
    text_layer_set_font(qtp_battery_text_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIXEL_16)));
	text_layer_set_text_alignment(qtp_battery_text_layer, GTextAlignmentCenter);
	qtp_update_battery_status(false);
	layer_add_child(window_get_root_layer(qtp_window2), text_layer_get_layer(qtp_battery_text_layer));

	force_update();
}

/* Deallocate QTPlus items when window is hidden */
void qtp_deinit() {

	layer_remove_from_parent(bitmap_layer_get_layer(qtp_battery_image_layer));
    bitmap_layer_destroy(qtp_battery_image_layer);
	qtp_battery_image = NULL;
	
	window_destroy(qtp_window2);

}

/* Deallocate QTPlus items when watchface exits */
void qtp_app_deinit() {
	
  accel_tap_service_unsubscribe();

  text_layer_destroy( qtp_time_layer );
  text_layer_destroy( qtp_date_layer );
  text_layer_destroy( qtp_day_layer );
  text_layer_destroy( qtp_battery_text_layer );

  gbitmap_destroy(qtp_battery_images[0]);
  gbitmap_destroy(qtp_battery_images[1]);  
  gbitmap_destroy(qtp_battery_images[2]);
	
}

/* Create window, layers, text. Display QTPlus */
void qtp_show() {
	qtp_init();
	window_stack_push(qtp_window2, true);
	
	display_timer = app_timer_register(3000, qtp_timeout, NULL);
	
}

/* Hide QTPlus. Free memory */
void qtp_hide() {
	window_stack_pop(true);
	qtp_deinit();
}


