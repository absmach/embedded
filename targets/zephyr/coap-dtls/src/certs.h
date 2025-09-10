#ifndef CERTS_H
#define CERTS_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/tls_credentials.h>

// TLS security tags for mutual TLS authentication
#define TLS_TAG_CA_CERT 1
#define TLS_TAG_CLIENT_CERT 2
#define TLS_TAG_CLIENT_KEY 3

// CA Certificate
static const char ca_cert[] = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIICNTCCAdygAwIBAgIPAQIDBAUGBwgJCgsMDQ4PMAoGCCqGSM49BAMCMFMxCzAJ\n"
    "BgNVBAYTAkJSMQ8wDQYDVQQIEwZQYXJhbmExETAPBgNVBAcTCEN1cml0aWJhMQ0w\n"
    "CwYDVQQKEwRUZXN0MREwDwYDVQQDEwh0ZXN0LmNvbTAeFw0yNTA5MTAwODEwMjZa\n"
    "Fw0yNTA5MTAwOTEwMjZaMFMxCzAJBgNVBAYTAkJSMQ8wDQYDVQQIEwZQYXJhbmEx\n"
    "ETAPBgNVBAcTCEN1cml0aWJhMQ0wCwYDVQQKEwRUZXN0MREwDwYDVQQDEwh0ZXN0\n"
    "LmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABFFcPW6545a5BNP+yn9U/c0M\n"
    "wemXvzddylFa0KbDtANfRTa+OlDzGPv5pUdZAqIhUCvvDVfgjFOyzApW8X2fk1Sj\n"
    "gZIwgY8wDgYDVR0PAQH/BAQDAgKEMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEF\n"
    "BQcDATAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBRCaYiUMeMTGWb8r2pFcUGU\n"
    "PtLDWzAuBgNVHREEJzAlgQtjYUB0ZXN0LmNvbYcEfwAAAYcQAAAAAAAAAAAAAAAA\n"
    "AAAAATAKBggqhkjOPQQDAgNHADBEAiBL3l0pOMOv/JRYhaMW5tYAANaCVuV8UxHb\n"
    "GhaWkIDS5AIgRpVFWY9yUjPBqpkaBKJ6HfxAVtYidpM7Ix0QtX1jgJw=\n"
    "-----END CERTIFICATE-----\n";

static const size_t ca_cert_len = sizeof(ca_cert);

// Client Certificate
static const char client_cert[] = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIICGTCCAb+gAwIBAgIQPxyp2HjLMSgTPQWGI/rX5DAKBggqhkjOPQQDAjBTMQsw\n"
    "CQYDVQQGEwJCUjEPMA0GA1UECBMGUGFyYW5hMREwDwYDVQQHEwhDdXJpdGliYTEN\n"
    "MAsGA1UEChMEVGVzdDERMA8GA1UEAxMIdGVzdC5jb20wHhcNMjUwOTEwMDgxMDI2\n"
    "WhcNMjUwOTEwMDkxMDI2WjBTMQswCQYDVQQGEwJCUjEPMA0GA1UECBMGUGFyYW5h\n"
    "MREwDwYDVQQHEwhDdXJpdGliYTENMAsGA1UEChMEVGVzdDERMA8GA1UEAxMIdGVz\n"
    "dC5jb20wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASn3z2iLvgsONyy3YLwFtAG\n"
    "SJ1F/7n5b2aMtv9YqOF0QTLBZ+EzLv9tzws6CN8ldlD83QgD9fLkz+rXXfOWrNRf\n"
    "o3UwczAOBgNVHQ8BAf8EBAMCB4AwHQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUF\n"
    "BwMBMA4GA1UdDgQHBAUBAgMEBjAyBgNVHREEKzApgQ9jbGllbnRAdGVzdC5jb22H\n"
    "BH8AAAGHEAAAAAAAAAAAAAAAAAAAAAEwCgYIKoZIzj0EAwIDSAAwRQIgJbCP2/mR\n"
    "V080adSG+7AkoCny2WWbS8datU0x8kFOvi0CIQCf3N8uYY7eCVZSzPrWpBSDwJLG\n"
    "Ky05u7yja8m88wQayA==\n"
    "-----END CERTIFICATE-----\n";

static const size_t client_cert_len = sizeof(client_cert);

// Client Private Key
static const char client_key[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg4b9jAE6iNA3Nsm04\n"
    "gkLWq4FaGVmtuUM2SfMcgfsYArmhRANCAASn3z2iLvgsONyy3YLwFtAGSJ1F/7n5\n"
    "b2aMtv9YqOF0QTLBZ+EzLv9tzws6CN8ldlD83QgD9fLkz+rXXfOWrNRf\n"
    "-----END EC PRIVATE KEY-----\n";
static const size_t client_key_len = sizeof(client_key);

static bool certs_initialized = false;

/**
 * @brief Initialize TLS certificates for mutual authentication
 */
static inline int certs_init(void)
{
    int ret;
    
    if (certs_initialized) {
        LOG_INF("Certificates already initialized");
        return 0;
    }
    
    LOG_INF("Initializing mutual TLS certificates...");
    
    // Add the CA certificate
    ret = tls_credential_add(TLS_TAG_CA_CERT, TLS_CREDENTIAL_NONE,
                            ca_cert, ca_cert_len);
    if (ret < 0) {
        LOG_ERR("Failed to add CA certificate: %d", ret);
        return ret;
    }
    LOG_INF("CA certificate added successfully");
    
    // Add the client certificate
    ret = tls_credential_add(TLS_TAG_CLIENT_CERT, TLS_CREDENTIAL_SERVER_CERTIFICATE,
                            client_cert, client_cert_len);
    if (ret < 0) {
        LOG_ERR("Failed to add client certificate: %d", ret);
        return ret;
    }
    LOG_INF("Client certificate added successfully");
    
    // Add the client private key
    ret = tls_credential_add(TLS_TAG_CLIENT_KEY, TLS_CREDENTIAL_PRIVATE_KEY,
                            client_key, client_key_len);
    if (ret < 0) {
        LOG_ERR("Failed to add client private key: %d", ret);
        return ret;
    }
    LOG_INF("Client private key added successfully");
    
    certs_initialized = true;
    LOG_INF("Mutual TLS certificates initialized successfully");
    
    return 0;
}

#endif /* CERTS_H */