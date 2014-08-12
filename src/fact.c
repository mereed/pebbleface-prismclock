/*

Copyright (C) 2014 Mark Reed

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

-------------------------------------------------------------------


   Big Time watch

   A digital watch with large digits.


   A few things complicate the implementation of this watch:

   a) The largest size of the Nevis font which the Pebble handles
      seems to be ~47 units (points or pixels?). But the size of
      characters we want is ~100 points.

      This requires us to generate and use images instead of
      fonts--which complicates things greatly.

   b) When I started it wasn't possible to load all the images into
      RAM at once--this means we have to load/unload each image when
      we need it. The images are slightly smaller now than they were
      but I figured it would still be pushing it to load them all at
      once, even if they just fit, so I've stuck with the load/unload
      approach.

 */

#include "pebble.h"
#include "QTPlus.h"

static Window *window;
static Layer *window_layer_b;

static GBitmap *background_image_a;
static BitmapLayer *background_layer_a;

//InverterLayer *inverter_layer_bt = NULL;

//
// There's only enough memory to load about 6 of 10 required images
// so we have to swap them in & out...
//
// We have one "slot" per digit location on screen.
//
// Because layers can only have one parent we load a digit for each
// slot--even if the digit image is already in another slot.
//
// Slot on-screen layout:
//     0 1
//     2 3
//
#define TOTAL_IMAGE_SLOTS 4

#define NUMBER_OF_IMAGES 10

// These images are 72 x 84 pixels i.e. a quarter of the display
// black and white with the digit character centered in the image.

	const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_NUM_0, RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

static GBitmap *images[TOTAL_IMAGE_SLOTS];
static BitmapLayer *image_layers[TOTAL_IMAGE_SLOTS];

#define EMPTY_SLOT -1

// The state is either "empty" or the digit of the image currently in
// the slot--which was going to be used to assist with de-duplication
// but we're not doing that due to the one parent-per-layer
// restriction mentioned above.
static int image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};

static void load_digit_image_into_slot(int slot_number, int digit_value) {
  /*

     Loads the digit image from the application's resources and
     displays it on-screen in the correct location.

     Each slot is a quarter of the screen.

   */

  // TODO: Signal these error(s)?

  if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
    return;
  }

  if ((digit_value < 0) || (digit_value > 9)) {
    return;
  }

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    return;
  }

  image_slot_state[slot_number] = digit_value;
  images[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS[digit_value]);
  GRect frame = (GRect) {
    .origin = { (slot_number % 2) * 72, (slot_number / 2) * 84 },
    .size = images[slot_number]->bounds.size
  };
  BitmapLayer *bitmap_layer = bitmap_layer_create(frame);
  image_layers[slot_number] = bitmap_layer;
  bitmap_layer_set_bitmap(bitmap_layer, images[slot_number]);
  Layer *window_layer = window_get_root_layer(window);
  layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

static void unload_digit_image_from_slot(int slot_number) {
  /*

     Removes the digit from the display and unloads the image resource
     to free up RAM.

     Can handle being called on an already empty slot.

   */

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    layer_remove_from_parent(bitmap_layer_get_layer(image_layers[slot_number]));
    bitmap_layer_destroy(image_layers[slot_number]);
    gbitmap_destroy(images[slot_number]);
    image_slot_state[slot_number] = EMPTY_SLOT;
  }

}

static void display_value(unsigned short value, unsigned short row_number, bool show_first_leading_zero) {
  /*

     Displays a numeric value between 0 and 99 on screen.

     Rows are ordered on screen as:

       Row 0
       Row 1

     Includes optional blanking of first leading zero,
     i.e. displays ' 0' rather than '00'.

   */
  value = value % 100; // Maximum of two digits per row.

  // Column order is: | Column 0 | Column 1 |
  // (We process the columns in reverse order because that makes
  // extracting the digits from the value easier.)
  for (int column_number = 1; column_number >= 0; column_number--) {
    int slot_number = (row_number * 2) + column_number;
    unload_digit_image_from_slot(slot_number);
    if (!((value == 0) && (column_number == 0) && !show_first_leading_zero)) {
      load_digit_image_into_slot(slot_number, value % 10);
    }
    value = value / 10;
  }
}

static unsigned short get_display_hour(unsigned short hour) {

  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;

}

static void display_time(struct tm *tick_time) {

  // TODO: Use `units_changed` and more intelligence to reduce
  //       redundant digit unload/load?

  display_value(get_display_hour(tick_time->tm_hour), 0, false);
  display_value(tick_time->tm_min, 1, true);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  display_time(tick_time);
}

static void init() {
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorWhite);

	 
  window_stack_push(window, true /* Animated */);
  window_layer_b = window_get_root_layer(window);

  background_image_a = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG);
  background_layer_a = bitmap_layer_create(layer_get_frame(window_layer_b));
  bitmap_layer_set_bitmap(background_layer_a, background_image_a);
  layer_add_child(window_layer_b, bitmap_layer_get_layer(background_layer_a));
	
	
  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  display_time(tick_time);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	

}

static void deinit() {
   
	layer_remove_from_parent(bitmap_layer_get_layer(background_layer_a));
    bitmap_layer_destroy(background_layer_a);
    gbitmap_destroy(background_image_a);
    background_image_a = NULL;
	
 for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
    unload_digit_image_from_slot(i);
 }

	layer_remove_from_parent(window_layer_b);
    layer_destroy(window_layer_b);
		
  qtp_app_deinit();

}

int main(void) {
  init();
  qtp_setup();
  app_event_loop();
  deinit();
}
