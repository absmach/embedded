#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>

#include "config.h"
#include "wifi.h"
#include <zephyr/net/coap.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/udp.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(coap_client);

static struct net_if *sta_iface;

/* CoAP client socket */
static int coap_sock = -1;
static struct sockaddr_in magistrala_addr;

#define MAX_COAP_MSG_LEN 512
#define TELEMETRY_INTERVAL_SEC 30

typedef struct {
  double temperature;
  double humidity;
  int battery_level;
  bool led_state;
} sensor_data_t;

static sensor_data_t current_data = {.temperature = 23.5,
                                     .humidity = 65.0,
                                     .battery_level = 85,
                                     .led_state = false};

static uint16_t message_id = 1;

/* Network event callback */
static struct net_mgmt_event_callback mgmt_cb;
static K_SEM_DEFINE(dhcp_sem, 0, 1);

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint32_t mgmt_event, struct net_if *iface) {
  if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
    LOG_INF("DHCP bound - got IP address");
    k_sem_give(&dhcp_sem);
  }
}

static int coap_client_init(void) {
  int ret;

  /* Create UDP socket */
  coap_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (coap_sock < 0) {
    LOG_ERR("Failed to create CoAP socket: %d", errno);
    return -errno;
  }

  memset(&magistrala_addr, 0, sizeof(magistrala_addr));
  magistrala_addr.sin_family = AF_INET;
  magistrala_addr.sin_port = htons(MAGISTRALA_COAP_PORT);

  ret = inet_pton(AF_INET, MAGISTRALA_IP, &magistrala_addr.sin_addr);
  if (ret != 1) {
    LOG_ERR("Invalid IP address: %s", MAGISTRALA_IP);
    close(coap_sock);
    return -EINVAL;
  }

  LOG_INF("Magistrala CoAP client initialized - IP: %s:%d", MAGISTRALA_IP,
          MAGISTRALA_COAP_PORT);

  return 0;
}

static int send_coap_message(const char *uri_path, const char *payload,
                             size_t payload_len) {
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
  if (ret < 0) {
    LOG_ERR("Failed to init CoAP request: %d", ret);
    return ret;
  }

  /* Add URI path */
  ret = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, uri_path,
                                  strlen(uri_path));
  if (ret < 0) {
    LOG_ERR("Failed to add URI path: %d", ret);
    return ret;
  }

  /* Add authorization header with Client secret */
  char auth_query[128];
  ret = snprintf(auth_query, sizeof(auth_query), "auth=%s", CLIENT_SECRET);
  if (ret >= sizeof(auth_query)) {
    LOG_ERR("URI path too large");
    return -E2BIG;
  }

  ret = coap_packet_append_option(&request, COAP_OPTION_URI_QUERY, auth_query,
                                  strlen(auth_query));
  if (ret < 0) {
    LOG_ERR("Failed to add auth query: %d", ret);
    return ret;
  }

  /* Add content format for JSON */
  if (payload && payload_len > 0) {
    /* Add payload */
    ret = coap_packet_append_payload_marker(&request);
    if (ret < 0) {
      LOG_ERR("Failed to add payload marker: %d", ret);
      return ret;
    }

    ret = coap_packet_append_payload(&request, payload, payload_len);
    if (ret < 0) {
      LOG_ERR("Failed to add payload: %d", ret);
      return ret;
    }
  }

  /* Send request */
  ret = sendto(coap_sock, request_buf, request.offset, 0,
               (struct sockaddr *)&magistrala_addr, sizeof(magistrala_addr));
  if (ret < 0) {
    LOG_ERR("Failed to send CoAP request: %d", errno);
    return -errno;
  }

  LOG_INF("CoAP request sent to %s", uri_path);

  /* Wait for response (with timeout) */
  struct pollfd pfd = {.fd = coap_sock, .events = POLLIN};

  ret = poll(&pfd, 1, 5000); // 5 second timeout
  if (ret <= 0) {
    LOG_WRN("CoAP response timeout or error: %d", ret);
    return -ETIMEDOUT;
  }

  /* Receive response */
  recv_len = recvfrom(coap_sock, response_buf, sizeof(response_buf), 0,
                      (struct sockaddr *)&recv_addr, &recv_addr_len);
  if (recv_len < 0) {
    LOG_ERR("Failed to receive CoAP response: %d", errno);
    return -errno;
  }

  /* Parse response */
  ret = coap_packet_parse(&response, response_buf, recv_len, NULL, 0);
  if (ret < 0) {
    LOG_ERR("Failed to parse CoAP response: %d", ret);
    return ret;
  }

  uint8_t response_code = coap_header_get_code(&response);
  LOG_DBG("CoAP response code: %d", response_code);

  return 0;
}

