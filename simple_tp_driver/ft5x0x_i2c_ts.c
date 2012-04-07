#include <linux/irq.h>
#include <asm/mach/irq.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <linux/earlysuspend.h>

#include "ft5x0x_i2c_ts.h"

struct ft_priv_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct touch_event_work;
	struct workqueue_struct *ts_workqueue;
	struct early_suspend early_suspend;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft_early_suspend(struct early_suspend *h);
static void ft_late_resume(struct early_suspend *h);
#endif


static int ft_i2c_rxdata(struct ft_priv_data *priv_data, u8 *rxdata, int length)
{
	int ret;
	struct i2c_msg msg;
	struct i2c_client *client = priv_data->client;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = rxdata;
	msg.scl_rate = FT5X0X_I2C_SPEED;

	ret = i2c_transfer(client->adapter, &msg, 1);

	if(ret == 0){
		pr_err("msg %s line:%d i2c write error: %d\n", __func__, __LINE__,ret);
		return -EBUSY;
	}else if(ret < 0){
		pr_err("msg %s line:%d i2c write error: %d\n", __func__, __LINE__,ret);
		return ret;
	}

	msg.addr = client->addr;
	msg.flags = I2C_M_RD;
	msg.len = length;
	msg.buf = rxdata;
	msg.scl_rate = FT5X0X_I2C_SPEED;
	ret = i2c_transfer(client->adapter, &msg, 1);

	if(ret == 0){
		pr_err("msg %s line:%d i2c write error: %d\n", __func__, __LINE__,ret);
		return -EBUSY;
	}else if(ret < 0){
		pr_err("msg %s line:%d i2c write error: %d\n", __func__, __LINE__,ret);
		return ret;
	}

	return ret;
}

static int ft_read_data(struct ft_priv_data *priv_data)
{
	u8 buf[32] = {0};
        int fingers = 0;
        int track_id;
        int i;
        int ret;

        /* how many fingers touching */
	buf[0] = 2;
	ret = fts_i2c_rxdata(priv_data, buf, 1);
	if (ret > 0) 
		fingers = buf[0] & 0xf;

	i = 0;
	do {
                /* get point info */
		buf[0] = 3 + 6 * i;
		ret = ft_i2c_rxdata(buf, 6);
		if (ret > 0) {
			/* get track id */
			track_id = buf[2] >> 4;  
                        if (track_id >= 0 && track_id < CFG_MAX_POINT_NUM) {
				int tmp, x, y, t, w;
				int touch_event;
                                
				tmp = buf[0] & 0x0f;
				tmp = tmp << 8;
				tmp = tmp | buf[1];
				x = tmp; 

				tmp = (buf[2])& 0x0f;
				tmp = tmp << 8;
				tmp = tmp | buf[3];
				y = tmp;
				
				touch_event = buf[0] >> 6;
				if (touch_event == 0) { /* down event */
					t = 255;
					w = 15;
				} else if (touch_event == 1) { /* up event */
					t = w = 0;
				} else if (touch_event == 2) { /* move event */
					t = 255;
					w = 15;
				} else
					continue;  

				input_report_abs(priv_data->input_dev, ABS_MT_TRACKING_ID, track_id);
				input_report_abs(priv_data->input_dev, ABS_MT_TOUCH_MAJOR, t);
				input_report_abs(priv_data->input_dev, ABS_MT_WIDTH_MAJOR, w);
				input_report_abs(priv_data->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(priv_data->input_dev, ABS_MT_POSITION_Y, y);
				
				input_mt_sync(priv_data->input_dev);
			}
		} 
		
		i++;
	} while (i < fingers);

	input_sync(priv_data->input_dev);

	return 0;
}

static void ft_work_func(struct work_struct *work)
{
	struct ft_priv_data *priv_data = container_of(work, struct ft_priv_data, touch_event_work);
	
	ft_read_data(priv_data);    
	
	enable_irq(priv_data->client->irq);
}

static irqreturn_t ft_irq(int irq, void *dev_id)
{
	struct ft_priv_data *priv_data = dev_id;
	
	if (!work_pending(&priv_data->touch_event_work)) {
		disable_irq_nosync(irq);
		queue_work(priv_data->ts_workqueue, &priv_data->touch_event_work);
	}

	return IRQ_HANDLED;
}

void ft_set_standby(struct i2c_client *client, int enable)
{
	struct ft_platform_data *mach_info = client->dev.platform_data;
	unsigned pwr_pin = mach_info->pwr_pin;
	unsigned pwr_on_value = mach_info->pwr_on_value;
	unsigned reset_pin = mach_info->reset_pin;
	unsigned reset_value = mach_info->reset_value;

	if (pwr_pin != INVALID_GPIO) {
		gpio_direction_output(pwr_pin, 0);
		gpio_set_value(pwr_pin, enable ? pwr_on_value : !pwr_on_value);				
	}
        
	if (reset_pin != INVALID_GPIO) {
		gpio_direction_output(reset_pin, enable ? reset_value : !reset_value);
		gpio_set_value(reset_pin, enable ? reset_value : !reset_value);				
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft_early_suspend(struct early_suspend *h)
{
	struct ft_priv_data *data = i2c_get_clientdata(this_client);


	cancel_work_sync(&data->touch_event_work);

	disable_irq(this_client->irq);


	ft_set_standby(this_client, 0);

	return;
}

static void ft_late_resume(struct early_suspend *h)
{
	struct ft_priv_data *data = i2c_get_clientdata(this_client);

	ft_set_standby(this_client, 1);

	enable_irq(this_client->irq);

	return ;
}
#else
#define ft_early_suspend NULL
#define ft_late_resume NULL
#endif

static int ft_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft_priv_data *priv_data;
	struct input_dev *input_dev;
	int err = 0;
	int _sui_irq_num;
	unsigned char reg_value;
	unsigned char reg_version;
	int i;

	struct ft_platform_data *pdata = client->dev.platform_data;


	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
                dev_err(&client->dev, "ft_probe: i2c_check_functionality error\n");
		goto exit_check_functionality_failed;
	}

	if (pdata->init_platform_hw) {
		err = pdata->init_platform_hw();
		if (err < 0) {
                        dev_err(&client->dev, "ft_probe: init_platform_hw error\n");  
		        goto exit_init_platform_hw_failed;
                }
	}

	this_client = client;
	

	client->irq = gpio_to_irq(client->irq);
	_sui_irq_num = client->irq;

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data)    {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	priv_data->client = client;
	i2c_set_clientdata(client, priv_data);

	INIT_WORK(&priv_data->touch_event_work, ft_work_func);

	priv_data->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!priv_data->ts_workqueue) {
		err = -ESRCH;
                dev_err(&client->dev, "ft_probe: create_singlethread_workqueue error\n");  
		goto exit_create_singlethread;
	}


	err = request_irq(client->irq, ft_irq, GPIOEdgelFalling, client->dev.driver->name, priv_data);
	if (err < 0) {
                dev_err(&client->dev, "ft_probe: request_irq error\n");  
		goto exit_irq_request_failed;
        }
	
	disable_irq(client->irq);


	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
                dev_err(&client->dev, "ft_probe: input_allocate_device error\n");
		goto exit_input_dev_alloc_failed;
	}

	priv_data->input_dev = input_dev;

	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

	input_set_abs_params(input_dev,
			ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_TRACKING_ID, 0, CFG_MAX_POINT_NUM, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->name = FT5X0X_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0xdead;
	input_dev->id.product = 0xbeef;
	input_dev->id.version = 10427;

	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
				"ft_probe: failed to register input device: %s\n",
				dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	priv_data->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	priv_data->early_suspend.suspend = ft_early_suspend;
	priv_data->early_suspend.resume = ft_late_resume;
	register_early_suspend(&priv_data->early_suspend);
#endif

	enable_irq(client->irq);    

	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);

