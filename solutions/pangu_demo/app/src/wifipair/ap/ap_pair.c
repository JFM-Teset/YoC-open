


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <lwip/netdb.h>
#include <aos/aos.h>
#include <yoc/sysinfo.h>
#include <yoc/uservice.h>
#include <yoc/eventid.h>
#include <yoc/netmgr_service.h>
#include <yoc/init.h>

#include "devices/wifi.h"
#include "devices/device.h"
#include "ap_pair.h"
#include "lwip/apps/dhcps.h"
#include "index_html.h"
#include "errno.h"

#define TAG "ap_pair"

#define MAP_AP 15
static wifi_ap_record_t g_ap_records[MAP_AP];
static int g_ap_number;

const char *ok_html =
    "<meta charset=\"UTF-8\">\
<script>alert(\"Processing, please wait...\")</script>";

#define MAX_SSID_SIZE 32
#define MAX_PWD_SIZE 64

static uint8_t is_dns_server_run = 0;

struct ap_pair_context {
    aos_dev_t *dev;
    uint8_t ssid[MAX_SSID_SIZE];    //the result
    uint8_t password[MAX_PWD_SIZE]; //the result
    int pair_enable;
    ap_pair_callback_fn calllback_fn;
    aos_timer_t pair_timer;
    int pair_is_timeout;
    int timeout_ms;
};

typedef struct http_client {
    struct ap_pair_context* context;
    int fd;
} http_client_t;

void ap_pair_callback(struct ap_pair_context *context);


/* Function: urlDecode */
static char *url_decode(const char *str)
{
    int d = 0; /* whether or not the string is decoded */

    char *dStr = malloc(strlen(str) + 1);
    char eStr[] = "00"; /* for a hex code */

    strcpy(dStr, str);

    while (!d) {
        d = 1;
        int i; /* the counter for the string */

        for (i = 0; i < strlen(dStr); ++i) {

            if (dStr[i] == '%') {
                if (dStr[i + 1] == 0) {
                    return dStr;
                }

                if (isxdigit(dStr[i + 1]) && isxdigit(dStr[i + 2])) {

                    d = 0;

                    /* combine the next to numbers into one */
                    eStr[0] = dStr[i + 1];
                    eStr[1] = dStr[i + 2];

                    /* convert it to decimal */
                    long int x = strtol(eStr, NULL, 16);

                    /* remove the hex */
                    memmove(&dStr[i + 1], &dStr[i + 3], strlen(&dStr[i + 3]) + 1);

                    dStr[i] = x;
                }
            }
        }
    }

    return dStr;
}


void http_client_handle(void *args)
{
    struct ap_pair_context *context = ((http_client_t*)args)->context;
    int iNewSockFD = ((http_client_t*)args)->fd;
    int iStatus;
    int socket_recv_size = 2048;

    LOGD(TAG, "Start process client %d", iNewSockFD);
    // waits packets from the connected TCP client

#if 1
    /** set socket options */
    int timeout_ms = 10000;
    struct timeval interval = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};

    if (interval.tv_sec < 0 || (interval.tv_sec == 0 && interval.tv_usec <= 100)) {
        interval.tv_sec = 0;
        interval.tv_usec = 10000;
    }

    if (setsockopt(iNewSockFD, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval))) {
        LOGE(TAG, "SetSocket SO_RCVTIMEO failed");
        return;
    }

