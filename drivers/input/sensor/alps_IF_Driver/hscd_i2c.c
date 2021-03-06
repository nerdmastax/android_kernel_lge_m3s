#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* LGE_CHANGE_S */
#include <mach/board_lge.h> /* platform data */
static struct ecom_platform_data *ecom_pdata;
/* LGE_CHANGE_E */

#define I2C_RETRY_DELAY		5
#define I2C_RETRIES		5

/* LGE_CHANGE_S, dont need to use */
#if 0
#define I2C_HSCD_ADDR (0x0c)	/* 000 1100	*/
#define I2C_BUS_NUMBER	2
#endif
/* LGE_CHANGE_E */

#define HSCD_DRIVER_NAME "hscd_i2c"

#define HSCD_XOUT		0x10
#define HSCD_YOUT		0x12
#define HSCD_ZOUT		0x14
#define HSCD_XOUT_H		0x11
#define HSCD_XOUT_L		0x10
#define HSCD_YOUT_H		0x13
#define HSCD_YOUT_L		0x12
#define HSCD_ZOUT_H		0x15
#define HSCD_ZOUT_L		0x14

#define HSCD_STATUS		0x18
#define HSCD_CTRL1		0x1b
#define HSCD_CTRL2		0x1c
#define HSCD_CTRL3		0x1d
#ifdef HSCDTD002A
	#define HSCD_CTRL4	0x28
#endif

/* #define ALPS_DEBUG 1 */

static struct i2c_driver hscd_driver;
static struct i2c_client *client_hscd = NULL;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hscd_early_suspend_handler;
#endif

static atomic_t flgEna;
static atomic_t delay;

