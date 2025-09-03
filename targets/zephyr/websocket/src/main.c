#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>

#include "config.h"
#include "wifi.h"
#include <zephyr/misc/lorem_ipsum.h>
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

static const char lorem_ipsum[] = LOREM_IPSUM;

#define MAX_RECV_BUF_LEN (sizeof(lorem_ipsum) - 1)

const int ipsum_len = MAX_RECV_BUF_LEN;

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

static int setup_socket(sa_family_t family, const char *server, int port,
                        int *sock, struct sockaddr *addr, socklen_t addr_len) {
  const char *family_str = family == AF_INET ? "IPv4" : "IPv6";
  int ret = 0;

  memset(addr, 0, addr_len);

  if (family == AF_INET) {
    net_sin(addr)->sin_family = AF_INET;
    net_sin(addr)->sin_port = htons(port);
    inet_pton(family, server, &net_sin(addr)->sin_addr);
  } else {
    net_sin6(addr)->sin6_family = AF_INET6;
    net_sin6(addr)->sin6_port = htons(port);
    inet_pton(family, server, &net_sin6(addr)->sin6_addr);
  }

  *sock = socket(family, SOCK_STREAM, IPPROTO_TCP);

  if (*sock < 0) {
    LOG_ERR("Failed to create %s HTTP socket (%d)", family_str, -errno);
  }

  return ret;
}

