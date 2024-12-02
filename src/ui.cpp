#include "ui.hpp"
// font is a TTF/OTF from downloaded from fontsquirrel.com
// converted to a header with https://honeythecodewitch.com/gfx/converter
#define OPENSANS_REGULAR_IMPLEMENTATION
#include "assets/OpenSans_Regular.h" // our font
// icons generated using https://honeythecodewitch.com/gfx/iconPack
#define ICONS_IMPLEMENTATION
#include "assets/icons.h" // our icons

gfx::const_buffer_stream text_font_stream(OpenSans_Regular,sizeof(OpenSans_Regular));

gfx::tt_font ttf_text_font_speed;
gfx::tt_font ttf_text_font_speed_unit;
gfx::tt_font ttf_clock_font;
gfx::tt_font ttf_timer_font;
gfx::tt_font ttf_trip_units_font;
gfx::tt_font ttf_big_font;
gfx::tt_font ttf_stat_font;

const gfx::const_bitmap<gfx::alpha_pixel<8>> faBatteryEmptyIco({faBatteryEmpty_size.width,faBatteryEmpty_size.height},faBatteryEmpty);

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
painter_t speed_battery_painter;

screen_t big_screen;
label_t big_label;
label_t big_units_label;
painter_t big_battery_painter;

screen_t stat_screen;
label_t loc_lat_label;
label_t loc_lon_label;
label_t loc_alt_label;
label_t stat_sat_label;
painter_t stat_battery_painter;

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
    draw::icon(destination,point16::zero(),faBatteryEmptyIco,px);
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
    speed_battery_painter.bounds(((srect16)faBatteryEmptyIco.bounds()).offset(speed_screen.dimensions().width-faBatteryEmpty_size.width-1,0));
    speed_battery_painter.on_paint_callback(ui_battery_icon_paint);
    speed_screen.register_control(speed_battery_painter);
    speed_needle.bounds(srect16(0,0,127,127).center(speed_screen.bounds()).offset(0,32));
    speed_needle.needle_border_color(color32_t::dark_red);
    speed_needle.needle_color(color32_t::red);
    speed_needle.angle(270);
    speed_screen.register_control(speed_needle);
    const int16_t speed_height = speed_needle.bounds().x1-2;
    text_info ti;
    ttf_text_font_speed = tt_font(text_font_stream,speed_height,font_size_units::px,true);
    ti.text_font = &ttf_text_font_speed;
    ti.text_sz("888");
    size16 speedsz;
    ti.text_font->measure(-1,ti,&speedsz);
    srect16 speed_rect = (srect16)speedsz.bounds();
    speed_rect.y2+=2;
    speed_rect.center_horizontal_inplace(speed_screen.bounds());
    speed_rect.offset_inplace(20,0);
    speed_label.bounds(speed_rect);
    speed_label.text_justify(uix_justify::top_right);
    speed_label.font(ttf_text_font_speed);
    speed_label.padding({5,0});
    speed_label.color(color32_t::white);
    speed_label.text("--");
    speed_screen.register_control(speed_label);
    const size_t speed_unit_height = speed_height/3;
    ttf_text_font_speed_unit = tt_font(text_font_stream,speed_unit_height,font_size_units::px,true);
    ti.text_font = &ttf_text_font_speed_unit;
    ti.text_sz("MMM");
    ttf_text_font_speed_unit.measure(-1,ti,&speedsz);
    const size_t speed_unit_width = speedsz.width;
    srect16 speed_unit_rect(0,0,speed_unit_width-1,speed_unit_height-1);
    speed_unit_rect.offset_inplace(speed_label.bounds().x2+2,(speed_height-speed_unit_height)/2);
    speed_units_label.bounds(speed_unit_rect);
    speed_units_label.font(ttf_text_font_speed_unit);
    speed_units_label.text_justify(uix_justify::top_right);
    speed_units_label.color(color32_t::white);
    speed_units_label.padding({0,0});
    speed_units_label.text("---");
    speed_screen.register_control(speed_units_label);
    const int16_t clock_height = 240-speed_needle.bounds().y2-2+10;
    ttf_clock_font = tt_font(text_font_stream,clock_height,font_size_units::px,true);
    clock_label.bounds(srect16(0,speed_needle.bounds().y2-40,159,speed_screen.bounds().y2));
    clock_label.color(color32_t::yellow);
    clock_label.font(ttf_clock_font);
    clock_label.padding({5,0});
    clock_label.text("12:00 am");
    speed_screen.register_control(clock_label);
    const int16_t timer_height = clock_height;
    ttf_timer_font = tt_font(text_font_stream,timer_height,font_size_units::px,true);
    timer_label.bounds(srect16(0,0,159,clock_height-1));
    timer_label.color(color32_t::aquamarine);
    timer_label.font(ttf_timer_font);
    timer_label.padding({5,0});
    timer_label.text("00:00:00");
    speed_screen.register_control(timer_label);
    const int16_t trip_height = 240-speed_needle.bounds().y2-2+10;
    const int16_t trip_unit_height = trip_height;
    ttf_trip_units_font = tt_font(text_font_stream,trip_unit_height,font_size_units::px,true);
    ti.text_font = &ttf_trip_units_font;
    ti.text_sz("MMM");
    ttf_trip_units_font.measure(-1,ti,&speedsz);
    const size_t trip_unit_width = speedsz.width;
    trip_label.bounds(srect16(160,speed_needle.bounds().y2-40,320-trip_unit_width-2,speed_screen.bounds().y2));
    trip_label.font(ttf_trip_units_font);
    trip_label.text_justify(uix_justify::top_right);
    trip_label.text("----.--");
    trip_label.color(color32_t::orange);
    trip_label.padding({0,0});
    speed_screen.register_control(trip_label);
    srect16 trip_unit_rect(0,0,trip_unit_width-1,trip_unit_height-1);
    trip_unit_rect.offset_inplace(trip_label.bounds().x2+1,trip_label.bounds().y1+((trip_height-trip_unit_height)/2));
    trip_units_label.bounds(trip_unit_rect);
    trip_units_label.font(ttf_text_font_speed_unit);
    trip_units_label.text_justify(uix_justify::top_right);
    trip_units_label.color(color32_t::white);
    trip_units_label.text("---");
    speed_screen.register_control(trip_units_label);

    big_screen.dimensions({320,240});
    big_battery_painter.bounds(((srect16)faBatteryEmptyIco.bounds()).offset(big_screen.dimensions().width-faBatteryEmpty_size.width-1,0));
    big_battery_painter.on_paint_callback(ui_battery_icon_paint);
    big_screen.register_control(big_battery_painter);
    srect16 big_unit_rect(0,0,speed_unit_width-1,speed_unit_height-1);
    big_unit_rect.offset_inplace(big_screen.dimensions().width-2-speed_unit_width,(big_screen.dimensions().height-speed_unit_height)/2);
    big_units_label.bounds(big_unit_rect);
    big_units_label.font(ttf_text_font_speed_unit);
    big_units_label.text_justify(uix_justify::top_right);
    big_units_label.color(color32_t::white);
    big_units_label.padding({0,0});
    big_units_label.text("---");
    big_screen.register_control(big_units_label);
    srect16 big_rect(0,0,big_unit_rect.x1-2,big_screen.dimensions().height-2);
    big_rect.center_vertical_inplace(big_screen.bounds());
    big_label.bounds(big_rect);
    uint16_t big_height = big_label.dimensions().height-2;
    ttf_big_font = tt_font(text_font_stream,big_height,font_size_units::px,true);
    big_label.text_justify(uix_justify::center_right);
    big_label.font(ttf_big_font);
    big_label.padding({20,0});
    big_label.color(color32_t::white);
    big_label.text("--");
    big_screen.register_control(big_label);
    
    stat_screen.dimensions({320,240});
    stat_battery_painter.bounds(((srect16)faBatteryEmptyIco.bounds()).offset(stat_screen.dimensions().width-faBatteryEmpty_size.width-1,0));
    stat_battery_painter.on_paint_callback(ui_battery_icon_paint);
    stat_screen.register_control(stat_battery_painter);
    const size_t stat_height = stat_screen.dimensions().height/10.f;
    ttf_stat_font = tt_font(text_font_stream,stat_height,font_size_units::px,true);
    srect16 stat_rect(0,stat_height/2,stat_screen.bounds().x2,stat_height+10);
    stat_sat_label.bounds(stat_rect);
    stat_sat_label.font(ttf_stat_font);
    stat_sat_label.text_justify(uix_justify::center);
    stat_sat_label.padding({10,0});
    stat_sat_label.color(color32_t::light_blue);
    stat_sat_label.text("-/- sats");
    stat_screen.register_control(stat_sat_label);
    stat_rect.offset_inplace(0,stat_rect.height());
    loc_lat_label.bounds(stat_rect);
    loc_lat_label.font(ttf_stat_font);
    loc_lat_label.padding({10,0});
    loc_lat_label.color(color32_t::aqua);
    loc_lat_label.text("lat: --");
    stat_screen.register_control(loc_lat_label);
    stat_rect.offset_inplace(0,stat_rect.height());
    loc_lon_label.bounds(stat_rect);
    loc_lon_label.font(ttf_stat_font);
    loc_lon_label.padding({10,0});
    loc_lon_label.color(color32_t::aqua);
    loc_lon_label.text("lon: --");
    stat_screen.register_control(loc_lon_label);
    stat_rect.offset_inplace(0,stat_rect.height());
    loc_alt_label.bounds(stat_rect);
    loc_alt_label.font(ttf_stat_font);
    loc_alt_label.padding({10,0});
    loc_alt_label.color(color32_t::aqua);
    loc_alt_label.text("alt: --");
    stat_screen.register_control(loc_alt_label);

}
