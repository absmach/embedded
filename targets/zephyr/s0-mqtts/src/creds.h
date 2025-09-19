#ifndef CREDS_H
#define CREDS_H

#include <zephyr/kernel.h>
#include <zephyr/net/tls_credentials.h>

/* Security tags for credentials */
#define TLS_TAG_CA_CERT 1
#define TLS_TAG_CLIENT_CERT 2
#define TLS_TAG_CLIENT_KEY 3

/* CA Certificate - Replace with your broker's CA certificate */
const unsigned char ca_cert[] = 
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDyzCCArOgAwIBAgIUTowZsUNQos3WatB8OL5gfFpdDnQwDQYJKoZIhvcNAQEN\r\n"
    "BQAwdTEiMCAGA1UEAwwZTWFnaXN0cmFsYV9TZWxmX1NpZ25lZF9DQTETMBEGA1UE\r\n"
    "CgwKTWFnaXN0cmFsYTEWMBQGA1UECwwNbWFnaXN0cmFsYV9jYTEiMCAGCSqGSIb3\r\n"
    "DQEJARYTaW5mb0BtYWdpc3RyYWxhLmNvbTAeFw0yNTA5MTcwOTI3NTZaFw0yODA5\r\n"
    "MTYwOTI3NTZaMHUxIjAgBgNVBAMMGU1hZ2lzdHJhbGFfU2VsZl9TaWduZWRfQ0Ex\r\n"
    "EzARBgNVBAoMCk1hZ2lzdHJhbGExFjAUBgNVBAsMDW1hZ2lzdHJhbGFfY2ExIjAg\r\n"
    "BgkqhkiG9w0BCQEWE2luZm9AbWFnaXN0cmFsYS5jb20wggEiMA0GCSqGSIb3DQEB\r\n"
    "AQUAA4IBDwAwggEKAoIBAQDGuKkwKPVfmV/NH/v0d93sv3kggV5WC8VBYkE76ZVp\r\n"
    "0dx8U8GtA9l4+mQ7+mMYChYjFX3uG4kYsgDJ5BsOUkFgrBmQ/BotLDVqu1x2sCfF\r\n"
    "DUI6PxkL7WTHsNm8hneU9LaFhFgLz8AjjwHeA+AfGrSDPNgfKP2B7KVPkEh34Tvb\r\n"
    "wi36+Wag9of9am+vLeV36XNT6t6Zk8GBTPpx2irg4cDB+E5utIoDMQRU+YFmJq/Y\r\n"
    "S4/7kAxLucSTh99hcelT7nlTZCenxNdBBut+1RHRYdd9gmSWfL2XDsGYFOgl3EEd\r\n"
    "ST9CAW2vBB3UCEEAAX4Q/+V1ku5I44XFlKTPgLlnZLgnAgMBAAGjUzBRMB0GA1Ud\r\n"
    "DgQWBBQv+ml0advUEElKkTuKOQQnB4LK8zAfBgNVHSMEGDAWgBQv+ml0advUEElK\r\n"
    "kTuKOQQnB4LK8zAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBDQUAA4IBAQB/\r\n"
    "4s9Es+uoZXeMYjJel9cTdKdzjc4+BuZmHAP0G+cPV7Ya0lBzmli/4mefii4QGEbF\r\n"
    "BL4v8nWwQtVDrIFLnApz+YG5DPPcKin9AEoJXHsQ3ypk4XuK4s+Wx7LwgyocOFzu\r\n"
    "sEzmgOzmr+ahp+GzLQz/HVsm2Yi3Al3ooczBXNEVKfJtalJJyjZF2X52gNopbJzb\r\n"
    "JD8qRsSYh57onaVBvfdULRMHhtN419Z254yuVHCFXp5va9ZRoKEdtFYEGZiSVWNe\r\n"
    "DuXhy/u4LBu8sMi/JTGAe+LZDn6WID15YuVmbvVrosOTljJ3NvMQ+JOExob9g0ha\r\n"
    "eZM4syAR8LnypWrvnkvK\r\n"
    "-----END CERTIFICATE-----\r\n";

const size_t ca_cert_len = sizeof(ca_cert) - 1;

