// Copyright (C) 2013-2016 by Tom Fukushima. All Rights Reserved.
// Copyright (c) 2013 Douwe Maan <http://www.douwemaan.com/>
// The above copyright notice shall be included in all copies or substantial portions of the program.

// Revolution watchface...
// Envisioned as a watchface by Jean-Noël Mattern
// Based on the display of the Freebox Revolution, which was designed by Philippe Starck.

#include <pebble.h>
#include "effect_layer.h"

#define KEY_MINUTE_COLOR_R 0
#define KEY_MINUTE_COLOR_G 1
#define KEY_MINUTE_COLOR_B 2
#define KEY_HOUR_COLOR_R 3
#define KEY_HOUR_COLOR_G 4
#define KEY_HOUR_COLOR_B 5

#define GRECT_FULL_WINDOW GRect(0,0,144,168)

// Settings
#define USE_AMERICAN_DATE_FORMAT      true

// Magic numbers
#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168
#define XCENTER 72
#define YCENTER 84

#define TIME_IMAGE_WIDTH    58
#define TIME_IMAGE_HEIGHT   70

#define SMALL_DIGIT_IMAGE_WIDTH    11
#define SMALL_DIGIT_IMAGE_HEIGHT   18

#define DAY_IMAGE_WIDTH     20
#define DAY_IMAGE_HEIGHT    20

#define MARGIN              1
#define MARGIN_TIME_X       13
#define MARGIN_DATE_X       2
#define TIME_SLOT_SPACE     2
#define DATE_PART_SPACE     7
#define DATE_CONTAINER_HEIGHT SCREEN_HEIGHT - SCREEN_WIDTH
#define DATE_DAY_GAP        2

#define BATTERY_IMAGE_WIDTH 8
#define BATTERY_IMAGE_HEIGHT 15

#define MINUTE_COLOR GColorArmyGreen
#define HOUR_COLOR GColorLiberty
#define MINUTE_BUFFER 15
#define MINUTE_SIZE MINUTE_BUFFER
#define HOUR_BUFFER 40
#define HOUR_SIZE 20



// Images
#define NUMBER_OF_TIME_IMAGES 10
const int TIME_IMAGE_RESOURCE_IDS[NUMBER_OF_TIME_IMAGES] = {
  RESOURCE_ID_IMAGE_TIME_0, 
  RESOURCE_ID_IMAGE_TIME_1, RESOURCE_ID_IMAGE_TIME_2, RESOURCE_ID_IMAGE_TIME_3, 
  RESOURCE_ID_IMAGE_TIME_4, RESOURCE_ID_IMAGE_TIME_5, RESOURCE_ID_IMAGE_TIME_6, 
  RESOURCE_ID_IMAGE_TIME_7, RESOURCE_ID_IMAGE_TIME_8, RESOURCE_ID_IMAGE_TIME_9
};

#define NUMBER_OF_SMALL_DIGIT_IMAGES 10
const int SMALL_DIGIT_IMAGE_RESOURCE_IDS[NUMBER_OF_SMALL_DIGIT_IMAGES] = {
  RESOURCE_ID_IMAGE_SMALL_DIGIT_0, 
  RESOURCE_ID_IMAGE_SMALL_DIGIT_1, RESOURCE_ID_IMAGE_SMALL_DIGIT_2, RESOURCE_ID_IMAGE_SMALL_DIGIT_3, 
  RESOURCE_ID_IMAGE_SMALL_DIGIT_4, RESOURCE_ID_IMAGE_SMALL_DIGIT_5, RESOURCE_ID_IMAGE_SMALL_DIGIT_6, 
  RESOURCE_ID_IMAGE_SMALL_DIGIT_7, RESOURCE_ID_IMAGE_SMALL_DIGIT_8, RESOURCE_ID_IMAGE_SMALL_DIGIT_9
};

#define NUMBER_OF_DAY_IMAGES 7
const int DAY_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_IMAGE_DAY_0, RESOURCE_ID_IMAGE_DAY_1, RESOURCE_ID_IMAGE_DAY_2, 
  RESOURCE_ID_IMAGE_DAY_3, RESOURCE_ID_IMAGE_DAY_4, RESOURCE_ID_IMAGE_DAY_5, 
  RESOURCE_ID_IMAGE_DAY_6
};