static int wait_for_ip_address(void) {
  struct net_if_addr *if_addr;
  struct net_if *iface = net_if_get_default();
  int timeout_count = 0;
  const int max_timeout = 30; // 30 seconds max wait

  /* Check if we already have an IP */
  if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
  if (if_addr) {
    char addr_str[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &if_addr->address.in_addr, addr_str,
                  sizeof(addr_str));
    LOG_INF("Already have IP address: %s", addr_str);
    return 0;
  }

  LOG_INF("Waiting for IP address via DHCP...");

  /* Wait for DHCP with timeout */
  int ret = k_sem_take(&dhcp_sem, K_SECONDS(max_timeout));
  if (ret != 0) {
    LOG_ERR("Timeout waiting for DHCP - checking if we have IP anyway");

    /* Final check if we somehow got an IP */
    if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (!if_addr) {
      LOG_ERR("No IP address available");
      return -ETIMEDOUT;
    }
  }

  /* Verify we have a valid IP */
  if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
  if (if_addr) {
    char addr_str[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &if_addr->address.in_addr, addr_str,
                  sizeof(addr_str));
    LOG_INF("Got IP address: %s", addr_str);
    return 0;
  }

  LOG_ERR("Still no valid IP address");
  return -1;
}

static int send_telemetry(void) {
  char json_payload[256];
  int ret;

  ret = snprintf(json_payload, sizeof(json_payload),
                 "{"
                 "\"temperature\":%.1f,"
                 "\"humidity\":%.1f,"
                 "\"battery\":%d,"
                 "\"led_state\":%s,"
                 "\"timestamp\":%lld"
                 "}",
                 current_data.temperature, current_data.humidity,
                 current_data.battery_level,
                 current_data.led_state ? "true" : "false", k_uptime_get());

  if (ret >= sizeof(json_payload)) {
    LOG_ERR("JSON payload too large");
    return -E2BIG;
  }

  // Construct URI path: m/{domain_id}/c/{channel_id} --auth {client_secret}
  char uri_path[128];
  ret =
      snprintf(uri_path, sizeof(uri_path), "m/%s/c/%s", DOMAIN_ID, CHANNEL_ID);
  if (ret >= sizeof(uri_path)) {
    LOG_ERR("URI path too large");
    return -E2BIG;
  }

  ret = send_coap_message(uri_path, json_payload, strlen(json_payload));
  if (ret < 0) {
    LOG_ERR("Failed to send telemetry: %d", ret);
    return ret;
  }

  LOG_INF("Telemetry sent: temp=%.2f °C, humidity=%.2f %%, battery=%d %%",
          current_data.temperature, current_data.humidity,
          current_data.battery_level);

  return 0;
}

int main(void) {
  LOG_INF("Magistrala CoAP Client Starting");

  k_sleep(K_SECONDS(5));

  LOG_INF("Initializing wifi");

  initialize_wifi();

  /* Setup network management callback for DHCP events */
  net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                               NET_EVENT_IPV4_DHCP_BOUND);
  net_mgmt_add_event_callback(&mgmt_cb);

  /* Get STA interface in AP-STA mode. */
  sta_iface = net_if_get_wifi_sta();

  int ret = connect_to_wifi(sta_iface, WIFI_SSID, WIFI_PSK);
  if (ret) {
    LOG_ERR("Unable to Connect to (%s)", WIFI_SSID);
    return ret;
  }

  /* Wait for IP address via DHCP */
  ret = wait_for_ip_address();
  if (ret < 0) {
    LOG_ERR("Failed to get IP address: %d", ret);
    return ret;
  }

  LOG_INF("Will send telemetry every %d seconds", TELEMETRY_INTERVAL_SEC);

  /* Initialize CoAP client */
  ret = coap_client_init();
  if (ret < 0) {
    LOG_ERR("Failed to initialize CoAP client: %d", ret);
    return ret;
  }

  LOG_INF("Starting telemetry transmission to Magistrala");

  /* Send telemetry data */
  ret = send_telemetry();
  if (ret < 0) {
    LOG_ERR("Failed to send telemetry: %d", ret);
  }

  return 0;
}
