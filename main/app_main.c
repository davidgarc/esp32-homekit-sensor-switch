#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <dht.h>

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include <hap_fw_upgrade.h>
#include <iot_button.h>

#include <app_wifi.h>
#include <app_hap_setup_payload.h>


static const char *TAG = "HAP Sensor Switch";

/*  Required for server verification during OTA, PEM format as string  */
char server_cert[] = {};

#define SWITCH_TASK_PRIORITY  1
#define SWITCH_TASK_STACKSIZE 4 * 1024
#define SWITCH_TASK_NAME      "hap_switch"

/* LED GPIO pin - you can change this to any available GPIO */
#define LED_GPIO           GPIO_NUM_2

/* Relay GPIO pin for hardware control */
#define RELAY_GPIO         GPIO_NUM_26

/* DHT Sensor GPIO pin */
#define DHT_GPIO           GPIO_NUM_23

/* DHT Sensor type - options: DHT_TYPE_DHT11, DHT_TYPE_AM2301 (DHT22), DHT_TYPE_SI7021 */
#define DHT_TYPE           DHT_TYPE_AM2301  /* DHT22/AM2301 - change to DHT_TYPE_DHT11 for DHT11 */

/* Temperature and humidity update interval in seconds */
#define SENSOR_UPDATE_INTERVAL  30

/* Reset network credentials if button is pressed for more than 3 seconds and then released */
#define RESET_NETWORK_BUTTON_TIMEOUT        3

/* Reset to factory if button is pressed and held for more than 10 seconds */
#define RESET_TO_FACTORY_BUTTON_TIMEOUT     10

/* The button "Boot" will be used as the Reset button for the example */
#define RESET_GPIO  GPIO_NUM_0

/* Global variables for temperature and humidity services */
static hap_serv_t *temp_service = NULL;
static hap_serv_t *humidity_service = NULL;
static hap_char_t *temp_char = NULL;
static hap_char_t *humidity_char = NULL;

/* Global variables to store current sensor values */
static float current_temperature = 20.0;
static float current_humidity = 50.0;

/**
 * @brief The network reset button callback handler.
 * Useful for testing the Wi-Fi re-configuration feature of WAC2
 */
static void reset_network_handler(void* arg)
{
    hap_reset_network();
}
/**
 * @brief The factory reset button callback handler.
 */
static void reset_to_factory_handler(void* arg)
{
    hap_reset_to_factory();
}

/**
 * The Reset button  GPIO initialisation function.
 * Same button will be used for resetting Wi-Fi network as well as for reset to factory based on
 * the time for which the button is pressed.
 */
static void reset_key_init(uint32_t key_gpio_pin)
{
    button_handle_t handle = iot_button_create(key_gpio_pin, BUTTON_ACTIVE_LOW);
    iot_button_add_on_release_cb(handle, RESET_NETWORK_BUTTON_TIMEOUT, reset_network_handler, NULL);
    iot_button_add_on_press_cb(handle, RESET_TO_FACTORY_BUTTON_TIMEOUT, reset_to_factory_handler, NULL);
}

/**
 * @brief Initialize the LED GPIO
 */
static void led_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    /* Start with LED off */
    gpio_set_level(LED_GPIO, 0);
    ESP_LOGI(TAG, "LED initialized on GPIO %d", LED_GPIO);
}

/**
 * @brief Turn LED on
 */
static void led_on(void)
{
    gpio_set_level(LED_GPIO, 1);
    ESP_LOGI(TAG, "LED turned ON");
}

/**
 * @brief Turn LED off
 */
static void led_off(void)
{
    gpio_set_level(LED_GPIO, 0);
    ESP_LOGI(TAG, "LED turned OFF");
}

/**
 * @brief Initialize the Relay GPIO
 */
static void relay_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    /* Start with relay off */
    gpio_set_level(RELAY_GPIO, 0);
    ESP_LOGI(TAG, "Relay initialized on GPIO %d", RELAY_GPIO);
}

/**
 * @brief Turn relay on
 */