// Main
Layer *root_layer;
Window *window;

static uint8_t battery_level;
static bool battery_plugged;
static GBitmap *icon_battery;
static GBitmap *icon_battery_charge;
static Layer *battery_layer;

static AppTimer *recheck_bluetooth_timer;

static EffectLayer *full_inverse_layer;
static Layer *inverter_layer;

#define EMPTY_SLOT -1

typedef struct Slot {
  int           number;
  BitmapLayer   *image_layer;
  GBitmap       *bitmap;
  int           state;
} Slot;

#define NUMBER_OF_TIME_SLOTS 4
Layer *time_layer;
Slot time_slots[NUMBER_OF_TIME_SLOTS];

// Date
typedef struct DateSlot {
  Slot slot;
  GRect frame;
} DateSlot;

#define NUMBER_OF_DATE_SLOTS 4
Layer *date_layer;
int date_layer_width;
DateSlot date_slots[NUMBER_OF_DATE_SLOTS];

// Day
typedef struct ImageItem {
  BitmapLayer   *image_layer;
  GBitmap       *bitmap;
  GRect         frame;
  bool          loaded;
} ImageItem;
ImageItem day_item;
ImageItem slash_item;

// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_digit_image_from_slot(Slot *slot);
void unload_image_item(ImageItem * item);
void unload_day();
void unload_slash();
void create_date_layer(struct tm *tick_time);

// Display
void display_time(struct tm *tick_time);
void display_date(struct tm *tick_time);
void display_day(struct tm *tick_time);
void display_slash();

// Time
void display_time_value(int value, int row_number);
void update_time_slot(Slot *time_slot, int digit_value);
GRect frame_for_time_slot(Slot *time_slot);

// Date
void display_date_value(int value, int part_number);
void update_date_slot(DateSlot *date_slot, int digit_value);

// Connection
void fail_mode();
void reset_fail_mode();

// handlers
static void handle_battery(BatteryChargeState charge_state);
static void handle_tick(struct tm *tick_time, TimeUnits units_changed);

// startup
void init();
void deinit();

// Config
static GColor minute_color;
static GColor hour_color;

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  int red, green, blue;
  Tuple *color_red_t, *color_green_t, *color_blue_t;
  // Minute color?
  color_red_t = dict_find(iter, KEY_MINUTE_COLOR_R);
  color_green_t = dict_find(iter, KEY_MINUTE_COLOR_G);
  color_blue_t = dict_find(iter, KEY_MINUTE_COLOR_B);
  if(color_red_t && color_green_t && color_blue_t) {
    // Apply the color if available
      red = color_red_t->value->int32;
      green = color_green_t->value->int32;
      blue = color_blue_t->value->int32;
  
      // Persist values
      persist_write_int(KEY_MINUTE_COLOR_R, red);
      persist_write_int(KEY_MINUTE_COLOR_G, green);
      persist_write_int(KEY_MINUTE_COLOR_B, blue);
  
      minute_color = GColorFromRGB(red, green, blue);
  }
  // Hour color?
  color_red_t = dict_find(iter, KEY_HOUR_COLOR_R);
  color_green_t = dict_find(iter, KEY_HOUR_COLOR_G);
  color_blue_t = dict_find(iter, KEY_HOUR_COLOR_B);
  if(color_red_t && color_green_t && color_blue_t) {
    // Apply the color if available
      red = color_red_t->value->int32;
      green = color_green_t->value->int32;
      blue = color_blue_t->value->int32;
  
      // Persist values
      persist_write_int(KEY_HOUR_COLOR_R, red);
      persist_write_int(KEY_HOUR_COLOR_G, green);
      persist_write_int(KEY_HOUR_COLOR_B, blue);
  
      hour_color = GColorFromRGB(red, green, blue);
  }

  layer_mark_dirty(root_layer);

}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 19) {
    return NULL;
  }

  if (slot->state != EMPTY_SLOT) {
    return NULL;
  }

  slot->state = digit_value;
  slot->image_layer = bitmap_layer_create(frame);
  slot->bitmap = gbitmap_create_with_resource(digit_resource_ids[digit_value]);
  bitmap_layer_set_bitmap(slot->image_layer, slot->bitmap);
  bitmap_layer_set_compositing_mode(slot->image_layer, GCompOpSet);
  Layer * layer = bitmap_layer_get_layer(slot->image_layer);
  layer_set_clips(layer, true);
  layer_add_child(parent_layer, layer);

  return slot->image_layer;
}

