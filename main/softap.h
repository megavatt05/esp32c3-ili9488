/*
 * ESP32C3-DGW-SA1 — модуль точки доступа (Soft Access Point)
 * Поднимает WPA2-AP, DHCP-сервер, HTTP-веб-интерфейс и DNS-перехват
 * для автоматического открытия страницы управления при подключении телефона.
 */
#pragma once

#include "esp_err.h"

#define DGW_PROJECT_NAME    "ESP32C3-DGW-SA1"   // инженерное название проекта
#define DGW_AP_SSID_PREFIX  "DGW-SA1-"          // префикс SSID (SSID = префикс + MAC)
#define DGW_AP_PASS         "displaygw"         // пароль точки доступа
#define DGW_HTTP_PORT       80                  // порт веб-сервера
#define DGW_MAX_CONN        4                   // максимум клиентов

/**
 * @brief Инициализировать Wi-Fi в режиме SoftAP, DHCP, mDNS, DNS-перехват и веб-сервер.
 * @return ESP_OK при успехе, иначе код ошибки.
 */
esp_err_t softap_start(void);

/** @brief Получить текущий SSID точки доступа (префикс + MAC). */
void softap_get_ssid(char *out, size_t len);

/** @brief Число подключённых к AP клиентов. */
int softap_client_count(void);
