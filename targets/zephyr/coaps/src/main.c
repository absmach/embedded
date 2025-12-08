#include "ca_cert.h"
#include "config.h"
#include "wifi.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/udp.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(coaps_client);

static struct net_if *sta_iface;

/* CoAP client socket */
static int coap_sock = -1;
static struct sockaddr_in magistrala_addr;

#define MAX_COAP_MSG_LEN 512

/* DTLS credential tag */
#define DTLS_SEC_TAG 1

typedef struct
{
  double temperature;
  double humidity;
  int battery_level;
  bool led_state;
} sensor_data_t;

static sensor_data_t current_data = {.temperature = 23.5,
                                     .humidity = 65.0,
                                     .battery_level = 85,
                                     .led_state = false};

/* Network event callback */
static struct net_mgmt_event_callback mgmt_cb;
static K_SEM_DEFINE(dhcp_sem, 0, 1);

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint32_t mgmt_event, struct net_if *iface)
{
  if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND)
  {
    LOG_INF("DHCP bound - got IP address");
    k_sem_give(&dhcp_sem);
  }
}

static int resolve_hostname(const char *hostname,
                            struct sockaddr_storage *addr)
{
  int ret;
  struct addrinfo *result;
  struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_DGRAM,
  };

  LOG_INF("Resolving %s...", hostname);

  ret = getaddrinfo(hostname, NULL, &hints, &result);
  if (ret != 0)
  {
    LOG_ERR("getaddrinfo failed: %d", ret);
    return -EINVAL;
  }

  struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
  memcpy(addr4, result->ai_addr, sizeof(struct sockaddr_in));

  char addr_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &addr4->sin_addr, addr_str, sizeof(addr_str));
  LOG_INF("Resolved to %s", addr_str);

  freeaddrinfo(result);
  return 0;
}

static void update_sensor_data(sensor_data_t *data)
{
  data->temperature = 20.0 + (sys_rand32_get() % 1000) / 100.0; // 20-30°C
  data->humidity = 50.0 + (sys_rand32_get() % 3000) / 100.0;    // 50-80%
  data->battery_level = 70 + (sys_rand32_get() % 30);           // 70-99%
  data->led_state = !data->led_state;                           // Toggle LED
}

static int setup_dtls_credentials(void)
{
  int ret;

  ret = tls_credential_add(DTLS_SEC_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_cert,
                           sizeof(ca_cert));
  if (ret < 0)
  {
    LOG_ERR("Failed to add CA certificate: %d", ret);
    return ret;
  }

  LOG_INF("DTLS credentials registered");
  return 0;
}

static int coap_client_init(void)
{
  int ret;
  int proto = IPPROTO_DTLS_1_0;

  LOG_INF("Using DTLS 1.2 protocol (proto=%d)", proto);

  /* Create DTLS socket */
  coap_sock = socket(AF_INET, SOCK_DGRAM, proto);
  if (coap_sock < 0)
  {
    LOG_ERR("Failed to create CoAPS socket (proto=%d): %d", proto, errno);
    return -errno;
  }

  /* Configure DTLS socket options */

  /* Set DTLS security tag */
  sec_tag_t sec_tag_opt[] = {DTLS_SEC_TAG};
  ret = setsockopt(coap_sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt,
                   sizeof(sec_tag_opt));
  if (ret < 0)
  {
    LOG_ERR("Failed to set DTLS_SEC_TAG option: %d", errno);
    close(coap_sock);
    return -errno;
  }

  /* Set hostname for DTLS SNI */
  ret = setsockopt(coap_sock, SOL_TLS, TLS_HOSTNAME, MAGISTRALA_HOSTNAME,
                   strlen(MAGISTRALA_HOSTNAME));
  if (ret < 0)
  {
    LOG_ERR("Failed to set TLS_HOSTNAME option: %d", errno);
    close(coap_sock);
    return -errno;
  }

  int verify = TLS_PEER_VERIFY_REQUIRED;
  ret = setsockopt(coap_sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
  if (ret < 0)
  {
    LOG_ERR("Failed to set TLS_PEER_VERIFY option: %d", errno);
    close(coap_sock);
    return -errno;
  }

  LOG_INF("DTLS socket configured for %s (peer verify disabled)", MAGISTRALA_HOSTNAME);

  struct sockaddr_storage server_addr;
  ret = resolve_hostname(MAGISTRALA_HOSTNAME, &server_addr);
  if (ret < 0)
  {
    LOG_ERR("Failed to resolve hostname: %d", ret);
    close(coap_sock);
    return ret;
  }

  memcpy(&magistrala_addr, &server_addr, sizeof(magistrala_addr));
  magistrala_addr.sin_port = htons(MAGISTRALA_COAPS_PORT);

  char addr_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &magistrala_addr.sin_addr, addr_str, sizeof(addr_str));
  LOG_INF("Magistrala CoAPS client initialized - %s:%d", addr_str,
          MAGISTRALA_COAPS_PORT);

  return 0;
}