void unload_digit_image_from_slot(Slot *slot) {
  if (slot->state == EMPTY_SLOT) {
    return;
  }

  layer_remove_from_parent(bitmap_layer_get_layer(slot->image_layer));
  gbitmap_destroy(slot->bitmap);
  bitmap_layer_destroy(slot->image_layer);

  slot->state = EMPTY_SLOT;
}

void unload_image_item(ImageItem *item) {
  if (item->loaded) {
    layer_remove_from_parent(bitmap_layer_get_layer(item->image_layer));
    gbitmap_destroy(item->bitmap);
    bitmap_layer_destroy(item->image_layer);
    item->loaded = false;
  }
}


void unload_day() {
  unload_image_item(&day_item);
}

void unload_slash() {
  unload_image_item(&slash_item);
}

void create_date_frames(int left_digit_count, int right_digit_count) {
  GRect base_frame = GRect(0, 0, SMALL_DIGIT_IMAGE_WIDTH, SMALL_DIGIT_IMAGE_HEIGHT);
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; ++i)
    date_slots[i].frame = base_frame;
  date_slots[1].frame.origin.x = date_slots[0].frame.origin.x + (left_digit_count > 1 ? SMALL_DIGIT_IMAGE_WIDTH : 0);
  date_slots[2].frame.origin.x = date_slots[1].frame.origin.x + SMALL_DIGIT_IMAGE_WIDTH + DATE_PART_SPACE;
  date_slots[3].frame.origin.x = date_slots[2].frame.origin.x + (right_digit_count > 1 ? SMALL_DIGIT_IMAGE_WIDTH : 0);
}

void create_date_layer(struct tm *tick_time) {
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; ++i) {
    unload_digit_image_from_slot(&date_slots[i].slot);
  }
  unload_slash();

  if (date_layer != NULL) {
    layer_remove_from_parent(date_layer);
    layer_destroy(date_layer);
    date_layer = NULL;
  }
  int month_digit_count = tick_time->tm_mon > 8 ? 2 : 1;
  int day_digit_count = tick_time->tm_mday > 9 ? 2 : 1; 

  date_layer_width = SMALL_DIGIT_IMAGE_WIDTH * month_digit_count + DATE_PART_SPACE + SMALL_DIGIT_IMAGE_WIDTH * day_digit_count;
  GRect date_layer_rect = GRect(MARGIN, SCREEN_WIDTH + 4, date_layer_width, SMALL_DIGIT_IMAGE_HEIGHT + MARGIN);
  date_layer = layer_create(date_layer_rect);  
  layer_set_clips(date_layer, true);
  layer_add_child(root_layer, date_layer);

#if USE_AMERICAN_DATE_FORMAT
  slash_item.frame = GRect(month_digit_count * SMALL_DIGIT_IMAGE_WIDTH, 
    0, DATE_PART_SPACE, SMALL_DIGIT_IMAGE_HEIGHT);
  create_date_frames(month_digit_count, day_digit_count);
#else
  slash_item.frame = GRect(day_digit_count * SMALL_DIGIT_IMAGE_WIDTH, 
    0, DATE_PART_SPACE, SMALL_DIGIT_IMAGE_HEIGHT);
  create_date_frames(day_digit_count, month_digit_count);
#endif
}


// Display
void display_time(struct tm *tick_time) {
  int hour = tick_time->tm_hour;

  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  display_time_value(hour,              0);
  display_time_value(tick_time->tm_min, 1);
}

void display_date(struct tm *tick_time) {
  int day   = tick_time->tm_mday;
  int month = tick_time->tm_mon + 1;

#if USE_AMERICAN_DATE_FORMAT
  display_date_value(month, 0);
  display_date_value(day,   1);
#else
  display_date_value(day,   0);
  display_date_value(month, 1);
#endif
}