#endif

    uint8_t *http_header = aos_zalloc(1024);
    uint8_t *socket_recv_buf = aos_zalloc(socket_recv_size);
    aos_assert(http_header);
    aos_assert(socket_recv_buf);


    while (context->pair_enable == 1) {
        iStatus = recv(iNewSockFD, socket_recv_buf, socket_recv_size, 0);

        if (iStatus < 0) {
            LOGE(TAG, "TCP ERROR: server recv data error iStatus:%d error: %d", iStatus, errno);
            goto Exit;
        } else if (iStatus == 0) {
            LOGD(TAG, "TCP CLOSE");
            goto Exit;
        }

        // LOGD(TAG, "Received:%s\n", socket_recv_buf);

        /** get index.html */
        if ((strstr((char *)socket_recv_buf, "GET / HTTP/1.0") != NULL ||
            strstr((char *)socket_recv_buf, "GET /index.html HTTP/1.0") != NULL) &&
            (strstr((char *)socket_recv_buf, "Host: 192.168.1.1") != NULL)) {

            LOGD(TAG, "Received HTTP GET, Send response HTTP/1.0");
            /** send response */
            char *http_header_fmt = "HTTP/1.0 200 OK\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: %d\r\n\r\n";

            char *http_ssid_list = aos_zalloc_check(2000);

            const char *ssid_list_fmt = "\t\t\t<option value =\"%s\">%s</option>\n";

            for (int i = 0; i < g_ap_number; i++) {
                if (g_ap_records[i].ssid[0] != 0)
                    sprintf(http_ssid_list + strlen(http_ssid_list), ssid_list_fmt, g_ap_records[i].ssid, g_ap_records[i].ssid);
            }

            sprintf((char *)http_header, http_header_fmt, strlen(index_html_start) + strlen(index_html_end) + strlen(http_ssid_list));
            send(iNewSockFD, http_header, strlen((char *)http_header), 0);
            send(iNewSockFD, index_html_start, strlen(index_html_start), 0);
            send(iNewSockFD, http_ssid_list, strlen(http_ssid_list), 0);
            send(iNewSockFD, index_html_end, strlen(index_html_end), 0);

            aos_free(http_ssid_list);
            goto Exit;

        }

        if ((strstr((char *)socket_recv_buf, "GET / HTTP/1.1") != NULL ||
            strstr((char *)socket_recv_buf, "GET /index.html HTTP/1.1") != NULL) &&
            (strstr((char *)socket_recv_buf, "Host: 192.168.1.1") != NULL)) {

            LOGD(TAG, "Received HTTP GET, Send response HTTP/1.1");
            /** send response */
            char *http_header_fmt = "HTTP/1.0 200 OK\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: %d\r\n\r\n";

            char *http_ssid_list = aos_zalloc_check(2000);

            const char *ssid_list_fmt = "\t\t\t<option value =\"%s\">%s</option>\n";

            for (int i = 0; i < g_ap_number; i++) {
                if (g_ap_records[i].ssid[0] != 0)
                    sprintf(http_ssid_list + strlen(http_ssid_list), ssid_list_fmt, g_ap_records[i].ssid, g_ap_records[i].ssid);
            }

            sprintf((char *)http_header, http_header_fmt, strlen(index_html_start) + strlen(index_html_end) + strlen(http_ssid_list));
            send(iNewSockFD, http_header, strlen((char *)http_header), 0);
            send(iNewSockFD, index_html_start, strlen(index_html_start), 0);
            send(iNewSockFD, http_ssid_list, strlen(http_ssid_list), 0);
            send(iNewSockFD, index_html_end, strlen(index_html_end), 0);

            aos_free(http_ssid_list);
            goto Exit;
        }


        if ((strstr((char *)socket_recv_buf, "GET ")) != NULL) {

            LOGD(TAG, "Received HTTP GET, Send response 301");
            /** send response */
            char *http_header_fmt = "HTTP/1.0 302 Moved Temporarily\r\n"
                                    "Location: http://192.168.1.1/index.html\r\n\r\n";

            send(iNewSockFD, http_header_fmt, strlen((char *)http_header_fmt), 0);

            goto Exit;
        }

        /* received ssid&password */
        if ((strstr((char *)socket_recv_buf, "POST / HTTP/1.1")) != NULL ||
            (strstr((char *)socket_recv_buf, "POST /index.html HTTP/1.1")) != NULL) {

            LOGD(TAG, "Received HTTP POST, Send response");
            /** send response */
            char *http_header_fmt = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: %d\r\n\r\n";

            char *pos1 = strstr((char *)socket_recv_buf, "Content-Length:");
            char *pos2 = strstr(pos1, "\r\n");

            if (!pos1 || !pos2) {
                goto Exit;
            }

            char content_length_str[10] = {0};
            memcpy(content_length_str, pos1 + 16, pos2 - pos1 - 16);
            int content_length = atoi(content_length_str);

            char *pos_content = strstr(pos2, "\r\n\r\n");

            if (!pos_content || content_length <= 0) {
                goto Exit;
            }

            char *content = aos_zalloc(content_length + 1);

            memcpy(content, pos_content + 4, strlen(pos_content) - 4);

            if (content_length > strlen(pos_content)) {
                iStatus = recv(iNewSockFD, content + strlen(pos_content) - 4, content_length - strlen(pos_content) + 4, 0);

                if (iStatus < 0) {
                    aos_free(content);
                    goto Exit;
                } else if (iStatus == 0) {
                    aos_free(content);
                    goto Exit;
                }
            }

            /*
                Client browser makes 2 3+4 to 2+3%2B4,
                so replace '+' with ' '
            */
            int i = 0;

            if (strstr((char *)socket_recv_buf, "User-Agent: okhttp") == NULL) {
                for (i = 0; i < content_length; i++) {
                    if (content[i] == '+') {
                        content[i] = ' ';
                    }
                }
                LOGD(TAG, "Normal browser fix + symbol");
            }

            char *decoded_content = url_decode(content);
            LOGD(TAG, "pair string=%s", decoded_content);
            char *ssid_pos = strstr(decoded_content, "SSID=");
            char *ssid_end = strstr(ssid_pos, "&");
            char *pass_pos = strstr(decoded_content, "PASS=");

            if (ssid_pos == NULL || pass_pos == NULL || ssid_end == NULL) {
                free(decoded_content);
                free(content);
                goto Exit;
            }

            *ssid_end = 0;

            strcpy((char *)context->ssid, ssid_pos + 5);
            strcpy((char *)context->password, pass_pos + 5);

            context->pair_enable = 0;

            sprintf((char *)http_header, http_header_fmt, strlen(ok_html));
            send(iNewSockFD, http_header, strlen((char *)http_header), 0);
            send(iNewSockFD, ok_html, strlen(ok_html), 0);

            free(decoded_content);
            free(content);
            goto Exit;

        }


    }

