#pragma once
#include <gfx.hpp>
#include <uix.hpp>
#include "svg_needle.hpp"
// colors for the UI
using color_t = gfx::color<gfx::rgb_pixel<16>>; // native
using color32_t = gfx::color<gfx::rgba_pixel<32>>; // uix

// the screen template instantiation aliases
using screen_t = uix::screen<gfx::rgb_pixel<16>>;
using surface_t = screen_t::control_surface_type;

// the control template instantiation aliases
using label_t = uix::label<surface_t>;
using canvas_t = uix::canvas<surface_t>;
using needle_t = svg_needle<surface_t>;
// the screen/control declarations
extern screen_t speed_screen;
extern needle_t speed_needle;
extern label_t speed_label;
extern label_t speed_units_label;
extern label_t timer_label;
extern label_t clock_label;
extern label_t trip_label;
extern label_t trip_units_label;
extern canvas_t speed_battery_canvas;

extern screen_t big_screen;
extern label_t big_label;
extern label_t big_units_label;
extern canvas_t big_battery_canvas;

extern screen_t stat_screen;
extern label_t loc_lat_label;
extern label_t loc_lon_label;
extern label_t loc_alt_label;
extern label_t stat_sat_label;
extern canvas_t stat_battery_canvas;

// for the battery meter
extern int ui_battery_level;
extern bool ui_ac_in;

extern void ui_init();
