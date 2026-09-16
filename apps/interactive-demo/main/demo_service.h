#pragma once

#include "demo_model.h"

/* Initialize once before starting the UI task. Device I/O stays in this task. */
esp_err_t demo_service_start(void);
demo_model_t demo_service_snapshot(void);
/* Nonblocking UI-to-service messages. Brightness replaces the pending value. */
esp_err_t demo_service_queue_action(unsigned action);
esp_err_t demo_service_queue_brightness(uint8_t percent);