Exit:
    aos_free(http_header);
    aos_free(socket_recv_buf);
    aos_free(args);

    close(iNewSockFD);
}

int http_server(struct ap_pair_context *context)
{
    struct sockaddr_in  sAddr;
    struct sockaddr_in  sLocalAddr;
    int                 iAddrSize;
    int                 iSockFD;
    int                 iNewSockFD;
    int                 n;
    int                 iStatus;

    // creating a TCP socket
    iSockFD = socket(AF_INET, SOCK_STREAM, 0);

    if (iSockFD < 0) {
        goto Exit2;
    }

    n = 1;
    setsockopt(iSockFD, SOL_SOCKET, SO_REUSEADDR, (const char *) &n, sizeof(n));


    /** set socket timeout to 5 s */
    int timeout_ms = 5 * 1000;
    struct timeval interval = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};

    if (interval.tv_sec < 0 || (interval.tv_sec == 0 && interval.tv_usec <= 100)) {
        interval.tv_sec = 0;
        interval.tv_usec = 10000;
    }

    if (setsockopt(iSockFD, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval))) {
        LOGE(TAG, "SetSocket SO_RCVTIMEO failed");
        return -1;
    }

    memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
    sLocalAddr.sin_family      = AF_INET;
    sLocalAddr.sin_port        = htons(80);
    sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    iAddrSize = sizeof(sLocalAddr);

    // binding the TCP socket to the TCP server address
    iStatus = bind(iSockFD, (struct sockaddr *)&sLocalAddr, iAddrSize);

    if (iStatus < 0) {
        LOGE(TAG, "ERROR: bind tcp server socket fd error! errno=%d", errno);
        goto Exit1;
    }

    LOGD(TAG, "Bind successfully.");


    // putting the socket for listening to the incoming TCP connection
    iStatus = listen(iSockFD, 20);

    if (iStatus != 0) {
        LOGE(TAG, "TCP ERROR: listen tcp server socket fd error %d", iStatus);
        goto Exit1;
    }


