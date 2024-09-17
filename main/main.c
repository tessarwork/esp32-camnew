/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "esp_spiffs.h"

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

static const char *TAG = "camera_capture";

// Configuration for the camera
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1 // Software reset will be performed
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

// Initialize SPIFFS
esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}

// Initialize camera
esp_err_t init_camera(void) {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_PIN_D0;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d7 = CAM_PIN_D7;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 3;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_gain_ctrl(s, 0);        // auto gain off
    s->set_awb_gain(s, 1);         // Auto White Balance enable (0 or 1)
    s->set_exposure_ctrl(s, 0);    // auto exposure off
    s->set_brightness(s, 2);       // (-2 to 2) - set brightness
    s->set_agc_gain(s, 10);        // set gain manually (0 - 30)
    s->set_aec_value(s, 500);      // set exposure manually (0-1200)

    return ESP_OK;
}

void init_opencv_orb();
void orb_features2d(uint8_t* data, int width, int height, int* descriptor, int* desc_size);

// Save image to SPIFFS
esp_err_t save_image_to_spiffs(camera_fb_t *fb) {
    // Open file for writing
    FILE* file = fopen("/spiffs/picture.jpg", "wb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    // Write image data to the file
    fwrite(fb->buf, 1, fb->len, file);
    ESP_LOGI(TAG, "Image written to SPIFFS, size: %zu bytes", fb->len);

    // Close the file
    fclose(file);

    return ESP_OK;
}

esp_err_t save_descriptor_to_spiffs(int* descriptor, int desc_size){ 
    FILE* file = fopen("/spiffs/descriptor.bin", "wb");
    if(!file){ 
        ESP_LOGE(TAG, "Failed to open file for writing descriptor");
        return ESP_FAIL;
    }
    size_t written = fwrite(descriptor, sizeof(int), desc_size, file);
    if(written != desc_size){ 
        ESP_LOGE(TAG, "Failed to write file");
        fclose(file);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Descriptor written to SPIFFS, size: %d integers", desc_size);

    // Close the file
    fclose(file);

    return ESP_OK;
}

esp_err_t camera_capture() {
    // Acquire a frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE("CAM", "Camera Capture Failed");
        return ESP_FAIL;
    }

    // Test Features extraction
    int descriptor[500];
    int desc_size = 0;
    orb_features2d(fb->buf, fb->width, fb->height, descriptor, &desc_size);  

    // Save the image to SPIFFS
    save_image_to_spiffs(fb);

    save_descriptor_to_spiffs(descriptor, desc_size);
    // ESP_LOGI("TEST", "Descriptor: ");
    // printf("%p",descriptor);

    // Return frame buffer to free memory
    esp_camera_fb_return(fb);
    return ESP_OK;
}


void app_main(void) {
    printf("Init\n");

    // Initialize SPIFFS
    if (init_spiffs() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        return;
    }

    init_opencv_orb();

    // Initialize camera
    if (init_camera() != ESP_OK) {
        return;
    }

    int64_t fr_start = esp_timer_get_time();

    // Capture image and save it
    camera_capture();

    int64_t fr_end = esp_timer_get_time();
    float fps = 1 * 1000000 / (fr_end - fr_start);
    ESP_LOGW("OpenCV", "Feature2d decode & compute - %2.2f FPS", fps);
}
