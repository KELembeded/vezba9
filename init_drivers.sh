if [ ! -e /dev/button ]
then
    insmod /Drivers/button_driver/button_driver.ko
fi
if [ ! -e /dev/switch ]
then
    insmod /Drivers/switch_driver/switch_driver.ko
fi
if [ ! -e /dev/led ]
then
    insmod /Drivers/led_driver/led_driver.ko
fi
