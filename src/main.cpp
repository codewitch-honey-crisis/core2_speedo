#define MAX_SPEED 40
#include <Arduino.h>
#include <esp_i2c.hpp> // i2c initialization
#include <m5core2_power.hpp> // AXP192 power management (core2)
#include <bm8563.hpp> // real-time clock
#include <ft6336.hpp> // touch
#include <uix.hpp> // user interface library
#include <gfx.hpp> // graphics library
#include "ui.hpp" // ui declarations
#include "panel.hpp" // display panel functionality
#include "lwgps.h"
// namespace imports
using namespace arduino; // libs (arduino)
using namespace gfx; // graphics
using namespace uix; // user interface

using power_t = m5core2_power;
// for AXP192 power management
static power_t power(esp_i2c<1,21,22>::instance);

// for the time stuff
static bm8563 time_rtc(esp_i2c<1,21,22>::instance);

// for touch
ft6336<320,280,32> touch(esp_i2c<1,21,22>::instance);

// serial incoming
static char rx_buffer[1024];
// reads from Serial1/UART_NUM_1
static size_t serial_read(char* buffer, size_t size) {
    if(Serial1.available()) {
        return Serial1.read(buffer,size);
    } else {
        return 0;
    }
}
static lwgps_t gps;
static char clock_buffer[32];
static time_t timer_counter=0;
static char timer_buffer[32];
static char speed_units[16];
static char trip_units[16];
static lwgps_speed_t gps_units;
static double trip_counter_miles = 0;
static double trip_counter_kilos = 0;
static char trip_buffer[64];
static char speed_buffer[16];
static char loc_lat_buffer[64];
static char loc_lon_buffer[64];
static char loc_alt_buffer[64];
static char stat_sat_buffer[64];
static int current_screen = 0;
static bool is_big_speed = false;
// switch between imperial and metric units
static void toggle_units() {
    if(gps_units==LWGPS_SPEED_KPH) {
        gps_units = LWGPS_SPEED_MPH;
        strcpy(speed_units,"mph");
        strcpy(trip_units,"mi");
    } else {
        gps_units = LWGPS_SPEED_KPH;
        strcpy(speed_units,"kph");
        strcpy(trip_units,"km");
    }
    speed_label.invalidate();
    speed_units_label.text(speed_units);
    big_units_label.text(speed_units);
    trip_label.invalidate();
    trip_units_label.text(trip_units);
}
// updates the time string with the current time
static void update_clock(time_t time) {
    tm tim = *localtime(&time);
    strftime(clock_buffer, sizeof(clock_buffer), "%I:%M ", &tim);
    if(*clock_buffer=='0') {
        *clock_buffer=' ';
    }
    if(tim.tm_hour<12) {
        strcat(clock_buffer,"am");
    } else {
        strcat(clock_buffer,"pm");
    }
}
// updates the time string with the current time
static void update_timer(time_t time) {
    int scs = time % 60;
    int mns = (time / 60) % 60;
    int hrs = (time / (60 * 60)) % 100;
    snprintf(timer_buffer,sizeof(timer_buffer),"%02d:%02d:%02d",hrs,mns,scs);
}
static void button_a_on_press() {
    toggle_units();
}
static void button_b_on_press() {
    if(++current_screen>1) {
        current_screen = 0;
    }
    switch (current_screen)
    {
    case 1:
        panel_set_active_screen(stat_screen);
        break;
    default: // 0
        if(is_big_speed) {
            panel_set_active_screen(big_screen);
        } else {
            panel_set_active_screen(speed_screen);
        }
        break;
    }
    
}
static void button_c_on_press() {
    trip_counter_miles = 0;
    trip_counter_kilos = 0;
    timer_counter = 0;
    snprintf(trip_buffer,sizeof(trip_buffer),"% .2f",0.0f);
    trip_label.text(trip_buffer);
    update_timer(0);
    timer_label.text(timer_buffer);

}
void setup() {
    Serial.begin(115200);
    printf("Arduino version: %d.%d.%d\n",ESP_ARDUINO_VERSION_MAJOR,ESP_ARDUINO_VERSION_MINOR,ESP_ARDUINO_VERSION_PATCH);
    Serial1.begin(9600,SERIAL_8N1,32,33);
    power.initialize(); // do this first
    panel_init(); // do this next
    touch.initialize();
    time_rtc.initialize();
    lwgps_init(&gps);
    ui_init();
    *clock_buffer=0;
    clock_label.text(clock_buffer);
    *timer_buffer=0;
    timer_label.text(timer_buffer);
    strcpy(speed_units,"kph");
    strcpy(trip_units,"km");
    speed_units_label.text(speed_units);
    big_units_label.text(speed_units);
    trip_units_label.text(trip_units);
#ifdef MILES
    toggle_units();
#endif
    panel_set_active_screen(speed_screen);   
}

