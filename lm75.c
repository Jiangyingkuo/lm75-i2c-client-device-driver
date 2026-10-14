#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/kernel.h>

#define LED0 5
#define COM 22
#define BUZZER 16
#define LM75_OS_GPIO 18
#define LM75_IRQ_NAME "lm75_overtemp"

/*#define LM75_ALARM_BUZZER */


static int lm75_irq;

static int lm75_read_temp(struct i2c_client *client, int *milldeg)
{
	u8 reg =0x00;
	u8 buf[2];
	int ret;
	signed short raw=0;

	struct i2c_msg msgs[2];
	msgs[0].addr = client ->addr;
	msgs[0].flags =0;
	msgs[0].buf =&reg;	//pointer to msg data
	msgs[0].len =1;
	
	msgs[1].addr =client->addr;
	msgs[1].flags =I2C_M_RD;
	msgs[1].buf =buf;
	msgs[1].len =2;
	
	ret=i2c_transfer(client->adapter,msgs,2);
	if(ret !=2)
		return ret < 0 ? ret: -EIO;
	raw |= ((signed short)buf[0] << 8);
	raw |= buf[1];

	//milli degree
	*milldeg =(raw >>5)*125;	//only the 11 most significant bits should be used, and the 5 least
								//significant bits of the LSByte are zero and should be ignored
	return 0;
}

static ssize_t temp_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret,milldeg;
	ret =lm75_read_temp(to_i2c_client(dev),&milldeg);
	if(ret<0) {
		printk(KERN_WARNING "LM75 read temp failed: %d\n", ret);
		return ret;
	}
	return sprintf(buf,"%d\n",milldeg);
}

//read overtemperature shutdown threshold
static int lm75_read_overtemp(struct i2c_client *client, int *milldeg)
{
	u8 reg =0x03;
	u8 buf[2];
	int ret;
	signed short raw=0;

	struct i2c_msg msgs[2];
	msgs[0].addr = client ->addr;
	msgs[0].flags =0;
	msgs[0].buf =&reg;	//pointer to msg data
	msgs[0].len =1;
	
	msgs[1].addr =client->addr;
	msgs[1].flags =I2C_M_RD;
	msgs[1].buf =buf;
	msgs[1].len =2;
	
	ret=i2c_transfer(client->adapter,msgs,2);
	if(ret !=2)
		return ret < 0 ? ret: -EIO;
	raw |= ((signed short)buf[0] << 8);
	raw |= buf[1];

	//milli degree
	*milldeg =(raw >>5)*125;	//only the 11 most significant bits should be used, and the 5 least
								//significant bits of the LSByte are zero and should be ignored
	return 0;
}
static ssize_t overtemp_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret,milldeg;
	ret =lm75_read_overtemp(to_i2c_client(dev),&milldeg);
	if(ret<0) {
		printk(KERN_WARNING "LM75 read overtemp failed: %d\n", ret);
		return ret;
	}
	return sprintf(buf,"%d\n",milldeg);
}

static int lm75_write_overtemp(struct i2c_client *client)
{
	u8 msb =0x1E,lsb=0x00;
	u8 reg[3] ={0x03,msb,lsb}; // 30deg  0x00F0  0000 0000 1111 0000 , shift left 5 bit equals 0001 1110 0000 0000
	int ret;
	struct i2c_msg msg;
	msg.addr =client ->addr;
	msg.flags =0;
	msg.buf = reg;
	msg.len =3;
	
	ret=i2c_transfer(client->adapter,&msg,1);
	if(ret !=1)
		return ret < 0 ?ret: -EIO;
	return 0;
}

//write overtemp with parameters
static int lm75_write_overtemp_parm(struct i2c_client *client, int milldeg)
{
	u8 msb,lsb, reg[3];
	int ret;
	struct i2c_msg msg;

	milldeg =(milldeg/125) *32 ;
	msb = (milldeg &(0xFF00)) >> 8;
	lsb= milldeg&(0x00FF);
	reg[0] =0x03;
	reg[1] =msb;
	reg[2] =lsb;

	msg.addr =client ->addr;
	msg.flags =0;
	msg.buf = reg;
	msg.len =3;
	
	ret=i2c_transfer(client->adapter,&msg,1);
	if(ret !=1)
		return ret < 0 ?ret: -EIO;
	return 0;
}

static ssize_t overtemp_write(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	int ret,milldeg;
	ret = kstrtoint(buf, 10, &milldeg);		//int kstrtoint(const char *s, unsigned int base, int *res);
	if (ret < 0)
		return ret;
	ret = lm75_write_overtemp_parm(to_i2c_client(dev),milldeg);
	if (ret < 0)
		return ret;
	return count;
}

//read Thyst temperature
static int lm75_read_thyst_temp(struct i2c_client *client, int *milldeg)
{
	u8 reg =0x02;
	u8 buf[2];
	int ret;
	signed short raw=0;

	struct i2c_msg msgs[2];
	msgs[0].addr = client ->addr;
	msgs[0].flags =0;
	msgs[0].buf =&reg;	//pointer to msg data
	msgs[0].len =1;
	
	msgs[1].addr =client->addr;
	msgs[1].flags =I2C_M_RD;
	msgs[1].buf =buf;
	msgs[1].len =2;
	
	ret=i2c_transfer(client->adapter,msgs,2);
	if(ret !=2)
		return ret < 0 ? ret: -EIO;
	raw |= ((signed short)buf[0] << 8);
	raw |= buf[1];

	//milli degree
	*milldeg =(raw >>5)*125;	//only the 11 most significant bits should be used, and the 5 least
								//significant bits of the LSByte are zero and should be ignored
	return 0;
}

