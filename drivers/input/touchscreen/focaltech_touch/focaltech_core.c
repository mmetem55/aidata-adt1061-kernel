#include "focaltech_core.h"
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif

#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   100  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      2600000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

struct fts_ts_data *fts_data;

static void fts_release_all_finger(void);
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);

int fts_reset_proc(int hdelayms)
{
    FTS_FUNC_ENTER();
    if (gpio_is_valid(fts_data->pdata->reset_gpio)) {
        gpio_direction_output(fts_data->pdata->reset_gpio, 0);
        msleep(5);
        gpio_direction_output(fts_data->pdata->reset_gpio, 1);
        if (hdelayms) {
            msleep(hdelayms);
        }
    }
    FTS_FUNC_EXIT();
    return 0;
}

int fts_wait_tp_to_valid(struct i2c_client *client)
{
    int ret = 0;
    int cnt = 0;
    u8 reg_value = 0;
    u8 chip_id = fts_data->ic_info.ids.chip_idh;
    fts_reset_proc(200);
    do {
        ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &reg_value);
        if ((ret < 0) || (reg_value != chip_id)) {
            FTS_DEBUG("TP Not Ready, ReadData = 0x%x", reg_value);
        } else if (reg_value == chip_id) {
            FTS_INFO("TP Ready, Device ID = 0x%x", reg_value);
            return 0;
        }
        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    return -EIO;
}

static int fts_get_chip_types(
    struct fts_ts_data *ts_data,
    u8 id_h, u8 id_l, bool fw_valid)
{
    int i = 0;
    struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
    u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

    if ((0x0 == id_h) || (0x0 == id_l)) {
        FTS_ERROR("id_h/id_l is 0");
        return -EINVAL;
    }

    FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
    for (i = 0; i < ctype_entries; i++) {
        if (VALID == fw_valid) {
            if ((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
                break;
        } else {
            if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
                || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
                || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
                break;
        }
    }

    if (ctype_entries >= 2)
        ts_data->ic_info.ids = ctype[1];

    return 0;
}

static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int ret1 = 0;
    int cnt = 0;
    u8 chip_id[2] = { 0 };
    u8 id_cmd[4] = { 0 };
    u32 id_cmd_len = 0;
    struct i2c_client *client = ts_data->client;

    ts_data->ic_info.is_incell = FTS_CHIP_IDC;
    ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;

    fts_reset_proc(200);

    do {
        ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id[0]);
        ret1 = fts_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id[1]);
        if ((ret < 0) || (ret1 < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_DEBUG("i2c read invalid, read:0x%02x%02x", chip_id[0], chip_id[1]);
        } else {
            ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
            if (!ret)
                break;
            FTS_DEBUG("TP not ready, read:0x%02x%02x", chip_id[0], chip_id[1]);
        }

        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
        FTS_INFO("fw is invalid, need read boot id");
        if (ts_data->ic_info.hid_supported) {
            fts_i2c_hid2std(client);
        }

        id_cmd[0] = FTS_CMD_START1;
        id_cmd[1] = FTS_CMD_START2;
        ret = fts_i2c_write(client, id_cmd, 2);
        if (ret < 0) {
            FTS_ERROR("start cmd write fail");
            return ret;
        }

        msleep(FTS_CMD_START_DELAY);
        id_cmd[0] = FTS_CMD_READ_ID;
        id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
        if (ts_data->ic_info.is_incell)
            id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
        else
            id_cmd_len = FTS_CMD_READ_ID_LEN;
        ret = fts_i2c_read(client, id_cmd, id_cmd_len, chip_id, 2);
        if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
            FTS_ERROR("read boot id fail");
            return -EIO;
        }
        ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
        if (ret < 0) {
            FTS_ERROR("can't get ic informaton");
            return ret;
        }
    }

    FTS_INFO("get ic information, chip id = 0x%02x%02x",
             ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

    return 0;
}

void fts_tp_state_recovery(struct i2c_client *client)
{
    FTS_FUNC_ENTER();
    fts_wait_tp_to_valid(client);
    fts_ex_mode_recovery(client);
    #if FTS_GESTURE_EN
    fts_gesture_recovery(client);
    #endif
    FTS_FUNC_EXIT();
}

