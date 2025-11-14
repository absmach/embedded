#include <errno.h>
#include <string.h>

#include "config.h"
#include "wifi.h"
#include <zephyr/app_memory/app_memdomain.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/reboot.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

K_APPMEM_PARTITION_DEFINE(app_partition);
#define APP_BMEM K_APP_BMEM(app_partition)
#define APP_DMEM K_APP_DMEM(app_partition)
#define SUCCESS_OR_EXIT(rc) success_or_exit(rc)
#define SUCCESS_OR_BREAK(rc) success_or_break(rc)
#define PROG_DELAY 5000
#define APP_MQTT_BUFFER_SIZE 1024 // Replace 1024 with the desired buffer size
#define APP_CONNECT_TRIES 3
#define APP_CONNECT_TIMEOUT_MS 1000
#define APP_SLEEP_MSECS 1000

LOG_MODULE_REGISTER(mqtt_client, LOG_LEVEL_DBG);

static struct net_mgmt_event_callback mgmt_cb;
static K_SEM_DEFINE(dhcp_sem, 0, 1);

static struct net_if *sta_iface;

static APP_BMEM uint8_t rx_buffer[APP_MQTT_BUFFER_SIZE];
static APP_BMEM uint8_t tx_buffer[APP_MQTT_BUFFER_SIZE];

static APP_BMEM struct mqtt_client client_ctx;

static APP_BMEM struct sockaddr_storage broker_addr;

static APP_BMEM struct zsock_pollfd fds[1];
static APP_BMEM int nfds;

static APP_BMEM bool connected;

char mqttTopic[150];

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

static void prepare_fds(struct mqtt_client *client) {
  if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
    fds[0].fd = client->transport.tcp.sock;
  }
  fds[0].events = ZSOCK_POLLIN;
  nfds = 1;
}

static void clear_fds(void) { nfds = 0; }

static int wait(int timeout) {
  int ret = 0;

  if (nfds > 0) {
    ret = zsock_poll(fds, nfds, timeout);
    if (ret < 0) {
      LOG_ERR("poll error: %d", errno);
    }
  }

  return ret;
}

void mqtt_evt_handler(struct mqtt_client *const client,
                      const struct mqtt_evt *evt) {
  int err;

  switch (evt->type) {
  case MQTT_EVT_CONNACK:
    if (evt->result != 0) {
      LOG_ERR("MQTT connect failed %d", evt->result);
      break;
    }

    connected = true;
    LOG_INF("MQTT client connected!");

    break;

  case MQTT_EVT_DISCONNECT:
    LOG_INF("MQTT client disconnected %d", evt->result);

    connected = false;
    clear_fds();

    break;

  case MQTT_EVT_PUBACK:
    if (evt->result != 0) {
      LOG_ERR("MQTT PUBACK error %d", evt->result);
      break;
    }

    LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);

    break;

  case MQTT_EVT_PUBREC:
    if (evt->result != 0) {
      LOG_ERR("MQTT PUBREC error %d", evt->result);
      break;
    }

    LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

    const struct mqtt_pubrel_param rel_param = {
        .message_id = evt->param.pubrec.message_id};

    err = mqtt_publish_qos2_release(client, &rel_param);
    if (err != 0) {
      LOG_ERR("Failed to send MQTT PUBREL: %d", err);
    }

    break;

  case MQTT_EVT_PUBCOMP:
    if (evt->result != 0) {
      LOG_ERR("MQTT PUBCOMP error %d", evt->result);
      break;
    }

    LOG_INF("PUBCOMP packet id: %u", evt->param.pubcomp.message_id);

    break;

  case MQTT_EVT_PINGRESP:
    LOG_INF("PINGRESP packet");
    break;

  default:
    break;
  }
}

