// Project Yurei - High-performance Solana data engine (MIT License)
// Copyright (c) 2025 Project Yurei
// https://x.com/yureiai  PRD: yurei-jsonrpc-client
#include "websocket_client.h"

#include <libwebsockets.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "logging.h"
#include "parser.h"

typedef struct {
    char address[256];
    char path[256];
    int port;
    bool secure;
} WsEndpoint;

typedef struct {
    char outbound[512];
    size_t outbound_len;
    bool send_pending;
} WsSessionData;

static YureiWebsocketClient *g_client = NULL;

static int parse_endpoint(const char *url, WsEndpoint *endpoint) {
    if (!url || !endpoint) {
        return -1;
    }
    memset(endpoint, 0, sizeof(*endpoint));

    const char *scheme_separator = strstr(url, "://");
    if (!scheme_separator) {
        return -1;
    }
    size_t scheme_len = scheme_separator - url;
    char scheme[8];
    if (scheme_len >= sizeof(scheme)) {
        return -1;
    }
    memcpy(scheme, url, scheme_len);
    scheme[scheme_len] = '\0';

    if (strcasecmp(scheme, "wss") == 0) {
        endpoint->secure = true;
        endpoint->port = 443;
    } else if (strcasecmp(scheme, "ws") == 0) {
        endpoint->secure = false;
        endpoint->port = 80;
    } else {
        return -1;
    }

    const char *host_start = scheme_separator + 3;
    const char *path_start = strchr(host_start, '/');
    if (!path_start) {
        path_start = url + strlen(url);
    }

    const char *port_sep = strchr(host_start, ':');
    size_t host_len = 0;
    if (port_sep && port_sep < path_start) {
        host_len = (size_t)(port_sep - host_start);
        endpoint->port = atoi(port_sep + 1);
    } else {
        host_len = (size_t)(path_start - host_start);
    }
    if (host_len == 0 || host_len >= sizeof(endpoint->address)) {
        return -1;
    }
    memcpy(endpoint->address, host_start, host_len);
    endpoint->address[host_len] = '\0';

    if (*path_start) {
        snprintf(endpoint->path, sizeof(endpoint->path), "%s", path_start);
    } else {
        snprintf(endpoint->path, sizeof(endpoint->path), "/");
    }
    return 0;
}

static void build_mentions_array(const YureiConfig *config, char *out, size_t len) {
    bool first = true;
    size_t offset = 0;
    if (len == 0) {
        return;
    }
    offset += snprintf(out + offset, len - offset, "[");
    if (config->pumpfun_program[0]) {
        offset += snprintf(out + offset,
                           len - offset,
                           "%s\"%s\"",
                           first ? "" : ",",
                           config->pumpfun_program);
        first = false;
    }
    if (config->raydium_program[0]) {
        offset += snprintf(out + offset,
                           len - offset,
                           "%s\"%s\"",
                           first ? "" : ",",
                           config->raydium_program);
        first = false;
    }
    if (first) {
        offset += snprintf(out + offset,
                           len - offset,
                           "\"%s\"",
                           config->pumpfun_program);
    }
    snprintf(out + offset, len - offset, "]");
}

static void schedule_subscription(WsSessionData *session, const YureiConfig *config) {
    if (!session || !config) {
        return;
    }
    char mentions[256];
    build_mentions_array(config, mentions, sizeof(mentions));
    session->outbound_len = (size_t)snprintf(
        session->outbound,
        sizeof(session->outbound),
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"logsSubscribe\","
        "\"params\":[{\"mentions\":%s},{\"commitment\":\"confirmed\"}]}",
        mentions);
    session->send_pending = true;
}