static int send_coap_message(const char *uri_path, const char *payload,
                             size_t payload_len)
{
  static uint8_t request_buf[MAX_COAP_MSG_LEN];
  static uint8_t response_buf[MAX_COAP_MSG_LEN];
  struct coap_packet request, response;
  int ret, recv_len;
  struct sockaddr_in recv_addr;
  socklen_t recv_addr_len = sizeof(recv_addr);

  /* Initialize CoAP request */
  ret = coap_packet_init(&request, request_buf, sizeof(request_buf),
                         COAP_VERSION_1, COAP_TYPE_CON, COAP_TOKEN_MAX_LEN,
                         coap_next_token(), COAP_METHOD_POST, coap_next_id());
  if (ret < 0)
  {
    LOG_ERR("Failed to init CoAP request: %d", ret);
    return ret;
  }

  /* Add URI path */
  ret = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, uri_path,
                                  strlen(uri_path));
  if (ret < 0)
  {
    LOG_ERR("Failed to add URI path: %d", ret);
    return ret;
  }

  /* Add authorization header with Client secret */
  char auth_query[128];
  ret = snprintf(auth_query, sizeof(auth_query), "auth=%s", CLIENT_SECRET);
  if (ret >= sizeof(auth_query))
  {
    LOG_ERR("URI path too large");
    return -E2BIG;
  }

  ret = coap_packet_append_option(&request, COAP_OPTION_URI_QUERY, auth_query,
                                  strlen(auth_query));
  if (ret < 0)
  {
    LOG_ERR("Failed to add auth query: %d", ret);
    return ret;
  }

  /* Add content format for JSON */
  if (payload && payload_len > 0)
  {
    /* Add payload */
    ret = coap_packet_append_payload_marker(&request);
    if (ret < 0)
    {
      LOG_ERR("Failed to add payload marker: %d", ret);
      return ret;
    }

    ret = coap_packet_append_payload(&request, payload, payload_len);
    if (ret < 0)
    {
      LOG_ERR("Failed to add payload: %d", ret);
      return ret;
    }
  }

  /* Send request */
  ret = sendto(coap_sock, request_buf, request.offset, 0,
               (struct sockaddr *)&magistrala_addr, sizeof(magistrala_addr));
  if (ret < 0)
  {
    LOG_ERR("Failed to send CoAP request: %d", errno);
    return -errno;
  }

  LOG_INF("CoAPS request sent to %s", uri_path);

  /* Wait for response (with timeout) */
  struct pollfd pfd = {.fd = coap_sock, .events = POLLIN};

  ret = poll(&pfd, 1, 5000); // 5 second timeout
  if (ret <= 0)
  {
    LOG_WRN("CoAPS response timeout or error: %d", ret);
    return -ETIMEDOUT;
  }

  /* Receive response */
  recv_len = recvfrom(coap_sock, response_buf, sizeof(response_buf), 0,
                      (struct sockaddr *)&recv_addr, &recv_addr_len);
  if (recv_len < 0)
  {
    LOG_ERR("Failed to receive CoAP response: %d", errno);
    return -errno;
  }

  /* Parse response */
  ret = coap_packet_parse(&response, response_buf, recv_len, NULL, 0);
  if (ret < 0)
  {
    LOG_ERR("Failed to parse CoAP response: %d", ret);
    return ret;
  }

  uint8_t response_code = coap_header_get_code(&response);
  LOG_DBG("CoAPS response code: %d", response_code);

  return 0;
}

static int wait_for_ip_address(struct net_if *iface)
{
  struct net_if_addr *if_addr;
  const int max_timeout = 30; // 30 seconds max wait

  /* Check if we already have an IP */
  if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
  if (if_addr)
  {
    char addr_str[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &if_addr->address.in_addr, addr_str,
                  sizeof(addr_str));
    LOG_INF("Already have IP address: %s", addr_str);
    return 0;
  }

  LOG_INF("Waiting for IP address via DHCP...");

  /* Wait for DHCP with timeout */
  int ret = k_sem_take(&dhcp_sem, K_SECONDS(max_timeout));
  if (ret != 0)
  {
    LOG_ERR("Timeout waiting for DHCP - checking if we have IP anyway");

    /* Final check if we somehow got an IP */
    if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (!if_addr)
    {
      LOG_ERR("No IP address available");
      return -ETIMEDOUT;
    }
  }

  /* Verify we have a valid IP */
  if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
  if (if_addr)
  {
    char addr_str[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &if_addr->address.in_addr, addr_str,
                  sizeof(addr_str));
    LOG_INF("Got IP address: %s", addr_str);
    return 0;
  }

  LOG_ERR("Still no valid IP address");
  return -1;
}

