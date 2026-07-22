/* CHİPHOMER CP2155 FLASH İC DRİVER */
/* mmetem55 */

#ifndef __CP2155_H__
#define __CP2155_H__

#include <linux/leds.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>

struct cp2155_data {
    struct platform_device *pdev;

    struct led_classdev torch_led;
    struct led_classdev flash_led;

    struct gpio_desc *flash_gpio;
    struct gpio_desc *torch_gpio;

    u32 torch_led_idx;
    u32 flash_ic;
};

#endif /* __CP2155_H__ */
