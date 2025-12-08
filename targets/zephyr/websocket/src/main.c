#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/reboot.h>

#include "config.h"
#include "wifi.h"
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/udp.h>
#include <zephyr/net/websocket.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(websocket_client);

static struct net_if *sta_iface;

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

#define MAX_RECV_BUF_LEN 512

static uint8_t recv_buf_ipv4[MAX_RECV_BUF_LEN];

/* We need to allocate bigger buffer for the websocket data we receive so that
 * the websocket header fits into it.
 */
#define EXTRA_BUF_SPACE 30

static uint8_t temp_recv_buf_ipv4[MAX_RECV_BUF_LEN + EXTRA_BUF_SPACE];

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

static int wait_for_ip_address(struct net_if *iface) {
  struct net_if_addr *if_addr;
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

static void update_sensor_data(sensor_data_t *data)
{
  data->temperature = 20.0 + (sys_rand32_get() % 1000) / 100.0; // 20-30°C
  data->humidity = 50.0 + (sys_rand32_get() % 3000) / 100.0;    // 50-80%
  data->battery_level = 70 + (sys_rand32_get() % 30);           // 70-99%
  data->led_state = !data->led_state;                           // Toggle LED
}

static int generate_senml_payload(char *buf, size_t buf_len)
{
  update_sensor_data(&current_data);

  int64_t unix_time = k_uptime_get() / 1000 + 1761835000; // Approximate Unix time

  return snprintk(buf, buf_len,
           "[{\"bn\":\"esp32s3:\",\"bt\":%lld,\"bu\":\"Cel\",\"bver\":5,"
           "\"n\":\"temperature\",\"u\":\"Cel\",\"v\":%.2f},"
           "{\"n\":\"humidity\",\"u\":\"%%RH\",\"v\":%.2f},"
           "{\"n\":\"battery\",\"u\":\"%%\",\"v\":%d},"
           "{\"n\":\"led\",\"vb\":%s}]",
           unix_time, current_data.temperature, current_data.humidity,
           current_data.battery_level, current_data.led_state ? "true" : "false");
}

static int resolve_hostname(const char *hostname,
                            struct sockaddr_storage *addr)
{
  int ret;
  struct addrinfo *result;
  struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
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

static int setup_socket(sa_family_t family, int port,
                        int *sock, struct sockaddr *addr, socklen_t addr_len,
                        struct sockaddr_storage *resolved_addr) {
  const char *family_str = family == AF_INET ? "IPv4" : "IPv6";
  int ret = 0;

  memset(addr, 0, addr_len);

  if (family == AF_INET) {
    struct sockaddr_in *addr4 = net_sin(addr);
    struct sockaddr_in *resolved4 = (struct sockaddr_in *)resolved_addr;

    addr4->sin_family = AF_INET;
    addr4->sin_port = htons(port);
    memcpy(&addr4->sin_addr, &resolved4->sin_addr, sizeof(struct in_addr));
  } else {
    net_sin6(addr)->sin6_family = AF_INET6;
    net_sin6(addr)->sin6_port = htons(port);
  }

  *sock = socket(family, SOCK_STREAM, IPPROTO_TCP);

  if (*sock < 0) {
    LOG_ERR("Failed to create %s HTTP socket (%d)", family_str, -errno);
  }

  return ret;
}

static int connect_socket(sa_family_t family, int port,
                          int *sock, struct sockaddr *addr,
                          socklen_t addr_len, struct sockaddr_storage *resolved_addr) {
  int ret;

  ret = setup_socket(family, port, sock, addr, addr_len, resolved_addr);
  if (ret < 0 || *sock < 0) {
    return -1;
  }

  ret = connect(*sock, addr, addr_len);
  if (ret < 0) {
    LOG_ERR("Cannot connect to %s remote (%d)",
            family == AF_INET ? "IPv4" : "IPv6", -errno);
    ret = -errno;
  }

  return ret;
}

static int connect_cb(int sock, struct http_request *req, void *user_data) {
  LOG_INF("Websocket %d for %s connected.", sock, (char *)user_data);

  return 0;
}

static ssize_t sendall_with_ws_api(int sock, const void *buf, size_t len) {
  return websocket_send_msg(sock, buf, len, WEBSOCKET_OPCODE_DATA_TEXT, true,
                            true, SYS_FOREVER_MS);
}


static bool send_senml_msg(int sock, const char *proto,
                           uint8_t *buf, size_t buf_len) {
  int ret;
  int payload_len;

  if (sock < 0) {
    return true;
  }

  payload_len = generate_senml_payload((char *)buf, buf_len);
  if (payload_len < 0) {
    LOG_ERR("Failed to generate SenML payload");
    return false;
  }

  ret = sendall_with_ws_api(sock, buf, payload_len);

  if (ret <= 0) {
    if (ret < 0) {
      LOG_ERR("%s failed to send data (%d)", proto, ret);
    } else {
      LOG_DBG("%s connection closed", proto);
    }

    return false;
  } else {
    LOG_INF("%s sent %d bytes: %s", proto, ret, buf);
  }

  return true;
}

int main(void) {
  LOG_INF("Magistrala Websocket Client Starting");

  k_sleep(K_SECONDS(5));

  LOG_INF("Initializing wifi");

  initialize_wifi();

  /* Setup network management callback for DHCP events */
  net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                               NET_EVENT_IPV4_DHCP_BOUND);
  net_mgmt_add_event_callback(&mgmt_cb);

  /* Get STA interface in AP-STA mode. */
  sta_iface = net_if_get_wifi_sta();
  if (sta_iface == NULL) {
    LOG_ERR("Failed to get WiFi STA interface");
    LOG_ERR("Restarting system in 5 seconds...");
    k_sleep(K_SECONDS(5));
    sys_reboot(SYS_REBOOT_COLD);
    return -ENODEV;
  }

  int ret = connect_to_wifi(sta_iface, WIFI_SSID, WIFI_PSK);
  if (ret) {
    LOG_ERR("Unable to Connect to (%s)", WIFI_SSID);
    LOG_ERR("Restarting system in 5 seconds...");
    k_sleep(K_SECONDS(5));
    sys_reboot(SYS_REBOOT_COLD);
    return ret;
  }

  /* Wait for IP address via DHCP */
  ret = wait_for_ip_address(sta_iface);
  if (ret < 0) {
    LOG_ERR("Failed to get IP address: %d", ret);
    LOG_ERR("Restarting system in 5 seconds...");
    k_sleep(K_SECONDS(5));
    sys_reboot(SYS_REBOOT_COLD);
    return ret;
  }

  /* Add a small delay to ensure network is fully ready */
  k_sleep(K_SECONDS(2));

  LOG_INF("Will send telemetry every %d seconds", TELEMETRY_INTERVAL_SEC);

  /* Resolve hostname */
  struct sockaddr_storage resolved_addr;
  ret = resolve_hostname(MAGISTRALA_HOSTNAME, &resolved_addr);
  if (ret < 0) {
    LOG_ERR("Failed to resolve hostname: %d", ret);
    return ret;
  }

  /* Just an example how to set extra headers */
  const char *extra_headers[] = {"Origin: http://foobar\r\n", NULL};

  int sock4 = -1;
  int websock4 = -1;
  int32_t timeout = 3 * MSEC_PER_SEC;
  struct sockaddr_in addr4;

  ret = connect_socket(AF_INET, MAGISTRALA_WS_PORT, &sock4,
                       (struct sockaddr *)&addr4, sizeof(addr4), &resolved_addr);
  if (ret < 0 || sock4 < 0) {
    LOG_ERR("Cannot create or connect IPv4 HTTP socket.");
    return -1;
  }

  LOG_INF("Connected to IPv4 websocket");

  struct websocket_request req;

  memset(&req, 0, sizeof(req));

  req.host = MAGISTRALA_HOSTNAME;

  // Construct URI path:
  // m/{domain_id}/c/{channel_id}?authorization={client_secret}
  char uri_path[256];
  ret = snprintf(uri_path, sizeof(uri_path), "m/%s/c/%s?authorization=%s",
                 DOMAIN_ID, CHANNEL_ID, CLIENT_SECRET);
  req.url = uri_path;

  req.optional_headers = extra_headers;
  req.cb = connect_cb;
  req.tmp_buf = temp_recv_buf_ipv4;
  req.tmp_buf_len = sizeof(temp_recv_buf_ipv4);

  websock4 = websocket_connect(sock4, &req, timeout, "IPv4");
  if (websock4 < 0) {
    LOG_ERR("Cannot connect to %s:%d with error %d", MAGISTRALA_HOSTNAME,
            MAGISTRALA_WS_PORT, websock4);
    close(sock4);
    return -1;
  }

  LOG_INF("Websocket IPv4 %d", websock4);

  while (1) {
    if (websock4 >= 0 &&
        !send_senml_msg(websock4, "IPv4", recv_buf_ipv4,
                        sizeof(recv_buf_ipv4))) {
      break;
    }

    k_sleep(K_SECONDS(TELEMETRY_INTERVAL_SEC));
  }

  if (websock4 >= 0) {
    close(websock4);
  }

  if (sock4 >= 0) {
    close(sock4);
  }

  k_sleep(K_FOREVER);

  return 0;
}