exit_input_dev_alloc_failed:
	free_irq(client->irq, priv_data);

exit_irq_request_failed:
	cancel_work_sync(&priv_data->touch_event_work);
	destroy_workqueue(priv_data->ts_workqueue);

exit_create_singlethread:
	i2c_set_clientdata(client, NULL);
	kfree(priv_data);

exit_alloc_data_failed:
	if (pdata->exit_platform_hw)
		pdata->exit_platform_hw();

exit_init_platform_hw_failed:
exit_check_functionality_failed:
	return err;
}

static int __devexit ft_remove(struct i2c_client *client)
{
	struct ft_priv_data *priv_data;
	
	priv_data = (struct ft_priv_data *)i2c_get_clientdata(client);
	
	free_irq(client->irq, priv_data);
	
	input_unregister_device(priv_data->input_dev);
	
	cancel_work_sync(&priv_data->touch_event_work);
	destroy_workqueue(priv_data->ts_workqueue);
	
	i2c_set_clientdata(client, NULL);

	kfree(priv_data);
	
	return 0;
}

static const struct i2c_device_id id[] = {
	{FT5X0X_NAME, 0},
	{}
};


MODULE_DEVICE_TABLE(i2c, id);

static struct i2c_driver ft_driver = {
	.probe	= ft_probe,
	.remove = ft_remove,
	.id_table = id,
	.driver = {
		.name = FT5X0X_NAME,
	},
};

static void __init ft_initasync(void *unused, async_cookie_t cookie)
{
	i2c_add_driver(&ft_driver);
}

static int __init ft_init(void)
{
	async_schedule(ft_initasync, NULL);
	return 0;
}

static void __exit ft_exit(void)
{
	i2c_del_driver(&ft_driver);
}

module_init(ft_init);
module_exit(ft_exit);

MODULE_LICENSE("GPL");
