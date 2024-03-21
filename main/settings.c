#include "settings.h"
#include "rest_server.h"
#include "spiffs_settings.h"
#include "esp_wifi_types.h"
#include "wifi.h"
static const char* TAG_SETTINGS = "SETTINGS";
const char * settings_filename_old = "settings.json";
const char * settings_filename = "settings_new.json";

Settings_t _settings;

static int8_t lastEnabledADC = -1;
static int8_t lastEnabledGPIO = -1;

void settings_init()
{
    if (settings_load_persisted_settings() != ESP_OK)
    {
        settings_set_default();
        settings_persist_settings();
    }
}

// Determines the last enabled channel for ADC and GPIO inputs
void settings_determine_last_enabled_channel()
{
    for (int i = NUM_ADC_CHANNELS - 1; i >= 0; i--)
    {
        if (settings_get_adc_channel_enabled(i))
        {
            lastEnabledADC = i;
            break; // Found the last enabled ADC, exit the loop
        }

        if (i == 0)
        {
            lastEnabledADC = -1;
        }
    }

    for (int i = 5; i >= 0; i--) // Assuming 6 DI channels
    {
        if (settings_get_gpio_channel_enabled(i))
        {
            lastEnabledGPIO = i;
            break; // Found the last enabled GPIO, exit the loop
        }

        if (i == 0)
        {
            lastEnabledGPIO = -1;
        }
    }
}

int8_t settings_get_last_enabled_ADC_channel()
{
    return lastEnabledADC;
}

int8_t settings_get_last_enabled_GPIO_channel()
{
    return lastEnabledGPIO;
}

uint8_t settings_get_adc_channel_enabled(adc_channel_t channel)
{
    return _settings.adc_channels_enabled & (0x01 << channel);
}


esp_err_t settings_set_adc_channel_enabled(adc_channel_t channel, adc_channel_enable_t value)
{
    // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.adc_channels_enabled = _settings.adc_channels_enabled & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.adc_channels_enabled |= ((value << channel));

    return ESP_OK;
}

uint8_t settings_get_adc_channel_enabled_all()
{
    return _settings.adc_channels_enabled;
}


uint8_t settings_get_adc_channel_type(Settings_t *settings, adc_channel_t channel)
{
    return _settings.adc_channel_type & (0x01 << channel);
}

uint8_t settings_get_adc_channel_type_all()
{
    return _settings.adc_channel_type;
}

esp_err_t settings_set_adc_channel_type(adc_channel_t channel, adc_channel_type_t value)
{
     // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.adc_channel_type = _settings.adc_channel_type & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.adc_channel_type |= ((value << channel));
    return ESP_OK;
}

Settings_t* settings_get(){
    return &_settings;
}

uint8_t settings_get_adc_channel_range(Settings_t * settings, adc_channel_t channel)
{
    return _settings.adc_channel_range & (0x01 << channel);
}

uint8_t settings_get_adc_channel_range_all()
{
    return _settings.adc_channel_range;
}

esp_err_t settings_set_adc_channel_range(adc_channel_t channel, adc_channel_range_t value)
{
      // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.adc_channel_range = _settings.adc_channel_range & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.adc_channel_range |= ((value << channel));

    return ESP_OK;
}