void display_item(ImageItem * item, uint32_t resource_id, Layer *parent) {
  if (item->loaded) {
    layer_remove_from_parent(bitmap_layer_get_layer(item->image_layer));
    gbitmap_destroy(item->bitmap);
    bitmap_layer_destroy(item->image_layer);
  }

  item->image_layer = bitmap_layer_create(item->frame);
  item->bitmap = gbitmap_create_with_resource(resource_id);
  bitmap_layer_set_bitmap(item->image_layer, item->bitmap);
  bitmap_layer_set_compositing_mode(item->image_layer, GCompOpSet);
  Layer * layer = bitmap_layer_get_layer(item->image_layer);
  layer_set_clips(layer, true);
  layer_add_child(parent, layer);

  item->loaded = true;
}

void display_day(struct tm *tick_time) {
  int ix = tick_time->tm_wday;
  day_item.frame = GRect(
    date_layer_width + MARGIN + DATE_DAY_GAP, 
    SCREEN_WIDTH + 4, 
    DAY_IMAGE_WIDTH, 
    DAY_IMAGE_HEIGHT
  );
  display_item(&day_item, DAY_IMAGE_RESOURCE_IDS[ix], root_layer);
}

void display_slash() {
  display_item(&slash_item, RESOURCE_ID_IMAGE_SLASH, date_layer);
}

// Time
void display_time_value(int value, int row_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {

    int time_slot_number = (row_number * 2) + column_number;

    Slot *time_slot = &time_slots[time_slot_number];

    if (row_number == 0 && value == 0 && column_number == 0) { // ignore the leading 0 for hours
      unload_digit_image_from_slot(time_slot);
      return;
    }
	
    int ix = value % 10;       
    update_time_slot(time_slot, ix);

    value = value / 10;
  }
}

void update_time_slot(Slot *time_slot, int digit_value) {
  if (time_slot->state == digit_value) {
    return;
  }

  GRect frame = frame_for_time_slot(time_slot);

  unload_digit_image_from_slot(time_slot);
  load_digit_image_into_slot(time_slot, digit_value, time_layer, frame, TIME_IMAGE_RESOURCE_IDS);

}

GRect frame_for_time_slot(Slot *time_slot) {
  int x = MARGIN_TIME_X + (time_slot->number % 2) * (TIME_IMAGE_WIDTH + TIME_SLOT_SPACE);
  int y = MARGIN + (time_slot->number / 2) * (TIME_IMAGE_HEIGHT + TIME_SLOT_SPACE);

  return GRect(x, y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);
}


// Date
void display_date_value(int value, int part_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {

    DateSlot *date_slot = &date_slots[column_number + part_number*2];

    if (column_number == 0 && value == 0) {  // ignore the leading 0
      unload_digit_image_from_slot(&date_slot->slot);
    } else {
      int ix = value % 10;
      update_date_slot(date_slot, ix);
    }

    value = value / 10;
  }
}

void update_date_slot(DateSlot *date_slot, int digit_value) {
  if (date_slot->slot.state == digit_value) {
    return;
  }

  unload_digit_image_from_slot(&date_slot->slot);
  load_digit_image_into_slot(&date_slot->slot, digit_value, date_layer, 
    date_slot->frame, SMALL_DIGIT_IMAGE_RESOURCE_IDS);
}


void fail_mode() {
  vibes_long_pulse();
  layer_add_child(root_layer, inverter_layer);
}

void reset_fail_mode() {
  layer_remove_from_parent(inverter_layer);
}


int main() {
  init();
  app_event_loop();
  deinit();
}