void fts_irq_disable(void)
{
    unsigned long irqflags;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if (!fts_data->irq_disabled) {
        disable_irq_nosync(fts_data->irq);
        fts_data->irq_disabled = true;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
    unsigned long irqflags = 0;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_data->irq_lock, irqflags);

    if (fts_data->irq_disabled) {
        enable_irq(fts_data->irq);
        fts_data->irq_disabled = false;
    }

    spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

#if FTS_POWER_SOURCE_CUST_EN
static int fts_power_source_init(struct fts_ts_data *data)
{
    int ret = 0;

    FTS_FUNC_ENTER();

    data->vdd = regulator_get(&data->client->dev, "power");
    if (IS_ERR(data->vdd)) {
        ret = PTR_ERR(data->vdd);
        FTS_ERROR("get vdd regulator failed,ret=%d", ret);
        return ret;
    }

    if (regulator_count_voltages(data->vdd) > 0) {
        ret = regulator_set_voltage(data->vdd, FTS_VTG_MIN_UV, FTS_VTG_MAX_UV);
        if (ret) {
            FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
            goto err_set_vtg_vdd;
        }
    }

    data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
    if (IS_ERR(data->vcc_i2c)) {
        ret = PTR_ERR(data->vcc_i2c);
        FTS_ERROR("ret vcc_i2c regulator failed,ret=%d", ret);
        goto err_get_vcc;
    }

    if (regulator_count_voltages(data->vcc_i2c) > 0) {
        ret = regulator_set_voltage(data->vcc_i2c, FTS_I2C_VTG_MIN_UV, FTS_I2C_VTG_MAX_UV);
        if (ret) {
            FTS_ERROR("vcc_i2c regulator set_vtg failed ret=%d", ret);
            goto err_set_vtg_vcc;
        }
    }

    FTS_FUNC_EXIT();
    return 0;

    err_set_vtg_vcc:
    regulator_put(data->vcc_i2c);
    err_get_vcc:
    if (regulator_count_voltages(data->vdd) > 0)
        regulator_set_voltage(data->vdd, 0, FTS_VTG_MAX_UV);
    err_set_vtg_vdd:
    regulator_put(data->vdd);

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_power_source_release(struct fts_ts_data *data)
{
    if (regulator_count_voltages(data->vdd) > 0)
        regulator_set_voltage(data->vdd, 0, FTS_VTG_MAX_UV);
    regulator_put(data->vdd);

    if (regulator_count_voltages(data->vcc_i2c) > 0)
        regulator_set_voltage(data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
    regulator_put(data->vcc_i2c);

    return 0;
}

static int fts_power_source_ctrl(struct fts_ts_data *data, int enable)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    if (enable) {
        if (data->power_disabled) {
            FTS_DEBUG("regulator enable !");
            gpio_direction_output(fts_data->pdata->reset_gpio, 0);
            msleep(1);
            ret = regulator_enable(data->vdd);
            if (ret) {
                FTS_ERROR("enable vdd regulator failed,ret=%d", ret);
            }

            ret = regulator_enable(data->vcc_i2c);
            if (ret) {
                FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
            }
            data->power_disabled = false;
        }
    } else {
        if (!data->power_disabled) {
            FTS_DEBUG("regulator disable !");
            gpio_direction_output(fts_data->pdata->reset_gpio, 0);
            msleep(1);
            ret = regulator_disable(data->vdd);
            if (ret) {
                FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
            }
            ret = regulator_disable(data->vcc_i2c);
            if (ret) {
                FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
            }
            data->power_disabled = true;
        }
    }

    FTS_FUNC_EXIT();
    return ret;
}
#endif

static void fts_release_all_finger(void)
{
    struct input_dev *input_dev = fts_data->input_dev;
    #if FTS_MT_PROTOCOL_B_EN
    u32 finger_count = 0;
    #endif

    FTS_FUNC_ENTER();
    mutex_lock(&fts_data->report_mutex);
    #if FTS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < fts_data->pdata->max_touch_number; finger_count++) {
        input_mt_slot(input_dev, finger_count);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
    #else
    input_mt_sync(input_dev);
    #endif
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_sync(input_dev);

    mutex_unlock(&fts_data->report_mutex);
    FTS_FUNC_EXIT();
}

static int fts_input_report_key(struct fts_ts_data *data, int index)
{
    u32 ik;
    int id = data->events[index].id;
    int x = data->events[index].x;
    int y = data->events[index].y;
    int flag = data->events[index].flag;
    u32 key_num = data->pdata->key_number;

    if (!KEY_EN(data)) {
        return -EINVAL;
    }
    for (ik = 0; ik < key_num; ik++) {
        if (TOUCH_IN_KEY(x, data->pdata->key_x_coords[ik])) {
            if (EVENT_DOWN(flag)) {
                data->key_down = true;
                input_report_key(data->input_dev, data->pdata->keys[ik], 1);
                FTS_DEBUG("Key%d(%d, %d) DOWN!", ik, x, y);
            } else {
                data->key_down = false;
                input_report_key(data->input_dev, data->pdata->keys[ik], 0);
                FTS_DEBUG("Key%d(%d, %d) Up!", ik, x, y);
            }
            return 0;
        }
    }

    FTS_ERROR("invalid touch for key, [%d](%d, %d)", id, x, y);
    return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
    int i = 0;
    int uppoint = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 max_touch_num = data->pdata->max_touch_number;
    u32 key_y_coor = data->pdata->key_y_coord;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (KEY_EN(data) && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
            fts_input_report_key(data, i);
            continue;
        }

        if (events[i].id >= max_touch_num)
            break;

        va_reported = true;
        input_mt_slot(data->input_dev, events[i].id);

        if (EVENT_DOWN(events[i].flag)) {
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

            #if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
            #endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            touchs |= BIT(events[i].id);
            data->touchs |= BIT(events[i].id);

            FTS_DEBUG("[B]P%d(0x%x, 0x%x)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x,
                      events[i].y, events[i].p, events[i].area);
        } else {
            uppoint++;
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            data->touchs &= ~BIT(events[i].id);
            FTS_DEBUG("[B]P%d UP!", events[i].id);
        }
    }

    if (unlikely(data->touchs ^ touchs)) {
        for (i = 0; i < max_touch_num; i++)  {
            if (BIT(i) & (data->touchs ^ touchs)) {
                FTS_DEBUG("[B]P%d UP!", i);
                va_reported = true;
                input_mt_slot(data->input_dev, i);
                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            }
        }
    }
    data->touchs = touchs;

    if (va_reported) {
        if (EVENT_NO_DOWN(data) || (!touchs)) {
            FTS_DEBUG("[B]Points All Up!");
            input_report_key(data->input_dev, BTN_TOUCH, 0);
        } else {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}
#endif

static int fts_read_touchdata(struct fts_ts_data *data)
{
    int ret = 0;
    int i = 0;
    u8 pointid;
    int base;
    struct ts_event *events = data->events;
    int max_touch_num = data->pdata->max_touch_number;
    u8 *buf = data->point_buf;
    struct i2c_client *client = data->client;

    data->point_num = 0;
    data->touch_point = 0;

    memset(buf, 0xFF, data->pnt_buf_size);
    buf[0] = 0x00;

    ret = fts_i2c_read(data->client, buf, 1, buf, data->pnt_buf_size);
    if (ret < 0) {
        FTS_ERROR("read touchdata failed, ret:%d", ret);
        return ret;
    }
    data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;

    if (data->ic_info.is_incell) {
        if ((data->point_num == 0x0F) && (buf[1] == 0xFF) && (buf[2] == 0xFF)
            && (buf[3] == 0xFF) && (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
            FTS_INFO("touch buff is 0xff, need recovery state");
        fts_tp_state_recovery(client);
        return -EIO;
            }
    }

    if (data->point_num > max_touch_num) {
        FTS_INFO("invalid point_num(%d)", data->point_num);
        return -EIO;
    }

    for (i = 0; i < max_touch_num; i++) {
        base = FTS_ONE_TCH_LEN * i;

        pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
        if (pointid >= FTS_MAX_ID)
            break;
        else if (pointid >= max_touch_num) {
            FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
            return -EINVAL;
        }

        data->touch_point++;

        events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
        (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
        events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
        (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
        events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
        events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
        events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
        events[i].p =  buf[FTS_TOUCH_PRE_POS + base];

        if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
            FTS_INFO("abnormal touch data from fw");
            return -EIO;
        }
    }
    if (data->touch_point == 0) {
        FTS_INFO("no touch point information");
        return -EIO;
    }

    return 0;
}

static void fts_report_event(struct fts_ts_data *data)
{
    #if FTS_MT_PROTOCOL_B_EN
    fts_input_report_b(data);
    #endif
}

static irqreturn_t fts_ts_interrupt(int irq, void *data)
{
    int ret = 0;
    struct fts_ts_data *ts_data = (struct fts_ts_data *)data;

    if (!ts_data) {
        FTS_ERROR("[INTR]: Invalid fts_ts_data");
        return IRQ_HANDLED;
    }

    ret = fts_read_touchdata(ts_data);
    if (ret == 0) {
        mutex_lock(&ts_data->report_mutex);
        fts_report_event(ts_data);
        mutex_unlock(&ts_data->report_mutex);
    }

    return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

    ts_data->irq = gpio_to_irq(pdata->irq_gpio);
    FTS_INFO("irq in ts_data:%d irq in client:%d", ts_data->irq, ts_data->client->irq);

    if (0 == pdata->irq_gpio_flags)
        pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING;

    ret = request_threaded_irq(ts_data->irq, NULL, fts_ts_interrupt,
                               pdata->irq_gpio_flags | IRQF_ONESHOT,
                               ts_data->client->name, ts_data);
    return ret;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;
    struct input_dev *input_dev;
    int point_num;

    FTS_FUNC_ENTER();

    input_dev = input_allocate_device();
    if (!input_dev) {
        FTS_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }

    input_dev->name = FTS_DRIVER_NAME;
    input_dev->id.bustype = BUS_I2C;
    input_dev->dev.parent = &ts_data->client->dev;

    input_set_drvdata(input_dev, ts_data);

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    #if FTS_MT_PROTOCOL_B_EN
    input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
    #endif

    input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->y_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
    #if FTS_REPORT_PRESSURE_EN
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
    #endif

    point_num = pdata->max_touch_number;
    ts_data->pnt_buf_size = point_num * FTS_ONE_TCH_LEN + 3;
    ts_data->point_buf = (u8 *)kzalloc(ts_data->pnt_buf_size, GFP_KERNEL);
    if (!ts_data->point_buf) {
        ret = -ENOMEM;
        goto err_point_buf;
    }

    ts_data->events = (struct ts_event *)kzalloc(point_num * sizeof(struct ts_event), GFP_KERNEL);
    if (!ts_data->events) {
        ret = -ENOMEM;
        goto err_event_buf;
    }

    ret = input_register_device(input_dev);
    if (ret) {
        goto err_input_reg;
    }

    ts_data->input_dev = input_dev;
    FTS_FUNC_EXIT();
    return 0;

    err_input_reg:
    kfree_safe(ts_data->events);
    err_event_buf:
    kfree_safe(ts_data->point_buf);
    err_point_buf:
    input_set_drvdata(input_dev, NULL);
    input_free_device(input_dev);
    return ret;
}

static int fts_gpio_configure(struct fts_ts_data *data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    if (gpio_is_valid(data->pdata->irq_gpio)) {
        ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]irq gpio request failed");
            goto err_irq_gpio_req;
        }

        ret = gpio_direction_input(data->pdata->irq_gpio);
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for irq gpio failed");
            goto err_irq_gpio_dir;
        }
    }

    if (gpio_is_valid(data->pdata->reset_gpio)) {
        ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
        if (ret) {
            FTS_ERROR("[GPIO]reset gpio request failed");
            goto err_irq_gpio_dir;
        }

        ret = gpio_direction_output(data->pdata->reset_gpio, 1);
        if (ret) {
            FTS_ERROR("[GPIO]set_direction for reset gpio failed");
            goto err_reset_gpio_dir;
        }
    }

    FTS_FUNC_EXIT();
    return 0;

    err_reset_gpio_dir:
    if (gpio_is_valid(data->pdata->reset_gpio))
        gpio_free(data->pdata->reset_gpio);
    err_irq_gpio_dir:
    if (gpio_is_valid(data->pdata->irq_gpio))
        gpio_free(data->pdata->irq_gpio);
    err_irq_gpio_req:
    FTS_FUNC_EXIT();
    return ret;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
    struct device_node *np = dev->of_node;

    FTS_FUNC_ENTER();

    pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
    pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
    pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
    pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;

    /* DTS standart isimleri: rst-gpios ve irq-gpios okuma */
    pdata->reset_gpio = of_get_named_gpio_flags(np, "rst-gpios", 0, &pdata->reset_gpio_flags);
    if (!gpio_is_valid(pdata->reset_gpio)) {
        pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio", 0, &pdata->reset_gpio_flags);
    }

    pdata->irq_gpio = of_get_named_gpio_flags(np, "irq-gpios", 0, &pdata->irq_gpio_flags);
    if (!gpio_is_valid(pdata->irq_gpio)) {
        pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio", 0, &pdata->irq_gpio_flags);
    }

    if (of_property_read_u32(np, "focaltech,max-touch-number", &pdata->max_touch_number)) {
        pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
    }

    FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
             pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);

    FTS_FUNC_EXIT();
    return 0;
}

static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct fts_ts_platform_data *pdata;
    struct fts_ts_data *ts_data;

    FTS_FUNC_ENTER();
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        FTS_ERROR("I2C not supported");
        return -ENODEV;
    }

    if (client->dev.of_node) {
        pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
        if (!pdata) {
            return -ENOMEM;
        }
        ret = fts_parse_dt(&client->dev, pdata);
        if (ret)
            FTS_ERROR("[DTS]DT parsing failed");
    } else {
        pdata = client->dev.platform_data;
    }

    if (!pdata) {
        return -EINVAL;
    }

    ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        return -ENOMEM;
    }

    fts_data = ts_data;
    ts_data->client = client;
    ts_data->pdata = pdata;
    i2c_set_clientdata(client, ts_data);

    ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");

    spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->report_mutex);

    ret = fts_input_init(ts_data);
    if (ret) {
        goto err_input_init;
    }

    ret = fts_gpio_configure(ts_data);
    if (ret) {
        goto err_gpio_config;
    }

    #if (!FTS_CHIP_IDC)
    fts_reset_proc(200);
    #endif

    /* Çip algılanamazsa (başka bir ekran takılıysa) ENODEV dönerek sıradaki sürücülere geçer */
    ret = fts_get_ic_information(ts_data);
    if (ret) {
        FTS_ERROR("Not FocalTech IC, skipping driver...");
        ret = -ENODEV;
        goto err_irq_req;
    }

    ret = fts_ex_mode_init(client);
    if (ret) {
        FTS_ERROR("init glove/cover/charger fail");
    }

    ret = fts_irq_registration(ts_data);
    if (ret) {
        FTS_ERROR("request irq failed");
        goto err_irq_req;
    }

    FTS_FUNC_EXIT();
    return 0;

    err_irq_req:
    if (gpio_is_valid(pdata->reset_gpio))
        gpio_free(pdata->reset_gpio);
    if (gpio_is_valid(pdata->irq_gpio))
        gpio_free(pdata->irq_gpio);
    err_gpio_config:
    kfree_safe(ts_data->point_buf);
    kfree_safe(ts_data->events);
    input_unregister_device(ts_data->input_dev);
    err_input_init:
    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
    devm_kfree(&client->dev, ts_data);

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_ts_remove(struct i2c_client *client)
{
    struct fts_ts_data *ts_data = i2c_get_clientdata(client);

    FTS_FUNC_ENTER();
    fts_ex_mode_exit(client);

    free_irq(ts_data->irq, ts_data);
    input_unregister_device(ts_data->input_dev);

    if (gpio_is_valid(ts_data->pdata->reset_gpio))
        gpio_free(ts_data->pdata->reset_gpio);

    if (gpio_is_valid(ts_data->pdata->irq_gpio))
        gpio_free(ts_data->pdata->irq_gpio);

    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);

    kfree_safe(ts_data->point_buf);
    kfree_safe(ts_data->events);
    devm_kfree(&client->dev, ts_data);

    FTS_FUNC_EXIT();
    return 0;
}

