/*
 * ESP32C3-DGW-SA1 — модуль точки доступа (SoftAP) + веб-интерфейс
 *
 * Возможности:
 *  - автоматический старт WPA2-AP при включении (SSID = DGW-SA1-<последние байты MAC>);
 *  - DHCP-сервер (клиенты получают адрес 192.168.4.x, шлюз = IP устройства);
 *  - DNS-перехват (порт 53): любой домен резолвится на IP устройства,
 *    поэтому телефон сам открывает страницу управления при подключении к AP;
 *  - HTTP-сервер на порту 80: страница управления экраном LVGL
 *    (цвет фона, демо-режим, статус клиентов).
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "mdns.h"

#include "esp_http_server.h"
#include "softap.h"
#include "ui.h"

static const char *TAG = "SOFTAP";

// IP-адрес самой точки доступа (шлюз для клиентов)
#define AP_IP_ADDR        "192.168.4.1"
#define AP_IP_NETMASK     "255.255.255.0"

static esp_netif_t *s_ap_netif = NULL;
static httpd_handle_t s_server = NULL;
static volatile int s_client_count = 0;   // число подключённых телефонов/ПК
static volatile bool s_portal_done = false; // хотя бы один клиент подключился

/* ---------- Обработчики событий Wi-Fi ---------- */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_AP_STARTUP) {
            ESP_LOGI(TAG, "Точка доступа запущена");
        } else if (id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)data;
            if (++s_client_count == 1) {
                ESP_LOGI(TAG, "Клиент подключился (mac=" MACSTR ", aid=%d) — откройте http://" AP_IP_ADDR,
                         MAC2STR(ev->mac), ev->aid);
            } else {
                ESP_LOGI(TAG, "Подключён ещё клиент (всего: %d)", s_client_count);
            }
            s_portal_done = true;
            // Автоматически показываем на экране баннер точки доступа
            char ssid_now[40];
            softap_get_ssid(ssid_now, sizeof(ssid_now));
            ui_show_ap_banner(true, ssid_now);
        } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *ev = (wifi_event_ap_stadisconnected_t *)data;
            if (s_client_count > 0) s_client_count--;
            ESP_LOGI(TAG, "Клиент отключился (aid=%d), осталось: %d", ev->aid, s_client_count);
        }
    }
}

/* ---------- DNS-перехват: любой запрос ведёт на страницу управления ---------- */

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

static void dns_task(void *arg)
{
    const int sock = (int)arg;
    uint8_t buf[512];

    while (1) {
        struct sockaddr_in src;
        socklen_t slen = sizeof(src);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&src, &slen);
        if (len < (int)sizeof(dns_header_t) + 1) continue;

        // Разбираем заголовок и имя вопроса, чтобы ответить тем же именем
        dns_header_t *h = (dns_header_t *)buf;
        int idx = sizeof(dns_header_t);
        while (idx < len && buf[idx] != 0) {          // метки имени
            int l = buf[idx];
            if ((l & 0xC0) == 0xC0) { idx += 2; break; } // сжатое имя
            idx += 1 + l;
        }
        idx += 5;                                     // нуль-метка + QTYPE + QCLASS
        if (idx > len) continue;

        // Формируем ответ: тот же ID, флаг QR+RA, одна A-запись = IP точки доступа
        h->flags = htons(0x8180);
        h->qdcount = htons(1);
        h->ancount = htons(1);
        h->nscount = 0;
        h->arcount = 0;

        int pos = idx;
        buf[pos++] = 0xC0; buf[pos++] = 0x0C;         // указатель на имя вопроса
        buf[pos++] = 0x00; buf[pos++] = 0x01;         // тип A
        buf[pos++] = 0x00; buf[pos++] = 0x01;         // класс IN
        buf[pos++] = 0x00; buf[pos++] = 0x00;         // TTL = 0 (не кэшировать)
        buf[pos++] = 0x00; buf[pos++] = 0x00;
        buf[pos++] = 0x00; buf[pos++] = 0x04;         // длина 4 байта
        buf[pos++] = 192; buf[pos++] = 168; buf[pos++] = 4; buf[pos++] = 1;

        sendto(sock, buf, pos, 0, (struct sockaddr *)&src, slen);
    }
}

