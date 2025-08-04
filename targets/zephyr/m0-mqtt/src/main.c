#include <string.h>
#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/random/random.h>
#include <zephyr/app_memory/app_memdomain.h>
#include <zephyr/net/net_event.h>
//#include <zephyr/net/tcp.h>
#include "config.h"
#include "wifi.h"

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

LOG_MODULE_REGISTER(mqtt_client);

static APP_BMEM uint8_t rx_buffer[APP_MQTT_BUFFER_SIZE];
static APP_BMEM uint8_t tx_buffer[APP_MQTT_BUFFER_SIZE];

static APP_BMEM struct mqtt_client client_ctx;

static APP_BMEM struct sockaddr_storage magistrala_broker_addr;

static APP_BMEM struct zsock_pollfd fds[1];
static APP_BMEM int nfds;

static APP_BMEM bool connected;

static struct net_if *sta_iface;
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

char mgTopic[150];

static void prepare_fds(struct mqtt_client *client)
{
	if (client->transport.type == MQTT_TRANSPORT_NON_SECURE)
	{
		fds[0].fd = client->transport.tcp.sock;
	}
	fds[0].events = ZSOCK_POLLIN;
	nfds = 1;
}

static void clear_fds(void)
{
	nfds = 0;
}

static int wait(int timeout)
{
	int ret = 0;

	if (nfds > 0)
	{
		ret = zsock_poll(fds, nfds, timeout);
		if (ret < 0)
		{
			LOG_ERR("poll error: %d", errno);
		}
	}

	return ret;
}

void mqtt_evt_handler(struct mqtt_client *const client,
					  const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type)
	{
	case MQTT_EVT_CONNACK:
		if (evt->result != 0)
		{
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
		if (evt->result != 0)
		{
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);

		break;

	case MQTT_EVT_PUBREC:
		if (evt->result != 0)
		{
			LOG_ERR("MQTT PUBREC error %d", evt->result);
			break;
		}

		LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param rel_param = {
			.message_id = evt->param.pubrec.message_id};

		err = mqtt_publish_qos2_release(client, &rel_param);
		if (err != 0)
		{
			LOG_ERR("Failed to send MQTT PUBREL: %d", err);
		}

		break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0)
		{
			LOG_ERR("MQTT PUBCOMP error %d", evt->result);
			break;
		}

		LOG_INF("PUBCOMP packet id: %u",
				evt->param.pubcomp.message_id);

		break;

	case MQTT_EVT_PINGRESP:
		LOG_INF("PINGRESP packet");
		break;

	default:
		break;
	}
}

static char *get_mqtt_payload(enum mqtt_qos qos)
{
	static APP_DMEM char payload[] = "{'message':'hello'}";

	payload[strlen(payload) - 1] = '0' + qos;
	return payload;
}

static char *get_mqtt_topic(void)
{
	const char *_preId = "m/";
	const char *_postId = "/esp32c6In";
	strcpy(mgTopic, _preId);
	strcat(mgTopic, DOMAIN_ID);
	strcat(mgTopic, "/c/");
	strcat(mgTopic, CHANNEL_ID);
	strcat(mgTopic, _postId);
	return mgTopic;
}

static int publish(struct mqtt_client *client, enum mqtt_qos qos)
{
	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = (uint8_t *)get_mqtt_topic();
	param.message.topic.topic.size =
		strlen(param.message.topic.topic.utf8);
	param.message.payload.data = get_mqtt_payload(qos);
	param.message.payload.len =
		strlen(param.message.payload.data);
	param.message_id = sys_rand32_get();
	param.dup_flag = 0U;
	param.retain_flag = 0U;

	return mqtt_publish(client, &param);
}

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc) \
	LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

static void broker_init(void)
{
	struct sockaddr_in *broker4 = (struct sockaddr_in *)&magistrala_broker_addr;

	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(MAGISTRALA_MQTT_PORT);
	zsock_inet_pton(AF_INET, MAGISTRALA_IP , &broker4->sin_addr);

	if (zsock_inet_pton(AF_INET, MAGISTRALA_IP, &broker4->sin_addr) != 1) {
		LOG_ERR("Invalid IP address: %s", MAGISTRALA_IP);
	}
	else {
		LOG_INF("Broker address initialized: %s:%d", MAGISTRALA_IP, MAGISTRALA_MQTT_PORT);
	}

}

