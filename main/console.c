

#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "argtable3/argtable3.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sd_card.h"
#include "logger.h"
#include "config.h"
#include "settings.h"
#include "fileman.h"
#include "firmwareSTM32.h"
#include "firmwareESP32.h"
#include "firmwareWWW.h"


static const char* TAG_CONSOLE = "CONSOLE";

static struct {
    struct arg_str *arg0;
    struct arg_int *val;
    struct arg_end *end;
} settings_args;

static struct {
    struct arg_str *arg0;
    struct arg_str *val;
    struct arg_end *end;
} logger_args;


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
    } else if (!strcmp(settings_args.arg0->sval[0], "print")){
        return settings_print();
    } else {
        return 1;
    }
    
    // ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    
}

static int cmd_logger(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &logger_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, logger_args.end, argv[0]);
        return 1;
    }
    
    if (!strcmp(logger_args.arg0->sval[0],  "mode"))
    {
        if (!strcmp(logger_args.val->sval[0], "csv"))
        {
            return Logger_setCsvLog(LOGMODE_CSV);
        } 
        else  if (!strcmp(logger_args.val->sval[0], "raw")) 
        {
            return Logger_setCsvLog(LOGMODE_RAW);
        } 
    } else if (!strcmp(logger_args.arg0->sval[0], "start"))  {   
        return LogTask_start();
    } else if (!strcmp(logger_args.arg0->sval[0], "stop")) {
        return LogTask_stop();
    } 
    
    return RET_NOK;
    // ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(timeout) );
    
}


static void register_settings_sample_rate(void)
{
 int num_args = 2;
    settings_args.arg0 =
        
    arg_str0(NULL, NULL, "<rate|res|print>", "Set Sample rate, resolution or print the settings");

    settings_args.val =
        
    arg_int1(NULL, NULL, "<n>", "Sample rate or resolution");
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

static void register_logger_cmd(void)
{
    int num_args = 2;
    logger_args.arg0 = 
    arg_str0(NULL, NULL, "<mode|start|stop>", "Mode (csv/raw), start or stop logger.");
    
    logger_args.val =
        
    arg_str0(NULL, NULL, "<csv|raw>", "Set mode to csv or raw data");
    logger_args.end = arg_end(num_args);

    

    const esp_console_cmd_t cmd = {
        .command = "logger",
        .help = "Controller logger mode or start/stop it.",
        .hint = NULL,
        .func = &cmd_logger,
        .argtable = &logger_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}




static void register_restart(void)
{

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

    const esp_console_cmd_t cmd = {
        .command = "sync",
        .help = "STM32 sync settings",
        .hint = NULL,
        .func = &Logger_syncSettings
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_singleshot(void)
{

    const esp_console_cmd_t cmd = {
        .command = "ss",
        .help = "Single shot measurment",
        .hint = NULL,
        .func = &Logger_singleShot
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_update_stm32(){
    const esp_console_cmd_t cmd = {
        .command = "update-stm32",
        .help = "Update STM32 firmware",
        .hint = NULL,
        .func = &flash_stm32
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_update_esp32(){
    const esp_console_cmd_t cmd = {
        .command = "update-esp32",
        .help = "Update ESP32 firmware",
        .hint = NULL,
        .func = &updateESP32
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_update_www(){
    const esp_console_cmd_t cmd = {
        .command = "update-www",
        .help = "Update www files",
        .hint = NULL,
        .func = &update_www
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
    #ifdef DEBUG_CONSOLE
    ESP_LOGI(TAG_CONSOLE, "Command history disabled");
    #endif
    #endif


    // register_log_start();
    // register_log_stop();
    
    register_logger_cmd();
    register_restart();
    register_settings_sample_rate();
    register_singleshot();
    register_stm32_sync();
    register_update_esp32();
    register_update_stm32();
    register_update_www();
    esp_console_register_help_command();

    // esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t hw_config =    ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();

    // Enable console
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}