static esp_err_t start_dns_captive(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Не удалось создать DNS-сокет");
        return ESP_FAIL;
    }
    // Разрешаем повторное использование порта: иначе bind() может отказать,
    // если mDNS/другая служба уже держит сокет на 0.0.0.0
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Не удалось занять порт 53 (DNS)");
        close(sock);
        return ESP_FAIL;
    }
    xTaskCreate(dns_task, "dns_captive", 3072, (void *)sock, 5, NULL);
    ESP_LOGI(TAG, "DNS-перехват активен: любой домен ведёт на http://" AP_IP_ADDR);
    return ESP_OK;
}

/* ---------- HTTP-страница управления ---------- */

static const char PAGE_HEAD[] =
    "<!DOCTYPE html><html lang='ru'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>" DGW_PROJECT_NAME "</title><style>"
    "body{font-family:sans-serif;background:#111;color:#eee;text-align:center;padding:20px}"
    "h2{color:#4fc3f7}.btn{display:inline-block;width:56px;height:56px;margin:6px;"
    "border-radius:12px;border:2px solid #333;font-size:11px;color:#fff;line-height:56px}"
    "input[type=range]{width:80%}#st{color:#8f8;font-size:13px}</style></head><body>";

static const char PAGE_TAIL[] =
    "<h2>ESP32C3-DGW-SA1</h2><p>Display Gateway — SoftAP Edition</p>"
    "<p><b>Цвет экрана:</b></p><div>"
    "<a class=btn style='background:#e53935' href='/color?r=229&g=57&b=53'>RED</a>"
    "<a class=btn style='background:#43a047' href='/color?r=67&g=160&b=71'>GREEN</a>"
    "<a class=btn style='background:#1e88e5' href='/color?r=30&g=136&b=229'>BLUE</a>"
    "<a class=btn style='background:#fdd835;color:#333' href='/color?r=253&g=216&b=53'>YELLOW</a>"
    "<a class=btn style='background:#8e24aa' href='/color?r=142&g=36&b=170'>PURPLE</a>"
    "<a class=btn style='background:#fb8c00' href='/color?r=251&g=140&b=0'>ORANGE</a>"
    "<a class=btn style='background:#fafafa;color:#333' href='/color?r=250&g=250&b=250'>WHITE</a>"
    "<a class=btn style='background:#111' href='/color?r=17&g=17&b=17'>BLACK</a>"
    "</div><p id=st></p>"
    "<script>function st(){fetch('/status').then(r=>r.json()).then(j=>"
    "document.getElementById('st').textContent="
    "'Клиентов: '+j.clients+' | SSID: '+j.ssid)}st();setInterval(st,3000)</script>"
    "</body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, PAGE_HEAD);
    httpd_resp_sendstr(req, PAGE_TAIL);
    return ESP_OK;
}

static esp_err_t color_handler(httpd_req_t *req)
{
    // Смена цвета экрана через LVGL (блокировка внутри ui_set_bg_color)
    char q[64] = {0};
    int r = 0, g = 0, b = 0;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        sscanf(strstr(q, "r=") ? strstr(q, "r=") + 2 : "0", "%d", &r);
        sscanf(strstr(q, "g=") ? strstr(q, "g=") + 2 : "0", "%d", &g);
        sscanf(strstr(q, "b=") ? strstr(q, "b=") + 2 : "0", "%d", &b);
    }
    ui_set_bg_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
    ESP_LOGI(TAG, "Web: цвет фона -> RGB(%d,%d,%d)", r, g, b);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, "<meta http-equiv='refresh' content='0;url=/'/>");
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char ssid[40];
    softap_get_ssid(ssid, sizeof(ssid));
    char json[128];
    snprintf(json, sizeof(json),
             "{\"clients\":%d,\"ssid\":\"%s\",\"ip\":\"" AP_IP_ADDR "\"}",
             s_client_count, ssid);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

