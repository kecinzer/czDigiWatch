#include <pebble.h>
#include "namedays-cs.h"

static Window *s_main_window;
static TextLayer *main_digits;
static TextLayer *seconds;
static TextLayer *day_in_week;
static TextLayer *date;
static TextLayer *nameday1_line;
static TextLayer *nameday2_line;
static TextLayer *now_layer;
static TextLayer *tomorrow_layer;
static TextLayer *temperature_layer;
static TextLayer *temperature_next_layer;
static BitmapLayer *icon_layer;
static BitmapLayer *icon_next_layer;
static GBitmap *icon_bitmap = NULL;
static GBitmap *icon_bitmap_next = NULL;

enum {
  WEATHER_ICON_KEY,
  WEATHER_TEMPERATURE_KEY,
  WEATHER_ICON_NEXT_KEY,
  WEATHER_TEMPERATURE_NEXT_KEY
};

static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_IMAGE_CLEAR_DAY,
  RESOURCE_ID_IMAGE_CLEAR_NIGHT,
  RESOURCE_ID_IMAGE_CLOUDY,
  RESOURCE_ID_IMAGE_FOG,
  RESOURCE_ID_IMAGE_PARTLY_CLOUDY_DAY,
  RESOURCE_ID_IMAGE_PARTLY_CLOUDY_NIGHT,
  RESOURCE_ID_IMAGE_RAIN,
  RESOURCE_ID_IMAGE_SLEET,
  RESOURCE_ID_IMAGE_SNOW,
  RESOURCE_ID_IMAGE_WIND
};

static void in_received_handler(DictionaryIterator *received, void *context) {
	Tuple *t = dict_read_first(received);
  // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
    	case WEATHER_TEMPERATURE_KEY:
    		text_layer_set_text(temperature_layer, t->value->cstring);
      	break;
    	case WEATHER_TEMPERATURE_NEXT_KEY:
    		text_layer_set_text(temperature_next_layer, t->value->cstring);
    		break;
    	case WEATHER_ICON_KEY:
    		if (icon_bitmap) {
		    	gbitmap_destroy(icon_bitmap);
		    }
		    icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[2]);
		    bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
		    break;
    	case WEATHER_ICON_NEXT_KEY:
    		if (icon_bitmap_next) {
		      gbitmap_destroy(icon_bitmap_next);
		    }
		    icon_bitmap_next = gbitmap_create_with_resource(WEATHER_ICONS[t->value->uint8]);
		    bitmap_layer_set_bitmap(icon_next_layer, icon_bitmap_next);
      	break;
    }
    // Look for next item
    t = dict_read_next(received);
  }
}

static void request_weather(void) {
  Tuplet weather_current = TupletInteger(WEATHER_TEMPERATURE_KEY, 1);
  Tuplet weather_next = TupletInteger(WEATHER_TEMPERATURE_NEXT_KEY, 1);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    return;
  }

  dict_write_tuplet(iter, &weather_current);
  dict_write_tuplet(iter, &weather_next);
  dict_write_end(iter);

  app_message_outbox_send();
}

static void configureLayer(TextLayer *textlayer, const char *font, int alignment) {
  text_layer_set_font(textlayer, fonts_get_system_font(font));
  text_layer_set_text_color(textlayer, GColorWhite);
  text_layer_set_background_color(textlayer, GColorClear);
  text_layer_set_text_alignment(textlayer, alignment);
}

static void update_day() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int day = t->tm_mday;
  int month_no = t->tm_mon;
  int year = t->tm_year+1900;

  char *day_of_week[7] = {"neděle", "pondělí", "úterý", "středa", "čtvrtek", "pátek", "sobota"};
  char *month[12] = {"ledna", "února", "března", "dubna", "května", "června", "července", "srpna", "září", "října",
      "listopadu", "prosince"};

  configureLayer(day_in_week, FONT_KEY_GOTHIC_18_BOLD, GTextAlignmentCenter);
  configureLayer(date, FONT_KEY_GOTHIC_18, GTextAlignmentCenter);
  configureLayer(nameday1_line, public_holiday(t, 0) ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_24, GTextAlignmentCenter);
  configureLayer(nameday2_line, public_holiday(t, 1) ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_18, GTextAlignmentCenter);

  int date_size = sizeof(day) + 2 + strlen(month[month_no]) + 1 + sizeof(year);
  char *complete_date = malloc(date_size);
  snprintf(complete_date, date_size, "%i. %s %i", day, month[month_no], year);
  
  text_layer_set_text(day_in_week, day_of_week[t->tm_wday]);
  text_layer_set_text(date, complete_date);
  text_layer_set_text(nameday1_line, get_nameday(t, 0));
  text_layer_set_text(nameday2_line, get_nameday(t, 1));

}

