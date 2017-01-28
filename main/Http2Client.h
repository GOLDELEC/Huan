/*
 * Alexa Voice Service (AVS) Client for ESP32
 *
 * Copyright (C) 2016, www.goldelec.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MAIN_HTTP2CLIENT_H_
#define MAIN_HTTP2CLIENT_H_

#include <string>
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#include "nghttp2/nghttp2.h"

using namespace std;

struct Connection {
	mbedtls_ssl_context *ssl;
	nghttp2_session *session;
	/* WANT_READ if SSL/TLS connection needs more input; or WANT_WRITE
     if it needs more output; or IO_NONE. This is necessary because
     SSL/TLS re-negotiation is possible at any time. nghttp2 API
     offers similar functions like nghttp2_session_want_read() and
     nghttp2_session_want_write() but they do not take into account
     SSL/TSL connection. */
	int want_io;
};

/**
 *Http2 client based on mbedtls and nghttp2
 */
class Http2Client
{
private:

public:
    Http2Client();
    ~Http2Client();

    bool begin(std::string host, std::string path, int port);
    void end();

    std::string getString(void);

    bool connected();

    void setTimeOut(int milliSeconds);

    void sendGetRequest();
    void sendPostRequest();

private:
    string host;
    string path;
    int port;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;

    Connection connection;

    bool connectToServer(void);

    bool initNghttp2();
    void setupNghttp2Callbacks(nghttp2_session_callbacks *callbacks);
    void submitRequest(struct Connection *connection, struct Request *req);
    void handleIo(struct Connection *connection);
};


#endif /* MAIN_HTTP2CLIENT_H_ */