static int connect_socket(sa_family_t family, const char *server, int port,
                          int *sock, struct sockaddr *addr,
                          socklen_t addr_len) {
  int ret;

  ret = setup_socket(family, server, port, sock, addr, addr_len);
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

static size_t how_much_to_send(size_t max_len) {
  size_t amount;

  do {
    amount = sys_rand32_get() % max_len;
  } while (amount == 0U);

  return amount;
}

static ssize_t sendall_with_ws_api(int sock, const void *buf, size_t len) {
  return websocket_send_msg(sock, buf, len, WEBSOCKET_OPCODE_DATA_TEXT, true,
                            true, SYS_FOREVER_MS);
}

static ssize_t sendall_with_bsd_api(int sock, const void *buf, size_t len) {
  return send(sock, buf, len, 0);
}

static void recv_data_wso_api(int sock, size_t amount, uint8_t *buf,
                              size_t buf_len, const char *proto) {
  uint64_t remaining = ULLONG_MAX;
  int total_read;
  uint32_t message_type;
  int ret, read_pos;

  read_pos = 0;
  total_read = 0;

  while (remaining > 0) {
    ret = websocket_recv_msg(sock, buf + read_pos, buf_len - read_pos,
                             &message_type, &remaining, 0);
    if (ret < 0) {
      if (ret == -EAGAIN) {
        k_sleep(K_MSEC(50));
        continue;
      }

      LOG_DBG("%s connection closed while "
              "waiting (%d/%d)",
              proto, ret, errno);
      break;
    }

    read_pos += ret;
    total_read += ret;
  }

  if (remaining != 0 || total_read != amount ||
      /* Do not check the final \n at the end of the msg */
      memcmp(lorem_ipsum, buf, amount - 1) != 0) {
    LOG_ERR("%s data recv failure %zd/%d bytes (remaining %" PRId64 ")", proto,
            amount, total_read, remaining);
    LOG_HEXDUMP_DBG(buf, total_read, "received ws buf");
    LOG_HEXDUMP_DBG(lorem_ipsum, total_read, "sent ws buf");
  } else {
    LOG_DBG("%s recv %d bytes", proto, total_read);
  }
}

static void recv_data_bsd_api(int sock, size_t amount, uint8_t *buf,
                              size_t buf_len, const char *proto) {
  int remaining;
  int ret, read_pos;

  remaining = amount;
  read_pos = 0;

  while (remaining > 0) {
    ret = recv(sock, buf + read_pos, buf_len - read_pos, 0);
    if (ret <= 0) {
      if (errno == EAGAIN || errno == ETIMEDOUT) {
        k_sleep(K_MSEC(50));
        continue;
      }

      LOG_DBG("%s connection closed while "
              "waiting (%d/%d)",
              proto, ret, errno);
      break;
    }

    read_pos += ret;
    remaining -= ret;
  }

  if (remaining != 0 ||
      /* Do not check the final \n at the end of the msg */
      memcmp(lorem_ipsum, buf, amount - 1) != 0) {
    LOG_ERR("%s data recv failure %zd/%d bytes (remaining %d)", proto, amount,
            read_pos, remaining);
    LOG_HEXDUMP_DBG(buf, read_pos, "received bsd buf");
    LOG_HEXDUMP_DBG(lorem_ipsum, read_pos, "sent bsd buf");
  } else {
    LOG_DBG("%s recv %d bytes", proto, read_pos);
  }
}

static bool send_and_wait_msg(int sock, size_t amount, const char *proto,
                              uint8_t *buf, size_t buf_len) {
  static int count;
  int ret;

  if (sock < 0) {
    return true;
  }

  /* Terminate the sent data with \n so that we can use the
   *      websocketd --port=9001 cat
   * command in server side.
   */
  memcpy(buf, lorem_ipsum, amount);
  buf[amount] = '\n';

  /* Send every 2nd message using dedicated websocket API and generic
   * BSD socket API. Real applications would not work like this but here
   * we want to test both APIs. We also need to send the \n so add it
   * here to amount variable.
   */
  if (count % 2) {
    ret = sendall_with_ws_api(sock, buf, amount + 1);
  } else {
    ret = sendall_with_bsd_api(sock, buf, amount + 1);
  }

  if (ret <= 0) {
    if (ret < 0) {
      LOG_ERR("%s failed to send data using %s (%d)", proto,
              (count % 2) ? "ws API" : "socket API", ret);
    } else {
      LOG_DBG("%s connection closed", proto);
    }

    return false;
  } else {
    LOG_DBG("%s sent %d bytes", proto, ret);
  }

  if (count % 2) {
    recv_data_wso_api(sock, amount + 1, buf, buf_len, proto);
  } else {
    recv_data_bsd_api(sock, amount + 1, buf, buf_len, proto);
  }

  count++;

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
    return -ENODEV;
  }

  int ret = connect_to_wifi(sta_iface, WIFI_SSID, WIFI_PSK);
  if (ret) {
    LOG_ERR("Unable to Connect to (%s)", WIFI_SSID);
    return ret;
  }

  /* Wait for IP address via DHCP */
  ret = wait_for_ip_address(sta_iface);
  if (ret < 0) {
    LOG_ERR("Failed to get IP address: %d", ret);
    return ret;
  }

  LOG_INF("Will send telemetry every %d seconds", TELEMETRY_INTERVAL_SEC);

  /* Just an example how to set extra headers */
  const char *extra_headers[] = {"Origin: http://foobar\r\n", NULL};

  int sock4 = -1;
  int websock4 = -1;
  int32_t timeout = 3 * MSEC_PER_SEC;
  struct sockaddr_in addr4;
  size_t amount;

  ret = connect_socket(AF_INET, MAGISTRALA_IP, MAGISTRALA_WS_PORT, &sock4,
                       (struct sockaddr *)&addr4, sizeof(addr4));
  if (ret < 0 || sock4 < 0) {
    LOG_ERR("Cannot create or connect IPv4 HTTP socket.");
    return -1;
  }

  LOG_INF("Connected to IPv4 websocket");

  struct websocket_request req;

  memset(&req, 0, sizeof(req));

  req.host = MAGISTRALA_IP;

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
    LOG_ERR("Cannot connect to %s:%d with error %d", MAGISTRALA_IP,
            MAGISTRALA_WS_PORT, websock4);
    close(sock4);
    return -1;
  }

  LOG_INF("Websocket IPv4 %d", websock4);

  while (1) {
    amount = how_much_to_send(ipsum_len);

    if (websock4 >= 0 &&
        !send_and_wait_msg(websock4, amount, "IPv4", recv_buf_ipv4,
                           sizeof(recv_buf_ipv4))) {
      break;
    }

    k_sleep(K_MSEC(250));
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
