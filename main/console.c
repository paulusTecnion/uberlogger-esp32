

#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "u8g2.h"
#include "esp_log.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "argtable3/argtable3.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sd_card.h"
#include "logger.h"
#include "hw_config.h"
#include "settings.h"


static const char* TAG_CONSOLE = "CONSOLE";

static struct {
    struct arg_int *adc_en_level;
    struct arg_end *end;
} adc_en_args;

static struct {
    struct arg_str *arg0;
    struct arg_int *val;
    struct arg_end *end;
} settings_args;

static int adc_enable_pin(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &adc_en_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, adc_en_args.end, argv[0]);
        return 1;
    }
    if (adc_en_args.adc_en_level) {
        
        ESP_LOGI(TAG_CONSOLE, "ADC OUTPUT %d", adc_en_args.adc_en_level->ival[0]);
        gpio_set_level(GPIO_ADC_EN, adc_en_args.adc_en_level->ival[0]);
        // ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    }

    return 0;
}

static int cmd_sample_rate(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &settings_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, settings_args.end, argv[0]);
        return 1;
    }
    
    if (!strcmp(settings_args.arg0->sval[0],  "rate"))
    {
        return settings_set_samplerate(settings_args.val->ival[0]);
    } else if (!strcmp(settings_args.arg0->sval[0], "res"))  {   
        return settings_set_resolution(settings_args.val->ival[0]);
    } else {
        return 1;
    }
    
        // ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    
    

}

static int logger_csvlog_en(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &adc_en_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, adc_en_args.end, argv[0]);
        return 1;
    }
    if (adc_en_args.adc_en_level) {
        
        ESP_LOGI(TAG_CONSOLE, "LOGGER CSV %d", adc_en_args.adc_en_level->ival[0]);
        Logger_setCsvLog(adc_en_args.adc_en_level->ival[0]);
        // ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    }

    return 0;
}



static void register_adc_en_pin(void)
{
 int num_args = 1;
    adc_en_args.adc_en_level =
        
    arg_int0(NULL, NULL, "<0|1>", "GPIO level to trigger wakeup");
    adc_en_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "adc_en",
        .help = "Control adc enable pin",
        .hint = NULL,
        .func = &adc_enable_pin,
        .argtable = &adc_en_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_settings_sample_rate(void)
{
 int num_args = 2;
    settings_args.arg0 =
        
    arg_str0(NULL, NULL, "<rate|res>", "Sample rate or resolution");

    settings_args.samplerate =
        
    arg_int1(NULL, NULL, "<n>", "Sample rate in Hz");
    settings_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "settings",
        .help = "Set resolution or sample rate",
        .hint = NULL,
        .func = &cmd_sample_rate,
        .argtable = &settings_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_log_setcsv(void)
{
 int num_args = 1;
    adc_en_args.adc_en_level =
        
    arg_int0(NULL, NULL, "<0|1>", "Set CSV mode enabled (=1) or disabled (=0)");
    adc_en_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "logger_setcsv",
        .help = "Set CSV log mode",
        .hint = NULL,
        .func = &logger_csvlog_en,
        .argtable = &adc_en_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_log_start(void)
{
     const esp_console_cmd_t cmd = {
        .command = "logger_start",
        .help = "Start logger",
        .hint = NULL,
        .func = &Logger_start,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_log_stop(void)
{
     const esp_console_cmd_t cmd = {
        .command = "logger_stop",
        .help = "Stop logger",
        .hint = NULL,
        .func = &Logger_stop,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_sd_card_init(void)
{
     const esp_console_cmd_t cmd = {
        .command = "sd_init",
        .help = "Initialize SD card and file",
        .hint = NULL,
        .func = &esp_sd_card_init,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_sd_card_close(void)
{
    int num_args = 0;    
    adc_en_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "sd_close",
        .help = "Close file and unmount sd card",
        .hint = NULL,
        .func = &esp_sd_card_close_unmount,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_restart(void)
{
 int num_args = 0;
    
    adc_en_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Restart ESP32",
        .hint = NULL,
        .func = &esp_restart
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_stm32_sync(void)
{
    int num_args = 0;
    
    adc_en_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "stm32_sync",
        .help = "STM32 sync settings",
        .hint = NULL,
        .func = &Logger_syncSettings
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

void init_console(){
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

        /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    
    // repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;
    #if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG_CONSOLE, "Command history enabled");
    #else
    ESP_LOGI(TAG_CONSOLE, "Command history disabled");
    #endif


    register_adc_en_pin();
    register_log_setcsv();
    register_log_start();
    register_log_stop();
    register_restart();
    register_sd_card_init();
    register_sd_card_close();
    register_settings_sample_rate();
    register_stm32_sync();
    esp_console_register_help_command();

    // esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t hw_config =    ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();

    // Enable console
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}