static void update_sensor_data(sensor_data_t *data)
{
  data->temperature = 20.0 + (sys_rand32_get() % 1000) / 100.0; // 20-30°C
  data->humidity = 50.0 + (sys_rand32_get() % 3000) / 100.0;    // 50-80%
  data->battery_level = 70 + (sys_rand32_get() % 30);           // 70-99%
  data->led_state = !data->led_state;                           // Toggle LED
}

static char *get_mqtt_payload(enum mqtt_qos qos) {
  static APP_DMEM char payload[512];

  update_sensor_data(&current_data);

  int64_t unix_time = k_uptime_get() / 1000 + 1761835000; // Approximate Unix time

  snprintk(payload, sizeof(payload),
           "[{\"bn\":\"esp32s3:\",\"bt\":%lld,\"bu\":\"Cel\",\"bver\":5,"
           "\"n\":\"temperature\",\"u\":\"Cel\",\"v\":%.2f},"
           "{\"n\":\"humidity\",\"u\":\"%%RH\",\"v\":%.2f},"
           "{\"n\":\"battery\",\"u\":\"%%\",\"v\":%d},"
           "{\"n\":\"led\",\"vb\":%s}]",
           unix_time, current_data.temperature, current_data.humidity,
           current_data.battery_level, current_data.led_state ? "true" : "false");

  return payload;
}

static char *get_mqtt_topic(void) {
  // Construct URI path:
  // m/{domain_id}/c/{channel_id}
  const char *_preId = "m/";
  const char *_postId = "/c/";
  strcpy(mqttTopic, _preId);
  strcat(mqttTopic, DOMAIN_ID);
  strcat(mqttTopic, _postId);
  strcat(mqttTopic, CHANNEL_ID);
  return mqttTopic;
}

static int publish(struct mqtt_client *client, enum mqtt_qos qos) {
  struct mqtt_publish_param param;

  param.message.topic.qos = qos;
  param.message.topic.topic.utf8 = (uint8_t *)get_mqtt_topic();
  param.message.topic.topic.size = strlen(param.message.topic.topic.utf8);
  param.message.payload.data = get_mqtt_payload(qos);
  param.message.payload.len = strlen(param.message.payload.data);
  param.message_id = sys_rand32_get();
  param.dup_flag = 0U;
  param.retain_flag = 0U;

  return mqtt_publish(client, &param);
}

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc) LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

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

static int broker_init(void) {
  int ret;
  struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker_addr;

  ret = resolve_hostname(MAGISTRALA_HOSTNAME, &broker_addr);
  if (ret < 0)
  {
    LOG_ERR("Failed to resolve hostname: %d", ret);
    return ret;
  }

  broker4->sin_family = AF_INET;
  broker4->sin_port = htons(MAGISTRALA_MQTT_PORT);

  return 0;
}

static int client_init(struct mqtt_client *client) {
  int ret;

  mqtt_client_init(client);

  ret = broker_init();
  if (ret < 0)
  {
    return ret;
  }

  /* MQTT client configuration */
  client->broker = &broker_addr;
  client->evt_cb = mqtt_evt_handler;
  client->client_id.utf8 = (uint8_t *)CLIENT_ID;
  client->client_id.size = strlen(CLIENT_ID);

  static struct mqtt_utf8 password_utf8;
  static struct mqtt_utf8 user_name_utf8;

  password_utf8.utf8 = (uint8_t *)CLIENT_SECRET;
  password_utf8.size = strlen(CLIENT_SECRET);
  user_name_utf8.utf8 = (uint8_t *)CLIENT_ID;
  user_name_utf8.size = strlen(CLIENT_ID);

  client->password = &password_utf8;
  client->user_name = &user_name_utf8;
  client->protocol_version = MQTT_VERSION_3_1_1;

  /* MQTT buffers configuration */
  client->rx_buf = rx_buffer;
  client->rx_buf_size = sizeof(rx_buffer);
  client->tx_buf = tx_buffer;
  client->tx_buf_size = sizeof(tx_buffer);
  client->transport.type = MQTT_TRANSPORT_NON_SECURE;

  return 0;
}