Restart:
    iNewSockFD = -1;

    // waiting for an incoming TCP connection
    while (iNewSockFD < 0) {
        // accepts a connection form a TCP client, if there is any
        // otherwise returns SL_EAGAIN
        int addrlen = sizeof(sAddr);

        /* will block here... */
        iNewSockFD = accept(iSockFD, (struct sockaddr *)&sAddr, (socklen_t *)&addrlen);

        if (iNewSockFD < 0) {
            //printf("#");
            //LOGE(TAG, "ERROR: Accept tcp client socket fd error! ");
            if (context->pair_enable == 0) {
                goto Exit1;
            }
        } else {
            LOGD(TAG, "Accept socket %d successfully.", iNewSockFD);
        }
    }
    http_client_t *client = aos_zalloc_check(sizeof(http_client_t));
    client->context = context;
    client->fd = iNewSockFD;
    aos_task_new("http_server", http_client_handle, client, 2048);

    if (context->pair_enable == 0) {
        goto Exit1;    /* All done case */
    }

    goto Restart;

Exit1:
    LOGD(TAG, "Close server socket.");
    // close the listening socket
    shutdown(iSockFD, SHUT_RDWR);
    close(iSockFD);
Exit2:
    return 0;
}

typedef struct {
    unsigned short id;       // identification number
    unsigned char rd :1;     // recursion desired
    unsigned char tc :1;     // truncated message
    unsigned char aa :1;     // authoritive answer
    unsigned char opcode :4; // purpose of message
    unsigned char qr :1;     // query/response flag
    unsigned char rcode :4;  // response code
    unsigned char cd :1;     // checking disabled
    unsigned char ad :1;     // authenticated data
    unsigned char z :1;      // its z! reserved
    unsigned char ra :1;     // recursion available
    unsigned short qd_count; // number of question entries
    unsigned short an_count; // number of answer entries
    unsigned short ns_count; // number of authority entries
    unsigned short nr_count; // number of resource entries
} DNS_HDR;

typedef struct {
    unsigned short type;
    unsigned short classes;
} DNS_QDS;

typedef struct {
    unsigned short type;
    unsigned short classes;
    unsigned int ttl;
    unsigned short rd_length;
    char rd_data[0];
} DNS_RRS;
#define PACKAGE_SIZE 1500

static uint8_t rbuffer[PACKAGE_SIZE] __attribute__ ((aligned(4)));
static uint8_t sbuffer[PACKAGE_SIZE] __attribute__ ((aligned(4)));
static void process_query(int fd)
{
    DNS_HDR *hdr, *shdr;
    DNS_QDS qds;
    DNS_RRS rrs;
    socklen_t addrlen;
    struct sockaddr_in source;
    uint8_t *pos, *head, *rear;
    int size, dlen;
    unsigned char qlen;
    unsigned short q_len;
    uint8_t domain[256];
    ip_addr_t     ipaddr;

    addrlen = sizeof(struct sockaddr_in);
    size = recvfrom(fd, rbuffer, PACKAGE_SIZE, 0, (struct sockaddr*)&source, &addrlen);
    if(size <= (int)sizeof(DNS_HDR))
        return;

    hdr = (DNS_HDR*)rbuffer;
    shdr = (DNS_HDR*)sbuffer;
    memset(sbuffer, 0, PACKAGE_SIZE);

    shdr->id = hdr->id;
    shdr->qr = 1;
    q_len = 0;
    head = rbuffer + sizeof(DNS_HDR);
    rear = rbuffer + size;
    if(hdr->qr != 0 || hdr->tc != 0 || ntohs(hdr->qd_count) != 1)
        shdr->rcode = 1;
    else {
        dlen = 0;
        pos = head;
        while(pos < rear) {
            qlen = (unsigned char)*pos++;
            if(qlen > 63 || (pos + qlen) > (rear - sizeof(DNS_QDS))) {
                shdr->rcode = 1;
                break;
            }
            if(qlen > 0) {
                if(dlen > 0)
                    domain[dlen++] = '.';
                while(qlen-- > 0)
                    domain[dlen++] = (char)tolower(*pos++);
            }
            else {
                memcpy(&qds, pos, sizeof(DNS_QDS));
                if(ntohs(qds.classes) != 0x01)
                    shdr->rcode = 4;
                else {
                    pos += sizeof(DNS_QDS);
                    q_len = pos - head;
                }
                break;
            }
        }
        domain[dlen] = '\0';
    }

    LOGD(TAG, "DNS QUERY URL: %s", domain);

    if(shdr->rcode == 0 && ntohs(qds.type) == 0x01) {

        shdr->qd_count = htons(1);
        shdr->an_count = htons(1);
        pos = sbuffer + sizeof(DNS_HDR);
        memcpy(pos, head, q_len);
        pos += q_len;

        *pos++ = 0xc0;
        *pos++ = 0x0c;

        rrs.type = htons(1);
        rrs.classes = htons(1);
        rrs.ttl = htonl(36000);
        rrs.rd_length = htons(sizeof(ipaddr));
        memcpy(pos, &rrs, 10);

        pos += 10;

        IP4_ADDR(&ipaddr, 192, 168, 1, 1);
        memcpy(pos, &ipaddr, sizeof(ipaddr));

        pos += sizeof(ipaddr);

        sendto(fd, sbuffer, pos - sbuffer, 0, (struct sockaddr*)&source, sizeof(struct sockaddr_in));
        return;
    }

    if(shdr->rcode != 0)
        sendto(fd, sbuffer, sizeof(DNS_HDR), 0, (struct sockaddr*)&source, sizeof(struct sockaddr_in));
}