static esp_err_t captive_handler(httpd_req_t *req)
{
    // Ответы на проверки порталов Android/iOS/Windows (gen_204, hotspot-detect и т.п.).
    // ВАЖНО: чистый 302 Android-портал игнорирует (ожидает 204 от connectivitycheck),
    // поэтому отдаём минимальную HTML-страницу с <meta refresh> — она срабатывает
    // и в браузере, и в системном диалоге «Подключиться к Интернету».
    const char *uri = req->uri;
    if (strstr(uri, "hotspot-detect") || strstr(uri, "hwdetect") ||
        strstr(uri, "hntest") || strstr(uri, "gen_204") ||
        strstr(uri, "detectportal") || strstr(uri, "redirect")) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req,
            "<html><head><meta http-equiv='refresh' content='0;url=http://"
            AP_IP_ADDR "/'></head></html>");
        return ESP_OK;
    }
    // Остальные неизвестные пути — честный 302 на главную
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP_ADDR "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = DGW_HTTP_PORT;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.max_open_sockets = 7;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось запустить HTTP-сервер");
        return;
    }

    httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
    httpd_uri_t uri_color = { .uri = "/color", .method = HTTP_GET, .handler = color_handler };
    httpd_uri_t uri_status = { .uri = "/status", .method = HTTP_GET, .handler = status_handler };
    // Проверки доступности портала (gen_204, hotspot-detect и т.п.)
    httpd_uri_t uri_captive = { .uri = "/*", .method = HTTP_GET, .handler = captive_handler };

    httpd_register_uri_handler(s_server, &uri_root);
    httpd_register_uri_handler(s_server, &uri_color);
    httpd_register_uri_handler(s_server, &uri_status);
    httpd_register_uri_handler(s_server, &uri_captive);

    ESP_LOGI(TAG, "Веб-интерфейс: http://" AP_IP_ADDR "/");
}

/* ---------- Публичные функции ---------- */

void softap_get_ssid(char *out, size_t len)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(out, len, "%s%02X%02X", DGW_AP_SSID_PREFIX, mac[4], mac[5]);
}

int softap_client_count(void)
{
    return s_client_count;
}

esp_err_t softap_start(void)
{
    // Инициализация NVS обязательна для Wi-Fi
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "NVS не инициализирована");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_ap_netif = esp_netif_create_default_wifi_ap();

    // Статический IP точки доступа
    esp_netif_ip_info_t ip = {0};
    ip.ip.addr = esp_ip4addr_aton(AP_IP_ADDR);
    ip.netmask.addr = esp_ip4addr_aton(AP_IP_NETMASK);
    ip.gw.addr = ip.ip.addr;
    esp_netif_dhcps_stop(s_ap_netif);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_ap_netif, &ip));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(s_ap_netif));  // DHCP для клиентов

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                        &wifi_event_handler, NULL, NULL));

    char ssid[40];
    softap_get_ssid(ssid, sizeof(ssid));

    wifi_config_t wc = {0};
    strlcpy((char *)wc.ap.ssid, ssid, sizeof(wc.ap.ssid));
    strlcpy((char *)wc.ap.password, DGW_AP_PASS, sizeof(wc.ap.password));
    wc.ap.ssid_len = strlen(ssid);
    wc.ap.channel = 6;
    wc.ap.max_connection = DGW_MAX_CONN;
    wc.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    // mDNS: страница также доступна по http://dgw-sa1.local
    mdns_init();
    mdns_hostname_set("dgw-sa1");
    mdns_instance_name_set(DGW_PROJECT_NAME " Display Gateway");

    start_dns_captive();
    http_server_start();

    ESP_LOGI(TAG, "SSID: '%s'  пароль: '%s'  — подключитесь с телефона, страница откроется сама",
             ssid, DGW_AP_PASS);
    return ESP_OK;
}