static void client_init(struct mqtt_client *client)
{
	mqtt_client_init(client);

	broker_init();

	/* MQTT client configuration */
	client->broker = &magistrala_broker_addr;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = (uint8_t *)MQTT_CLIENT_ID;
	client->client_id.size = strlen(MQTT_CLIENT_ID);
	client->password = MQTT_CLIENT_SECRET;
	client->user_name = MQTT_CLIENT_ID;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);
	client->transport.type = MQTT_TRANSPORT_NON_SECURE;
}

static int try_to_connect(struct mqtt_client *client)
{
	int rc, i = 0;

	mqtt_connect(client);

	while (i++ < APP_CONNECT_TRIES && !connected)
	{
		LOG_INF("Attempting to connect to Magistrala MQTT broker %s:%d", MAGISTRALA_IP, MAGISTRALA_MQTT_PORT);
		client_init(client);
		LOG_INF("MQTT client and Broker initialized!");

		rc = mqtt_connect(client);
		if (rc != 0)
		{
			PRINT_RESULT("mqtt_connect", rc);
			k_sleep(K_MSEC(APP_SLEEP_MSECS));
			continue;
		}

		prepare_fds(client);

		if (wait(APP_CONNECT_TIMEOUT_MS))
		{
			mqtt_input(client);
		}

		if (!connected)
		{
			LOG_INF("Failed to connect to Magistrala MQTT broker %s:%d", MAGISTRALA_IP, MAGISTRALA_MQTT_PORT);
			mqtt_abort(client);
		}
	}

	if (connected)
	{
		LOG_INF("Connected to Magistrala MQTT broker %s:%d", MAGISTRALA_IP, MAGISTRALA_MQTT_PORT);
		return 0;
	}

	return -EINVAL;
}

static int process_mqtt_and_sleep(struct mqtt_client *client, int timeout)
{
	int64_t remaining = timeout;
	int64_t start_time = k_uptime_get();
	int rc;

	while (remaining > 0 && connected)
	{
		if (wait(remaining))
		{
			rc = mqtt_input(client);
			if (rc != 0)
			{
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		rc = mqtt_live(client);
		if (rc != 0 && rc != -EAGAIN)
		{
			PRINT_RESULT("mqtt_live", rc);
			return rc;
		}
		else if (rc == 0)
		{
			rc = mqtt_input(client);
			if (rc != 0)
			{
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		remaining = timeout + start_time - k_uptime_get();
	}

	return 0;
}

int success_or_exit(int rc)
{
	if (rc != 0)
	{
		return 1;
	}

	return 0;
}

void success_or_break(int rc)
{
	if (rc != 0)
	{
		return;
	}
}

static int publisher(void)
{
	int i, rc, r = 0;
	/* Start by connecting to MQTT Broker */
	LOG_INF("Attempting to connect: ");
	rc = try_to_connect(&client_ctx);
	PRINT_RESULT("try_to_connect", rc);
	SUCCESS_OR_EXIT(rc);
	

	i = 0;
	while (i++ < CONFIG_NET_SAMPLE_APP_MAX_ITERATIONS && connected)
	{
		r = -1;

		rc = mqtt_ping(&client_ctx);
		PRINT_RESULT("mqtt_ping", rc);
		LOG_INF("MQTT PING sent, waiting for PINGRESP");
		SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&client_ctx, APP_SLEEP_MSECS);
		LOG_INF("Processing MQTT events and sleeping for %d ms", APP_SLEEP_MSECS);
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

		LOG_INF("Published messages with QoS 0, 1, and 2");

		r = 0;
	}

	rc = mqtt_disconnect(&client_ctx, NULL);
	PRINT_RESULT("mqtt_disconnect", rc);

	LOG_INF("Bye!");

	return r;
}

static int start_app(void)
{
	int r = 0, i = 0;

	while (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS ||
		   i++ < CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS)
	{
		r = publisher();

		if (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS)
		{
			k_sleep(K_MSEC(PROG_DELAY));
		}
	}

	return r;
}

  
static int wait_for_ip_address(void) {
  struct net_if_addr *if_addr;
  struct net_if *iface = net_if_get_default();
  int timeout_count = 0;
  const int max_timeout = 50; // 50 seconds max wait

 /*Check if we already have an IP */
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
}


int main(void)
{
	LOG_INF("Magistrala MQTT Client starting...");

	k_sleep(K_MSEC(5));

	LOG_INF("Initializing WiFi");

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

	exit(start_app());
	return 0;
}