static int send_telemetry(void)
{
  char senml_payload[512];
  int ret;

  update_sensor_data(&current_data);

  int64_t unix_time = k_uptime_get() / 1000 + 1761835000; // Approximate Unix time

  ret = snprintf(senml_payload, sizeof(senml_payload),
                 "[{\"bn\":\"esp32s3:\",\"bt\":%lld,\"bu\":\"Cel\",\"bver\":5,"
                 "\"n\":\"temperature\",\"u\":\"Cel\",\"v\":%.2f},"
                 "{\"n\":\"humidity\",\"u\":\"%%RH\",\"v\":%.2f},"
                 "{\"n\":\"battery\",\"u\":\"%%\",\"v\":%d},"
                 "{\"n\":\"led\",\"vb\":%s}]",
                 unix_time, current_data.temperature, current_data.humidity,
                 current_data.battery_level, current_data.led_state ? "true" : "false");

  if (ret >= sizeof(senml_payload))
  {
    LOG_ERR("SenML payload too large");
    return -E2BIG;
  }

  // Construct URI path: m/{domain_id}/c/{channel_id} --auth {client_secret}
  char uri_path[128];
  ret =
      snprintf(uri_path, sizeof(uri_path), "m/%s/c/%s", DOMAIN_ID, CHANNEL_ID);
  if (ret >= sizeof(uri_path))
  {
    LOG_ERR("URI path too large");
    return -E2BIG;
  }

  ret = send_coap_message(uri_path, senml_payload, strlen(senml_payload));
  if (ret < 0)
  {
    LOG_ERR("Failed to send telemetry: %d", ret);
    return ret;
  }

  LOG_INF("Telemetry sent: temp=%.2f°C, humidity=%.2f%%, battery=%d%%, led=%s, time=%lld",
          current_data.temperature, current_data.humidity,
          current_data.battery_level, current_data.led_state ? "ON" : "OFF", unix_time);

  return 0;
}

int main(void)
{
  LOG_INF("Magistrala CoAPS Client Starting");

  k_sleep(K_SECONDS(5));

  LOG_INF("Initializing wifi");

  initialize_wifi();

  /* Setup network management callback for DHCP events */
  net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                               NET_EVENT_IPV4_DHCP_BOUND);
  net_mgmt_add_event_callback(&mgmt_cb);

  sta_iface = net_if_get_wifi_sta();
  if (sta_iface == NULL)
  {
    LOG_ERR("Failed to get WiFi STA interface");
    LOG_ERR("Restarting system in 5 seconds...");
    k_sleep(K_SECONDS(5));
    sys_reboot(SYS_REBOOT_COLD);
    return -ENODEV;
  }

  int ret = connect_to_wifi(sta_iface, WIFI_SSID, WIFI_PSK);
  if (ret)
  {
    LOG_ERR("Unable to Connect to (%s)", WIFI_SSID);
    LOG_ERR("Restarting system in 5 seconds...");
    k_sleep(K_SECONDS(5));
    sys_reboot(SYS_REBOOT_COLD);
    return ret;
  }

  ret = wait_for_ip_address(sta_iface);
  if (ret < 0)
  {
    LOG_ERR("Failed to get IP address: %d", ret);
    LOG_ERR("Restarting system in 5 seconds...");
    k_sleep(K_SECONDS(5));
    sys_reboot(SYS_REBOOT_COLD);
    return ret;
  }

  /* Add a small delay to ensure network is fully ready */
  k_sleep(K_SECONDS(2));

  /* Setup DTLS credentials */
  ret = setup_dtls_credentials();
  if (ret < 0)
  {
    LOG_ERR("Failed to setup DTLS credentials: %d", ret);
    return ret;
  }

  LOG_INF("Will send telemetry every %d seconds", TELEMETRY_INTERVAL_SEC);

  ret = coap_client_init();
  if (ret < 0)
  {
    LOG_ERR("Failed to initialize CoAPS client: %d", ret);
    return ret;
  }

  LOG_INF("Starting telemetry transmission to Magistrala");

  for (int i = 0; i < 10; i++)
  {
    ret = send_telemetry();
    if (ret < 0)
    {
      LOG_ERR("Failed to send telemetry: %d", ret);
    }
    k_sleep(K_SECONDS(TELEMETRY_INTERVAL_SEC));
  }

  return 0;
}
