#include <zephyr/logging/log.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>



LOG_MODULE_REGISTER(http_client, LOG_LEVEL_DBG);

/*WiFi configuration*/
#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |                        \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT |                      \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)

/* Magistrala Credentials */
#define DOMAIN_ID ""  /* Replace with your actual domain ID */
#define CHANNEL_ID "" /* Replace with your actual channel ID */
#define CLIENT_SECRET "" /* Replace with your actual client secret */

/* STA Mode Configuration */
#define WIFI_SSID "" /* Replace `SSID` with WiFi ssid. */
#define WIFI_PSK  "" /* Replace `PASSWORD` with Router password. */

#define HTTP_PORT 8008 /* supermq-http-adapter */ 
#define MAGISTRALA_IP "" /* Replace with the actual IP address of the Magistrala server */
#define MAX_RECV_BUF_LEN 1024

#define CONFIG_NET_SAMPLE_SEND_ITERATIONS 5

static const char *payload = 
    "["
    "  {\"bn\": \"some-base-name:\", \"bt\": 1.276020076001e+09, \"bu\": \"A\", \"bver\": 5, \"n\": \"voltage\", \"u\": \"V\", \"v\": 120.1},"
    "  {\"n\": \"current\", \"t\": -5, \"v\": 1.2},"
    "  {\"n\": \"current\", \"t\": -4, \"v\": 1.3}"
    "]";

static uint8_t recv_buf_ipv4[MAX_RECV_BUF_LEN]; 
static struct net_if *sta_iface;

static struct wifi_connect_req_params sta_config;

static struct net_mgmt_event_callback cb;

void set_static_ip(void)
{
	struct net_if *iface = net_if_get_default();

	struct in_addr static_ip;
	struct in_addr netmask;
	struct in_addr gw;

	// Set IP: 192.168.1.100
	net_addr_pton(AF_INET, "192.168.8.112", &static_ip);

	// Set Netmask: 255.255.255.0
	net_addr_pton(AF_INET, "255.255.255.0", &netmask);

	// Set Gateway: 192.168.1.1
	net_addr_pton(AF_INET, "192.168.1.1", &gw);

	net_if_ipv4_addr_add(iface, &static_ip, NET_ADDR_MANUAL, 0);
	net_if_ipv4_set_netmask(iface, &netmask);
	net_if_ipv4_set_gw(iface, &gw);

	LOG_INF("Static IP configured");
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		LOG_INF("Connected to %s", WIFI_SSID);
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT: {
		LOG_INF("Disconnected from %s", WIFI_SSID);
		break;
	}
	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		LOG_INF("AP Mode is enabled. Waiting for station to connect");
		break;
	}
	case NET_EVENT_WIFI_AP_DISABLE_RESULT: {
		LOG_INF("AP Mode is disabled.");
		break;
	}
	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;

		LOG_INF("station: " MACSTR " joined ", sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;

		LOG_INF("station: " MACSTR " leave ", sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	default:
		break;
	}
}

static int connect_to_wifi(void)
{
	if (!sta_iface) {
		LOG_INF("STA: interface no initialized");
		return -EIO;
	}

	sta_config.ssid = (const uint8_t *)WIFI_SSID;
	sta_config.ssid_length = strlen(WIFI_SSID);
	sta_config.psk = (const uint8_t *)WIFI_PSK;
	sta_config.psk_length = strlen(WIFI_PSK);
	sta_config.security = WIFI_SECURITY_TYPE_PSK;
	sta_config.channel = WIFI_CHANNEL_ANY;
	sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

	LOG_INF("Connecting to SSID: %s\n", sta_config.ssid);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
			   sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("Unable to Connect to (%s)", WIFI_SSID);
	}

	return ret;
}