static void handle_battery(BatteryChargeState charge) {

  battery_level = charge.charge_percent;
  battery_plugged = charge.is_plugged;
  layer_mark_dirty(battery_layer);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if ((units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
    display_time(tick_time);
  }

  if ((units_changed & DAY_UNIT) == DAY_UNIT) {
    create_date_layer(tick_time);
    display_day(tick_time);
    display_date(tick_time);
    display_slash();
  }

}

/*
 * Battery icon callback handler
 */
void battery_layer_update_callback(Layer *layer, GContext *ctx) {

  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  if (!battery_plugged) {
    graphics_draw_bitmap_in_rect(ctx, icon_battery, GRect(0, 0, BATTERY_IMAGE_WIDTH, BATTERY_IMAGE_HEIGHT));
    graphics_context_set_stroke_color(ctx, GColorBlack);
    if (battery_level >= 40)
        graphics_context_set_fill_color(ctx, GColorGreen);
    else if (battery_level >= 20)
        graphics_context_set_fill_color(ctx, GColorYellow);
    else
        graphics_context_set_fill_color(ctx, GColorRed);
    int height = (uint8_t)((battery_level / 100.0) * 10.0);
    graphics_fill_rect(ctx, GRect(2, 13 - height, 4, height), 0, GCornerNone);
  } else {
    graphics_draw_bitmap_in_rect(ctx, icon_battery_charge, GRect(0, 0, BATTERY_IMAGE_WIDTH, BATTERY_IMAGE_HEIGHT));
  }
}

void recheck_bluetooth(void *data) {
    app_timer_cancel(recheck_bluetooth_timer);
    if (!bluetooth_connection_service_peek()) {
      fail_mode();
    }
}

void bluetooth_connection_handler(bool connected) {
  if (connected) {
    reset_fail_mode();
  } else {
    recheck_bluetooth_timer = app_timer_register(3000, recheck_bluetooth, NULL);
  }
}

static GPoint get_frame_location(int buffer, int angle) {
  unsigned int x = 0;
  unsigned int y = 0;
  int32_t xsize = (SCREEN_WIDTH - 2 * buffer) / 2;
  int32_t ysize = (SCREEN_HEIGHT - 2 * buffer) / 2;
  unsigned int angle0 = 182 * angle;
  
  if ((angle >= 228) && (angle < 316)) {
    x = buffer;
    y = YCENTER + (ysize * cos_lookup(angle0) / sin_lookup(angle0)) ;
  } else if ((angle > 45) && (angle < 136)) {
    x = 144 - buffer;
    y = YCENTER - (ysize * cos_lookup(angle0) / sin_lookup(angle0)) ;
  } else if ((angle >= 136) && (angle < 228)) {
    x = XCENTER - (xsize * sin_lookup(angle0) / cos_lookup(angle0));
    y = 168 - buffer;
  } else {
    x = XCENTER + (xsize * sin_lookup(angle0) / cos_lookup(angle0));
    y = buffer;
  }
  return (GPoint) { .x = x, .y = y };
}

static void update_root_layer(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), 0, GCornerNone);

  struct tm *tick_time;
  time_t current_time = time(NULL);
  tick_time = localtime(&current_time);
 
  unsigned int hour_angle = (tick_time->tm_hour % 12) * 30 + (tick_time->tm_min / 2);
  graphics_context_set_fill_color(ctx, hour_color);
  GPoint hloc = get_frame_location(HOUR_BUFFER, hour_angle);
  graphics_fill_circle(ctx, hloc, HOUR_SIZE);
  graphics_context_set_stroke_color(ctx, hour_color);
  //graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, (GPoint){XCENTER, YCENTER}, hloc);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  //graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, hloc, (GPoint){2 * hloc.x - XCENTER, 2 * hloc.y - YCENTER}); 

  
  unsigned int minute_angle = tick_time->tm_min * 6;
  graphics_context_set_fill_color(ctx, minute_color);
  GPoint mloc = get_frame_location(MINUTE_BUFFER, minute_angle);
  graphics_fill_circle(ctx, mloc, MINUTE_SIZE);
  graphics_context_set_stroke_color(ctx, minute_color);
  //graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, (GPoint){XCENTER, YCENTER}, mloc);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  //graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, mloc, (GPoint){2 * mloc.x - XCENTER, 2 * mloc.y - YCENTER}); 

  graphics_context_set_fill_color(ctx, hour_color);
  graphics_fill_circle(ctx, (GPoint){XCENTER, YCENTER}, HOUR_SIZE / 3);
  graphics_context_set_fill_color(ctx, minute_color);
  graphics_fill_circle(ctx, (GPoint){XCENTER, YCENTER}, MINUTE_SIZE / 3);
}


