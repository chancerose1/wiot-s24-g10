#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
/* Includes for BLE*/
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>
/* Includes for NFC*/
#include <zephyr/sys/reboot.h>
#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>
#include <nfc/ndef/uri_msg.h>
#include <nfc/ndef/uri_rec.h>
#include <dk_buttons_and_leds.h>

/* NFC Defines */
#define MAX_REC_COUNT 10
#define NDEF_MSG_BUF_SIZE 128
#define NFC_NDEF_URI_REC_ENABLED 1

/*
        UUIDs for bluetooth  service and characteristic
*/
#define SERVICE_UUID BT_UUID_128_ENCODE(0x4fafc201, 0x1fb5, 0x459e, 0x8fcc, 0xc5c9c331914b)
#define SERVICE_CHARACTERISTIC_UUID 0xbeb5

/* Declare UUIDs for use*/
static struct bt_uuid *search_service_uuid = BT_UUID_DECLARE_128(SERVICE_UUID);
static struct bt_uuid *characteristic_uuids[] = {
    BT_UUID_DECLARE_16(SERVICE_CHARACTERISTIC_UUID),
};

/*
        defines device tree aliaes for gpio for leds
*/
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

/*
        creates array of specs to use in main
*/
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    GPIO_DT_SPEC_GET(LED3_NODE, gpios)};

/* function prototypes */
static void start_scan(void);

static struct bt_conn *default_conn;

static struct bt_gatt_discover_params discover_params;

static struct bt_gatt_subscribe_params subscribe_params[1];

/* Begin NFC Code */
/* NFC Code */

/* Buffer for NFC NDEF message */
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];

/* NFC Callback */
static void nfc_callback(void *context,
                         nfc_t2t_event_t event,
                         const uint8_t *data,
                         size_t data_length)
{
        ARG_UNUSED(context);
        ARG_UNUSED(data);
        ARG_UNUSED(data_length);

        switch (event)
        {
        case NFC_T2T_EVENT_FIELD_ON:
                gpio_pin_set_dt(&leds[3], 1);
                break;
        case NFC_T2T_EVENT_FIELD_OFF:
                gpio_pin_set_dt(&leds[3], 0);
                break;
        default:
                break;
        }
}

static int welcome_msg_encode(uint8_t *buffer, uint32_t *len)
{
        int err;
        static const uint8_t en_payload[] = {
            'N', 'o', ' ', 'd', 'a', 't', 'a', ' ', 'r', 'e', 'c', 'e', 'i', 'v', 'e', 'd'};
        static const uint8_t en_code[] = {'e', 'n'};
        NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_welcome_text_rec,
                                      UTF_8,
                                      en_code,
                                      sizeof(en_code),
                                      en_payload,
                                      sizeof(en_payload));
        NFC_NDEF_MSG_DEF(nfc_text_msg, 1);
        err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
                                      &NFC_NDEF_TEXT_RECORD_DESC(nfc_welcome_text_rec));
        if (err < 0)
        {
                printk("Cannot add first record!\n");
                return err;
        }
        err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_text_msg),
                                  buffer,
                                  len);
        if (err < 0)
        {
                printk("Cannot encode message!\n");
        }
        return err;
}

static int encode_msg(uint8_t *buffer, uint32_t *len, char *message_recv)
{
        int err;

        static const uint8_t en_code[] = {'e', 'n'};
        static const uint8_t en_code2[] = {'e', 'n'};
        static uint8_t en_payload[25];
        memcpy(en_payload, message_recv, strlen(message_recv));
        char *link = "192.168.4.1/present";
        NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_en_text_rec,
                                      UTF_8,
                                      en_code,
                                      sizeof(en_code),
                                      en_payload,
                                      sizeof(en_payload));

        NFC_NDEF_URI_RECORD_DESC_DEF(link_record,
                                     NFC_URI_HTTP,
                                     link,
                                     strlen(link));

        NFC_NDEF_MSG_DEF(nfc_msg, MAX_REC_COUNT - 1);
        err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_msg),
                                      &NFC_NDEF_TEXT_RECORD_DESC(nfc_en_text_rec));
        if (err < 0)
        {
                printk("Could not add first record!\n");
                return err;
        }
        printk("Code record added.\n");

        err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_msg),
                                      &NFC_NDEF_URI_RECORD_DESC(link_record));
        if (err < 0)
        {
                printk("Could not add second record!\n");
                return err;
        }
        printk("Link record added.\n");

        err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_msg),
                                  buffer,
                                  len);
        if (err < 0)
        {
                printk("Cannot text encode message!\n");
        }
        return err;
}
/* End NFC Code */

struct discovered_gatt_descriptor
{
        uint16_t handle;
        struct bt_uuid_128 uuid;
};

