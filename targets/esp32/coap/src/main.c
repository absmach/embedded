#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "protocol_examples_common.h"

#include "coap3/coap.h"

#define COAP_DEFAULT_TIME_SEC 60

const static char *TAG = "CoAP_client";

static int resp_wait = 1;
static coap_optlist_t *optlist = NULL;
static int wait_ms;

static coap_response_t message_handler(coap_session_t *session,
                                       const coap_pdu_t *sent,
                                       const coap_pdu_t *received,
                                       const coap_mid_t mid)
{
    const unsigned char *data = NULL;
    size_t data_len;
    size_t offset;
    size_t total;
    coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);

    if (COAP_RESPONSE_CLASS(rcvd_code) == 2)
    {
        if (coap_get_data_large(received, &data_len, &data, &offset, &total))
        {
            if (data_len != total)
            {
                printf("Unexpected partial data received offset %u, length %u\n", offset, data_len);
            }
            printf("Received:\n%.*s\n", (int)data_len, data);
            resp_wait = 0;
        }
        return COAP_RESPONSE_OK;
    }
    printf("%d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
    if (coap_get_data_large(received, &data_len, &data, &offset, &total))
    {
        printf(": ");
        while (data_len--)
        {
            printf("%c", isprint(*data) ? *data : '.');
            data++;
        }
    }
    printf("\n");
    resp_wait = 0;
    return COAP_RESPONSE_OK;
}

static void coap_log_handler(coap_log_t level, const char *message)
{
    uint32_t esp_level = ESP_LOG_INFO;
    const char *cp = strchr(message, '\n');

    while (cp)
    {
        ESP_LOG_LEVEL(esp_level, TAG, "%.*s", (int)(cp - message), message);
        message = cp + 1;
        cp = strchr(message, '\n');
    }
    if (message[0] != '\000')
    {
        ESP_LOG_LEVEL(esp_level, TAG, "%s", message);
    }
}