void init() {

        // Register callbacks
        app_message_register_inbox_received(inbox_received_callback);
        app_message_register_inbox_dropped(inbox_dropped_callback);
        app_message_register_outbox_failed(outbox_failed_callback);
        app_message_register_outbox_sent(outbox_sent_callback);

        // Open AppMessage
        app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());  
  
        // Get stored colors
        #ifdef PBL_COLOR
        int red, green, blue;
        if (persist_exists(KEY_MINUTE_COLOR_R)) {
            red = persist_read_int(KEY_MINUTE_COLOR_R);
            green = persist_read_int(KEY_MINUTE_COLOR_G);
	    blue = persist_read_int(KEY_MINUTE_COLOR_B);
	    minute_color = GColorFromRGB(red, green, blue);
        } else {
	    minute_color = MINUTE_COLOR;
        }
	if (persist_exists(KEY_HOUR_COLOR_R)) {
	    red = persist_read_int(KEY_HOUR_COLOR_R);
	    green = persist_read_int(KEY_HOUR_COLOR_G);
	    blue = persist_read_int(KEY_HOUR_COLOR_B);  
	    hour_color = GColorFromRGB(red, green, blue);
	} else {
	    hour_color = HOUR_COLOR;
	}
	#endif
    
  window = window_create();
  window_stack_push(window, true /* Animated */);

  // Time slots
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    Slot *time_slot = &time_slots[i];
    time_slot->number  = i;
    time_slot->state   = EMPTY_SLOT;
    time_slot->image_layer = NULL;
    time_slot->bitmap  = NULL;
  }

  // Date slots
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    DateSlot *date_slot = &date_slots[i];
    date_slot->slot.number = i;
    date_slot->slot.state  = EMPTY_SLOT;
    date_slot->slot.image_layer = NULL;
    date_slot->slot.bitmap  = NULL;
    date_slot->frame = GRectZero;
  }

  // Root layer
  root_layer = window_get_root_layer(window);
  layer_set_update_proc(root_layer, update_root_layer);

  // Time
  time_layer = layer_create(GRect(0, 0, SCREEN_WIDTH, SCREEN_WIDTH));
  layer_set_clips(time_layer, true);
  layer_add_child(root_layer, time_layer);

  struct tm *tick_time;
  time_t current_time = time(NULL);
  tick_time = localtime(&current_time);

  // Slash
  slash_item.loaded = false;

  // Day slot
  day_item.loaded = false;

  // Date
  date_layer = NULL;
  create_date_layer(tick_time);

  // Battery status setup
  icon_battery = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
  icon_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING);

  battery_layer = layer_create(GRect(SCREEN_WIDTH-BATTERY_IMAGE_WIDTH-MARGIN-MARGIN,SCREEN_WIDTH+7,BATTERY_IMAGE_WIDTH,BATTERY_IMAGE_HEIGHT));
  BatteryChargeState initial = battery_state_service_peek();
  battery_level = initial.charge_percent;
  battery_plugged = initial.is_plugged;
  layer_set_update_proc(battery_layer, &battery_layer_update_callback);
  layer_add_child(root_layer, battery_layer);

  // Inverter
  full_inverse_layer = effect_layer_create(GRECT_FULL_WINDOW);
  effect_layer_add_effect(full_inverse_layer, effect_invert, NULL);
  inverter_layer = effect_layer_get_layer(full_inverse_layer);

  // Display
  window_set_background_color(window, GColorBlack);

  display_time(tick_time);
  display_day(tick_time);
  display_date(tick_time);
  display_slash();

  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  battery_state_service_subscribe(&handle_battery);
  handle_battery(battery_state_service_peek());
  bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
  bluetooth_connection_handler(bluetooth_connection_service_peek());
}

void deinit() {
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    unload_digit_image_from_slot(&time_slots[i]);
  }
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    unload_digit_image_from_slot(&date_slots[i].slot);
  }

  gbitmap_destroy(icon_battery);
  gbitmap_destroy(icon_battery_charge);
  layer_destroy(battery_layer);

  unload_day();
  unload_slash();
  layer_destroy(time_layer);
  effect_layer_destroy(full_inverse_layer);
  window_destroy(window);

}