struct discovered_gatt_characteristic
{
        uint16_t handle;
        uint16_t value_handle;
        struct bt_uuid_128 uuid;
        int num_descriptors;
        struct discovered_gatt_descriptor descriptors[10];
};

struct discovered_gatt_service
{
        uint16_t handle;
        uint16_t end_handle;
        struct bt_uuid_128 uuid;
        int num_characteristics;
        struct discovered_gatt_characteristic characteristics[10];
};

int num_discovered_services = 0;
struct discovered_gatt_service services[10];

enum discovering_state
{
        DISC_STATE_SERVICES = 0,
        DISC_STATE_CHARACTERISTICS = 1,
        DISC_STATE_DESCRIPTORS = 2,
        DISC_STATE_DONE = 3,
};

enum discovering_state disc_state = DISC_STATE_SERVICES;
int discovering_index_svc = 0;

static uint8_t notify_func_0(struct bt_conn *conn,
                             struct bt_gatt_subscribe_params *params,
                             const void *data, uint16_t length)
{
        if (!data)
        {
                printk("[UNSUBSCRIBED]\n");
                params->value_handle = 0U;
                return BT_GATT_ITER_STOP;
        }
        else
        {
                char received_data[100];
                memcpy(received_data, data, length);
                received_data[length] = '\0';
                printk("Data Received [%s]\n", received_data);
                nfc_t2t_emulation_stop();
                uint32_t len = sizeof(ndef_msg_buf);
                memset(ndef_msg_buf, 0, NDEF_MSG_BUF_SIZE);
                if (encode_msg(ndef_msg_buf, &len, received_data) < 0)
                {
                        printk("Failed to encode message!\n");
                        return -EIO;
                }
                if (nfc_t2t_payload_set(ndef_msg_buf, len) < 0)
                {
                        printk("Cannot set payload!\n");
                        return -EIO;
                }
                if (nfc_t2t_emulation_start() < 0)
                {
                        printk("Cannot start emulation!\n");
                        return -EIO;
                }
        }
        return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
        int err;

        if (!attr)
        {
                if (disc_state == DISC_STATE_SERVICES)
                {
                        disc_state = DISC_STATE_CHARACTERISTICS;
                        discovering_index_svc = 0;

                        if (num_discovered_services > 0)
                        {
                                // Discover characteristics for first service.
                                discover_params.uuid = NULL;
                                discover_params.func = discover_func;
                                discover_params.start_handle = services[discovering_index_svc].handle + 1;
                                discover_params.end_handle = services[discovering_index_svc].end_handle;
                                discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
                                err = bt_gatt_discover(conn, &discover_params);
                        }
                }
                else if (disc_state == DISC_STATE_CHARACTERISTICS)
                {
                        discovering_index_svc += 1;

                        if (discovering_index_svc < num_discovered_services)
                        {
                                // Discover characteristics for each additional service.
                                discover_params.uuid = NULL;
                                discover_params.func = discover_func;
                                discover_params.start_handle = services[discovering_index_svc].handle + 1;
                                discover_params.end_handle = services[discovering_index_svc].end_handle;
                                discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
                                err = bt_gatt_discover(conn, &discover_params);
                        }
                        else
                        {
                                // Move to descriptors
                                disc_state = DISC_STATE_DESCRIPTORS;
                                discovering_index_svc = 0;

                                discover_params.uuid = NULL;
                                discover_params.func = discover_func;
                                discover_params.start_handle = services[discovering_index_svc].handle + 1;
                                discover_params.end_handle = services[discovering_index_svc].end_handle;
                                discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
                                err = bt_gatt_discover(conn, &discover_params);
                        }
                }
                else if (disc_state == DISC_STATE_DESCRIPTORS)
                {
                        discovering_index_svc += 1;

                        if (discovering_index_svc < num_discovered_services)
                        {
                                discover_params.uuid = NULL;
                                discover_params.func = discover_func;
                                discover_params.start_handle = services[discovering_index_svc].handle + 1;
                                discover_params.end_handle = services[discovering_index_svc].end_handle;
                                discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
                                err = bt_gatt_discover(conn, &discover_params);
                        }
                        else
                        {
                                disc_state = DISC_STATE_DONE;
                                char str[128];
                                int s, c, d;
                                for (s = 0; s < num_discovered_services; s++)
                                {
                                        bool our_service = false;
                                        struct discovered_gatt_service *disc_serv = &services[s];

                                        bt_uuid_to_str((struct bt_uuid *)&disc_serv->uuid, str, 128);
                                        if (strcmp(str, "4fafc201-1fb5-459e-8fcc-c5c9c331914b") == 0)
                                        {
                                                our_service = true;
                                        }
                                        for (c = 0; c < disc_serv->num_characteristics; c++)
                                        {
                                                struct discovered_gatt_characteristic *disc_char = &disc_serv->characteristics[c];
                                                /* assign subscribe params onces we have found them */

                                                if (our_service)
                                                {
                                                        subscribe_params[0].value_handle = disc_char->value_handle;
                                                        subscribe_params[0].value = BT_GATT_CCC_NOTIFY;
                                                }

                                                for (d = 0; d < disc_char->num_descriptors; d++)
                                                {
                                                        struct discovered_gatt_descriptor *disc_desc = &disc_char->descriptors[d];

                                                        if (bt_uuid_cmp((struct bt_uuid *)&disc_desc->uuid, BT_UUID_GATT_CCC) == 0)
                                                        {
                                                                // printk("  ---CCC - handle: 0x%02x\n", disc_desc->handle);
                                                                subscribe_params[0].ccc_handle = disc_desc->handle;
                                                        }
                                                        else
                                                        {
                                                                bt_uuid_to_str((struct bt_uuid *)&disc_desc->uuid, str, 128);
                                                                // printk("  ---Descriptor [%s] - handle: 0x%02x\n", str, disc_char->handle);
                                                        }
                                                }
                                                if (our_service)
                                                {
                                                        subscribe_params[0].notify = notify_func_0;
                                                        int subscribe_err = bt_gatt_subscribe(conn, &subscribe_params[0]);
                                                        if (subscribe_err && subscribe_err != -EALREADY)
                                                        {
                                                                printk("Subscribe failed (err %d)\n", err);
                                                        }
                                                        else
                                                        {
                                                                printk("[SUBSCRIBED for  characteristic %d]\n", c);
                                                        }
                                                }
                                        }
                                }
                        }
                }
                return BT_GATT_ITER_STOP;
        }

        if (params->type == BT_GATT_DISCOVER_PRIMARY)
        {
                // We are looking for the services

                struct bt_gatt_service_val *gatt_service = (struct bt_gatt_service_val *)attr->user_data;

                services[num_discovered_services] = (struct discovered_gatt_service){
                    .handle = attr->handle,
                    .end_handle = gatt_service->end_handle,
                    .num_characteristics = 0};
                memcpy(&services[num_discovered_services].uuid, BT_UUID_128(gatt_service->uuid),
                       sizeof(services[num_discovered_services].uuid));

                num_discovered_services += 1;
        }
        else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC)
        {

                struct bt_gatt_chrc *gatt_characteristic = (struct bt_gatt_chrc *)attr->user_data;

                struct discovered_gatt_service *disc_serv_loc = &services[discovering_index_svc];
                struct discovered_gatt_characteristic *disc_char_loc = &disc_serv_loc->characteristics[disc_serv_loc->num_characteristics];

                *disc_char_loc = (struct discovered_gatt_characteristic){
                    .handle = attr->handle,
                    .value_handle = gatt_characteristic->value_handle,
                    .num_descriptors = 0,
                };
                memcpy(&disc_char_loc->uuid, BT_UUID_128(gatt_characteristic->uuid), sizeof(disc_char_loc->uuid));

                services[discovering_index_svc].num_characteristics += 1;
        }
        else if (params->type == BT_GATT_DISCOVER_DESCRIPTOR)
        {
                uint16_t handle = attr->handle;
                int i;

                int matched_characteristic = 0;
                for (i = 1; i < services[discovering_index_svc].num_characteristics; i++)
                {
                        if (handle < services[discovering_index_svc].characteristics[i].handle)
                        {
                                break;
                        }
                        matched_characteristic = i;
                }

                struct discovered_gatt_service *disc_serv_loc = &services[discovering_index_svc];
                struct discovered_gatt_characteristic *disc_char_loc = &disc_serv_loc->characteristics[matched_characteristic];
                struct discovered_gatt_descriptor *disc_desc_loc = &disc_char_loc->descriptors[disc_char_loc->num_descriptors];

                *disc_desc_loc = (struct discovered_gatt_descriptor){
                    .handle = handle,
                };
                memcpy(&disc_desc_loc->uuid, BT_UUID_128(attr->uuid), sizeof(disc_desc_loc->uuid));

                disc_char_loc->num_descriptors += 1;
        }

        return BT_GATT_ITER_CONTINUE;
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
        char addr[BT_ADDR_LE_STR_LEN];
        int err;

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        if (conn_err)
        {
                printk("Failed to connect to %s (%u)\n", addr, conn_err);

                bt_conn_unref(default_conn);
                default_conn = NULL;

                start_scan();
                return;
        }
        gpio_pin_set_dt(&leds[0], 1); // Turn on LED 1 if connected
        printk("Connected: %s\n", addr);

        if (conn == default_conn)
        {
                discover_params.uuid = NULL;
                discover_params.func = discover_func;
                discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
                discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
                discover_params.type = BT_GATT_DISCOVER_PRIMARY;

                disc_state = DISC_STATE_SERVICES;

                err = bt_gatt_discover(default_conn, &discover_params);
                if (err)
                {
                        printk("Discover failed(err %d)\n", err);
                        return;
                }
        }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        gpio_pin_set_dt(&leds[0], 0); // Turn off LED 1 if connected
        printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);
        if (default_conn != conn)
        {
                return;
        }

        bt_conn_unref(default_conn);
        default_conn = NULL;

        start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static bool ad_found(struct bt_data *data, void *user_data)
{
        bt_addr_le_t *addr = user_data;

        // printk("[AD]: %u data_len %u\n", data->type, data->data_len);

        switch (data->type)
        {
        case BT_DATA_UUID128_ALL:
                if (data->data_len != 16)
                {
                        printk("AD malformed\n");
                        return true;
                }

                struct bt_le_conn_param *param;
                struct bt_uuid uuid;
                int err;

                bt_uuid_create(&uuid, data->data, 16);
                if (bt_uuid_cmp(&uuid, BT_UUID_DECLARE_128(SERVICE_UUID)) == 0)
                {
                        printk("Found matching advertisement\n");

                        err = bt_le_scan_stop();
                        if (err)
                        {
                                printk("Stop LE scan failed (err %d)\n", err);
                                return false;
                        }

                        param = BT_LE_CONN_PARAM_DEFAULT;
                        err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &default_conn);
                        if (err)
                        {
                                printk("Create conn failed (err %d)\n", err);
                                start_scan();
                        }
                }

                return false;
        }

        return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
        char dev[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(addr, dev, sizeof(dev));
        if (type == BT_GAP_ADV_TYPE_ADV_IND ||
            type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND)
        {
                bt_data_parse(ad, ad_found, (void *)addr);
        }
}

