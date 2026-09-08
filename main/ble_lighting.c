#include "ble_lighting.h"
#include "ble_light_protocol.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#if CONFIG_BT_NIMBLE_SECURITY_ENABLE
#error "Lighthouse bondless mode requires CONFIG_BT_NIMBLE_SECURITY_ENABLE=n"
#endif

#define LIGHT_UUID(id) BLE_UUID128_INIT(0x00, 0x10, 0xc4, 0x37, 0x5a, 0x2f, 0x76, 0x9d, \
                                       0x5c, 0x4b, 0x58, 0x8f, (id), 0x00, 0x7f, 0x8e)

static const char *TAG = "ble_lighting";
static const ble_uuid128_t service_uuid = LIGHT_UUID(0);
static const ble_uuid128_t field_uuids[] = {
    LIGHT_UUID(1), LIGHT_UUID(2), LIGHT_UUID(3), LIGHT_UUID(4),
    LIGHT_UUID(5), LIGHT_UUID(6), LIGHT_UUID(7), LIGHT_UUID(8),
    LIGHT_UUID(9), LIGHT_UUID(10), LIGHT_UUID(11),
};
static const ble_uuid16_t description_uuid = BLE_UUID16_INIT(0x2901);
static struct ble_gatt_chr_def characteristics[12];
static struct ble_gatt_dsc_def descriptions[11][2];
static const struct ble_gatt_svc_def services[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &service_uuid.u,
     .characteristics = characteristics},
    {0},
};
static ble_light_state_t requested;
static uint8_t own_addr_type;
static bool initialized;

static int gatt_access(uint16_t connection, uint16_t attribute,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)attribute;
    ble_light_field_t field = (ble_light_field_t)(uintptr_t)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
        const char *name = ble_light_field_name(field);
        return os_mbuf_append(ctxt->om, name, strlen(name)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    uint8_t value[BLE_LIGHT_VALUE_MAX];
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int result = ble_light_read(&requested, field, value, sizeof(value));
        ESP_LOGI(TAG, "GATT read: conn=%u field=%s result=0x%02x",
                 connection, ble_light_field_name(field), result);
        if (result) return result;
        return os_mbuf_append(ctxt->om, value, ble_light_field_size(field)) == 0 ?
               0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        size_t length = OS_MBUF_PKTLEN(ctxt->om);
        if (length != ble_light_field_size(field) || length > sizeof(value)) {
            ESP_LOGW(TAG, "GATT write rejected: conn=%u field=%s length=%u expected=%u",
                     connection, ble_light_field_name(field), (unsigned)length,
                     (unsigned)ble_light_field_size(field));
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint16_t copied;
        if (ble_hs_mbuf_to_flat(ctxt->om, value, sizeof(value), &copied) != 0 || copied != length) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        int result = ble_light_write(&requested, field, value, copied);
        ESP_LOGI(TAG, "GATT write: conn=%u field=%s result=0x%02x",
                 connection, ble_light_field_name(field), result);
        return result;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void advertise(void);

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "BLE connected: handle=%u", event->connect.conn_handle);
        } else {
            ESP_LOGW(TAG, "BLE connection failed: status=%d", event->connect.status);
            advertise();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected: handle=%u reason=0x%03x",
                 event->disconnect.conn.conn_handle, event->disconnect.reason);
        advertise();
        break;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "BLE MTU updated: handle=%u mtu=%u",
                 event->mtu.conn_handle, event->mtu.value);
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        break;
    default:
        break;
    }
    return 0;
}

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) goto failed;
    // Put the name in scan response to keep the 128-bit service in 31-byte ADV.
    struct ble_hs_adv_fields response = {0};
    response.name = (uint8_t *)ble_svc_gap_device_name();
    response.name_len = strlen((char *)response.name);
    response.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response);
    if (rc != 0) goto failed;
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = 160; // 100-150 ms, in 0.625 ms units.
    params.itvl_max = 240;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0) goto failed;
    ESP_LOGI(TAG, "Advertising as Lighthouse (no pairing or bonds)");
    return;
failed:
    ESP_LOGE(TAG, "Advertising failed: rc=%d", rc);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "BLE address setup failed: rc=%d", rc);
        return;
    }
    uint8_t address[6];
    rc = ble_hs_id_copy_addr(own_addr_type, address, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "BLE identity: %02X:%02X:%02X:%02X:%02X:%02X type=%u",
                 address[5], address[4], address[3], address[2], address[1], address[0],
                 own_addr_type);
    }
    advertise();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset: reason=%d; advertising resumes on sync", reason);
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    vTaskDelete(NULL);
}

esp_err_t ble_lighting_init(const big_light_settings_t *startup)
{
    if (!startup) return ESP_ERR_INVALID_ARG;
    if (initialized) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err; // Do not erase unrelated NVS on error.
    err = nimble_port_init();
    if (err != ESP_OK) return err;

    requested = (ble_light_state_t){.big_light = *startup};
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    for (unsigned i = 0; i < 11; ++i) {
        void *arg = (void *)(uintptr_t)(i + 1);
        descriptions[i][0] = (struct ble_gatt_dsc_def){
            .uuid = &description_uuid.u, .att_flags = BLE_ATT_F_READ,
            .access_cb = gatt_access, .arg = arg,
        };
        characteristics[i] = (struct ble_gatt_chr_def){
            .uuid = &field_uuids[i].u, .access_cb = gatt_access, .arg = arg,
            .flags = BLE_GATT_CHR_F_READ | (i == 0 ? 0 : BLE_GATT_CHR_F_WRITE),
            .descriptors = descriptions[i],
        };
    }
    int rc = ble_svc_gap_device_name_set("Lighthouse");
    if (rc == 0) rc = ble_gatts_count_cfg(services);
    if (rc == 0) rc = ble_gatts_add_svcs(services);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT registration failed: rc=%d", rc);
        (void)nimble_port_deinit();
        return ESP_FAIL;
    }
    // This IDF version's esp_nimble_enable ignores task-allocation failure.
    // Create the same host task explicitly so initialization can report it.
    if (xTaskCreatePinnedToCore(host_task, "nimble_host", NIMBLE_HS_STACK_SIZE,
                               NULL, configMAX_PRIORITIES - 4, NULL, NIMBLE_CORE) != pdPASS) {
        (void)nimble_port_deinit();
        return ESP_ERR_NO_MEM;
    }
    initialized = true;
    return ESP_OK;
}
