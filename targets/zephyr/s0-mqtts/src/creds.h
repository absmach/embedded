#ifndef CREDS_H
#define CREDS_H

#include <zephyr/kernel.h>
#include <zephyr/net/tls_credentials.h>

/* Security tags for credentials */
#define TLS_TAG_CA_CERT 1
#define TLS_TAG_CLIENT_CERT 2
#define TLS_TAG_CLIENT_KEY 3

/* CA Certificate - Replace with your broker's CA certificate */
const unsigned char ca_cert[] = ""; // Replace with your CA certificate in PEM format

const size_t ca_cert_len = sizeof(ca_cert) - 1;

/* Client Certificate - Optional, for client authentication */
const unsigned char client_cert[] = ""; // Replace with your client certificate in PEM format

const size_t client_cert_len = sizeof(client_cert) - 1;

/* Client Private Key - Optional, for client authentication */
const unsigned char client_key[] = ""; // Replace with your client private key in PEM format

const size_t client_key_len = sizeof(client_key) - 1; 

static bool certs_initialized = false;

static inline int certs_init(void)
{
    int ret;
    
    if (certs_initialized) {
        printk("Certificates already initialized");
        return 0;
    }
    
    printk("Initializing mutual TLS certificates...");
    
    // Add the CA certificate
    ret = tls_credential_add(TLS_TAG_CA_CERT, TLS_CREDENTIAL_CA_CERTIFICATE,
                            ca_cert, ca_cert_len);
    if (ret < 0) {
        printk("Failed to add CA certificate: %d", ret);
        return ret;
    }
    printk("CA certificate added successfully");

    /* Add the client certificate */
    ret = tls_credential_add(TLS_TAG_CLIENT_CERT, TLS_CREDENTIAL_PUBLIC_CERTIFICATE,
                            client_cert, client_cert_len);
    if (ret < 0) {
        printk("Failed to add client certificate: %d", ret);
        return ret;
    }
    printk("Client certificate added successfully");
    
    // Add the client private key
    ret = tls_credential_add(TLS_TAG_CLIENT_KEY, TLS_CREDENTIAL_PRIVATE_KEY,
                            client_key, client_key_len);
    if (ret < 0) {
        printk("Failed to add client private key: %d", ret);
        return ret;
    }
    printk("Client private key added successfully");


    certs_initialized = true;
    printk("Standard TLS certificates initialized successfully");
    
    return ret;
}

#endif /* CREDS_H */