static void start_scan(void)
{
        int err;

        struct bt_le_scan_param scan_param = {
            .type = BT_LE_SCAN_TYPE_PASSIVE,
            .options = BT_LE_SCAN_OPT_NONE,
            .interval = BT_GAP_SCAN_FAST_INTERVAL,
            .window = BT_GAP_SCAN_FAST_WINDOW,
        };

        err = bt_le_scan_start(&scan_param, device_found);
        if (err)
        {
                printk("Scanning failed to start (err %d)\n", err);
                return;
        }

        printk("Scanning successfully started\n");
}

static void bt_ready(int err)
{
        if (err)
        {
                printk("Bluetooth init failed (err %d)\n", err);
                return;
        }
        printk("Bluetooth initialized\n");

        /* I had add these lines because I was getting errno -11 without */
        // int load_err = settings_load();
        // if (load_err)
        // {
        //         printk("Settings load failed (err %d)\n", load_err);
        // }

        start_scan();
}

int main(void)
{
        printk("Starting Board...\n");

        int gpio_err;
        for (int i = 0; i < 4; i++)
        {
                if (!gpio_is_ready_dt(&leds[i]))
                {
                        printk("Error: LED GPIO initialization failed\n");
                        return;
                }
                gpio_err = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_ACTIVE);
                if (gpio_err)
                {
                        printk("Error: LED GPIO configuration failed (err %d)\n", gpio_err);
                        return;
                }
                gpio_pin_set_dt(&leds[i], 0);
        }

        int err = bt_enable(bt_ready);

        if (err)
        {
                printk("Bluetooth init failed (err %d)", err);
                return -1;
        }
        printk("Bluetooth init successful!\n");

        printk("Starting NFC init...\n");
        uint32_t len = sizeof(ndef_msg_buf);
        if (nfc_t2t_setup(nfc_callback, NULL) < 0)
        {
                printk("Cannot setup NFC T2T library!\n");
                goto fail;
        }
        if (welcome_msg_encode(ndef_msg_buf, &len) < 0)
        {
                printk("Cannot encode message!\n");
                goto fail;
        }
        if (nfc_t2t_payload_set(ndef_msg_buf, len) < 0)
        {
                printk("Cannot set payload!\n");
                goto fail;
        }
        if (nfc_t2t_emulation_start() < 0)
        {
                printk("Cannot start emulation!\n");
                goto fail;
        }
        printk("NFC configuration done\n");
        return 0;

fail:
#if CONFIG_REBOOT
        sys_reboot(SYS_REBOOT_COLD);
#endif /* CONFIG_REBOOT */

        return -EIO;
}