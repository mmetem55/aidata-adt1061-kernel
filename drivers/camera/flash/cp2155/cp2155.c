#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/property.h>

#include "cp2155.h"


static void cp2155_torch_brightness_set(struct led_classdev *led_cdev,
                                        enum led_brightness brightness)
{
    struct cp2155_data *data = container_of(led_cdev, struct cp2155_data, torch_led);

    if (brightness == LED_OFF)
        gpiod_set_value_cansleep(data->torch_gpio, 0);
    else
        gpiod_set_value_cansleep(data->torch_gpio, 1);
}


static void cp2155_flash_brightness_set(struct led_classdev *led_cdev,
                                        enum led_brightness brightness)
{
    struct cp2155_data *data = container_of(led_cdev, struct cp2155_data, flash_led);

    if (brightness == LED_OFF)
        gpiod_set_value_cansleep(data->flash_gpio, 0);
    else
        gpiod_set_value_cansleep(data->flash_gpio, 1);
}

static int cp2155_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct cp2155_data *data;
    int ret;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->pdev = pdev;
    platform_set_drvdata(pdev, data);

    device_property_read_u32(dev, "torch-led-idx", &data->torch_led_idx);
    device_property_read_u32(dev, "flash-ic", &data->flash_ic);

    dev_info(dev, "CP2155 Flash IC detected (IC: 0x%x, IDX: 0x%x)\n",
             data->flash_ic, data->torch_led_idx);

    data->flash_gpio = devm_gpiod_get(dev, "flash-en", GPIOD_OUT_LOW);
    if (IS_ERR(data->flash_gpio)) {
        dev_err(dev, "Failed to get flash-en GPIO\n");
        return PTR_ERR(data->flash_gpio);
    }

    data->torch_gpio = devm_gpiod_get(dev, "flash-torch-en", GPIOD_OUT_LOW);
    if (IS_ERR(data->torch_gpio)) {
        dev_err(dev, "Failed to get flash-torch-en GPIO\n");
        return PTR_ERR(data->torch_gpio);
    }

    data->torch_led.name = "cp2155:torch";
    data->torch_led.max_brightness = 1;
    data->torch_led.brightness_set = cp2155_torch_brightness_set;

    ret = devm_led_classdev_register(dev, &data->torch_led);
    if (ret) {
        dev_err(dev, "Failed to register torch LED device\n");
        return ret;
    }

    data->flash_led.name = "cp2155:flash";
    data->flash_led.max_brightness = 1;
    data->flash_led.brightness_set = cp2155_flash_brightness_set;

    ret = devm_led_classdev_register(dev, &data->flash_led);
    if (ret) {
        dev_err(dev, "Failed to register flash LED device\n");
        return ret;
    }

    dev_info(dev, "CP2155 LED driver probed successfully.\n");
    return 0;
}

static int cp2155_remove(struct platform_device *pdev)
{
    struct cp2155_data *data = platform_get_drvdata(pdev);

    gpiod_set_value_cansleep(data->torch_gpio, 0);
    gpiod_set_value_cansleep(data->flash_gpio, 0);

    return 0;
}

static const struct of_device_id cp2155_match_table[] = {
    { .compatible = "elink,cp2155", },
    { },
};
MODULE_DEVICE_TABLE(of, cp2155_match_table);

static struct platform_driver cp2155_driver = {
    .probe      = cp2155_probe,
    .remove     = cp2155_remove,
    .driver     = {
        .name   = "cp2155",
        .of_match_table = cp2155_match_table,
    },
};

module_platform_driver(cp2155_driver);

MODULE_AUTHOR("mmetem55");
MODULE_DESCRIPTION("CP2155 Charge Pump Flash LED Driver");
MODULE_LICENSE("GPL v2");