void dns_server_entry(void *arg)
{
    int maxfd, fds;
    fd_set readfds;
    struct sockaddr_in addr;
    int enable = 1;
    struct timeval tv;
    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_fd < 0) {
        LOGE(TAG, "create socket");
        return;
    }
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(53);
    if(bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        LOGE(TAG, "bind service port");
        return;
    }

    is_dns_server_run = 1;

    while(is_dns_server_run == 1) {
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        maxfd = socket_fd + 1;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        fds = select(maxfd, &readfds, NULL, NULL, &tv);
        if(fds > 0) {

            if(FD_ISSET(socket_fd, &readfds))
                process_query(socket_fd);
        }
    }

    close(socket_fd);

    is_dns_server_run = 0;
    aos_task_exit(0);
}

static void pair_thread(void *arg)
{
    int i = 0;
    struct ap_pair_context *context = (struct ap_pair_context *)arg;

    aos_task_t task_handle;
    aos_check_param(!aos_task_new_ext(&task_handle, "dns_server", dns_server_entry, NULL, 2048, 32));

    while (1) {
        if (dhcps_get_client_number() > 0)
            break;
        else
            aos_msleep(1000);

        if (context->pair_enable == 0)
            goto exit;
    }

    {
        wifi_sta_list_t sta_list;
        hal_wifi_ap_get_sta_list(context->dev, &sta_list);
        for (i = 0; i < sta_list.num; i++) {
            LOGD(TAG, "STA[%d]=%02x:%02x:%02x:%02x:%02x:%02x\n", 
                i, 
                sta_list.sta[i].mac[0],
                sta_list.sta[i].mac[1],
                sta_list.sta[i].mac[2],
                sta_list.sta[i].mac[3],
                sta_list.sta[i].mac[4],
                sta_list.sta[i].mac[5]);
        }
    }

    http_server(context);

exit:
    is_dns_server_run = 2;
    while (is_dns_server_run) {
        aos_msleep(1000);
        LOGD(TAG, "wait dns server exit");
    }

    //do all the callback to user here!!!
    ap_pair_callback(context);

    aos_task_exit(0);

}


static void pair_thread_start(struct ap_pair_context *context)
{
    aos_task_t task_handle;

    if (0 != aos_task_new_ext(&task_handle, "pair_thread", pair_thread, context, 5*1024, AOS_DEFAULT_APP_PRI + 3)) {
        LOGE(TAG, "Create network task failed.");
    }
}


static void pair_timer_expire(void *timer, void *arg)
{
    struct ap_pair_context *context = (struct ap_pair_context *)arg;

    LOGD(TAG, "AP Pair timer expires");

    context->pair_is_timeout = 1;
    context->pair_enable = 0; /** main loop will exit */
}