static int lm75_write_Thystemp_parm(struct i2c_client *client, int milldeg)
{
	u8 msb,lsb, reg[3];
	int ret;
	struct i2c_msg msg;

	milldeg =(milldeg/125) *32 ;
	msb = (milldeg &(0xFF00)) >> 8;
	lsb= milldeg&(0x00FF);
	reg[0] =0x02;
	reg[1] =msb;
	reg[2] =lsb;

	msg.addr =client ->addr;
	msg.flags =0;
	msg.buf = reg;
	msg.len =3;
	
	ret=i2c_transfer(client->adapter,&msg,1);
	if(ret !=1)
		return ret < 0 ?ret: -EIO;
	return 0;
}

static ssize_t thystemp_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret,milldeg;
	ret =lm75_read_thyst_temp(to_i2c_client(dev),&milldeg);
	if(ret<0) {
		printk(KERN_WARNING "LM75 read thyst failed: %d\n", ret);
		return ret;
	}
	return sprintf(buf,"%d\n",milldeg);
}

static ssize_t thystemp_write(struct device *dev,struct device_attribute *attr, const char *buf,size_t count)
{
	int ret,milldeg;
	ret = kstrtoint(buf, 10, &milldeg);		//int kstrtoint(const char *s, unsigned int base, int *res);
	if (ret < 0)
		return ret;
	ret = lm75_write_Thystemp_parm(to_i2c_client(dev),milldeg);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(temp, S_IRUGO, temp_show, NULL);
static DEVICE_ATTR(overtemp,S_IRUGO | S_IWUSR,overtemp_show,overtemp_write);
static DEVICE_ATTR(thystemp,S_IRUGO | S_IWUSR,thystemp_show,thystemp_write);


static const struct of_device_id lm75_of_match[] = {
     { .compatible = "lm75bd"},
   {},
};

//isr
static irqreturn_t lm75_isr(int irq, void *data)
{
	int level;
	level =gpio_get_value(LM75_OS_GPIO);
	if (level ==0){
		printk("LM75 OS LOW: overtemperature\n");
		#ifdef LM75_ALARM_BUZZER
			gpio_set_value(BUZZER, 1);
		#else
			gpio_set_value(LED0,1);
		#endif
	}
	else{
		printk("LM75 OS High: alarm clear\n");
		#ifdef LM75_ALARM_BUZZER
			gpio_set_value(BUZZER, 0);
		#else
			gpio_set_value(LED0,0);
		#endif
	}
	return IRQ_HANDLED;
}

static int lm75_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	int ret,milldeg=0,Thyst=26000;
	printk(KERN_INFO "LM75 probe called\n");
	
	ret =lm75_read_temp(client,&milldeg);
	if(ret<0) {
		printk(KERN_WARNING "LM75 read temp failed: %d\n", ret);
		return ret;
	}

	//write overtemperature threshold
	ret=lm75_write_overtemp(client);
	if(ret<0) {
		printk(KERN_WARNING "LM75 write overtemp failed: %d\n", ret);
		return ret;
	}

	//write Thyst
	ret=lm75_write_Thystemp_parm(client,Thyst);		//26000
	if(ret<0) {
		printk(KERN_WARNING "LM75 write thyst temp failed: %d\n", ret);
		return ret;
	}

	// int device_create_file(struct device *dev, const struct device_attribute *attr)
	device_create_file(&client->dev ,&dev_attr_temp);
	device_create_file(&client->dev ,&dev_attr_overtemp);
	device_create_file(&client->dev ,&dev_attr_thystemp);

	//setup gpio 18 as input and request irq
	if(gpio_request(LM75_OS_GPIO,"LM75_OS")) return -1;
	if(gpio_direction_input(LM75_OS_GPIO)) return -1;
	
	//buzzer
	#ifdef LM75_ALARM_BUZZER
		if(gpio_request(BUZZER, "BUZZER")) return -1;
		gpio_direction_output(BUZZER, 0 );
	#else
		if(gpio_request(LED0,"LED0_GPIO")) return -1;
		if(gpio_request(COM,"COM_GPIO")) return -1;

		gpio_direction_output(LED0, 0);
		gpio_direction_output(COM, 1);
	#endif

	if( (lm75_irq= gpio_to_irq(LM75_OS_GPIO)) < 0 )  return -1;
	if( request_irq( lm75_irq, lm75_isr ,IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, LM75_IRQ_NAME, client)) return -1;
	
	return 0;
}
static int lm75_remove(struct i2c_client *client)
{
	printk(KERN_INFO "LM75 remove called\n");
	device_remove_file(&client->dev,&dev_attr_temp);
	device_remove_file(&client->dev ,&dev_attr_overtemp);
	device_remove_file(&client->dev ,&dev_attr_thystemp);

	free_irq(lm75_irq, client);
	
	#ifdef LM75_ALARM_BUZZER
		gpio_set_value(BUZZER, 0);
		gpio_free(BUZZER);
	#else
		gpio_set_value(LED0,0);
		gpio_set_value(COM,0);
		gpio_free(LED0);
		gpio_free(COM);
	#endif
	gpio_free(LM75_OS_GPIO);
	return 0;
}
static const struct i2c_device_id lm75_id[] = {
	{ "lm75bd", 0 },
	{}
};

static struct i2c_driver lm75_driver = {
        .driver = {
                .name = "lm75_i2c",
                .owner = THIS_MODULE,
                .of_match_table = lm75_of_match,
        },
        .probe = lm75_probe,
        .remove = lm75_remove,
    .id_table = lm75_id,
};

static int __init lm75_init(void)
{
	return i2c_add_driver(&lm75_driver);
}

static void __exit lm75_exit(void)
{
    i2c_del_driver(&lm75_driver);
}
module_init(lm75_init);
module_exit(lm75_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple LM75 I2C client driver");