static int hscd_i2c_readm(char *rxData, int length)
{
	int err;
	int tries = 0;
/* for debugging */
	struct i2c_msg msgs[2];
	if(client_hscd != NULL){
		if(rxData!=NULL){
//			printk("[HSCD] hscd_i2c_readm param check(addr %x, length %d, rxData %x)\n ", client_hscd->addr,length,rxData[0]);
		}else{
			printk("[HSCD] hscd_i2c_readm rxData is NULL\n");
			return -EIO;
		}
	}else{
		printk("[HSCD] hscd_i2c_readm client_hscd is NULL\n");
		return -EIO;
	}
	msgs[0].addr = client_hscd->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = rxData;
	
	msgs[1].addr = client_hscd->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = length;
	msgs[1].buf = rxData;	
#if 0
	struct i2c_msg msgs[] = {
		{
		 .addr = client_hscd->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = rxData,
		 },
		{
		 .addr = client_hscd->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};
#endif
	do {
		err = i2c_transfer(client_hscd->adapter, msgs, 2);
	} while ((err != 2) && (++tries < I2C_RETRIES));
	
	if (err != 2) {
		dev_err(&client_hscd->adapter->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int hscd_i2c_writem(char *txData, int length)
{
	int err;
	int tries = 0;
#ifdef ALPS_DEBUG
	int i;
#endif
#if 0
	struct i2c_msg msg[] = {
		{
		 .addr = client_hscd->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};
#else
/* for debugging */
	struct i2c_msg msg[1];
	if(client_hscd != NULL){
		if(txData!=NULL){
//			printk("[HSCD] i2c_writem param check(addr %x, length %d, txData %x %x) \n ", client_hscd->addr,length,txData[0],txData[1]);
		}else{
			printk("[HSCD] i2c_writem txData is NULL\n");
			return -EIO;
		}
	}else{
		printk("[HSCD] i2c_writem client_hscd is NULL\n");
		return -EIO;
	}
	msg[0].addr = client_hscd->addr;
	msg[0].flags = 0;
	msg[0].len = length;
	msg[0].buf = txData;
#endif
#ifdef ALPS_DEBUG
	printk("[HSCD] i2c_writem(0x%x): ",client_hscd->addr);

	for (i = 0; i < length; i++)
		printk("0X%02X, ", txData[i]);

	printk("\n");
#endif
	do {
		err = i2c_transfer(client_hscd->adapter, msg, 1);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&client_hscd->adapter->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

int hscd_get_magnetic_field_data(int *xyz)
{
	int err = -1;
	int i;
	u8 sx[6];

	sx[0] = HSCD_XOUT;
	err = hscd_i2c_readm(sx, 6);
	if (err < 0)
		return err;

	for (i = 0; i < 3; i++) {
		xyz[i] = (int) ((short)((sx[2*i+1] << 8) | (sx[2*i])));
	}

#ifdef ALPS_DEBUG
	/*** DEBUG OUTPUT - REMOVE ***/
	printk("Mag_I2C, x:%d, y:%d, z:%d\n", xyz[0], xyz[1], xyz[2]);
	/*** <end> DEBUG OUTPUT - REMOVE ***/
#endif

	return err;
}

void hscd_activate(int flgatm, int flg, int dtime)
{
	u8 buf[2];

	if (flg != 0)
		flg = 1;

	if (dtime <= 10)
		buf[1] = (0x60 | 3<<2);		/* 100Hz- 10msec */

	else if (dtime <= 20)
		buf[1] = (0x60 | 2<<2);		/*  50Hz- 20msec */

	else if (dtime <= 60)
		buf[1] = (0x60 | 1<<2);		/*  20Hz- 50msec */

	else
		buf[1] = (0x60 | 0<<2);		/*  10Hz-100msec */

	buf[0]  = HSCD_CTRL1;
	buf[1] |= (flg<<7);
	buf[1] |= 0x60;	/* RS1:1(Reverse Input Substraction drive), RS2:1(13bit) */

	hscd_i2c_writem(buf, 2);
	mdelay(1);
	if (flg) {
		buf[0] = HSCD_CTRL3;
		buf[1] = 0x02;
		hscd_i2c_writem(buf, 2);
	}

	if (flgatm) {
		atomic_set(&flgEna, flg);
		atomic_set(&delay, dtime);
	}
}
//Testmode 8.7 Start
int get_mode(void)
{

	int enable=0;
	int ret;
	u8 sx[2];

	sx[0] = HSCD_CTRL1;
	hscd_i2c_readm(sx, 2);

	printk("hscd ctrl mode : %8x, %8x\n", sx[0],sx[1]);
	
	
	if((sx[0]&0x60))
	{
		return 0;
	}else
	{
		return 1;
	}
}

void mode_control(int mode)
{

	u8 sx[2];
	
	if(mode==0)
	{
		hscd_activate(1, 0, atomic_read(&delay));
				
		atomic_set(&flgEna,0);
		ecom_pdata=client_hscd->dev.platform_data;
		ecom_pdata->power(0);
	}
	else
	{	
		ecom_pdata=client_hscd->dev.platform_data;
		ecom_pdata->power(1);
		mdelay(5);
		
		hscd_activate(1, atomic_read(&flgEna), atomic_read(&delay));
		atomic_set(&flgEna,1);
		
		sx[0]=HSCD_CTRL1;
		sx[1]=0x80;	
		hscd_i2c_writem(sx,2);	
	}
	
}
//Testmode 8.7 End
static void hscd_register_init(void)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] register_init\n");
#endif
}

static int hscd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int d[3];

	printk("[HSCD] probe\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->adapter->dev, "client not i2c capable\n");
		return -ENOMEM;
	}

	client_hscd = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!client_hscd) {
		dev_err(&client->adapter->dev, "failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, client_hscd);

	client_hscd = client;
#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&hscd_early_suspend_handler);
#endif

/* LGE_CHANGE_S, for platform data */
	ecom_pdata = client->dev.platform_data;

	ecom_pdata->power(1);

	mdelay(5);
/* LGE_CHANGE_E */

	hscd_register_init();

	dev_info(&client->adapter->dev, "detected HSCD magnetic field sensor\n");

	hscd_activate(0, 1, atomic_read(&delay));
	hscd_get_magnetic_field_data(d);
	printk("[HSCD] x:%d y:%d z:%d\n", d[0], d[1], d[2]);
	hscd_activate(0, 0, atomic_read(&delay));

	return 0;
}

static void hscd_shutdown(struct i2c_client *client)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] shutdown\n");
#endif
	hscd_activate(0, 0, atomic_read(&delay));
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&hscd_early_suspend_handler);
#endif
}

static int hscd_suspend(struct i2c_client *client, pm_message_t mesg)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] suspend\n");
#endif
	hscd_activate(0, 0, atomic_read(&delay));
//	ecom_pdata->power(0);
	return 0;
}