static int fts_ts_suspend(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_FUNC_ENTER();
    if (ts_data->suspended)
        return 0;

    fts_irq_disable();
    fts_i2c_write_reg(ts_data->client, FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP_VALUE);

    ts_data->suspended = true;
    FTS_FUNC_EXIT();
    return 0;
}

static int fts_ts_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    FTS_FUNC_ENTER();
    if (!ts_data->suspended)
        return 0;

    fts_release_all_finger();
    fts_reset_proc(200);
    fts_tp_state_recovery(ts_data->client);

    ts_data->suspended = false;
    fts_irq_enable();

    FTS_FUNC_EXIT();
    return 0;
}

static const struct dev_pm_ops focaltech_ts_pm_ops = {
    .suspend = fts_ts_suspend,
    .resume = fts_ts_resume,
};

static const struct i2c_device_id fts_ts_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, fts_ts_id);

/* DTS tablosuna "elink,focaltech" eklendi */
static struct of_device_id fts_match_table[] = {
    { .compatible = "elink,focaltech", },
    { .compatible = "focaltech,fts", },
    { },
};

static struct i2c_driver fts_ts_driver = {
    .probe = fts_ts_probe,
    .remove = fts_ts_remove,
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = fts_match_table,
    },
    .id_table = fts_ts_id,
};

static int __init fts_ts_init(void)
{
    return i2c_add_driver(&fts_ts_driver);
}

static void __exit fts_ts_exit(void)
{
    i2c_del_driver(&fts_ts_driver);
}

module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
