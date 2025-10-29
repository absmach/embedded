#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/reboot.h>

#include "ca_cert.h"
#include "config.h"
#include "wifi.h"
#include <zephyr/net/http/client.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/udp.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(https_client);

static struct net_if *sta_iface;

#define TELEMETRY_INTERVAL_SEC 30

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

static struct net_mgmt_event_callback mgmt_cb;
static K_SEM_DEFINE(dhcp_sem, 0, 1);

#define MAX_RECV_BUF_LEN 512

static uint8_t recv_buf_ipv4[MAX_RECV_BUF_LEN];
static uint8_t recv_buf_ipv6[MAX_RECV_BUF_LEN];

/* TLS credential tag */
#define TLS_SEC_TAG 1

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint32_t mgmt_event, struct net_if *iface)
{
  if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND)
  {
    LOG_INF("DHCP bound - got IP address");
    k_sem_give(&dhcp_sem);
  }
}

static int wait_for_ip_address(struct net_if *iface)
{
  struct net_if_addr *if_addr;
  const int max_timeout = 30; // 30 seconds max wait

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

  int ret = k_sem_take(&dhcp_sem, K_SECONDS(max_timeout));
  if (ret != 0)
  {
    LOG_ERR("Timeout waiting for DHCP - checking if we have IP anyway");

    if_addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (!if_addr)
    {
      LOG_ERR("No IP address available");
      return -ETIMEDOUT;
    }
  }

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

static int setup_tls_credentials(void)
{
  int ret;

  ret = tls_credential_add(TLS_SEC_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_cert,
                           sizeof(ca_cert));
  if (ret < 0)
  {
    LOG_ERR("Failed to add CA certificate: %d", ret);
    return ret;
  }

  LOG_INF("TLS credentials registered");
  return 0;
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

static int setup_socket(int *sock)
{
  int ret = 0;

  *sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);

  if (*sock < 0)
  {
    LOG_ERR("Failed to create HTTPS socket (%d)", -errno);
    return -errno;
  }

  /* Set TLS security tag */
  sec_tag_t sec_tag_opt[] = {TLS_SEC_TAG};
  ret = setsockopt(*sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt,
                   sizeof(sec_tag_opt));
  if (ret < 0)
  {
    LOG_ERR("Failed to set TLS_SEC_TAG option: %d", errno);
    close(*sock);
    return -errno;
  }

  /* Set hostname for TLS SNI */
  ret = setsockopt(*sock, SOL_TLS, TLS_HOSTNAME, MAGISTRALA_HOSTNAME,
                   strlen(MAGISTRALA_HOSTNAME));
  if (ret < 0)
  {
    LOG_ERR("Failed to set TLS_HOSTNAME option: %d", errno);
    close(*sock);
    return -errno;
  }

  /* Set peer verification */
  int verify = TLS_PEER_VERIFY_REQUIRED;
  ret = setsockopt(*sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
  if (ret < 0)
  {
    LOG_ERR("Failed to set TLS_PEER_VERIFY option: %d", errno);
    close(*sock);
    return -errno;
  }

  LOG_INF("TLS socket configured for %s", MAGISTRALA_HOSTNAME);

  return ret;
}

static int payload_cb(int sock, struct http_request *req, void *user_data)
{
  const char *content[] = {"foobar", "chunked", "last"};
  char tmp[64];
  int i, pos = 0;

  for (i = 0; i < ARRAY_SIZE(content); i++)
  {
    pos += snprintk(tmp + pos, sizeof(tmp) - pos, "%x\r\n%s\r\n",
                    (unsigned int)strlen(content[i]), content[i]);
  }

  pos += snprintk(tmp + pos, sizeof(tmp) - pos, "0\r\n\r\n");

  (void)send(sock, tmp, pos, 0);

  return pos;
}

static int response_cb(struct http_response *rsp,
                       enum http_final_call final_data, void *user_data)
{
  if (final_data == HTTP_DATA_MORE)
  {
    LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
  }
  else if (final_data == HTTP_DATA_FINAL)
  {
    LOG_INF("All the data received (%zd bytes)", rsp->data_len);
  }

  LOG_INF("Response to %s", (const char *)user_data);
  LOG_INF("Response status %s", rsp->http_status);

  return 0;
}

static int connect_socket(int *sock, struct sockaddr_storage *addr)
{
  int ret;

  ret = setup_socket(sock);
  if (ret < 0 || *sock < 0)
  {
    return -1;
  }

  struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
  char addr_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &addr4->sin_addr, addr_str, sizeof(addr_str));

  LOG_INF("Connecting to %s:%d...", addr_str, ntohs(addr4->sin_port));

  ret = connect(*sock, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
  if (ret < 0)
  {
    LOG_ERR("Cannot connect to remote (%d)", -errno);
    close(*sock);
    *sock = -1;
    ret = -errno;
  }
  else
  {
    LOG_INF("Connected successfully via TLS");
  }

  return ret;
}

static void update_sensor_data(sensor_data_t *data)
{
  data->temperature = 20.0 + (sys_rand32_get() % 1000) / 100.0; // 20-30°C
  data->humidity = 50.0 + (sys_rand32_get() % 3000) / 100.0;    // 50-80%
  data->battery_level = 70 + (sys_rand32_get() % 30);           // 70-99%
  data->led_state = !data->led_state;                           // Toggle LED
}

static int run_queries(void)
{
  static struct sockaddr_storage server_addr;
  static bool addr_resolved = false;
  int sock = -1;
  int32_t timeout = 3 * MSEC_PER_SEC;
  int ret = 0;

  if (!addr_resolved)
  {
    ret = resolve_hostname(MAGISTRALA_HOSTNAME, &server_addr);
    if (ret < 0)
    {
      LOG_ERR("Failed to resolve hostname: %d", ret);
      return ret;
    }
    ((struct sockaddr_in *)&server_addr)->sin_port =
        htons(MAGISTRALA_HTTPS_PORT);
    addr_resolved = true;
  }

  /* Create socket and connect */
  ret = connect_socket(&sock, &server_addr);
  if (ret < 0 || sock < 0)
  {
    LOG_ERR("Cannot create HTTPS connection for SenML telemetry.");
    return -ECONNABORTED;
  }

  update_sensor_data(&current_data);

  struct http_request req;

  int64_t unix_time = k_uptime_get() / 1000 + 1761700000; // Approximate Unix time

  char senml_payload[512];
  snprintf(senml_payload, sizeof(senml_payload),
           "[{\"bn\":\"esp32s3:\",\"bt\":%lld,\"bu\":\"Cel\",\"bver\":5,"
           "\"n\":\"temperature\",\"u\":\"Cel\",\"v\":%.2f},"
           "{\"n\":\"humidity\",\"u\":\"%%RH\",\"v\":%.2f},"
           "{\"n\":\"battery\",\"u\":\"%%\",\"v\":%d},"
           "{\"n\":\"led\",\"vb\":%s}]",
           unix_time, current_data.temperature, current_data.humidity,
           current_data.battery_level, current_data.led_state ? "true" : "false");

  LOG_INF("Sending telemetry: temp=%.2f°C, humidity=%.2f%%, battery=%d%%, led=%s, time=%lld",
          current_data.temperature, current_data.humidity, current_data.battery_level,
          current_data.led_state ? "ON" : "OFF", unix_time);

  char auth_header[128];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Client %s\r\n",
           CLIENT_SECRET);

  const char *headers[] = {"Content-Type: application/senml+json\r\n",
                           auth_header, NULL};

  // Prepare URL /api/http/m/domain/c/channel
  char url[256];
  snprintf(url, sizeof(url), "/api/http/m/%s/c/%s", DOMAIN_ID, CHANNEL_ID);

  memset(&req, 0, sizeof(req));
  req.method = HTTP_POST;
  req.url = url;
  req.host = MAGISTRALA_HOSTNAME;
  req.protocol = "HTTP/1.1";
  req.payload = senml_payload;
  req.payload_len = strlen(senml_payload);
  req.header_fields = headers;
  req.response = response_cb;
  req.recv_buf = recv_buf_ipv4;
  req.recv_buf_len = sizeof(recv_buf_ipv4);

  ret = http_client_req(sock, &req, timeout, "SenML POST");

  close(sock);

  return ret;
}

static int start_app(void)
{
  int r = 0;

  for (int i = 0; i < 10; i++)
  {
    r = run_queries();
    k_sleep(K_SECONDS(TELEMETRY_INTERVAL_SEC));
  }

  return r;
}

int main(void)
{
  LOG_INF("Magistrala HTTPS Client Starting");

  k_sleep(K_SECONDS(5));

  LOG_INF("Initializing wifi");

  initialize_wifi();

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

  ret = setup_tls_credentials();
  if (ret < 0)
  {
    LOG_ERR("Failed to setup TLS credentials: %d", ret);
    return ret;
  }

  LOG_INF("Will send telemetry every %d seconds", TELEMETRY_INTERVAL_SEC);

  exit(start_app());

  return 0;
}
