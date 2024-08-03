#include "panel.hpp"
#include "ui.hpp"
// font is a TTF/OTF from downloaded from fontsquirrel.com
// converted to a header with https://honeythecodewitch.com/gfx/converter
#define OPENSANS_REGULAR_IMPLEMENTATION
#include "assets/OpenSans_Regular.hpp" // our font
// icons generated using https://honeythecodewitch.com/gfx/iconPack
#define ICONS_IMPLEMENTATION
#include "assets/icons.hpp" // our icons

const gfx::open_font& text_font = OpenSans_Regular;

using namespace gfx;
using namespace uix;

// the screen/control declarations
screen_t speed_screen;
needle_t speed_needle;
label_t speed_label;
label_t speed_units_label;
label_t timer_label;
label_t clock_label;
label_t trip_label;
label_t trip_units_label;
canvas_t speed_battery_canvas;

screen_t big_screen;
label_t big_label;
label_t big_units_label;
canvas_t big_battery_canvas;

screen_t stat_screen;
label_t loc_lat_label;
label_t loc_lon_label;
label_t loc_alt_label;
label_t stat_sat_label;
canvas_t stat_battery_canvas;

int ui_battery_level = 0;
bool ui_ac_in = false;
static void ui_battery_icon_paint(surface_t& destination, 
                                const srect16& clip, 
                                void* state) {
    // show in green if it's on ac power.
    int pct = ui_battery_level;
    auto px = ui_ac_in?color_t::green:color_t::white;
   if(!ui_ac_in && pct<25) {
        px=color_t::red;
    }
    // draw an empty battery
    draw::icon(destination,point16::zero(),faBatteryEmpty,px);
    // now fill it up
    if(pct==100) {
        // if we're at 100% fill the entire thing
        draw::filled_rectangle(destination,rect16(3,7,22,16),px);
    } else {
        // otherwise leave a small border
        draw::filled_rectangle(destination,rect16(4,9,4+(0.18f*pct),14),px);
   }
}
void ui_init() {
    speed_screen.dimensions({320,240});
    speed_screen.buffer_size(panel_transfer_buffer_size);
    speed_screen.buffer1(panel_transfer_buffer1);
    speed_screen.buffer2(panel_transfer_buffer2);
    speed_battery_canvas.bounds(((srect16)faBatteryEmpty_size.bounds()).offset(speed_screen.dimensions().width-faBatteryEmpty_size.width-1,0));
    speed_battery_canvas.on_paint_callback(ui_battery_icon_paint);
    speed_screen.register_control(speed_battery_canvas);
    speed_needle.bounds(srect16(0,0,127,127).center(speed_screen.bounds()).offset(0,32));
    speed_needle.needle_border_color(color32_t::dark_red);
    speed_needle.needle_color(color32_t::red);
    speed_needle.angle(270);
    speed_screen.register_control(speed_needle);
    const int16_t speed_height = speed_needle.bounds().x1-2;
    srect16 speed_rect = text_font.measure_text(ssize16::max(),spoint16::zero(),"888",text_font.scale(speed_height)).bounds();
    speed_rect.y2+=2;
    speed_rect.center_horizontal_inplace(speed_screen.bounds());
    speed_rect.offset_inplace(20,0);
    speed_label.bounds(speed_rect);
    speed_label.text_justify(uix_justify::top_right);
    speed_label.text_open_font(&text_font);
    speed_label.padding({5,0});
    speed_label.text_line_height(speed_height);
    speed_label.background_color(color32_t::transparent);
    speed_label.border_color(color32_t::transparent);
    speed_label.text_color(color32_t::white);
    speed_label.text("--");
    speed_screen.register_control(speed_label);
    const size_t speed_unit_height = speed_height/3;
    const size_t speed_unit_width = text_font.measure_text(
        ssize16::max(),
        spoint16::zero(),
        "MMM",
        text_font.scale(speed_unit_height)).width;
    srect16 speed_unit_rect(0,0,speed_unit_width-1,speed_unit_height-1);
    speed_unit_rect.offset_inplace(speed_label.bounds().x2+2,(speed_height-speed_unit_height)/2);
    speed_units_label.bounds(speed_unit_rect);
    speed_units_label.text_open_font(&text_font);
    speed_units_label.text_line_height(speed_unit_height);
    speed_units_label.text_justify(uix_justify::top_right);
    speed_units_label.border_color(color32_t::transparent);
    speed_units_label.background_color(color32_t::transparent);
    speed_units_label.text_color(color32_t::white);
    speed_units_label.padding({0,0});
    speed_units_label.text("---");
    speed_screen.register_control(speed_units_label);
    const int16_t clock_height = 240-speed_needle.bounds().y2-2+10;
    clock_label.bounds(srect16(0,speed_needle.bounds().y2-40,159,speed_screen.bounds().y2));
    clock_label.background_color(color32_t::transparent);
    clock_label.border_color(color32_t::transparent);
    clock_label.text_color(color32_t::yellow);
    clock_label.text_open_font(&text_font);
    clock_label.text_line_height(clock_height);
    clock_label.padding({5,0});
    clock_label.text("12:00 am");
    speed_screen.register_control(clock_label);
    const int16_t timer_height = clock_height;
    timer_label.bounds(srect16(0,0,159,clock_height-1));
    timer_label.background_color(color32_t::transparent);
    timer_label.border_color(color32_t::transparent);
    timer_label.text_color(color32_t::aquamarine);
    timer_label.text_open_font(&text_font);
    timer_label.text_line_height(timer_height);
    timer_label.padding({5,0});
    timer_label.text("00:00:00");
    speed_screen.register_control(timer_label);
    const int16_t trip_height = 240-speed_needle.bounds().y2-2+10;
    const int16_t trip_unit_height = trip_height;
    const size_t trip_unit_width = text_font.measure_text(
        ssize16::max(),
        spoint16::zero(),
        "MMM",
        text_font.scale(trip_unit_height)).width;
    trip_label.bounds(srect16(160,speed_needle.bounds().y2-40,320-trip_unit_width-2,speed_screen.bounds().y2));
    trip_label.text_open_font(&text_font);
    trip_label.text_line_height(trip_height);
    trip_label.text_justify(uix_justify::top_right);
    trip_label.text("----.--");
    trip_label.background_color(color32_t::transparent);
    trip_label.border_color(color32_t::transparent);
    trip_label.text_color(color32_t::orange);
    trip_label.padding({0,0});
    speed_screen.register_control(trip_label);
    srect16 trip_unit_rect(0,0,trip_unit_width-1,trip_unit_height-1);
    trip_unit_rect.offset_inplace(trip_label.bounds().x2+1,trip_label.bounds().y1+((trip_height-trip_unit_height)/2));
    trip_units_label.bounds(trip_unit_rect);
    trip_units_label.text_open_font(&text_font);
    trip_units_label.text_line_height(speed_unit_height);
    trip_units_label.text_justify(uix_justify::top_right);
    trip_units_label.border_color(color32_t::transparent);
    trip_units_label.background_color(color32_t::transparent);
    trip_units_label.text_color(color32_t::white);
    trip_units_label.text("---");
    speed_screen.register_control(trip_units_label);

    big_screen.dimensions({320,240});
    big_screen.buffer_size(panel_transfer_buffer_size);
    big_screen.buffer1(panel_transfer_buffer1);
    big_screen.buffer2(panel_transfer_buffer2);
    big_battery_canvas.bounds(((srect16)faBatteryEmpty_size.bounds()).offset(big_screen.dimensions().width-faBatteryEmpty_size.width-1,0));
    big_battery_canvas.on_paint_callback(ui_battery_icon_paint);
    big_screen.register_control(big_battery_canvas);
    srect16 big_unit_rect(0,0,speed_unit_width-1,speed_unit_height-1);
    big_unit_rect.offset_inplace(big_screen.dimensions().width-2-speed_unit_width,(big_screen.dimensions().height-speed_unit_height)/2);
    big_units_label.bounds(big_unit_rect);
    big_units_label.text_open_font(&text_font);
    big_units_label.text_line_height(speed_unit_height);
    big_units_label.text_justify(uix_justify::top_right);
    big_units_label.border_color(color32_t::transparent);
    big_units_label.background_color(color32_t::transparent);
    big_units_label.text_color(color32_t::white);
    big_units_label.padding({0,0});
    big_units_label.text("---");
    big_screen.register_control(big_units_label);
    srect16 big_rect(0,0,big_unit_rect.x1-2,big_screen.dimensions().height-2);
    big_rect.center_vertical_inplace(big_screen.bounds());
    big_label.bounds(big_rect);
    big_label.text_justify(uix_justify::center_right);
    big_label.text_open_font(&text_font);
    big_label.padding({20,0});
    big_label.text_line_height(big_label.dimensions().height-2);
    big_label.background_color(color32_t::transparent);
    big_label.border_color(color32_t::transparent);
    big_label.text_color(color32_t::white);
    big_label.text("--");
    big_screen.register_control(big_label);
    
    stat_screen.dimensions({320,240});
    stat_screen.buffer_size(panel_transfer_buffer_size);
    stat_screen.buffer1(panel_transfer_buffer1);
    stat_screen.buffer2(panel_transfer_buffer2);
    stat_battery_canvas.bounds(((srect16)faBatteryEmpty_size.bounds()).offset(stat_screen.dimensions().width-faBatteryEmpty_size.width-1,0));
    stat_battery_canvas.on_paint_callback(ui_battery_icon_paint);
    stat_screen.register_control(stat_battery_canvas);
    const size_t stat_height = stat_screen.dimensions().height/4.5f;
    srect16 stat_rect(0,stat_height/2,stat_screen.bounds().x2,stat_height+10);
    stat_sat_label.bounds(stat_rect);
    stat_sat_label.text_open_font(&text_font);
    stat_sat_label.text_justify(uix_justify::center);
    stat_sat_label.text_line_height(stat_height);
    stat_sat_label.padding({10,0});
    stat_sat_label.background_color(color32_t::transparent);
    stat_sat_label.border_color(color32_t::transparent);
    stat_sat_label.text_color(color32_t::light_blue);
    stat_sat_label.text("-/- sats");
    stat_screen.register_control(stat_sat_label);
    stat_rect.offset_inplace(0,stat_rect.height());
    loc_lat_label.bounds(stat_rect);
    loc_lat_label.text_open_font(&text_font);
    loc_lat_label.text_line_height(stat_height);
    loc_lat_label.padding({10,0});
    loc_lat_label.border_color(color32_t::transparent);
    loc_lat_label.background_color(color32_t::transparent);
    loc_lat_label.text_color(color32_t::aqua);
    loc_lat_label.text("lat: --");
    stat_screen.register_control(loc_lat_label);
    stat_rect.offset_inplace(0,stat_rect.height());
    loc_lon_label.bounds(stat_rect);
    loc_lon_label.text_open_font(&text_font);
    loc_lon_label.text_line_height(stat_height);
    loc_lon_label.padding({10,0});
    loc_lon_label.border_color(color32_t::transparent);
    loc_lon_label.background_color(color32_t::transparent);
    loc_lon_label.text_color(color32_t::aqua);
    loc_lon_label.text("lon: --");
    stat_screen.register_control(loc_lon_label);
    stat_rect.offset_inplace(0,stat_rect.height());
    loc_alt_label.bounds(stat_rect);
    loc_alt_label.text_open_font(&text_font);
    loc_alt_label.text_line_height(stat_height);
    loc_alt_label.padding({10,0});
    loc_alt_label.border_color(color32_t::transparent);
    loc_alt_label.background_color(color32_t::transparent);
    loc_alt_label.text_color(color32_t::aqua);
    loc_alt_label.text("alt: --");
    stat_screen.register_control(loc_alt_label);

}