static int try_to_connect(struct mqtt_client *client) {
  int rc, i = 0;

  while (i++ < APP_CONNECT_TRIES && !connected) {

    rc = client_init(client);
    if (rc != 0)
    {
      PRINT_RESULT("client_init", rc);
      k_sleep(K_MSEC(APP_SLEEP_MSECS));
      continue;
    }

    rc = mqtt_connect(client);
    if (rc != 0) {
      PRINT_RESULT("mqtt_connect", rc);
      k_sleep(K_MSEC(APP_SLEEP_MSECS));
      continue;
    }

    prepare_fds(client);

    if (wait(APP_CONNECT_TIMEOUT_MS)) {
      mqtt_input(client);
    }

    if (!connected) {
      mqtt_abort(client);
    }
  }

  if (connected) {
    return 0;
  }

  return -EINVAL;
}

static int process_mqtt_and_sleep(struct mqtt_client *client, int timeout) {
  int64_t remaining = timeout;
  int64_t start_time = k_uptime_get();
  int rc;

  while (remaining > 0 && connected) {
    if (wait(remaining)) {
      rc = mqtt_input(client);
      if (rc != 0) {
        PRINT_RESULT("mqtt_input", rc);
        return rc;
      }
    }

    rc = mqtt_live(client);
    if (rc != 0 && rc != -EAGAIN) {
      PRINT_RESULT("mqtt_live", rc);
      return rc;
    } else if (rc == 0) {
      rc = mqtt_input(client);
      if (rc != 0) {
        PRINT_RESULT("mqtt_input", rc);
        return rc;
      }
    }

    remaining = timeout + start_time - k_uptime_get();
  }

  return 0;
}

int success_or_exit(int rc) {
  if (rc != 0) {
    return 1;
  }

  return 0;
}

void success_or_break(int rc) {
  if (rc != 0) {
    return;
  }
}

static int publisher(void) {
  int i, rc, r = 0;

  LOG_INF("attempting to connect: ");
  rc = try_to_connect(&client_ctx);
  PRINT_RESULT("try_to_connect", rc);
  SUCCESS_OR_EXIT(rc);

  i = 0;
  while (i++ < MAX_ITERATIONS && connected) {
    r = -1;

    rc = mqtt_ping(&client_ctx);
    PRINT_RESULT("mqtt_ping", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    rc = publish(&client_ctx, MQTT_QOS_0_AT_MOST_ONCE);
    PRINT_RESULT("mqtt_publish", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    rc = publish(&client_ctx, MQTT_QOS_1_AT_LEAST_ONCE);
    PRINT_RESULT("mqtt_publish", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    rc = publish(&client_ctx, MQTT_QOS_2_EXACTLY_ONCE);
    PRINT_RESULT("mqtt_publish", rc);
    SUCCESS_OR_BREAK(rc);

    rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
    SUCCESS_OR_BREAK(rc);

    r = 0;
  }

  rc = mqtt_disconnect(&client_ctx, NULL);
  PRINT_RESULT("mqtt_disconnect", rc);

  LOG_INF("Bye!");

  return r;
}

static int start_app(void) {
  int r = 0, i = 0;

  while (!MAX_CONNECTIONS ||
         i++ < MAX_CONNECTIONS) {
    r = publisher();

    if (!MAX_CONNECTIONS) {
      k_sleep(K_MSEC(PROG_DELAY));
    }
  }

  return r;
}

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface) {
  if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
    LOG_INF("DHCP bound - got IP address");
    k_sem_give(&dhcp_sem);
  }
}

static int wait_for_ip_address(struct net_if *iface) {
  struct net_if_addr *if_addr = NULL;
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

int main(void) {
  LOG_INF("Magistrala MQTT Client Starting");

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

  exit(start_app());
  return 0;
}