/* Client Certificate - Optional, for client authentication */
const unsigned char client_cert[] = 
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIEsTCCA5mgAwIBAgIUbZRy82pAXOYCZJyketruvVy0sR4wDQYJKoZIhvcNAQEL\r\n"
    "BQAwdTEiMCAGA1UEAwwZTWFnaXN0cmFsYV9TZWxmX1NpZ25lZF9DQTETMBEGA1UE\r\n"
    "CgwKTWFnaXN0cmFsYTEWMBQGA1UECwwNbWFnaXN0cmFsYV9jYTEiMCAGCSqGSIb3\r\n"
    "DQEJARYTaW5mb0BtYWdpc3RyYWxhLmNvbTAeFw0yNTA5MTcwOTQzNDZaFw0yNzA5\r\n"
    "MTcwOTQzNDZaMGwxGDAWBgNVBAMMDzxUSElOR19TRUNSRVQ+IDETMBEGA1UECgwK\r\n"
    "TWFnaXN0cmFsYTEXMBUGA1UECwwObWFnaXN0cmFsYV9jcnQxIjAgBgkqhkiG9w0B\r\n"
    "CQEWE2luZm9AbWFnaXN0cmFsYS5jb20wggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAw\r\n"
    "ggIKAoICAQCh6LinhfqaES3wiMbKn5mtAcSt6DgRuULoiRM0THJkY3BhiDSiQF+5\r\n"
    "d+dUPRBQOMgrOO62ff4VAz1yA+M0O90nK+u6t83pdsdfmYoX9527OoK77eh+4eFY\r\n"
    "vUv1EYHRBUHD/Z3F5uqde5MXAJKtTlWQgRrf6jrgVQzF6HGZ7gV2P0W/bl0uUCK6\r\n"
    "TZ5Yes+/2UYBASt9YnJyInY9dGJXOTeZZGX5LNV5Mlv+2Moup4TNtVCIDdRfro9k\r\n"
    "snjA366UCAr2ADKqlAkidRpj9r6Utsjsg/OvUs8HcDDp15ukdYVukYwCyvYhu28D\r\n"
    "uPIdwiIdEXN+F1eUvNPH/2hdJ2dBsukurAU7NjPygYwxl4+mn2fq0HcEAwXzrsKy\r\n"
    "t5BjgTPg5gNfABE18lrVuOuLHWjlVHcNEAiwJraXGxs/Vu09r2wNjPMQ2H4f8Q8M\r\n"
    "+0USDf/0vfPPDw5vNOpp3d74E5Wsdy43xAoLBnOW35FzY4COKc1ElA4sZsHiq52k\r\n"
    "RdUsqHqEw2FDFFXodpyOOdxJcGSOFWol5cAIAP6eeCAgVY3e/H3b0BTZg9iHst5J\r\n"
    "IvzFBQNzdk7YWmC1m+shA7hXMsdMQ8/Hv1kvjdUww1lUy+FtgjtMbn00Iea8DPwf\r\n"
    "hgKcSdNsBS3Ap9WI2ZFHbG8BB7uYgg2hWgVQbtq0EGPBDjCNCFG0iQIDAQABo0Iw\r\n"
    "QDAdBgNVHQ4EFgQUdXUpBHknPLqBgF0dgfuAWdxX/sowHwYDVR0jBBgwFoAUL/pp\r\n"
    "dGnb1BBJSpE7ijkEJweCyvMwDQYJKoZIhvcNAQELBQADggEBAJIISkjWk6UbrA/S\r\n"
    "GycgyscYeQ/CB0Uz5IxooqTzO1zXBZhJTzzd5VTr5cNG1hrzZy5MENe5YOZcZX4H\r\n"
    "/CUTAtsg0fKGPbfuwwfF0FRw5uL7/DGYduqVN3O7Z3es13a76cQBrlk2oipaRfRW\r\n"
    "LK6Gra0X/yQH8taGPK2VGaKJc51sqLmiATHqln6Orwn1Is/Gs07Zn22ya+uz2KXI\r\n"
    "zHscIz0mUNwFYgzpZLEccG/zxRwFCnucuVGha5ZQIzQzceUqqdkVt1/tvMDIPTEr\r\n"
    "9Hai9o1srairmwCSNiHHSCXFtEHu3KMs1Cx1NBeOmefQHBd0EHimgWdzJQI9Uq6E\r\n"
    "KFCwW7c=\r\n"
    "-----END CERTIFICATE-----\r\n";

const size_t client_cert_len = sizeof(client_cert) - 1;

/* Client Private Key - Optional, for client authentication */
const unsigned char client_key[] = 
    "-----BEGIN PRIVATE KEY-----\r\n"
    "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\r\n"
    "Wl5PoBJ9cqGgrS/3/T5Apmebi1937D0=\r\n"
    "-----END PRIVATE KEY-----\r\n";

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