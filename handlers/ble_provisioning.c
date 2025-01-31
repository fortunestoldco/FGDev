#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <sys/printk.h>

static struct bt_uuid_128 wifi_prov_uuid = BT_UUID_INIT_128(BT_UUID_WIFI_PROV_VAL);

int ble_provisioning_init(void)
{
    struct bt_le_adv_param adv_param = {
        .options = BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_CONNECTABLE,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    };

    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return err;
    }

    printk("Bluetooth Advertising successfully started\n");
    return 0;
}