static int ws_callback(struct lws *wsi,
                       enum lws_callback_reasons reason,
                       void *user,
                       void *in,
                       size_t len) {
    WsSessionData *session = (WsSessionData *)user;
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            if (g_client) {
                g_client->connected = true;
                g_client->backoff_ms = g_client->config->ws_backoff_ms;
                schedule_subscription(session, g_client->config);
                lws_callback_on_writable(wsi);
                YUREI_LOG_INFO("WebSocket connected to %s", g_client->config->wss_endpoint);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
            YUREI_LOG_WARN("WebSocket connection error: %s",
                           in ? (const char *)in : "unknown");
            if (g_client) {
                g_client->connected = false;
                g_client->wsi = NULL;
            }
            break;
        }
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CLOSED: {
            if (g_client) {
                g_client->connected = false;
                g_client->wsi = NULL;
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            if (!g_client || !g_client->queue || !g_client->config) {
                break;
            }
            char *payload = calloc(len + 1, 1);
            if (!payload) {
                break;
            }
            memcpy(payload, in, len);
            payload[len] = '\0';
            yurei_parser_handle_message(payload, g_client->config, g_client->queue, NULL);
            free(payload);
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            if (session && session->send_pending) {
                unsigned char buffer[LWS_PRE + 512];
                if (session->outbound_len > sizeof(buffer) - LWS_PRE) {
                    session->send_pending = false;
                    break;
                }
                memcpy(&buffer[LWS_PRE], session->outbound, session->outbound_len);
                lws_write(wsi,
                          &buffer[LWS_PRE],
                          session->outbound_len,
                          LWS_WRITE_TEXT);
                session->send_pending = false;
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

static int establish_connection(YureiWebsocketClient *client) {
    if (!client || !client->config) {
        return -1;
    }

    WsEndpoint endpoint;
    if (parse_endpoint(client->config->wss_endpoint, &endpoint) != 0) {
        YUREI_LOG_ERROR("Invalid WSS endpoint: %s", client->config->wss_endpoint);
        return -1;
    }

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = (struct lws_context *)client->context;
    ccinfo.address = endpoint.address;
    ccinfo.port = endpoint.port;
    ccinfo.path = endpoint.path;
    ccinfo.host = endpoint.address;
    ccinfo.origin = endpoint.address;
    ccinfo.protocol = "yurei-protocol";
    ccinfo.pwsi = (struct lws **)&client->wsi;
    ccinfo.ssl_connection = endpoint.secure ? LCCSCF_USE_SSL : 0;

    if (!lws_client_connect_via_info(&ccinfo)) {
        YUREI_LOG_WARN("Unable to connect to %s", client->config->wss_endpoint);
        client->wsi = NULL;
        return -1;
    }

    return 0;
}

static void *ws_thread(void *arg) {
    YureiWebsocketClient *client = (YureiWebsocketClient *)arg;
    struct lws_protocols protocols[] = {
        {
            .name = "yurei-protocol",
            .callback = ws_callback,
            .per_session_data_size = sizeof(WsSessionData),
            .rx_buffer_size = 0,
        },
        { NULL, NULL, 0, 0 }
    };

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    client->context = lws_create_context(&info);
    if (!client->context) {
        YUREI_LOG_ERROR("Failed to initialize libwebsockets context");
        client->running = false;
        return NULL;
    }

    while (client->running) {
        if (!client->wsi && !client->connected) {
            if (establish_connection(client) != 0) {
                usleep(client->backoff_ms * 1000);
                if (client->backoff_ms < client->config->ws_backoff_max_ms) {
                    client->backoff_ms = (client->backoff_ms * 2);
                    if (client->backoff_ms > client->config->ws_backoff_max_ms) {
                        client->backoff_ms = client->config->ws_backoff_max_ms;
                    }
                }
            }
        }
        lws_service((struct lws_context *)client->context, 100);
    }

    if (client->context) {
        lws_context_destroy((struct lws_context *)client->context);
        client->context = NULL;
    }
    return NULL;
}

int yurei_ws_client_start(YureiWebsocketClient *client,
                          const YureiConfig *config,
                          YureiEventQueue *queue) {
    if (!client || !config || !queue) {
        return -1;
    }
    if (g_client) {
        return -1;
    }

    memset(client, 0, sizeof(*client));
    client->config = config;
    client->queue = queue;
    client->running = true;
    client->backoff_ms = config->ws_backoff_ms;
    g_client = client;

    if (pthread_create(&client->thread, NULL, ws_thread, client) != 0) {
        g_client = NULL;
        return -1;
    }
    return 0;
}

void yurei_ws_client_stop(YureiWebsocketClient *client) {
    if (!client) {
        return;
    }
    client->running = false;
    if (client->context) {
        lws_cancel_service((struct lws_context *)client->context);
    }
    pthread_join(client->thread, NULL);
    g_client = NULL;
}