esp_err_t settings_clear_bootreason()
{
    _settings.bootReason = 0;
    if (settings_persist_settings() != ESP_OK)
    {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

uint8_t settings_get_boot_reason()
{
    return _settings.bootReason;
}

uint8_t settings_set_boot_reason(uint8_t reason)
{
    _settings.bootReason = reason;
    settings_persist_settings();
    return ESP_OK;
}

int32_t * settings_get_temp_offsets()
{
    return (int32_t*)_settings.temp_offsets;
}

esp_err_t settings_set_temp_offsets(int32_t * offsets)
{
    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        _settings.temp_offsets[i] = (uint16_t)offsets[i];
    }
    return ESP_OK;
}

int32_t * settings_get_adc_offsets()
{
    if (_settings.adc_resolution == ADC_12_BITS)
    {
        return (int32_t*)_settings.adc_offsets_12b;
    } else if (_settings.adc_resolution == ADC_16_BITS)
    {
        return (int32_t*)_settings.adc_offsets_16b;
    } else {
        return NULL;
    }
    
}

int32_t * settings_get_adc_offsets_12b()
{
    return (int32_t*)_settings.adc_offsets_12b;
}

int32_t * settings_get_adc_offsets_16b()
{
    return (int32_t*)_settings.adc_offsets_16b;
}

esp_err_t settings_set_adc_offset(int32_t * offsets, adc_resolution_t resolution)
{
    if (resolution == ADC_12_BITS)
    {
        for (int i = 0; i < NUM_ADC_CHANNELS; i++)
        {
            _settings.adc_offsets_12b[i] = offsets[i];
        }
    } else if (resolution == ADC_16_BITS)
    {
        for (int i = 0; i < NUM_ADC_CHANNELS; i++)
        {
            _settings.adc_offsets_16b[i] = offsets[i];
        }
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}

Settings_t settings_get_default()
{
    Settings_t default_settings;

    default_settings.adc_resolution = ADC_12_BITS;
    default_settings.adc_log_sample_rate = ADC_SAMPLE_RATE_10Hz; // 10Hz 
    default_settings.adc_channel_type = 0x00; // all channels normal ADC by default
    default_settings.adc_channels_enabled = 0xFF; // all channels are enabled by default
    default_settings.adc_channel_range = 0x00; // 10V by default
     default_settings.gpio_channels_enabled = 0x3F; // 6 always enabled. 2 not available
    default_settings.logMode = LOGMODE_CSV;
    default_settings.bootReason = 0;
    strcpy(_settings.file_prefix, "log");
    _settings.file_name_mode = FILE_NAME_MODE_TIMESTAMP;
    _settings.file_split_size = MAX_FILE_SPLIT_SIZE; // always in BYTES.
    _settings.file_split_size_unit  = FILE_SPLIT_SIZE_UNIT_GB; // 0 = KiB, 1 = MiB, 2 = GiB
    // Get mac address
    char buffer[8];
    wifi_get_trimmed_mac(buffer);
    
    sprintf(default_settings.wifi_ssid_ap, "Uberlogger-%s", buffer);
    
    
    strcpy(default_settings.wifi_ssid, default_settings.wifi_ssid_ap);
    strcpy(default_settings.wifi_password, "");
    default_settings.wifi_mode = WIFI_MODE_AP;
    default_settings.wifi_channel = 1;

    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        default_settings.adc_offsets_12b[i] = (1<<11);
        default_settings.adc_offsets_16b[i] = (1<<15);
        default_settings.temp_offsets[i] = 0;
    }

    return default_settings;
}

esp_err_t settings_set_default()
{
    #ifdef DEBUG_SETTINGS
    ESP_LOGI(TAG_SETTINGS, "Setting default settings");
    #endif
    _settings.adc_resolution = ADC_16_BITS;

    _settings.adc_log_sample_rate = ADC_SAMPLE_RATE_10Hz; // 10Hz 
    _settings.adc_channel_type = 0x00; // all channels normal ADC by default
    _settings.adc_channels_enabled = 0xFF; // all channels are enabled by default
    _settings.adc_channel_range = 0x00; // 10V by default
    _settings.gpio_channels_enabled = 0x3F;
    _settings.logMode = LOGMODE_CSV;
    _settings.bootReason = 0;
    _settings.file_decimal_char = FILE_DECIMAL_CHAR_DOT;
    strcpy(_settings.file_prefix, "log");
    _settings.file_name_mode = FILE_NAME_MODE_TIMESTAMP;
    _settings.file_separator_char = FILE_SEPARATOR_CHAR_COMMA;
    _settings.file_split_size = MAX_FILE_SPLIT_SIZE; // always in BYTES.
    _settings.file_split_size_unit  = FILE_SPLIT_SIZE_UNIT_GB; // 0 = KiB, 1 = MiB, 2 = GiB
    // Get mac address
    char buffer[8];
    wifi_get_trimmed_mac(buffer);
    
    sprintf(_settings.wifi_ssid_ap, "Uberlogger-%s", buffer);
    
    
    strcpy(_settings.wifi_ssid, _settings.wifi_ssid_ap);
    strcpy(_settings.wifi_password, "");
    _settings.wifi_mode = WIFI_MODE_AP;
    _settings.wifi_channel = 1;

    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        _settings.adc_offsets_12b[i] = (1<<11);
        _settings.adc_offsets_16b[i] = (1<<15);
        _settings.temp_offsets[i] = 0;
    }

    return ESP_OK;
}

file_decimal_character_t settings_get_file_decimal_char()
{
    return _settings.file_decimal_char;
}

