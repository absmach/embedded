#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>

#include "config.h"
#include "wifi.h"
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/udp.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(http_client);

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

static struct net_mgmt_event_callback mgmt_cb;
static K_SEM_DEFINE(dhcp_sem, 0, 1);

#define MAX_RECV_BUF_LEN 512

static uint8_t recv_buf_ipv4[MAX_RECV_BUF_LEN];
static uint8_t recv_buf_ipv6[MAX_RECV_BUF_LEN];

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

  if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
  if (if_addr) {
    char addr_str[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &if_addr->address.in_addr, addr_str,
                  sizeof(addr_str));
    LOG_INF("Already have IP address: %s", addr_str);
    return 0;
  }

  LOG_INF("Waiting for IP address via DHCP...");

  int ret = k_sem_take(&dhcp_sem, K_SECONDS(max_timeout));
  if (ret != 0) {
    LOG_ERR("Timeout waiting for DHCP - checking if we have IP anyway");

    if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (!if_addr) {
      LOG_ERR("No IP address available");
      return -ETIMEDOUT;
    }
  }

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

static int payload_cb(int sock, struct http_request *req, void *user_data) {
  const char *content[] = {"foobar", "chunked", "last"};
  char tmp[64];
  int i, pos = 0;

  for (i = 0; i < ARRAY_SIZE(content); i++) {
    pos += snprintk(tmp + pos, sizeof(tmp) - pos, "%x\r\n%s\r\n",
                    (unsigned int)strlen(content[i]), content[i]);
  }

  pos += snprintk(tmp + pos, sizeof(tmp) - pos, "0\r\n\r\n");

  (void)send(sock, tmp, pos, 0);

  return pos;
}

static int response_cb(struct http_response *rsp,
                       enum http_final_call final_data, void *user_data) {
  if (final_data == HTTP_DATA_MORE) {
    LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
  } else if (final_data == HTTP_DATA_FINAL) {
    LOG_INF("All the data received (%zd bytes)", rsp->data_len);
  }

  LOG_INF("Response to %s", (const char *)user_data);
  LOG_INF("Response status %s", rsp->http_status);

  return 0;
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
    close(*sock);
    *sock = -1;
    ret = -errno;
  }

  return ret;
}

static int run_queries(void) {
  struct sockaddr_in addr4;
  int sock4 = -1;
  int32_t timeout = 3 * MSEC_PER_SEC;
  int ret = 0;
  int port = MAGISTRALA_HTTP_PORT;

  if (IS_ENABLED(CONFIG_NET_IPV4)) {
    (void)connect_socket(AF_INET, MAGISTRALA_IP, port, &sock4,
                         (struct sockaddr *)&addr4, sizeof(addr4));
  }

  if (sock4 < 0) {
    LOG_ERR("Cannot create HTTP connection.");
    return -ECONNABORTED;
  }

  sock4 = -1;
  if (IS_ENABLED(CONFIG_NET_IPV4)) {
    (void)connect_socket(AF_INET, MAGISTRALA_IP, port, &sock4,
                         (struct sockaddr *)&addr4, sizeof(addr4));
  }

  if (sock4 < 0) {
    LOG_ERR("Cannot create HTTP connection for SenML telemetry.");
    return -ECONNABORTED;
  }

  if (sock4 >= 0 && IS_ENABLED(CONFIG_NET_IPV4)) {
    struct http_request req;

    const char *senml_payload = "[{\"bn\":\"some-base-name:\",\"bt\":1."
                                "276020076001e+09,\"bu\":\"A\",\"bver\":5,"
                                "\"n\":\"voltage\",\"u\":\"V\",\"v\":120.1},"
                                "{\"n\":\"current\",\"t\":-5,\"v\":1.2},"
                                "{\"n\":\"current\",\"t\":-4,\"v\":1.3}]";

    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Client %s\r\n",
             CLIENT_SECRET);

    const char *headers[] = {"Content-Type: application/senml+json\r\n",
                             auth_header, NULL};

    // Prepare URL m/domain/c/channel
    char url[256];
    snprintf(url, sizeof(url), "/m/%s/c/%s", DOMAIN_ID, CHANNEL_ID);

    memset(&req, 0, sizeof(req));
    req.method = HTTP_POST;
    req.url = url;
    req.host = MAGISTRALA_IP;
    req.protocol = "HTTP/1.1";
    req.payload = senml_payload;
    req.payload_len = strlen(senml_payload);
    req.header_fields = headers;
    req.response = response_cb;
    req.recv_buf = recv_buf_ipv4;
    req.recv_buf_len = sizeof(recv_buf_ipv4);

    ret = http_client_req(sock4, &req, timeout, "SenML POST");

    close(sock4);
  }

  return ret;
}

static int start_app(void) {
  int r = 0;

  for (int i = 0; i < 10; i++) {
    r = run_queries();
    k_sleep(K_SECONDS(TELEMETRY_INTERVAL_SEC));
  }

  return r;
}

int main(void) {
  LOG_INF("Magistrala HTTP Client Starting");

  k_sleep(K_SECONDS(5));

  LOG_INF("Initializing wifi");

  initialize_wifi();

  net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler,
                               NET_EVENT_IPV4_DHCP_BOUND);
  net_mgmt_add_event_callback(&mgmt_cb);

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

  ret = wait_for_ip_address(sta_iface);
  if (ret < 0) {
    LOG_ERR("Failed to get IP address: %d", ret);
    return ret;
  }

  LOG_INF("Will send telemetry every %d seconds", TELEMETRY_INTERVAL_SEC);

  exit(start_app());

  return 0;
}