static int coap_build_optlist(coap_uri_t *uri)
{
#define BUFSIZE 40
    unsigned char _buf[BUFSIZE];
    unsigned char *buf;
    size_t buflen;
    int res;

    optlist = NULL;

    if (uri->scheme == COAP_URI_SCHEME_COAPS && !coap_dtls_is_supported())
    {
        ESP_LOGE(TAG, "MbedTLS DTLS Client Mode not configured");
        return 0;
    }
    if (uri->scheme == COAP_URI_SCHEME_COAPS_TCP && !coap_tls_is_supported())
    {
        ESP_LOGE(TAG, "MbedTLS TLS Client Mode not configured");
        return 0;
    }
    if (uri->scheme == COAP_URI_SCHEME_COAP_TCP && !coap_tcp_is_supported())
    {
        ESP_LOGE(TAG, "TCP Client Mode not configured");
        return 0;
    }

    if (uri->path.length)
    {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_path(uri->path.s, uri->path.length, buf, &buflen);

        while (res--)
        {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_PATH,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }

    if (uri->query.length)
    {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_query(uri->query.s, uri->query.length, buf, &buflen);

        while (res--)
        {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }
    return 1;
}

static void coap_example_client(void *p)
{
    coap_address_t dst_addr;
    static coap_uri_t uri;
    const char *server_uri = COAP_DEFAULT_DEMO_URI;
    coap_context_t *ctx = NULL;
    coap_session_t *session = NULL;
    coap_pdu_t *request = NULL;
    unsigned char token[8];
    size_t tokenlength;
    coap_addr_info_t *info_list = NULL;
    coap_proto_t proto;
    char tmpbuf[INET6_ADDRSTRLEN];

    /* Initialize libcoap library */
    coap_startup();

    /* Set up the CoAP logging */
    coap_set_log_handler(coap_log_handler);
    coap_set_log_level(EXAMPLE_COAP_LOG_DEFAULT_LEVEL);

    /* Set up the CoAP context */
    ctx = coap_new_context(NULL);
    if (!ctx)
    {
        ESP_LOGE(TAG, "coap_new_context() failed");
        goto clean_up;
    }
    coap_context_set_block_mode(ctx,
                                COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

    coap_register_response_handler(ctx, message_handler);

    if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1)
    {
        ESP_LOGE(TAG, "CoAP server uri %s error", server_uri);
        goto clean_up;
    }
    if (!coap_build_optlist(&uri))
    {
        goto clean_up;
    }

    info_list = coap_resolve_address_info(&uri.host, uri.port, uri.port,
                                          uri.port, uri.port,
                                          0,
                                          1 << uri.scheme,
                                          COAP_RESOLVE_TYPE_REMOTE);

    if (info_list == NULL)
    {
        ESP_LOGE(TAG, "failed to resolve address");
        goto clean_up;
    }
    proto = info_list->proto;
    memcpy(&dst_addr, &info_list->addr, sizeof(dst_addr));
    coap_free_address_info(info_list);

    /* This is to keep the test suites happy */
    coap_print_ip_addr(&dst_addr, tmpbuf, sizeof(tmpbuf));
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);

    /*
     * Note that if the URI starts with just coap:// (not coaps://) the
     * session will still be plain text.
     */
    if (uri.scheme == COAP_URI_SCHEME_COAPS || uri.scheme == COAP_URI_SCHEME_COAPS_TCP || uri.scheme == COAP_URI_SCHEME_COAPS_WS)
    {
#ifndef CONFIG_MBEDTLS_TLS_CLIENT
        ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
        goto clean_up;
#endif /* CONFIG_MBEDTLS_TLS_CLIENT */

#ifdef CONFIG_COAP_MBEDTLS_PSK
        session = coap_start_psk_session(ctx, &dst_addr, &uri, proto);
#endif /* CONFIG_COAP_MBEDTLS_PSK */

#ifdef CONFIG_COAP_MBEDTLS_PKI
        session = coap_start_pki_session(ctx, &dst_addr, &uri, proto);
#endif /* CONFIG_COAP_MBEDTLS_PKI */
    }
    else
    {
        session = coap_new_client_session(ctx, NULL, &dst_addr, proto);
    }
    if (!session)
    {
        ESP_LOGE(TAG, "coap_new_client_session() failed");
        goto clean_up;
    }
#ifdef CONFIG_COAP_WEBSOCKETS
    if (proto == COAP_PROTO_WS || proto == COAP_PROTO_WSS)
    {
        coap_ws_set_host_request(session, &uri.host);
    }
#endif /* CONFIG_COAP_WEBSOCKETS */

    while (1)
    {
        request = coap_new_pdu(coap_is_mcast(&dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON,
                               COAP_REQUEST_CODE_GET, session);
        if (!request)
        {
            ESP_LOGE(TAG, "coap_new_pdu() failed");
            goto clean_up;
        }
        /* Add in an unique token */
        coap_session_new_token(session, &tokenlength, token);
        coap_add_token(request, tokenlength, token);
        coap_add_optlist_pdu(request, &optlist);

        resp_wait = 1;
        coap_send(session, request);

        wait_ms = COAP_DEFAULT_TIME_SEC * 1000;

        while (resp_wait)
        {
            int result = coap_io_process(ctx, wait_ms > 1000 ? 1000 : wait_ms);
            if (result >= 0)
            {
                if (result >= wait_ms)
                {
                    ESP_LOGE(TAG, "No response from server");
                    break;
                }
                else
                {
                    wait_ms -= result;
                }
            }
        }
        for (int countdown = 10; countdown >= 0; countdown--)
        {
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }

clean_up:
    if (optlist)
    {
        coap_delete_optlist(optlist);
        optlist = NULL;
    }
    if (session)
    {
        coap_session_release(session);
    }
    if (ctx)
    {
        coap_free_context(ctx);
    }
    coap_cleanup();

    ESP_LOGI(TAG, "Finished");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(coap_example_client, "coap", 8 * 1024, NULL, 5, NULL);
}