esp_err_t settings_set_file_decimal_char(file_decimal_character_t decimal_character)
{
    if ((decimal_character == FILE_DECIMAL_CHAR_DOT) ||  (decimal_character == FILE_DECIMAL_CHAR_COMMA))
    {
        _settings.file_decimal_char = decimal_character;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

file_separator_char_t settings_get_file_separator_char()
{
    return _settings.file_separator_char;
}

esp_err_t settings_set_file_separator(file_separator_char_t separator_character)
{
    if ((separator_character == FILE_SEPARATOR_CHAR_COMMA) ||  (separator_character == FILE_SEPARATOR_CHAR_SEMICOLON))
    {
        _settings.file_separator_char = separator_character;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

uint8_t settings_get_gpio_channel_enabled(uint8_t channel)
{
    return _settings.gpio_channels_enabled & (0x01 << channel);
}

esp_err_t settings_set_gpio_channel_enabled(uint8_t channel, uint8_t value)
{
    // strategie: zet bitje van channel X naar 0 en dan set of unset hem. 
    _settings.gpio_channels_enabled = _settings.gpio_channels_enabled & ~(0x01 << channel);
    // Set bit of channel to correct value
    _settings.gpio_channels_enabled |= ((value << channel));
    

    return ESP_OK;
}

uint8_t settings_get_file_name_mode()
{
    return _settings.file_name_mode;
}

esp_err_t settings_set_file_name_mode(uint8_t mode)
{
    if (mode > FILE_NAME_MODE_TIMESTAMP)   
        return ESP_ERR_INVALID_ARG;
    _settings.file_name_mode = mode;
    return ESP_OK;
}


esp_err_t settings_set_file_prefix(const char * prefix)
{
    if (strlen(prefix) > (MAX_FILE_PREFIX_LENGTH - 1))
        return ESP_ERR_INVALID_SIZE;
    
    strcpy(_settings.file_prefix, prefix);
    return ESP_OK;
}

char * settings_get_file_prefix()
{
    return _settings.file_prefix;
}

esp_err_t settings_set_file_split_size(uint32_t size)
{

    uint64_t sizeInKiB = 0;

    switch (_settings.file_split_size_unit)
    {
        case 0:
            // KB
           sizeInKiB = size;
        break;

        case 1:
            // MB
            sizeInKiB = size * 1024;
        break;

        case 2:
            // GB

            sizeInKiB = (size * 1024 * 1024); // From GB to KB. Subtract 1, else we get an overflow
        break;

        default:
        return ESP_ERR_INVALID_ARG;
    }

    if (sizeInKiB < (200) ||
        (sizeInKiB > (MAX_FILE_SPLIT_SIZE/1024) ))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    
    _settings.file_split_size =  (sizeInKiB*1024);
    ESP_LOGI(TAG_SETTINGS, "%llu, %lu", sizeInKiB, _settings.file_split_size);

    
    return ESP_OK;
}

// Returns the file split size in BYTES
uint32_t settings_get_file_split_size()
{
    return _settings.file_split_size;
}

esp_err_t settings_set_file_split_size_unit(uint8_t unit)
{
    if (unit > 2)
        return ESP_ERR_INVALID_ARG;
    _settings.file_split_size_unit = unit;    
    return ESP_OK;
}

uint8_t settings_get_file_split_size_unit()
{
    return _settings.file_split_size_unit;
}


log_mode_t settings_get_logmode()
{
    return _settings.logMode;
}

esp_err_t settings_set_logmode(log_mode_t mode)
{
    _settings.logMode = mode;
    if (mode == LOGMODE_CSV)
    {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "Logmode set to CSV");
        #endif
    } else if (mode == LOGMODE_RAW) {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "Logmode set to RAW");
        #endif
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Set system time. Timestamp in seconds
void settings_set_system_time(time_t timestamp)
{
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0; // Set microseconds to 0, as time_t is typically accurate only to seconds.

    // Set the system time
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG_SETTINGS, "System time set ");

    // Obtain and print the current system time
    time_t now;
    struct tm *timeinfo;
    char buffer[64]; // Make sure this buffer is large enough for your chosen date-time format

    time(&now); // Get the current time
    timeinfo = localtime(&now); // Convert the current time to local time

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo); // Format the time as a string
    ESP_LOGI(TAG_SETTINGS, "Current system time: %llu, %s", timestamp, buffer);
}


uint8_t settings_get_wifi_channel()
{
    return _settings.wifi_channel;
}

esp_err_t settings_set_wifi_channel(uint8_t channel)
{
    if (channel > 0 && channel < 14)
    {
        _settings.wifi_channel = channel;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

char * settings_get_wifi_password()
{
    return _settings.wifi_password;
}

esp_err_t settings_set_wifi_password(char *password)
{
    uint8_t passlen = strlen(password);
    if ( 
        (passlen == 0) ||  // password can be empty
        (passlen > 7 && passlen < MAX_WIFI_PASSW_LEN))
    {
        strcpy(_settings.wifi_password, password);
        return ESP_OK;
    }

    return ESP_FAIL;
}

char * settings_get_wifi_ssid()
{
    return _settings.wifi_ssid;
}

char * settings_get_wifi_ssid_ap()
{
    return _settings.wifi_ssid_ap;
}

esp_err_t settings_set_wifi_ssid(char * ssid)
{
    if (strlen(ssid) < MAX_WIFI_SSID_LEN)
    {
        strcpy(_settings.wifi_ssid, ssid);
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t settings_set_resolution(adc_resolution_t res)
{
    switch (res)
    {
        case ADC_12_BITS:
            _settings.adc_resolution = res;
        break;

        case ADC_16_BITS:
            _settings.adc_resolution = res;
        break;

        default:
        return ESP_FAIL;
    }
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "ADC RESOLUTION = %d", _settings.adc_resolution);
        #endif
        
    return ESP_OK;    
       
}

adc_resolution_t settings_get_resolution()
{
    return _settings.adc_resolution;
}



esp_err_t settings_set_samplerate(adc_sample_rate_t rate)
{
    if (rate >= ADC_SAMPLE_RATE_1Hz && rate < ADC_SAMPLE_RATE_NUM_ITEMS)
    {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "ADC SAMPLE RATE= %d", rate);
        #endif
        _settings.adc_log_sample_rate = rate;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
    
}

adc_sample_rate_t settings_get_samplerate()
{
    return _settings.adc_log_sample_rate;
}


char* settings_read_json_file(FILE* f) {
   
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = (char*)malloc(length + 1);
    if (buffer == NULL) {
        ESP_LOGE("FILE", "Failed to allocate memory for file content");
        fclose(f);
        return NULL;
    }

    fread(buffer, 1, length, f);
    fclose(f);
    buffer[length] = '\0'; // Null-terminate the string
    return buffer;
}


esp_err_t settings_load_json(FILE* f)
{

    char * json = settings_read_json_file(f);

    ESP_LOGI(TAG_SETTINGS, "%s", json);

    if (json == NULL)
    {
        ESP_LOGE(TAG_SETTINGS, "Error parsing JSON setting file.");
        return ESP_FAIL;
    }

    cJSON* root = cJSON_Parse(json);
    if (root == NULL) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE("JSON", "Error before: %s", error_ptr);
        }
        return ESP_FAIL;
    }


   const cJSON* adc_channel_type = cJSON_GetObjectItemCaseSensitive(root, "adc_channel_type");
    if (cJSON_IsNumber(adc_channel_type)) {
        _settings.adc_channel_type = adc_channel_type->valueint;
    }

    const cJSON* adc_channels_enabled = cJSON_GetObjectItemCaseSensitive(root, "adc_channels_enabled");
    if (cJSON_IsNumber(adc_channels_enabled)) {
        _settings.adc_channels_enabled = adc_channels_enabled->valueint;
    }

    const cJSON* adc_channel_range = cJSON_GetObjectItemCaseSensitive(root, "adc_channel_range");
    if (cJSON_IsNumber(adc_channel_range)) {
        _settings.adc_channel_range = adc_channel_range->valueint;
    }

    const cJSON* adc_log_sample_rate = cJSON_GetObjectItemCaseSensitive(root, "adc_log_sample_rate");
    if (cJSON_IsNumber(adc_log_sample_rate)) {
        _settings.adc_log_sample_rate = adc_log_sample_rate->valueint;
    }

    const cJSON* adc_resolution = cJSON_GetObjectItemCaseSensitive(root, "adc_resolution");
    if (cJSON_IsNumber(adc_resolution)) {
        _settings.adc_resolution = adc_resolution->valueint;
    }

    const cJSON* gpio_channels_enabled = cJSON_GetObjectItemCaseSensitive(root, "gpio_channels_enabled");
    if (cJSON_IsNumber(gpio_channels_enabled)) {
        _settings.gpio_channels_enabled = gpio_channels_enabled->valueint;
    }

    const cJSON* logMode = cJSON_GetObjectItemCaseSensitive(root, "log_mode");
    if (cJSON_IsNumber(logMode)) {
        _settings.logMode = logMode->valueint;
    }

    const cJSON* file_decimal_char = cJSON_GetObjectItemCaseSensitive(root, "file_decimal_char");
    if (cJSON_IsNumber(file_decimal_char)) {
        _settings.file_decimal_char = file_decimal_char->valueint;
    }

    const cJSON* file_name_mode = cJSON_GetObjectItemCaseSensitive(root, "file_name_mode");
    if (cJSON_IsNumber(file_name_mode)) {
        _settings.file_name_mode = file_name_mode->valueint;
    }

    const cJSON* file_prefix = cJSON_GetObjectItemCaseSensitive(root, "file_name_prefix");
    if (cJSON_IsString(file_prefix) && file_prefix->valuestring != NULL) {
        strncpy(_settings.file_prefix, file_prefix->valuestring, sizeof(_settings.file_prefix) - 1);
        _settings.file_prefix[sizeof(_settings.file_prefix) - 1] = '\0'; // Ensure null-terminated
    }

    const cJSON* file_separator_char = cJSON_GetObjectItemCaseSensitive(root, "file_separator_char");
    if (cJSON_IsNumber(file_separator_char)) {
        _settings.file_separator_char = file_separator_char->valueint;
    }

    const cJSON* file_split_size = cJSON_GetObjectItemCaseSensitive(root, "file_split_size");
    if (cJSON_IsNumber(file_split_size)) {
        _settings.file_split_size = file_split_size->valuedouble; // valueint returns the incorrect value. 
    }

    const cJSON* file_split_size_unit = cJSON_GetObjectItemCaseSensitive(root, "file_split_size_unit");
    if (cJSON_IsNumber(file_split_size_unit)) {
        _settings.file_split_size_unit = file_split_size_unit->valueint;
    }

    const cJSON* wifi_ssid = cJSON_GetObjectItemCaseSensitive(root, "wifi_ssid");
    if (cJSON_IsString(wifi_ssid) && wifi_ssid->valuestring != NULL) {
        strncpy(_settings.wifi_ssid, wifi_ssid->valuestring, sizeof(_settings.wifi_ssid) - 1);
        _settings.wifi_ssid[sizeof(_settings.wifi_ssid) - 1] = '\0'; // Ensure null-terminated
    }

    const cJSON* wifi_ssid_ap = cJSON_GetObjectItemCaseSensitive(root, "wifi_ssid_ap");
    if (cJSON_IsString(wifi_ssid_ap) && wifi_ssid_ap->valuestring != NULL) {
        strncpy(_settings.wifi_ssid_ap, wifi_ssid_ap->valuestring, sizeof(_settings.wifi_ssid_ap) - 1);
        _settings.wifi_ssid_ap[sizeof(_settings.wifi_ssid_ap) - 1] = '\0'; // Ensure null-terminated
    }

    const cJSON* wifi_password = cJSON_GetObjectItemCaseSensitive(root, "wifi_password");
    if (cJSON_IsString(wifi_password) && wifi_password->valuestring != NULL) {
        strncpy(_settings.wifi_password, wifi_password->valuestring, sizeof(_settings.wifi_password) - 1);
        _settings.wifi_password[sizeof(_settings.wifi_password) - 1] = '\0'; // Ensure null-terminated
    }

    const cJSON* wifi_channel = cJSON_GetObjectItemCaseSensitive(root, "wifi_channel");
    if (cJSON_IsNumber(wifi_channel)) {
        _settings.wifi_channel = wifi_channel->valueint;
    }

    const cJSON* wifi_mode = cJSON_GetObjectItemCaseSensitive(root, "wifi_mode");
    if (cJSON_IsNumber(wifi_mode)) {
        _settings.wifi_mode = wifi_mode->valueint;
    }

    const cJSON* timestamp = cJSON_GetObjectItemCaseSensitive(root, "timestamp");
    if (cJSON_IsNumber(timestamp)) {
        _settings.timestamp = timestamp->valuedouble; // Assuming timestamp is stored as a double
    }

    const cJSON* offsets_12b = cJSON_GetObjectItemCaseSensitive(root, "adc_offsets_12b");
    if (cJSON_IsArray(offsets_12b)) {
        for (int i = 0; i < cJSON_GetArraySize(offsets_12b) && i < NUM_ADC_CHANNELS; i++) {
            cJSON* item = cJSON_GetArrayItem(offsets_12b, i);
            if (cJSON_IsNumber(item)) {
                _settings.adc_offsets_12b[i] = item->valueint;
            }
        }
    }

    const cJSON* offsets_16b = cJSON_GetObjectItemCaseSensitive(root, "adc_offsets_16b");
    if (cJSON_IsArray(offsets_16b)) {
        for (int i = 0; i < cJSON_GetArraySize(offsets_16b) && i < NUM_ADC_CHANNELS; i++) {
            cJSON* item = cJSON_GetArrayItem(offsets_16b, i);
            if (cJSON_IsNumber(item)) {
                _settings.adc_offsets_16b[i] = item->valueint;
            }
        }
    }

    const cJSON* temp_offsets = cJSON_GetObjectItemCaseSensitive(root, "temp_offsets");
    if (cJSON_IsArray(temp_offsets)) {
        for (int i = 0; i < cJSON_GetArraySize(temp_offsets) && i < NUM_ADC_CHANNELS; i++) {
            cJSON* item = cJSON_GetArrayItem(temp_offsets, i);
            if (cJSON_IsNumber(item)) {
                _settings.temp_offsets[i] = item->valueint;
            }
        }
    }

    const cJSON* bootReason = cJSON_GetObjectItemCaseSensitive(root, "boot_reason");
    if (cJSON_IsNumber(bootReason)) {
        _settings.bootReason = bootReason->valueint;
    }


    cJSON_Delete(root);
    free((void*) json);

    return ESP_OK;
}

esp_err_t settings_migrate(Settings_old_t * oldSettings)
{

    _settings.bootReason = oldSettings->bootReason;
    _settings.adc_resolution = oldSettings->adc_resolution;
    _settings.adc_log_sample_rate = oldSettings->log_sample_rate; // 10Hz 
    _settings.adc_channel_type = oldSettings->adc_channel_type; // all channels normal ADC by default
    _settings.adc_channels_enabled = oldSettings->adc_channels_enabled; // all channels are enabled by default
    _settings.adc_channel_range = oldSettings->adc_channel_range; // 10V by default
    _settings.timestamp = oldSettings->timestamp;
    _settings.logMode = oldSettings->logMode;
    
    // strcpy(_settings.file_prefix, "log");
    // _settings.file_name_mode = FILE_NAME_MODE_TIMESTAMP;
    // _settings.file_split_size = MAX_FILE_SPLIT_SIZE; // always in BYTES.
    // _settings.file_split_size_unit  = FILE_SPLIT_SIZE_UNIT_GB; // 0 = KiB, 1 = MiB, 2 = GiB  
    
    strcpy(_settings.wifi_ssid, oldSettings->wifi_ssid);
    strcpy(_settings.wifi_ssid_ap, oldSettings->wifi_ssid_ap);
    strcpy(_settings.wifi_password, oldSettings->wifi_password);
    _settings.wifi_mode = oldSettings->wifi_mode;
    _settings.wifi_channel = oldSettings->wifi_channel;

    for (int i = 0; i < NUM_ADC_CHANNELS; i++)
    {
        _settings.adc_offsets_12b[i] = oldSettings->adc_offsets_12b[i];
        _settings.adc_offsets_16b[i] = oldSettings->adc_offsets_16b[i];
        _settings.temp_offsets[i] = oldSettings->temp_offsets[i];
    }

    // settings_print();

    return ESP_OK;
}

esp_err_t settings_load_persisted_settings()
{
    #ifdef DEBUG_SETTINGS
    ESP_LOGI(TAG_SETTINGS, "Loading persisted settings");
    #endif
    


    // By default load all settings into the new settings struct
    settings_set_default();
    // First check if there is an old settings.json file on the spiffs drive. 
    if (spiffs_init() == ESP_OK)
    {  
        Settings_old_t tmpOldSettings;
        // tmpOldSettings.adc_channel_range = ADC_RANGE_10V;
        // tmpOldSettings.adc_channel_type = (1 << 2) | (1<<4) | (1<<6);
        // tmpOldSettings.adc_channels_enabled = 255;
        // tmpOldSettings.adc_resolution = ADC_16_BITS;
        // for (int i = 0; i < 8; i++)
        // {
        //     tmpOldSettings.adc_offsets_12b[i] = 2024;
        //     tmpOldSettings.adc_offsets_16b[i] = 32343;
        //     tmpOldSettings.temp_offsets[i] = 0;
        // }
        // tmpOldSettings.wifi_channel = 1;
        // strcpy(tmpOldSettings.wifi_password, "");
        // strcpy(tmpOldSettings.wifi_ssid, "UBLOG");
        // strcpy(tmpOldSettings.wifi_ssid_ap, "UBLOG-AP");

        // spiffs_write((char*)&tmpOldSettings, sizeof(tmpOldSettings), settings_filename_old);
        // If old settings exist on spiffs, load them into the settings struct. 
        if (spiffs_read((char*)&tmpOldSettings, sizeof(Settings_old_t), settings_filename_old) == ESP_OK)
        {
            #ifdef DEBUG_SETTINGS
            ESP_LOGI(TAG_SETTINGS, "Old settings.json of spiffs exists. Settings loaded succesfully");
            #endif
            settings_migrate(&tmpOldSettings);
            settings_determine_last_enabled_channel();
            // Remove existing /www/settings.json
            unlink("/www/settings.json");
            // remove old /spiffs/settings.json
            spiffs_delete(settings_filename_old);
            // Store settings onto /spiffs/settings.json as an actual json file
            settings_persist_settings();
            return ESP_OK;     
        } 
    } else {
        return ESP_FAIL;
    }

    // Old settings don't exist
    char buf[30];
    sprintf(buf, "/spiffs/%s", settings_filename);
    // Check if new settings file exists
    FILE *fsetting = fopen(buf, "r");
    if (fsetting != NULL)  // Check if new settings.json exists.
    { 
        // read the json file and load the settings
        ESP_LOGI(TAG_SETTINGS, "Found new settings file. Loading settings");
        settings_load_json(fsetting);
    } else {
        // Both old and new settings don't exist, so this is a new device (or file corrupt). Store settings.
        ESP_LOGW(TAG_SETTINGS, "Error reading settings file, setting and persisting defaults");
        settings_persist_settings();
    }

    settings_determine_last_enabled_channel();
    return ESP_OK;
    
   
}

esp_err_t settings_print()
{
    int i =0;
    
    ESP_LOGI(TAG_SETTINGS, "ADC Resolution %d", _settings.adc_resolution);
    ESP_LOGI(TAG_SETTINGS, "ADC Sample rate %d", _settings.adc_log_sample_rate);
    

    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC ch%d type: %s", i, (_settings.adc_channel_type & (1<<i)) ? "NTC" : "Analog");
    }
    
    
    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC ch%d range:%s", i, (_settings.adc_channel_range & (1<<i)) ? "+/-60V" : "+/-10V");
    }

    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC 12 bit offset %d: %ld", i, _settings.adc_offsets_12b[i]);
    }

    for (i=0; i<8; i++)
    {
        ESP_LOGI(TAG_SETTINGS, "ADC 16 bit offset %d: %ld", i, _settings.adc_offsets_16b[i]);
    }

    ESP_LOGI(TAG_SETTINGS, "ADC Enabled: %d", _settings.adc_channels_enabled);
    ESP_LOGI(TAG_SETTINGS, "GPIO Enabled: %d", _settings.gpio_channels_enabled);
    ESP_LOGI(TAG_SETTINGS, "Log mode: %s", _settings.logMode ? "CSV" : "RAW");
    ESP_LOGI(TAG_SETTINGS, "File decimal character %u (%s)", _settings.file_decimal_char, ((settings_get_file_decimal_char() == FILE_DECIMAL_CHAR_COMMA) ? "," : ".") );
    ESP_LOGI(TAG_SETTINGS, "File prefix %s", _settings.file_prefix);
    ESP_LOGI(TAG_SETTINGS, "File separator character %u (%s)", _settings.file_separator_char, ((settings_get_file_separator_char() == FILE_SEPARATOR_CHAR_COMMA) ? "," : ";") );
    ESP_LOGI(TAG_SETTINGS, "File split size unit: %u", _settings.file_split_size_unit);
    ESP_LOGI(TAG_SETTINGS, "File split size: %lu", _settings.file_split_size);



    ESP_LOGI(TAG_SETTINGS, "Wifi SSID %s", _settings.wifi_ssid);
    ESP_LOGI(TAG_SETTINGS, "Wifi AP SSID %s", _settings.wifi_ssid_ap);
    ESP_LOGI(TAG_SETTINGS, "Wifi channel %d", _settings.wifi_channel);
    
    return ESP_OK;
}