static void update_time() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  configureLayer(main_digits, FONT_KEY_BITHAM_42_BOLD, GTextAlignmentCenter);
  configureLayer(seconds, FONT_KEY_GOTHIC_24_BOLD, GTextAlignmentCenter);

  static char digits[] = "00:00";

  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(digits, sizeof("00:00"), "%H:%M", t);
  } else {
    // Use 12 hour format
    strftime(digits, sizeof("00:00"), "%I:%M", t);
  }
  text_layer_set_text(main_digits, digits);

  static char secs[] = "00";
  strftime(secs, sizeof("00"), "%S", t);

  text_layer_set_text(seconds, secs);

  // aktualizace každý den o půlnoci
  if (t->tm_hour == 0 && t->tm_min == 0 && t->tm_sec == 0) {
    update_day();
  }
  // aktualizace počasí každých 20 minut
  if ((t->tm_min == 2 || t->tm_min == 22 || t->tm_min == 42) && t->tm_sec == 0) {
  	request_weather();
  }

}

static void main_window_load(Window *window) {
  window_set_background_color(window, GColorBlack);

  // Hodiny s minutami
  main_digits = text_layer_create(GRect(0, 0, 144, 50));
  // Sekundy
  seconds = text_layer_create(GRect(0, 40, 144, 25));

  update_time();

  // Dent v týdnu
  day_in_week = text_layer_create(GRect(0, 68, 144, 50));
  // Datum
  date = text_layer_create(GRect(0, 88, 144, 50));
  // Dnešní svátek
  nameday1_line = text_layer_create(GRect(0, 110, 144, 50));
  // Zítřejší svátek
  nameday2_line = text_layer_create(GRect(0, 140, 144, 50));
  
  update_day();

  icon_layer = bitmap_layer_create(GRect(0, 46, 20, 20));
  temperature_layer = text_layer_create(GRect(22, 48, 50, 20));
  configureLayer(temperature_layer, FONT_KEY_GOTHIC_14, GTextAlignmentLeft);

  icon_next_layer = bitmap_layer_create(GRect(124, 46, 20, 20));
  temperature_next_layer = text_layer_create(GRect(94, 48, 26, 20));
  configureLayer(temperature_next_layer, FONT_KEY_GOTHIC_14, GTextAlignmentRight);

  now_layer = text_layer_create(GRect(0, 64, 45, 20));
  configureLayer(now_layer, FONT_KEY_GOTHIC_14, GTextAlignmentCenter);
  text_layer_set_text(now_layer, "nyní");
  tomorrow_layer = text_layer_create(GRect(99, 64, 45, 20));
  configureLayer(tomorrow_layer, FONT_KEY_GOTHIC_14, GTextAlignmentCenter);
  text_layer_set_text(tomorrow_layer, "zítra");

  request_weather();

  layer_add_child(window_get_root_layer(window), text_layer_get_layer(main_digits));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(seconds));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(day_in_week));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(date));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(nameday1_line));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(nameday2_line));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(icon_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(temperature_layer));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(icon_next_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(temperature_next_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(now_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(tomorrow_layer));
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(main_digits);
  text_layer_destroy(seconds);
  text_layer_destroy(day_in_week);
  text_layer_destroy(date);
  text_layer_destroy(nameday1_line);
  text_layer_destroy(nameday2_line);
  bitmap_layer_destroy(icon_layer);
  text_layer_destroy(temperature_layer);
  bitmap_layer_destroy(icon_next_layer);
  text_layer_destroy(temperature_next_layer);
  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  }
  if (icon_bitmap_next) {
    gbitmap_destroy(icon_bitmap_next);
  }
  text_layer_destroy(now_layer);
  text_layer_destroy(tomorrow_layer);
}

static void refresh_every_second(struct tm *tick_time, TimeUnits units_changed) {
  // Aktualizace času
  update_time();
}

// Register any app message handlers.
static void app_message_init(void) {
    app_message_register_inbox_received(in_received_handler);

    app_message_open(128, 128);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  app_message_init();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  window_stack_push(s_main_window, true);
  // Registrace sekundového sledování
  tick_timer_service_subscribe(SECOND_UNIT, refresh_every_second);
}

static void deinit() {
    tick_timer_service_unsubscribe();
    app_message_deregister_callbacks();
    window_stack_remove(s_main_window, true);
    // Destroy Window
    window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}