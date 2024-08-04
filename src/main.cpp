#define MAX_SPEED 40
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ili9342.h>
#include <esp_i2c.hpp> // i2c initialization
#include <m5core2_power.hpp> // AXP192 power management (core2)
#include <bm8563.hpp> // real-time clock
#include <ft6336.hpp> // touch
#include <button.hpp> // button
#include <uix.hpp> // user interface library
#include <gfx.hpp> // graphics library
#include "ui.hpp" // ui declarations
#include "lwgps.h" // gps decoder 
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

class core2_button final : public button {
    int m_index;
    bool m_initialized;
    bool m_pressed;
    button_on_pressed_changed m_callback;
    void* m_callback_state;
public:
    core2_button(int index = 0) : m_index(index),m_initialized(false), m_pressed(false),m_callback(nullptr) {

    }
    virtual bool initialized() const override {
        return m_initialized;
    }
    virtual void initialize() override {
        touch.initialize();
        m_initialized = true;
    }
    virtual void deinitialize() override {
        m_initialized = false;
    }
    virtual bool pressed() override {
        return m_pressed;
    }
    virtual void update() override {
        touch.update();
        uint16_t x,y;
        if(touch.xy(&x,&y)) {
            if(y>=240) {
                switch(m_index) {
                    case 1:
                        if(x>106 && x<=329-106) {
                            if(!m_pressed) {
                                m_pressed = true;
                                if(m_callback) {
                                    m_callback(true,m_callback_state);
                                }
                            }
                            return;
                        } else {
                            if(m_pressed) {
                                m_pressed = false;
                                if(m_callback) {
                                    m_callback(false,m_callback_state);
                                }
                            }
                        }
                    case 2:
                        if(x>329-106) {
                            if(!m_pressed) {
                                m_pressed = true;
                                if(m_callback) {
                                    m_callback(true,m_callback_state);
                                }
                            }
                        } else {
                            if(m_pressed) {
                                m_pressed = false;
                                if(m_callback) {
                                    m_callback(false,m_callback_state);
                                }
                            }
                        }
                        return;
                    case 3:
                        if(m_pressed) {
                            m_pressed = false;
                            if(m_callback) {
                                m_callback(false,m_callback_state);
                            }
                        }
                        break;
                    default:
                        if(x<107) {
                            if(!m_pressed) {
                                m_pressed = true;
                                if(m_callback) {
                                    m_callback(true,m_callback_state);
                                }
                            }
                        } else {
                            if(m_pressed) {
                                m_pressed = false;
                                if(m_callback) {
                                    m_callback(false,m_callback_state);
                                }
                            }
                        }
                        return;
                }
            } else if(m_index==3) {
                if(!m_pressed) {
                    m_pressed = true;
                    if(m_callback) {
                        m_callback(true,m_callback_state);
                    }
                }
                return;
            } else {
                if(m_pressed) {
                    m_pressed = false;
                    if(m_callback) {
                        m_callback(false,m_callback_state);
                    }
                }
            }
        } else {
            if(m_pressed) {
                m_pressed = false;
                if(m_callback) {
                    m_callback(false,m_callback_state);
                }
            }
        }
    }
    virtual void on_pressed_changed(button_on_pressed_changed callback, void* state = nullptr) override {
        m_callback = callback;
        m_callback_state = state;
    }
};

core2_button button_a_raw(0);
core2_button button_b_raw(1);
core2_button button_c_raw(2);
core2_button button_d_raw(3);

multi_button button_a(button_a_raw);
multi_button button_b(button_b_raw);
multi_button button_c(button_c_raw);
multi_button button_d(button_d_raw);

// use two 32KB buffers (DMA)
constexpr const size_t lcd_transfer_buffer_size = 32*1024;
static uint8_t* lcd_transfer_buffer1;
static uint8_t* lcd_transfer_buffer2;

// handle to the display
static esp_lcd_panel_handle_t lcd_handle;

using display_t = uix::display;
static display_t lcd;

// tell UIX the DMA transfer is complete
static bool display_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                                esp_lcd_panel_io_event_data_t* edata, 
                                void* user_ctx) {
    lcd.flush_complete();
    return true;
}
// tell the lcd panel api to transfer data via DMA
static void display_on_flush(const rect16& bounds, const void* bmp, void* state) {
    int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 + 1, y2 = bounds.y2 + 1;
    esp_lcd_panel_draw_bitmap(lcd_handle, x1, y1, x2, y2, (void*)bmp);
}