esp_err_t settings_persist_settings()
{
    const char * json = settings_to_json(&_settings);
    settings_determine_last_enabled_channel();
    #ifdef DEBUG_SETTINGS
    ESP_LOGI(TAG_SETTINGS, "Persisting:\n\r %s", json);
    #endif

    if ( spiffs_write(json, strlen(json), settings_filename) == ESP_OK)
    {
        #ifdef DEBUG_SETTINGS
        ESP_LOGI(TAG_SETTINGS, "Settings persisted");
        #endif
        return ESP_OK;     
    }


    // FILE * f = fopen("/www/settings.json", "w");
    // if (f == NULL)
    // {
    //     ESP_LOGE(TAG_SETTINGS, "Failed to open file for writing");
    //     return ESP_FAIL;
    // }
    // if (!fputs(json, f))
    // {
    //     fclose(f);
    // }
    
    free((void *)json);

    /* We don't store data on spiffs anymore */
   

    ESP_LOGE(TAG_SETTINGS, "Persisting settings FAILED");
    return ESP_FAIL;
}


char * settings_to_json(Settings_t *settings)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE("JSON", "Failed to create cJSON object");
        return NULL;;
    }

     // Add data to the cJSON object
    cJSON_AddNumberToObject(root, "adc_channel_type", _settings.adc_channel_type);
    cJSON_AddNumberToObject(root, "adc_channels_enabled", _settings.adc_channels_enabled);
    cJSON_AddNumberToObject(root, "adc_channel_range", _settings.adc_channel_range);
    cJSON_AddNumberToObject(root, "adc_log_sample_rate", _settings.adc_log_sample_rate);
    cJSON_AddNumberToObject(root, "adc_resolution", _settings.adc_resolution);
    cJSON_AddNumberToObject(root, "gpio_channels_enabled", _settings.gpio_channels_enabled);
    cJSON_AddNumberToObject(root, "log_mode", _settings.logMode);
    cJSON_AddNumberToObject(root, "file_decimal_char", _settings.file_decimal_char);
    cJSON_AddNumberToObject(root, "file_name_mode", _settings.file_name_mode);
    cJSON_AddStringToObject(root, "file_name_prefix", _settings.file_prefix);
    cJSON_AddNumberToObject(root, "file_separator_char", _settings.file_separator_char);
    cJSON_AddNumberToObject(root, "file_split_size", _settings.file_split_size);
    cJSON_AddNumberToObject(root, "file_split_size_unit", _settings.file_split_size_unit);
    cJSON_AddNumberToObject(root, "wifi_channel", _settings.wifi_channel);
    cJSON_AddNumberToObject(root, "wifi_mode", _settings.wifi_mode);
    cJSON_AddStringToObject(root, "wifi_ssid", _settings.wifi_ssid);
    cJSON_AddStringToObject(root, "wifi_ssid_ap", _settings.wifi_ssid_ap);
    cJSON_AddStringToObject(root, "wifi_password", _settings.wifi_password); 
    cJSON_AddNumberToObject(root, "timestamp", _settings.timestamp);
    cJSON_AddNumberToObject(root, "boot_reason", _settings.bootReason);

    // Create JSON arrays for the offset values
    cJSON *adc_offsets_12b_array = cJSON_CreateArray();
    for (int i = 0; i < NUM_ADC_CHANNELS; i++) {
        cJSON_AddItemToArray(adc_offsets_12b_array, cJSON_CreateNumber(_settings.adc_offsets_12b[i]));
    }

    cJSON *adc_offsets_16b_array = cJSON_CreateArray();
    for (int i = 0; i < NUM_ADC_CHANNELS; i++) {
        cJSON_AddItemToArray(adc_offsets_16b_array, cJSON_CreateNumber(_settings.adc_offsets_16b[i]));
    }

    // For temp_offsets, since it's an int16_t array, we need to create the JSON array manually
    cJSON *temp_offsets_array = cJSON_CreateArray();
    for (int i = 0; i < NUM_ADC_CHANNELS; i++) {
        cJSON_AddItemToArray(temp_offsets_array, cJSON_CreateNumber(_settings.temp_offsets[i]));
    }

    // Add the arrays to the root object
    cJSON_AddItemToObject(root, "adc_offsets_12b", adc_offsets_12b_array);
    cJSON_AddItemToObject(root, "adc_offsets_16b", adc_offsets_16b_array);
    cJSON_AddItemToObject(root, "temp_offsets", temp_offsets_array);

    // Convert cJSON object to string (same as before)
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root); // Cleanup

    return json_string;
}

esp_err_t settings_set_timestamp(uint64_t timestamp)
{
    // take timestamp and convert to day, month, year and time
    #ifdef DEBUG_SETTINGS
    ESP_LOGI(TAG_SETTINGS, "Timstamp: %d", timestamp);
    #endif
    // Expecting this in ms, but convert it to seconds
    ESP_LOGI(TAG_SETTINGS, "Incoming timestamp: %lld, outgoing %ld", timestamp, (uint32_t)(timestamp/1000));

    _settings.timestamp = (uint32_t)(timestamp/1000);
    settings_set_system_time(_settings.timestamp);
    return ESP_OK;
}

uint32_t settings_get_timestamp()
{
    return _settings.timestamp;
}

uint8_t settings_get_wifi_mode()
{
    return _settings.wifi_mode;
}

esp_err_t settings_set_wifi_mode(uint8_t mode)
{
    if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP)
    {
        _settings.wifi_mode = mode;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}