void loop()
{
    int read = serial_read(rx_buffer,sizeof(rx_buffer));
    if(read>0) {
        lwgps_process(&gps,rx_buffer,read);
    }
    if(gps.is_valid) {
        // old values so we know when changes occur
        // (avoid refreshing unless changed)
        static double old_trip = NAN;
        static int old_sats_in_use = -1;
        static int old_sats_in_view = -1;
        static float old_lat = NAN, old_lon = NAN, old_alt = NAN;
        static float old_mph = NAN, old_kph = NAN;
        static int old_angle = -1;

        // for timing the trip counter
        static uint32_t poll_ts = millis();
        static uint64_t total_ts = 0;
        // compute how long since the last
        uint32_t diff_ts = millis()-poll_ts;
        poll_ts = millis();
        // add it to the total
        total_ts += diff_ts;
        
        float kph = lwgps_to_speed(gps.speed,LWGPS_SPEED_KPH);
        float mph = lwgps_to_speed(gps.speed,LWGPS_SPEED_MPH);

        if(total_ts>=100) {
            while(total_ts>=100) {
                total_ts-=100;
                trip_counter_miles+=mph;
                trip_counter_kilos+=kph;
            }
            double trip = 
                (double)((gps_units==LWGPS_SPEED_KPH)? 
                trip_counter_kilos:
                trip_counter_miles)
                /(60.0*60.0*10.0);
            if(round(old_trip*100.0)!=round(trip*100.0)) {
                snprintf(trip_buffer,sizeof(trip_buffer),"% .2f",trip);
                trip_label.text(trip_buffer);
                old_trip = trip;
            }
        }
        bool speed_changed = false;
        int sp;
        int old_sp;
        if(gps_units==LWGPS_SPEED_KPH) {
            sp=(int)roundf(kph);
            old_sp = (int)roundf(old_kph);
            if(old_sp!=sp) {
                speed_changed = true;
            }
        } else {
            sp=(int)roundf(mph);
            old_sp = (int)roundf(old_mph);
            if(old_sp!=sp) {
                speed_changed = true;
            }
        }
        old_mph = mph;
        old_kph = kph;
        if(speed_changed) {
            itoa((int)roundf(sp>MAX_SPEED?MAX_SPEED:sp),speed_buffer,10);
            speed_label.text(speed_buffer);
            big_label.text(speed_buffer);
            // figure the needle angle
            float f = gps_units == LWGPS_SPEED_KPH?kph:mph;
            int angle = (270 + 
                ((int)roundf(((f>MAX_SPEED?MAX_SPEED:f)/MAX_SPEED)*180.0f)));
            while(angle>=360) angle-=360;
            if(old_angle!=angle) {
                speed_needle.angle(angle);
                old_angle = angle;
            }
        }
        // update the stat data
        if(gps.sats_in_use!=old_sats_in_use||
                gps.sats_in_view!=old_sats_in_view) {
            snprintf(stat_sat_buffer,
                sizeof(stat_sat_buffer),"%d/%d sats",
                (int)gps.sats_in_use,
                (int)gps.sats_in_view);
            stat_sat_label.text(stat_sat_buffer);
            old_sats_in_use = gps.sats_in_use;
            old_sats_in_view = gps.sats_in_view;
        }
        // update the position data
        if(roundf(old_lat*100.0f)!=roundf(gps.latitude*100.0f)) {
            snprintf(loc_lat_buffer,sizeof(loc_lat_buffer),"lat: % .2f",gps.latitude);
            loc_lat_label.text(loc_lat_buffer);
            old_lat = gps.latitude;
        }
        if(roundf(old_lon*100.0f)!=roundf(gps.longitude*100.0f)) {
            snprintf(loc_lon_buffer,sizeof(loc_lon_buffer),"lon: % .2f",gps.longitude);
            loc_lon_label.text(loc_lon_buffer);
            old_lon = gps.longitude;
        }
        if(roundf(old_alt*100.0f)!=roundf(gps.altitude*100.0f)) {
            snprintf(loc_alt_buffer,sizeof(loc_lon_buffer),"alt: % .2f",gps.altitude);
            loc_alt_label.text(loc_alt_buffer);
            old_alt = gps.altitude;
        }
    }
    static time_t old_time = 0;
    time_t new_time = time_rtc.now();
    if(new_time!=old_time) {
        ++timer_counter;
        update_timer(timer_counter);
        timer_label.invalidate();
        if(old_time==0 || (new_time%60)==0) {
            update_clock(new_time);
            clock_label.invalidate();
        }
        old_time = new_time;
    }
    // update the battery level
    static int bat_level = -1;
    if((int)power.battery_level()!=bat_level) {
        ui_battery_level = power.battery_level();
        speed_battery_canvas.invalidate();
        big_battery_canvas.invalidate();
        stat_battery_canvas.invalidate();
        bat_level = ui_battery_level;
    }
    static int ac_in = -1;
    if(power.ac_in()!=ac_in) {
        ui_ac_in = power.ac_in();
        speed_battery_canvas.invalidate();
        big_battery_canvas.invalidate();
        stat_battery_canvas.invalidate();
        ac_in = ui_ac_in;
    }
    panel_update();
    
    // process "buttons"
    touch.update();  
    uint16_t tx,ty;
    static int old_button=-1;
    int button = -1;
    if(touch.xy(&tx,&ty)) {
        if(ty>239) {
            if(tx>213) {
                // button c
                button = 2;
            } else if(tx>107) {
                // button b
                button = 1;
            } else {
                // button a
                button = 0;
            }
        } else {
            button = 3;
        }
    } else {
        switch(old_button)  {
            case 0:
                button_a_on_press();
                break;
            case 1:
                button_b_on_press();
                break;
            case 2:
                button_c_on_press();
                break;
            case 3:
                if(current_screen==0) {
                    puts("touched");
                    if(!is_big_speed) {
                        panel_set_active_screen(big_screen);
                        is_big_speed = true;
                    } else {
                        panel_set_active_screen(speed_screen);
                        is_big_speed = false;
                    }
                }
                break;
            default:
                
                break;
        }
        button = -1;
    }
    old_button = button;

}