static void pair_timer_start(struct ap_pair_context *context)
{
    int ret = 0;
    if (context->timeout_ms == 0)
        return;

    aos_timer_new_ext(&context->pair_timer, pair_timer_expire,
        context, context->timeout_ms, 0, 0);
    if ((ret = aos_timer_start(&context->pair_timer)) < 0)
        LOGE(TAG, "AP Pair timer start failed, %d", ret);

    if (ret == 0) {
        LOGD(TAG, "AP Pair timer start success");
    }
}

static void pair_timer_stop(struct ap_pair_context *context)
{
    if (context->pair_timer.hdl) {
        aos_timer_stop(&context->pair_timer);
        aos_timer_free(&context->pair_timer);
    }
}

void ap_pair_callback(struct ap_pair_context *context)
{
    LOGD(TAG, "AP Pair Callback");

    pair_timer_stop(context);
    hal_wifi_stop(context->dev); // close AP when pair finished
    hal_wifi_install_event_cb(context->dev, NULL);
    device_close(context->dev);

    if (context->pair_is_timeout)
        context->calllback_fn(1, NULL, NULL);
    else if (strlen((char*)context->ssid) > 0)
        context->calllback_fn(0, (char*)context->ssid, (char*)context->password);
    else
        context->calllback_fn(1, NULL, NULL);

    aos_free(context);
}

void scan_compeleted(aos_dev_t *dev, uint16_t number, wifi_ap_record_t *ap_records)
{
    // 排序
    wifi_ap_record_t wifiApRecord;
    for (int j = 0; j < number; ++j) {
        for (int i = j + 1; i < number; ++i) {
            if (ap_records[i].rssi > ap_records[j].rssi) {
                wifiApRecord = ap_records[i];
                ap_records[i] = ap_records[j];
                ap_records[j] = wifiApRecord;
            }
        }
    }

    g_ap_number = 0;
    for (int i = 0; i < number; i++) {
        if (ap_records[i].ssid[0] != '\0') {
            if (g_ap_number >= MAP_AP) {
                return;
            }

            int j = 0;
            for (; j < g_ap_number; j++) {
                if (strcmp((char*)&g_ap_records[j].ssid, (char*)&ap_records[i].ssid) == 0) {
                    break;
                }
            }

            if (j == g_ap_number) {
                memcpy(&g_ap_records[g_ap_number++], &ap_records[i], sizeof(wifi_ap_record_t));
                LOGD(TAG, "ssid: %s, rssi, %d", g_ap_records[g_ap_number - 1].ssid, g_ap_records[g_ap_number - 1].rssi);
            }
        }
    }
}

static wifi_event_func wifi_event = {
    NULL,
    NULL,
    scan_compeleted,
    NULL
};

/**
    User starts the AP Pair process
*/
int ap_pair_start(int timeout_ms, char *ssid, char* password, ap_pair_callback_fn fn)
{
    uint8_t mac[6] = {0};
    wifi_config_t config;
    struct ap_pair_context *context = aos_zalloc(sizeof(struct ap_pair_context));

    context->dev = device_open_id("wifi", 0);
    context->calllback_fn = fn;
    context->pair_enable = 1;
    context->pair_is_timeout = 0;
    context->timeout_ms = timeout_ms;

    LOGI(TAG, "Scan Start");
    memset(&config, 0, sizeof(config));
    config.mode = WIFI_MODE_STA;
    hal_wifi_start(context->dev, &config);
    hal_wifi_install_event_cb(context->dev, &wifi_event);
    aos_msleep(2000);
    hal_wifi_get_mac_addr(context->dev, mac);

    hal_wifi_start_scan(context->dev, NULL, 1);
    hal_wifi_stop(context->dev);

    device_close(context->dev);
    aos_msleep(1000);

    context->dev = device_open_id("wifi", 0);

    pair_timer_start(context);


    LOGI(TAG, "Start AP");

    memset(&config, 0, sizeof(wifi_config_t));

    snprintf(config.ssid, sizeof(config.ssid), "%s[%02x:%02x:%02x:%02x:%02x:%02x]", ssid, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    config.ssid[sizeof(config.ssid) - 1] = 0;
    config.mode = WIFI_MODE_AP;
    // strcpy(config.ssid, ssid);
    strcpy(config.password, password);
    hal_wifi_start(context->dev, &config);


    pair_thread_start(context);

    return 0;
}