// initialize the screen using the esp panel API
void display_init() {
    lcd_transfer_buffer1 = (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size,MALLOC_CAP_DMA);
    lcd_transfer_buffer2 = (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size,MALLOC_CAP_DMA);
    if(lcd_transfer_buffer1==nullptr||lcd_transfer_buffer2==nullptr) {
        puts("Out of memory allocating transfer buffers");
        while(1) vTaskDelay(5);
    }
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = 18;
    buscfg.mosi_io_num = 23;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = lcd_transfer_buffer_size + 8;

    // Initialize the SPI bus on VSPI (SPI3)
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config;
    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = 15,
    io_config.cs_gpio_num = 5,
    io_config.pclk_hz = 40*1000*1000,
    io_config.lcd_cmd_bits = 8,
    io_config.lcd_param_bits = 8,
    io_config.spi_mode = 0,
    io_config.trans_queue_depth = 10,
    io_config.on_color_trans_done = display_flush_ready;
    // Attach the LCD to the SPI bus
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle);

    lcd_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config;
    memset(&panel_config, 0, sizeof(panel_config));
    panel_config.reset_gpio_num = -1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
#else
    panel_config.color_space = ESP_LCD_COLOR_SPACE_BGR;
#endif
    panel_config.bits_per_pixel = 16;

    // Initialize the LCD configuration
    esp_lcd_new_panel_ili9342(io_handle, &panel_config, &lcd_handle);

    // Reset the display
    esp_lcd_panel_reset(lcd_handle);

    // Initialize LCD panel
    esp_lcd_panel_init(lcd_handle);
    // esp_lcd_panel_io_tx_param(io_handle, LCD_CMD_SLPOUT, NULL, 0);
    //  Swap x and y axis (Different LCD screens may need different options)
    esp_lcd_panel_swap_xy(lcd_handle, false);
    esp_lcd_panel_set_gap(lcd_handle, 0, 0);
    esp_lcd_panel_mirror(lcd_handle, false, false);
    esp_lcd_panel_invert_color(lcd_handle, true);
    // Turn on the screen
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_lcd_panel_disp_on_off(lcd_handle, true);
#else
    esp_lcd_panel_disp_off(lcd_handle, true);
#endif
    lcd.buffer_size(lcd_transfer_buffer_size);
    lcd.buffer1(lcd_transfer_buffer1);
    lcd.buffer2(lcd_transfer_buffer2);
    lcd.on_flush_callback(display_on_flush);
}
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
static void button_a_on_click(int clicks, void* state) {
    if(clicks&1) {
        toggle_units();
    }
}
static void button_b_on_click(int clicks,void* state) {
    while(clicks--) {
        if(current_screen++>1) {
            current_screen = 0;
        }
    }
    switch (current_screen)
    {
    case 1:
        lcd.active_screen(&stat_screen);
        break;
    default: // 0
        if(is_big_speed) {
            lcd.active_screen(&big_screen);
        } else {
            lcd.active_screen(&speed_screen);
        }
        break;
    }
}
static void button_c_on_click(int clicks, void* state) {
    trip_counter_miles = 0;
    trip_counter_kilos = 0;
    timer_counter = 0;
    snprintf(trip_buffer,sizeof(trip_buffer),"% .2f",0.0f);
    trip_label.text(trip_buffer);
    update_timer(0);
    timer_label.text(timer_buffer);
}
static void button_d_on_click(int clicks, void* state) {
    if(clicks&1) {
        if(current_screen==0) {
            if(!is_big_speed) {
                lcd.active_screen(&big_screen);
                is_big_speed = true;
            } else {
                lcd.active_screen(&speed_screen);
                is_big_speed = false;
            }
        }
    }
}
void setup() {
    Serial.begin(115200);
    printf("Arduino version: %d.%d.%d\n",ESP_ARDUINO_VERSION_MAJOR,ESP_ARDUINO_VERSION_MINOR,ESP_ARDUINO_VERSION_PATCH);
    Serial1.begin(9600,SERIAL_8N1,32,33);
    power.initialize(); // do this first
    display_init(); // do this next
    touch.initialize();
    button_a.initialize();
    button_a.on_click(button_a_on_click);
    button_b.initialize();
    button_b.on_click(button_b_on_click);
    button_c.initialize();
    button_c.on_click(button_c_on_click);
    button_d.initialize();
    button_d.on_click(button_d_on_click);
    
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
    lcd.active_screen(&speed_screen);   
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
    lcd.update();
    
    // process "buttons"
    touch.update();  
    button_a.update();
    button_b.update();
    button_c.update();
    button_d.update();
}