static int setup_socket(sa_family_t family, const char *server, int port,
			int *sock, struct sockaddr *addr, socklen_t addr_len)
{
	const char *family_str = family == AF_INET;
	int ret = 0;

	memset(addr, 0, addr_len);

	net_sin(addr)->sin_family = AF_INET;
	net_sin(addr)->sin_port = htons(port);
	inet_pton(family, server, &net_sin(addr)->sin_addr);

	*sock = socket(family, SOCK_STREAM, IPPROTO_TCP);

	if (*sock < 0) {
		LOG_ERR("Failed to create %s HTTP socket (%d)", family_str,
			-errno);
	}

	LOG_INF("Socket created successfully. Socket FD: %d", *sock);
	LOG_INF("Socket ret is %d", ret);
	return ret;
}

static int response_cb(struct http_response *rsp,
		       enum http_final_call final_data,
		       void *user_data)
{
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
			  int *sock, struct sockaddr *addr, socklen_t addr_len)
{
	int ret;

	ret = setup_socket(family, server, port, sock, addr, addr_len);
	LOG_INF("setup_socket() ret is %d", ret);

	if (ret < 0 || *sock < 0) {
		return -1;
	}
	LOG_INF("Socket created successfully");
	LOG_INF("Connecting to %s:%d", server, port);

	ret = connect(*sock, addr, addr_len);
	LOG_INF("connect() ret is %d", ret);

	if (ret < 0) {
		LOG_ERR("Cannot connect to %s remote (%d)",
			family == AF_INET ? "IPv4" : "IPv6",
			-errno);
		close(*sock);
		*sock = -1;
		ret = -errno;
	}

	return ret;
}

static int run_queries(void)
{
	LOG_INF("Running HTTP POST queries...");
	struct sockaddr_in addr4; //ipv4 socket address
	int sock4 = -1; 
	int32_t timeout = 3 * MSEC_PER_SEC;
	int ret = 0;
	int port = HTTP_PORT;

	if (IS_ENABLED(CONFIG_NET_IPV4)) {
		(void)connect_socket(AF_INET, MAGISTRALA_IP, port,
				     &sock4, (struct sockaddr *)&addr4,
				     sizeof(addr4));
	}

	//IPV4 POST
	if (sock4 >= 0 && IS_ENABLED(CONFIG_NET_IPV4)) {
		struct http_request req;

		memset(&req, 0, sizeof(req));

		req.method = HTTP_POST;
		// Construct URI path:
		// m/{domain_id}/c/{channel_id}?authorization={client_secret}
		LOG_INF("Constructing URI path for POST request");
		char uri_path[512];  // Increased buffer size
		ret = snprintf(uri_path, sizeof(uri_path), "m/%s/c/%s?authorization=%s",
						DOMAIN_ID, CHANNEL_ID, CLIENT_SECRET);
		if (ret >= sizeof(uri_path)) {
			LOG_ERR("URI path too long");
			close(sock4);
			return -1;
		}
		LOG_INF("URI path: %s", uri_path);
		req.url = uri_path;
		req.host = MAGISTRALA_IP;
		req.protocol = "HTTP/1.1";
		req.payload = payload; // Content of payload
		req.payload_len = strlen(req.payload);
		req.response = response_cb;
		req.recv_buf = recv_buf_ipv4;
		req.recv_buf_len = sizeof(recv_buf_ipv4);

		ret = http_client_req(sock4, &req, timeout, "IPv4 POST");
		LOG_INF("HTTP client request returned %d", ret);

		close(sock4); 
	}

	return ret;
}

int main(void)
{
	k_sleep(K_SECONDS(5));

	net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
	net_mgmt_add_event_callback(&cb);

	/* Get STA interface in AP-STA mode. */
	sta_iface = net_if_get_wifi_sta();

	connect_to_wifi();
	set_static_ip();

	k_sleep(K_SECONDS(5));

	int iterations = CONFIG_NET_SAMPLE_SEND_ITERATIONS;
	int i = 0;
	int ret = 0;

	while (iterations == 0 || i < iterations) {
		ret = run_queries();
		if (ret < 0) {
			ret = 1;
			break;
		}

		if (iterations > 0) {
			i++;
			if (i >= iterations) {
				ret = 0;
				break;
			}
		} else {
			ret = 0;
			break;
		}
	}

	if (iterations == 0) {
		k_sleep(K_FOREVER);
	}

	exit(ret);
	return ret;
}
