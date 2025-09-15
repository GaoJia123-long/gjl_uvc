
#include <stdio.h>
#include "esp_log.h"
#include "app_https.h"
#include "app_wifi.h"
#include "app_uvc.h"

void app_main(void)
{
    app_wifi_init();
    app_https_init();
    app_uvc_init();
}
