#define LGFX_USE_V1

#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>


#define screenWidth  800
#define screenHeight 480


class LGFX : public lgfx::LGFX_Device
{

public:

    lgfx::Bus_RGB _bus_instance;
    lgfx::Panel_RGB _panel_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_GT911 _touch_instance;


    LGFX(void)
    {

        // Configuration écran RGB
        {
            auto cfg = _panel_instance.config();

            cfg.memory_width  = screenWidth;
            cfg.memory_height = screenHeight;

            cfg.panel_width  = screenWidth;
            cfg.panel_height = screenHeight;

            cfg.offset_x = 0;
            cfg.offset_y = 0;

            _panel_instance.config(cfg);
        }


        // Configuration bus RGB565
        {
            auto cfg = _bus_instance.config();

            cfg.panel = &_panel_instance;


            // BLUE
            cfg.pin_d0 = GPIO_NUM_14;
            cfg.pin_d1 = GPIO_NUM_38;
            cfg.pin_d2 = GPIO_NUM_18;
            cfg.pin_d3 = GPIO_NUM_17;
            cfg.pin_d4 = GPIO_NUM_10;


            // GREEN
            cfg.pin_d5  = GPIO_NUM_39;
            cfg.pin_d6  = GPIO_NUM_0;
            cfg.pin_d7  = GPIO_NUM_45;
            cfg.pin_d8  = GPIO_NUM_48;
            cfg.pin_d9  = GPIO_NUM_47;
            cfg.pin_d10 = GPIO_NUM_21;


            // RED
            cfg.pin_d11 = GPIO_NUM_1;
            cfg.pin_d12 = GPIO_NUM_2;
            cfg.pin_d13 = GPIO_NUM_42;
            cfg.pin_d14 = GPIO_NUM_41;
            cfg.pin_d15 = GPIO_NUM_40;



            // Synchronisation RGB

            cfg.pin_henable = GPIO_NUM_5;   // DE
            cfg.pin_vsync   = GPIO_NUM_3;
            cfg.pin_hsync   = GPIO_NUM_46;
            cfg.pin_pclk    = GPIO_NUM_7;


            // fréquence pixel

            cfg.freq_write = 16000000;



            // Timing RGB

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 8;


            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 8;


            cfg.pclk_active_neg = 1;
            cfg.de_idle_high = 0;
            cfg.pclk_idle_high = 0;



            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }



        // Backlight
        {
            auto cfg = _light_instance.config();

            cfg.pin_bl = -1;

            _light_instance.config(cfg);

            _panel_instance.light(&_light_instance);
        }



        // Touch GT911

        {
            auto cfg = _touch_instance.config();

            cfg.x_min = 0;
            cfg.x_max = 799;

            cfg.y_min = 0;
            cfg.y_max = 479;


            cfg.pin_int = -1;
            cfg.pin_rst = -1;


            cfg.bus_shared = true;


            cfg.offset_rotation = 0;


            cfg.i2c_port = I2C_NUM_0;


            // I2C Waveshare

            cfg.pin_sda = GPIO_NUM_8;
            cfg.pin_scl = GPIO_NUM_9;


            cfg.freq = 400000;


            cfg.i2c_addr = 0x14;


            _touch_instance.config(cfg);

            _panel_instance.setTouch(&_touch_instance);
        }


        setPanel(&_panel_instance);

    }

};


LGFX tft;