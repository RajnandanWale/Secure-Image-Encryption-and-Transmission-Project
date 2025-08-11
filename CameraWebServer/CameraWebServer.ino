#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "mbedtls/aes.h"

#define PIR_SENSOR_PIN 13
const char *ssid = "vivo";
const char *password = "12345678";
const char *serverUrl = "http://192.168.224.112:5000/upload";

// 🔐 16-byte AES key (MUST match the key user enters during signup)
const unsigned char aesKey[16] = {
  'S', 'e', 'c', 'r', 'e', 't', '@', '1',
  '2', '3', '4', '5', '6', '7', '8', '9'
};


#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

void setupCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed!");
        while (1);
    }
    Serial.println("Camera ready!");
}

void encryptAES128CBC(const uint8_t *input, size_t len, uint8_t *output, const uint8_t *key, uint8_t *iv) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);

    // PKCS7 padding
    int pad = 16 - (len % 16);
    size_t padded_len = len + pad;
    uint8_t *padded_input = (uint8_t *)malloc(padded_len);
    memcpy(padded_input, input, len);
    memset(padded_input + len, pad, pad);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, iv, padded_input, output);

    free(padded_input);
    mbedtls_aes_free(&aes);
}

void captureAndSendEncryptedImage() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    size_t padded_len = fb->len + (16 - (fb->len % 16));
    uint8_t *encryptedImage = (uint8_t *)malloc(padded_len);
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = random(0, 256);

    encryptAES128CBC(fb->buf, fb->len, encryptedImage, aesKey, iv);

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/octet-stream");
        http.addHeader("X-IV", String((const char *)iv, 16)); // Send IV as raw header

        int response = http.POST(encryptedImage, padded_len);
        Serial.printf("Encrypted image sent. Response: %d\n", response);
        http.end();
    }

    free(encryptedImage);
    esp_camera_fb_return(fb);
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    pinMode(PIR_SENSOR_PIN, INPUT);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    setupCamera();
}

void loop() {
    if (digitalRead(PIR_SENSOR_PIN) == HIGH) {
        Serial.println("Motion detected!");
        captureAndSendEncryptedImage();
        delay(3000);
    }
}