static void relay_on(void)
{
    gpio_set_level(RELAY_GPIO, 1);
    ESP_LOGI(TAG, "Relay turned ON");
}

/**
 * @brief Turn relay off
 */
static void relay_off(void)
{
    gpio_set_level(RELAY_GPIO, 0);
    ESP_LOGI(TAG, "Relay turned OFF");
}

/**
 * @brief Initialize the DHT sensor
 */
static void dht_init(void)
{
    /* Enable internal pull-up resistor for the DHT data line */
    gpio_pullup_en(DHT_GPIO);
    ESP_LOGI(TAG, "DHT sensor configured on GPIO %d with internal pull-up", DHT_GPIO);
}

/**
 * @brief Update both temperature and humidity characteristics from a single DHT read
 */
static void update_sensor_values(void)
{
    float humidity, temperature;
    esp_err_t ret = dht_read_float_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature);
    if (ret == ESP_OK) {
        current_temperature = temperature;
        current_humidity = humidity;
        if (temp_char) {
            hap_val_t temp_val = { .f = temperature };
            hap_char_update_val(temp_char, &temp_val);
        }
        if (humidity_char) {
            hap_val_t humidity_val = { .f = humidity };
            hap_char_update_val(humidity_char, &humidity_val);
        }
        ESP_LOGI(TAG, "Sensor updated: %.1f°C, %.1f%%", temperature, humidity);
    } else {
        ESP_LOGE(TAG, "Failed to read DHT sensor: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Task to periodically update temperature and humidity values
 */
static void sensor_update_task(void *pvParameters)
{
    while (1) {
        update_sensor_values();
        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL * 1000));
    }
}

/**
 * @brief Read callback for temperature sensor
 */
static int temperature_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
    if (hap_req_get_ctrl_id(read_priv)) {
        ESP_LOGI(TAG, "Temperature read request from %s", hap_req_get_ctrl_id(read_priv));
    }
    hap_val_t temp_val = { .f = current_temperature };
    hap_char_update_val(hc, &temp_val);
    *status_code = HAP_STATUS_SUCCESS;
    return HAP_SUCCESS;
}

/**
 * @brief Read callback for humidity sensor
 */
static int humidity_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
    if (hap_req_get_ctrl_id(read_priv)) {
        ESP_LOGI(TAG, "Humidity read request from %s", hap_req_get_ctrl_id(read_priv));
    }
    hap_val_t humidity_val = { .f = current_humidity };
    hap_char_update_val(hc, &humidity_val);
    *status_code = HAP_STATUS_SUCCESS;
    return HAP_SUCCESS;
}

/* Mandatory identify routine for the accessory.
 * In a real accessory, something like LED blink should be implemented
 * got visual identification
 */
static int switch_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Accessory identified");
    
    /* Blink LED 3 times for identification */
    for (int i = 0; i < 3; i++) {
        led_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        led_off();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    return HAP_SUCCESS;
}

/*
 * An optional HomeKit Event handler which can be used to track HomeKit
 * specific events.
 */
static void switch_hap_event_handler(void* arg, esp_event_base_t event_base, int32_t event, void *data)
{
    switch(event) {
        case HAP_EVENT_PAIRING_STARTED :
            ESP_LOGI(TAG, "Pairing Started");
            break;
        case HAP_EVENT_PAIRING_ABORTED :
            ESP_LOGI(TAG, "Pairing Aborted");
            break;
        case HAP_EVENT_CTRL_PAIRED :
            ESP_LOGI(TAG, "Controller %s Paired. Controller count: %d",
                        (char *)data, hap_get_paired_controller_count());
            break;
        case HAP_EVENT_CTRL_UNPAIRED :
            ESP_LOGI(TAG, "Controller %s Removed. Controller count: %d",
                        (char *)data, hap_get_paired_controller_count());
            break;
        case HAP_EVENT_CTRL_CONNECTED :
            ESP_LOGI(TAG, "Controller %s Connected", (char *)data);
            break;
        case HAP_EVENT_CTRL_DISCONNECTED :
            ESP_LOGI(TAG, "Controller %s Disconnected", (char *)data);
            break;
        case HAP_EVENT_ACC_REBOOTING : {
            char *reason = (char *)data;
            ESP_LOGI(TAG, "Accessory Rebooting (Reason: %s)",  reason ? reason : "null");
            break;
        case HAP_EVENT_PAIRING_MODE_TIMED_OUT :
            ESP_LOGI(TAG, "Pairing Mode timed out. Please reboot the device.");
        }
        default:
            /* Silently ignore unknown events */
            break;
    }
}

