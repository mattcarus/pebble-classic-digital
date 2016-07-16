#include <pebble.h>

static Window *s_main_window;
static Layer *s_simple_bg_layer;
static TextLayer *s_time_layer;
static Layer *s_battery_icon_layer;
static Layer *s_bluetooth_icon_layer;

static GPath *s_bluetooth_path_ptr = NULL;
static GPath *s_strikeout_path_ptr = NULL;

static const GPathInfo BLUETOOTH_PATH_INFO = {
  .num_points = 6,
  .points = (GPoint []) {{2, 0}, {7, 5}, {2, 10}, {7, 15}, {2, 20}, {2, 0}}
};

static const GPathInfo STRIKEOUT_PATH_INFO = {
  .num_points = 6,
  .points = (GPoint []) {{0, 0}, {10, 20}, {5, 10}, {0, 20}, {10, 0}, {5, 10}}
};

static void bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(layer_get_bounds(layer).size.w/2, 470), 330);
}

static void handle_battery(BatteryChargeState charge_state) {
  layer_mark_dirty(s_battery_icon_layer);
}

static void draw_battery_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, GRect(2, 0, 5, 2));
  graphics_draw_rect(ctx, GRect(0, 2, 9, 18));

  // Can't draw a 1 pixel rectangle.  Use 13 to ensure 10% => 2px height
  int gone = 13 - 0.13 * battery_state_service_peek().charge_percent;
  if (14-gone >= 2)
    graphics_fill_rect(ctx, GRect(2, 4+gone, 5, 14-gone), 0, GCornerNone);
}

static void handle_bluetooth(bool connected) {
  layer_mark_dirty(s_bluetooth_icon_layer);
}

void setup_bluetooth_path(void) {
  s_bluetooth_path_ptr = gpath_create(&BLUETOOTH_PATH_INFO);
  s_strikeout_path_ptr = gpath_create(&STRIKEOUT_PATH_INFO);
}

static void draw_bluetooth_proc(Layer *layer, GContext *ctx) {
  // Fill the path:
  //graphics_context_set_fill_color(ctx, GColorWhite);
  //gpath_draw_filled(ctx, s_my_path_ptr);
  // Stroke the path:
  bool connected = connection_service_peek_pebble_app_connection();
  
  // Draw the base bluetooth icon
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_draw_outline(ctx, s_bluetooth_path_ptr);
  
  if ( !connected ) {
    // Strikeout the icon
    graphics_context_set_stroke_color(ctx, GColorRed);
    graphics_context_set_stroke_width(ctx, 2);
    gpath_draw_outline(ctx, s_strikeout_path_ptr);
  }
}

static void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {
  // Needs to be static because it's used by the system later.
  static char s_time_text[] = "00:00";

  strftime(s_time_text, sizeof(s_time_text), "%T", tick_time);
  text_layer_set_text(s_time_layer, s_time_text);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  
  s_simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_simple_bg_layer);

  s_time_layer = text_layer_create(GRect(0, 60, bounds.size.w, 48));
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  
  s_battery_icon_layer = layer_create(GRect((bounds.size.w/2)-15, 150, 9, 20));
  layer_set_update_proc(s_battery_icon_layer, draw_battery_proc);
  battery_state_service_subscribe(handle_battery);

  s_bluetooth_icon_layer = layer_create(GRect((bounds.size.w/2)+6, 150, 10, 20));
  layer_set_update_proc(s_bluetooth_icon_layer, draw_bluetooth_proc);
  setup_bluetooth_path();
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth
  });

  // Ensures time is displayed immediately (will break if NULL tick event accessed).
  // (This is why it's a good idea to have a separate routine to do the update itself.)
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  
  handle_minute_tick(current_time, SECOND_UNIT);
  
  tick_timer_service_subscribe(SECOND_UNIT, handle_minute_tick);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, s_battery_icon_layer);
  layer_add_child(window_layer, s_bluetooth_icon_layer);
  
  // Set initial Values
  handle_battery(battery_state_service_peek());
  handle_bluetooth(connection_service_peek_pebble_app_connection());
}

static void main_window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  layer_destroy(s_simple_bg_layer);
  layer_destroy(s_battery_icon_layer);
  layer_destroy(s_bluetooth_icon_layer);
  text_layer_destroy(s_time_layer);
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
