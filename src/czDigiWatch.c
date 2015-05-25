#include <pebble.h>
#include "namedays-cs.h"

static Window *s_main_window;
static TextLayer *main_digits;
static TextLayer *seconds;
static TextLayer *day_in_week;
static TextLayer *date;
static TextLayer *nameday1_line;
static TextLayer *nameday2_line;
static TextLayer *temperature_layer;
static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
};

static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_IMAGE_SUN, //0
  RESOURCE_ID_IMAGE_CLOUD, //1
  RESOURCE_ID_IMAGE_RAIN, //2
  RESOURCE_ID_IMAGE_SNOW //3
};

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case WEATHER_ICON_KEY:
      if (icon_bitmap) {
        gbitmap_destroy(icon_bitmap);
      }

      icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[new_tuple->value->uint8]);
      
      bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
      break;

    case WEATHER_TEMPERATURE_KEY:
      // App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
      text_layer_set_text(temperature_layer, new_tuple->value->cstring);
      break;
  }
}

static void request_weather(void) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (!iter) {
    // Error creating outbound message
    return;
  }

  int value = 1;
  dict_write_int(iter, 1, &value, sizeof(int), true);
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
  configureLayer(seconds, FONT_KEY_GOTHIC_24_BOLD, GTextAlignmentRight);

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
  seconds = text_layer_create(GRect(90, 40, 30, 25));

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

  icon_layer = bitmap_layer_create(GRect(26, 46, 20, 20));
  temperature_layer = text_layer_create(GRect(40, 48, 50, 20));
  configureLayer(temperature_layer, FONT_KEY_GOTHIC_14, GTextAlignmentCenter);
  
  Tuplet initial_values[] = {
    TupletInteger(WEATHER_ICON_KEY, (uint8_t) 1),
    TupletCString(WEATHER_TEMPERATURE_KEY, "---"),
  };

  app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer), 
      initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL
  );

  request_weather();

  layer_add_child(window_get_root_layer(window), text_layer_get_layer(main_digits));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(seconds));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(day_in_week));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(date));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(nameday1_line));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(nameday2_line));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(icon_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(temperature_layer));
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
  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  }
}

static void refresh_every_second(struct tm *tick_time, TimeUnits units_changed) {
  // Aktualizace času
  update_time();
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  window_stack_push(s_main_window, true);
  // Registrace sekundového sledování
  tick_timer_service_subscribe(SECOND_UNIT, refresh_every_second);
  // Otevření komunikace s JS
  app_message_open(64, 64);
}

static void deinit() {
    tick_timer_service_unsubscribe();
    // Destroy Window
    window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}