/* A dummy callback for handling a read on the switch service.
 * In an actual accessory, this should read from hardware.
 * Read routines are generally not required as the value is available with the HAP core
 * when it is updated from write routines. For external triggers (like switch pressed
 * using physical button), accessories should explicitly call hap_char_update_val()
 * instead of waiting for a read request.
 */
static int switch_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
    if (hap_req_get_ctrl_id(read_priv)) {
        ESP_LOGI(TAG, "Received read from %s", hap_req_get_ctrl_id(read_priv));
    }
    /* For switch service, just return success - the HAP core manages the current value */
    *status_code = HAP_STATUS_SUCCESS;
    return HAP_SUCCESS;
}


/* A dummy callback for handling a write on the Switch service
 * In an actual accessory, this should also control the hardware.
 */
static int switch_write(hap_write_data_t write_data[], int count,
        void *serv_priv, void *write_priv)
{
    if (hap_req_get_ctrl_id(write_priv)) {
        ESP_LOGI(TAG, "Received write from %s", hap_req_get_ctrl_id(write_priv));
    }
    ESP_LOGI(TAG, "Switch Write called with %d characteristics", count);
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        const char *char_uuid = hap_char_get_type_uuid(write->hc);
        
        if (!strcmp(char_uuid, HAP_CHAR_UUID_ON)) {
            ESP_LOGI(TAG, "Switch %s", write->val.b ? "On" : "Off");
            
            /* Control LED and Relay based on switch state */
            if (write->val.b) {
                led_on();
                relay_on();
            } else {
                led_off();
                relay_off();
            }
            
            /* Update the characteristic value */
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            ESP_LOGI(TAG, "Unhandled characteristic: %s", char_uuid);
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    return ret;
}

/*The main thread for handling the Switch Accessory */
static void switch_thread_entry(void *p)
{
    hap_acc_t *accessory;
    hap_serv_t *service;

    /* Configure HomeKit core to make the Accessory name (and thus the WAC SSID) unique,
     * instead of the default configuration wherein only the WAC SSID is made unique.
     */
    hap_cfg_t hap_cfg;
    hap_get_config(&hap_cfg);
    hap_cfg.unique_param = UNIQUE_NAME;
    hap_set_config(&hap_cfg);

    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    /* Initialise the mandatory parameters for Accessory which will be added as
     * the mandatory services internally
     */
    hap_acc_cfg_t cfg = {
        .name = "Esp-Switch-Sensor",
        .manufacturer = "Espressif",
        .model = "EspSwitchSensor02",
        .serial_num = "001122335588",
        .fw_rev = "1.0.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = switch_identify,
        .cid = HAP_CID_SWITCH,
    };
    /* Create accessory object */
    accessory = hap_acc_create(&cfg);

    /* Add a dummy Product Data */
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

    /* Add Wi-Fi Transport service required for HAP Spec R16 */
    hap_acc_add_wifi_transport_service(accessory, 0);

    /* Create the Switch Service. Include the "name" since this is a user visible service  */
    service = hap_serv_switch_create(false);
    hap_serv_add_char(service, hap_char_name_create("Sensor Switch"));

    /* Set the write callback for the service */
    hap_serv_set_write_cb(service, switch_write);

    /* Set the read callback for the service (optional) */
    hap_serv_set_read_cb(service, switch_read);

    /* Add the Switch Service to the Accessory Object */
    hap_acc_add_serv(accessory, service);

    /* Create the Temperature Sensor Service */
    temp_service = hap_serv_temperature_sensor_create(20.0);  /* Initial temperature 20°C */
    hap_serv_add_char(temp_service, hap_char_name_create("Temperature Sensor"));
    temp_char = hap_serv_get_char_by_uuid(temp_service, HAP_CHAR_UUID_CURRENT_TEMPERATURE);
    hap_serv_set_read_cb(temp_service, temperature_read);
    hap_acc_add_serv(accessory, temp_service);

    /* Create the Humidity Sensor Service */
    humidity_service = hap_serv_humidity_sensor_create(50.0);  /* Initial humidity 50% */
    hap_serv_add_char(humidity_service, hap_char_name_create("Humidity Sensor"));
    humidity_char = hap_serv_get_char_by_uuid(humidity_service, HAP_CHAR_UUID_CURRENT_RELATIVE_HUMIDITY);
    hap_serv_set_read_cb(humidity_service, humidity_read);
    hap_acc_add_serv(accessory, humidity_service);

    /* Create the Firmware Upgrade HomeKit Custom Service.
     * Please refer the FW Upgrade documentation under components/homekit/extras/include/hap_fw_upgrade.h
     * and the top level README for more information.
     */
    hap_fw_upgrade_config_t ota_config = {
        .server_cert_pem = server_cert,
    };
    service = hap_serv_fw_upgrade_create(&ota_config);
    /* Add the service to the Accessory Object */
    hap_acc_add_serv(accessory, service);

    /* Add the Accessory to the HomeKit Database */
    hap_add_accessory(accessory);

    /* Register a common button for reset Wi-Fi network and reset to factory.
     */
    reset_key_init(RESET_GPIO);

    /* Query the controller count (just for information) */
    ESP_LOGI(TAG, "Accessory is paired with %d controllers",
                hap_get_paired_controller_count());

    /* Hardware initialization here */
    /* Initialize LED GPIO */
    led_init();

    /* Initialize Relay GPIO */
    relay_init();

    /* Initialize DHT sensor */
    dht_init();

    /* For production accessories, the setup code shouldn't be programmed on to
     * the device. Instead, the setup info, derived from the setup code must
     * be used. Use the factory_nvs_gen utility to generate this data and then
     * flash it into the factory NVS partition.
     *
     * By default, the setup ID and setup info will be read from the factory_nvs
     * Flash partition and so, is not required to set here explicitly.
     *
     * However, for testing purpose, this can be overridden by using hap_set_setup_code()
     * and hap_set_setup_id() APIs, as has been done here.
     */
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
    hap_set_setup_code(CONFIG_EXAMPLE_SETUP_CODE);
    hap_set_setup_id(CONFIG_EXAMPLE_SETUP_ID);
#else
    /* Use custom setup code when factory NVS is not available */
    hap_set_setup_code("123-45-678");
    hap_set_setup_id("SW01");
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
    app_hap_setup_payload("123-45-678", "SW01", true, cfg.cid);
#else
    app_hap_setup_payload("123-45-678", "SW01", false, cfg.cid);
#endif
#endif

    /* Enable Hardware MFi authentication (applicable only for MFi variant of SDK) */
    hap_enable_mfi_auth(HAP_MFI_AUTH_HW);

    /* Initialize Wi-Fi */
    app_wifi_init();

    /* Register an event handler for HomeKit specific events.
     * All event handlers should be registered only after app_wifi_init()
     */
    esp_event_handler_register(HAP_EVENT, ESP_EVENT_ANY_ID, &switch_hap_event_handler, NULL);

    /* After all the initializations are done, start the HAP core */
    hap_start();
    /* Start Wi-Fi */
    app_wifi_start(portMAX_DELAY);
    /* The task ends here. The read/write callbacks will be invoked by the HAP Framework */
    vTaskDelete(NULL);
}

void app_main()
{
    /* Create the main switch accessory task */
    xTaskCreate(switch_thread_entry, SWITCH_TASK_NAME, SWITCH_TASK_STACKSIZE, NULL, 1, NULL);
    
    /* Create the sensor update task with appropriate stack size and priority */
    xTaskCreate(sensor_update_task, "sensor_update_task", 4096, NULL, 5, NULL);
}