static int hscd_resume(struct i2c_client *client)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] resume\n");
#endif
//	ecom_pdata->power(1);
//	mdelay(5);
	hscd_activate(0, atomic_read(&flgEna), atomic_read(&delay));
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hscd_early_suspend(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
    printk("[HSCD] early_suspend\n");
#endif
    hscd_suspend(client_hscd, PMSG_SUSPEND);
}

static void hscd_early_resume(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
    printk("[HSCD] early_resume\n");
#endif
    hscd_resume(client_hscd);
}
#endif


static const struct i2c_device_id ALPS_id[] = {
	{ HSCD_DRIVER_NAME, 0 },
	{ }
};

static struct i2c_driver hscd_driver = {
	.probe    = hscd_probe,
	.id_table = ALPS_id,
	.driver   = {
		.name	= HSCD_DRIVER_NAME,
	},
	.shutdown		= hscd_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= hscd_suspend,
	.resume		= hscd_resume,
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hscd_early_suspend_handler = {
    .suspend = hscd_early_suspend,
    .resume  = hscd_early_resume,
};
#endif

static int __init hscd_init(void)
{
/* LGE_CHANGE_S, we already use board info in platform data */
#if 0
	struct i2c_board_info i2c_info;
	struct i2c_adapter *adapter;
#endif
/* LGE_CHANGE_E */
	int rc;

#ifdef ALPS_DEBUG
	printk("[HSCD] init\n");
#endif
	atomic_set(&flgEna, 0);
	atomic_set(&delay, 200);

	rc = i2c_add_driver(&hscd_driver);
	if (rc != 0) {
		printk("can't add i2c driver\n");
		rc = -ENOTSUPP;
		return rc;
	}

/* LGE_CHANGE_S, we already use board info in platform data */
#if 0
	memset(&i2c_info, 0, sizeof(struct i2c_board_info));
	i2c_info.addr = I2C_HSCD_ADDR;
	strlcpy(i2c_info.type, HSCD_DRIVER_NAME , I2C_NAME_SIZE);

	adapter = i2c_get_adapter(I2C_BUS_NUMBER);
	if (!adapter) {
		printk("can't get i2c adapter %d\n", I2C_BUS_NUMBER);
		rc = -ENOTSUPP;
		goto probe_done;
	}
	client_hscd = i2c_new_device(adapter, &i2c_info);
	client_hscd->adapter->timeout = 0;
	client_hscd->adapter->retries = 0;

	i2c_put_adapter(adapter);
	if (!client_hscd) {
		printk("can't add i2c device at 0x%x\n", (unsigned int)i2c_info.addr);
		rc = -ENOTSUPP;
	}
#endif
/* LGE_CHANGE_E */

#ifdef ALPS_DEBUG
	printk("hscd_open Init end!!!!\n");
#endif

/* probe_done: */

	return rc;
}

static void __exit hscd_exit(void)
{
#ifdef ALPS_DEBUG
	printk("[HSCD] exit\n");
#endif
	i2c_del_driver(&hscd_driver);
}

module_init(hscd_init);
module_exit(hscd_exit);

EXPORT_SYMBOL(hscd_get_magnetic_field_data);
EXPORT_SYMBOL(hscd_activate);

MODULE_DESCRIPTION("Alps hscd Device");
MODULE_AUTHOR("ALPS");
MODULE_LICENSE("GPL v2");
