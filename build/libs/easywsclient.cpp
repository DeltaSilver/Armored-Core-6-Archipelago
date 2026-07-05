// easywsclient is an imported library, this version has been edited to add wss support.

#ifdef _WIN32
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <wincrypt.h>
#pragma comment( lib, "ws2_32" )
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <io.h>
#ifndef _SSIZE_T_DEFINED
typedef int ssize_t;
#define _SSIZE_T_DEFINED
#endif
#ifndef _SOCKET_T_DEFINED
typedef SOCKET socket_t;
#define _SOCKET_T_DEFINED
#endif
#ifndef snprintf
#define snprintf _snprintf_s
#endif
#if _MSC_VER >=1600
#include <stdint.h>
#else
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif
#define socketerrno WSAGetLastError()
#define SOCKET_EAGAIN_EINPROGRESS WSAEINPROGRESS
#define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#ifndef _SOCKET_T_DEFINED
typedef int socket_t;
#define _SOCKET_T_DEFINED
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR   (-1)
#endif
#define closesocket(s) ::close(s)
#include <errno.h>
#define socketerrno errno
#define SOCKET_EAGAIN_EINPROGRESS EAGAIN
#define SOCKET_EWOULDBLOCK EWOULDBLOCK
#endif

#include <vector>
#include <string>

// OpenSSL for wss:// support
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef _WIN32
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "Crypt32.lib")
#endif

#include "easywsclient.hpp"
#define AC6AP_LOG_TAG "WS"
#include "../dllmain.h"

using easywsclient::Callback_Imp;
using easywsclient::BytesCallback_Imp;

namespace { // private module-only namespace

    socket_t hostname_connect(const std::string& hostname, int port) {
        struct addrinfo hints;
        struct addrinfo* result;
        struct addrinfo* p;
        int ret;
        socket_t sockfd = INVALID_SOCKET;
        char sport[16];
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        snprintf(sport, 16, "%d", port);
        if ((ret = getaddrinfo(hostname.c_str(), sport, &hints, &result)) != 0) {
            Log("getaddrinfo: %s", gai_strerror(ret));
            return INVALID_SOCKET;
        }
        for (p = result; p != NULL; p = p->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd == INVALID_SOCKET) { continue; }
            if (connect(sockfd, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) { break; }
            closesocket(sockfd);
            sockfd = INVALID_SOCKET;
        }
        freeaddrinfo(result);
        return sockfd;
    }


    class _DummyWebSocket : public easywsclient::WebSocket
    {
    public:
        void poll(int timeout) {}
        void send(const std::string& message) {}
        void sendBinary(const std::string& message) {}
        void sendBinary(const std::vector<uint8_t>& message) {}
        void sendPing() {}
        void close() {}
        readyStateValues getReadyState() const { return CLOSED; }
        void _dispatch(Callback_Imp& callable) {}
        void _dispatchBinary(BytesCallback_Imp& callable) {}
    };


    class _RealWebSocket : public easywsclient::WebSocket
    {
    public:
        struct wsheader_type {
            unsigned header_size;
            bool fin;
            bool mask;
            enum opcode_type {
                CONTINUATION = 0x0,
                TEXT_FRAME = 0x1,
                BINARY_FRAME = 0x2,
                CLOSE = 8,
                PING = 9,
                PONG = 0xa,
            } opcode;
            int N0;
            uint64_t N;
            uint8_t masking_key[4];
        };

        std::vector<uint8_t> rxbuf;
        std::vector<uint8_t> txbuf;
        std::vector<uint8_t> receivedData;

        socket_t sockfd;
        readyStateValues readyState;
        bool useMask;
        bool isRxBad;

        // ── TLS state (null for plain ws:// connections) ──────────────────
        SSL_CTX* ssl_ctx;
        SSL* ssl;

        // Plain ws:// constructor (original)
        _RealWebSocket(socket_t sockfd, bool useMask)
            : sockfd(sockfd), readyState(OPEN), useMask(useMask), isRxBad(false)
            , ssl_ctx(nullptr), ssl(nullptr) {
        }

        // wss:// constructor — takes ownership of the SSL objects
        _RealWebSocket(socket_t sockfd, bool useMask, SSL* ssl, SSL_CTX* ssl_ctx)
            : sockfd(sockfd), readyState(OPEN), useMask(useMask), isRxBad(false)
            , ssl_ctx(ssl_ctx), ssl(ssl) {
        }

        ~_RealWebSocket() {
            if (ssl) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            if (ssl_ctx) {
                SSL_CTX_free(ssl_ctx);
            }
        }

        // ── SSL-aware send / recv helpers ─────────────────────────────────

        ssize_t ssl_recv(char* buf, int len) {
            if (ssl) {
                int r = SSL_read(ssl, buf, len);
                if (r <= 0) {
                    int err = SSL_get_error(ssl, r);
                    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                        // Translate to EWOULDBLOCK so the poll loop treats it as
                        // "no data yet" rather than a fatal error.
#ifdef _WIN32
                        WSASetLastError(WSAEWOULDBLOCK);
#else
                        errno = EWOULDBLOCK;
#endif
                        return -1;
                    }
                    return 0; // treat as closed
                }
                return r;
            }
            return recv(sockfd, buf, len, 0);
        }

        int ssl_send(const char* buf, int len) {
            if (ssl) return SSL_write(ssl, buf, len);
            return ::send(sockfd, buf, len, 0);
        }

        readyStateValues getReadyState() const { return readyState; }

        void poll(int timeout) {
            if (readyState == CLOSED) {
                if (timeout > 0) {
                    timeval tv = { timeout / 1000, (timeout % 1000) * 1000 };
                    select(0, NULL, NULL, NULL, &tv);
                }
                return;
            }
            if (timeout != 0) {
                fd_set rfds;
                fd_set wfds;
                timeval tv = { timeout / 1000, (timeout % 1000) * 1000 };
                FD_ZERO(&rfds);
                FD_ZERO(&wfds);
                FD_SET(sockfd, &rfds);
                if (txbuf.size()) { FD_SET(sockfd, &wfds); }
                select(sockfd + 1, &rfds, &wfds, 0, timeout > 0 ? &tv : 0);
            }
            while (true) {
                int N = rxbuf.size();
                ssize_t ret;
                rxbuf.resize(N + 1500);
                ret = ssl_recv((char*)&rxbuf[0] + N, 1500);  // SSL-aware
                if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK ||
                    socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                    rxbuf.resize(N);
                    break;
                }
                else if (ret <= 0) {
                    rxbuf.resize(N);
                    closesocket(sockfd);
                    readyState = CLOSED;
                    Log(ret < 0 ? "Connection error!" : "Connection closed!");
                    break;
                }
                else {
                    rxbuf.resize(N + ret);
                }
            }
            while (txbuf.size()) {
                int ret = ssl_send((char*)&txbuf[0], (int)txbuf.size());  // SSL-aware
                if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK ||
                    socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                    break;
                }
                else if (ret <= 0) {
                    closesocket(sockfd);
                    readyState = CLOSED;
                    Log(ret < 0 ? "Connection error!" : "Connection closed!");
                    break;
                }
                else {
                    txbuf.erase(txbuf.begin(), txbuf.begin() + ret);
                }
            }
            if (!txbuf.size() && readyState == CLOSING) {
                closesocket(sockfd);
                readyState = CLOSED;
            }
        }

        virtual void _dispatch(Callback_Imp& callable) {
            struct CallbackAdapter : public BytesCallback_Imp {
                Callback_Imp& callable;
                CallbackAdapter(Callback_Imp& callable) : callable(callable) {}
                void operator()(const std::vector<uint8_t>& message) {
                    std::string stringMessage(message.begin(), message.end());
                    callable(stringMessage);
                }
            };
            CallbackAdapter bytesCallback(callable);
            _dispatchBinary(bytesCallback);
        }

        virtual void _dispatchBinary(BytesCallback_Imp& callable) {
            if (isRxBad) { return; }
            while (true) {
                wsheader_type ws;
                if (rxbuf.size() < 2) { return; }
                const uint8_t* data = (uint8_t*)&rxbuf[0];
                ws.fin = (data[0] & 0x80) == 0x80;
                ws.opcode = (wsheader_type::opcode_type)(data[0] & 0x0f);
                ws.mask = (data[1] & 0x80) == 0x80;
                ws.N0 = (data[1] & 0x7f);
                ws.header_size = 2 + (ws.N0 == 126 ? 2 : 0) + (ws.N0 == 127 ? 8 : 0) + (ws.mask ? 4 : 0);
                if (rxbuf.size() < ws.header_size) { return; }
                int i = 0;
                if (ws.N0 < 126) {
                    ws.N = ws.N0;
                    i = 2;
                }
                else if (ws.N0 == 126) {
                    ws.N = 0;
                    ws.N |= ((uint64_t)data[2]) << 8;
                    ws.N |= ((uint64_t)data[3]) << 0;
                    i = 4;
                }
                else if (ws.N0 == 127) {
                    ws.N = 0;
                    ws.N |= ((uint64_t)data[2]) << 56;
                    ws.N |= ((uint64_t)data[3]) << 48;
                    ws.N |= ((uint64_t)data[4]) << 40;
                    ws.N |= ((uint64_t)data[5]) << 32;
                    ws.N |= ((uint64_t)data[6]) << 24;
                    ws.N |= ((uint64_t)data[7]) << 16;
                    ws.N |= ((uint64_t)data[8]) << 8;
                    ws.N |= ((uint64_t)data[9]) << 0;
                    i = 10;
                    if (ws.N & 0x8000000000000000ull) {
                        isRxBad = true;
                        Log("ERROR: Frame has invalid frame length. Closing.");
                        close();
                        return;
                    }
                }
                if (ws.mask) {
                    ws.masking_key[0] = ((uint8_t)data[i + 0]) << 0;
                    ws.masking_key[1] = ((uint8_t)data[i + 1]) << 0;
                    ws.masking_key[2] = ((uint8_t)data[i + 2]) << 0;
                    ws.masking_key[3] = ((uint8_t)data[i + 3]) << 0;
                }
                else {
                    ws.masking_key[0] = 0;
                    ws.masking_key[1] = 0;
                    ws.masking_key[2] = 0;
                    ws.masking_key[3] = 0;
                }
                if (rxbuf.size() < ws.header_size + ws.N) { return; }
                if (false) {}
                else if (
                    ws.opcode == wsheader_type::TEXT_FRAME
                    || ws.opcode == wsheader_type::BINARY_FRAME
                    || ws.opcode == wsheader_type::CONTINUATION
                    ) {
                    if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3]; } }
                    receivedData.insert(receivedData.end(), rxbuf.begin() + ws.header_size, rxbuf.begin() + ws.header_size + (size_t)ws.N);
                    if (ws.fin) {
                        callable((const std::vector<uint8_t>) receivedData);
                        receivedData.erase(receivedData.begin(), receivedData.end());
                        std::vector<uint8_t>().swap(receivedData);
                    }
                }
                else if (ws.opcode == wsheader_type::PING) {
                    if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i + ws.header_size] ^= ws.masking_key[i & 0x3]; } }
                    std::string data(rxbuf.begin() + ws.header_size, rxbuf.begin() + ws.header_size + (size_t)ws.N);
                    sendData(wsheader_type::PONG, data.size(), data.begin(), data.end());
                }
                else if (ws.opcode == wsheader_type::PONG) {}
                else if (ws.opcode == wsheader_type::CLOSE) { close(); }
                else { Log("ERROR: Got unexpected WebSocket message."); close(); }
                rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size + (size_t)ws.N);
            }
        }

        void sendPing() {
            std::string empty;
            sendData(wsheader_type::PING, empty.size(), empty.begin(), empty.end());
        }

        void send(const std::string& message) {
            sendData(wsheader_type::TEXT_FRAME, message.size(), message.begin(), message.end());
        }

        void sendBinary(const std::string& message) {
            sendData(wsheader_type::BINARY_FRAME, message.size(), message.begin(), message.end());
        }

        void sendBinary(const std::vector<uint8_t>& message) {
            sendData(wsheader_type::BINARY_FRAME, message.size(), message.begin(), message.end());
        }

        template<class Iterator>
        void sendData(wsheader_type::opcode_type type, uint64_t message_size,
            Iterator message_begin, Iterator message_end) {
            const uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };
            if (readyState == CLOSING || readyState == CLOSED) { return; }
            std::vector<uint8_t> header;
            header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);
            header[0] = 0x80 | type;
            if (message_size < 126) {
                header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
                if (useMask) { header[2] = masking_key[0]; header[3] = masking_key[1]; header[4] = masking_key[2]; header[5] = masking_key[3]; }
            }
            else if (message_size < 65536) {
                header[1] = 126 | (useMask ? 0x80 : 0);
                header[2] = (message_size >> 8) & 0xff;
                header[3] = (message_size >> 0) & 0xff;
                if (useMask) { header[4] = masking_key[0]; header[5] = masking_key[1]; header[6] = masking_key[2]; header[7] = masking_key[3]; }
            }
            else {
                header[1] = 127 | (useMask ? 0x80 : 0);
                header[2] = (message_size >> 56) & 0xff;
                header[3] = (message_size >> 48) & 0xff;
                header[4] = (message_size >> 40) & 0xff;
                header[5] = (message_size >> 32) & 0xff;
                header[6] = (message_size >> 24) & 0xff;
                header[7] = (message_size >> 16) & 0xff;
                header[8] = (message_size >> 8) & 0xff;
                header[9] = (message_size >> 0) & 0xff;
                if (useMask) { header[10] = masking_key[0]; header[11] = masking_key[1]; header[12] = masking_key[2]; header[13] = masking_key[3]; }
            }
            txbuf.insert(txbuf.end(), header.begin(), header.end());
            txbuf.insert(txbuf.end(), message_begin, message_end);
            if (useMask) {
                size_t message_offset = txbuf.size() - message_size;
                for (size_t i = 0; i != message_size; ++i) {
                    txbuf[message_offset + i] ^= masking_key[i & 0x3];
                }
            }
        }

        void close() {
            if (readyState == CLOSING || readyState == CLOSED) { return; }
            readyState = CLOSING;
            uint8_t closeFrame[6] = { 0x88, 0x80, 0x00, 0x00, 0x00, 0x00 };
            std::vector<uint8_t> header(closeFrame, closeFrame + 6);
            txbuf.insert(txbuf.end(), header.begin(), header.end());
        }
    };


    // ===========================================================================
    //  TLS / wss:// support
    // ===========================================================================

    // Returns true if this host should use wss:// without attempting ws:// first.
    static bool host_requires_tls(const char* host) {
        return strstr(host, "archipelago.gg") != nullptr;
    }

    // Load trusted root CAs from the Windows certificate store into an SSL_CTX.
    static void load_windows_root_certs(SSL_CTX* ctx) {
        HCERTSTORE hStore = CertOpenSystemStoreA(0, "ROOT");
        if (!hStore) return;
        X509_STORE* store = SSL_CTX_get_cert_store(ctx);
        PCCERT_CONTEXT pCtx = nullptr;
        int added = 0;
        while ((pCtx = CertEnumCertificatesInStore(hStore, pCtx)) != nullptr) {
            const unsigned char* cert_data = pCtx->pbCertEncoded;
            X509* x509 = d2i_X509(nullptr, &cert_data, pCtx->cbCertEncoded);
            if (x509) {
                if (X509_STORE_add_cert(store, x509) == 1) added++;
                X509_free(x509);
            }
        }
        CertCloseStore(hStore, 0);
        Log("wss: loaded %d root CAs from Windows store", added);
    }

    static easywsclient::WebSocket::pointer
        try_ssl_handshake(socket_t sockfd, const char* host, int port, const char* path,
            bool useMask, const std::string& origin, bool verify,
            bool* out_cert_error) {
        if (out_cert_error) *out_cert_error = false;

        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) return nullptr;

        if (verify) {
            SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
            load_windows_root_certs(ctx);
        }
        else {
            SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
        }

        SSL* ssl = SSL_new(ctx);
        if (!ssl) { SSL_CTX_free(ctx); return nullptr; }
        SSL_set_fd(ssl, (int)sockfd);
        SSL_set_tlsext_host_name(ssl, host);  // SNI

        if (SSL_connect(ssl) != 1) {
            long vr = SSL_get_verify_result(ssl);
            if (out_cert_error && vr != X509_V_OK) *out_cert_error = true;
            ERR_print_errors_cb(
                [](const char* str, size_t len, void*) -> int {
                    Log("OpenSSL: %.*s", (int)len, str);
                    return 1; // continue iterating
                }, nullptr);
            Log("wss: SSL handshake failed for %s (verify=%d, verify_result=%ld)",
                host, (int)verify, vr);
            SSL_free(ssl); SSL_CTX_free(ctx);
            return nullptr;
        }

        // WebSocket HTTP upgrade (over SSL)
        auto ssl_send_str = [&](const char* msg) {
            SSL_write(ssl, msg, (int)strlen(msg));
            };
        char line[1024];
        snprintf(line, sizeof(line), "GET /%s HTTP/1.1\r\n", path); ssl_send_str(line);
        if (port == 443)
            snprintf(line, sizeof(line), "Host: %s\r\n", host);
        else
            snprintf(line, sizeof(line), "Host: %s:%d\r\n", host, port);
        ssl_send_str(line);
        ssl_send_str("Upgrade: websocket\r\n");
        ssl_send_str("Connection: Upgrade\r\n");
        if (!origin.empty()) {
            snprintf(line, sizeof(line), "Origin: %s\r\n", origin.c_str()); ssl_send_str(line);
        }
        ssl_send_str("Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n");
        ssl_send_str("Sec-WebSocket-Version: 13\r\n");
        ssl_send_str("\r\n");

        // Read status line
        int i = 0, status = 0;
        for (; i < 2 || (i < 1023 && line[i - 2] != '\r' && line[i - 1] != '\n'); ++i) {
            if (SSL_read(ssl, line + i, 1) <= 0) {
                SSL_free(ssl); SSL_CTX_free(ctx); return nullptr;
            }
        }
        line[i] = 0;
        if (sscanf(line, "HTTP/1.1 %d", &status) != 1 || status != 101) {
            Log("wss: upgrade failed (status %d) for %s", status, host);
            SSL_free(ssl); SSL_CTX_free(ctx); return nullptr;
        }
        // Drain headers
        while (true) {
            for (i = 0; i < 2 || (i < 1023 && line[i - 2] != '\r' && line[i - 1] != '\n'); ++i) {
                if (SSL_read(ssl, line + i, 1) <= 0) {
                    SSL_free(ssl); SSL_CTX_free(ctx); return nullptr;
                }
            }
            if (line[0] == '\r' && line[1] == '\n') break;
        }

        // Set non-blocking now that the handshake is done
        int flag = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
#ifdef _WIN32
        u_long on = 1; ioctlsocket(sockfd, FIONBIO, &on);
#else
        fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif

        Log("easywsclient: wss:// connected to %s:%d%s", host, port,
            verify ? "" : " (certificate verification disabled - fallback)");
        // Transfer ssl/ctx ownership into the WebSocket object
        return easywsclient::WebSocket::pointer(new _RealWebSocket(sockfd, useMask, ssl, ctx));
    }

    static easywsclient::WebSocket::pointer
        connect_wss(const char* host, int port, const char* path,
            bool useMask, const std::string& origin) {
        // Attempt 1: verified.
        socket_t sockfd = hostname_connect(host, port);
        if (sockfd == INVALID_SOCKET) {
            Log("wss: TCP connect failed to %s:%d", host, port);
            return nullptr;
        }
        bool cert_error = false;
        auto ws = try_ssl_handshake(sockfd, host, port, path, useMask, origin,
            /*verify=*/true, &cert_error);
        if (ws) return ws;
        closesocket(sockfd);  // verified attempt failed; socket is spent

        if (!cert_error) {
            Log("wss: connection failed and was not a certificate error - not retrying");
            return nullptr;
        }

        Log("wss: certificate verification failed - retrying without "
            "verification (connection will still be encrypted)");

        // Attempt 2: unverified, fresh socket.
        sockfd = hostname_connect(host, port);
        if (sockfd == INVALID_SOCKET) {
            Log("wss: TCP reconnect failed to %s:%d", host, port);
            return nullptr;
        }
        ws = try_ssl_handshake(sockfd, host, port, path, useMask, origin,
            /*verify=*/false, nullptr);
        if (!ws) closesocket(sockfd);
        return ws;
    }


    // ===========================================================================
    //  from_url — ws:// with automatic wss:// fallback
    // ===========================================================================

    easywsclient::WebSocket::pointer from_url(const std::string& url,
        bool useMask,
        const std::string& origin) {
        char host[512];
        int  port = 80;
        char path[512];
        path[0] = '\0';

        if (url.size() >= 512) {
            Log("ERROR: url size limit exceeded: %s", url.c_str());
            return nullptr;
        }
        if (origin.size() >= 200) {
            Log("ERROR: origin size limit exceeded: %s", origin.c_str());
            return nullptr;
        }

        // Parse scheme — accept both ws:// and wss://
        bool explicit_wss = false;
        if (sscanf(url.c_str(), "wss://%[^:/]:%d/%s", host, &port, path) == 3) { explicit_wss = true; }
        else if (sscanf(url.c_str(), "wss://%[^:/]/%s", host, path) == 2) { explicit_wss = true; port = 443; }
        else if (sscanf(url.c_str(), "wss://%[^:/]:%d", host, &port) == 2) { explicit_wss = true; }
        else if (sscanf(url.c_str(), "wss://%[^:/]", host) == 1) { explicit_wss = true; port = 443; }
        else if (sscanf(url.c_str(), "ws://%[^:/]:%d/%s", host, &port, path) == 3) {}
        else if (sscanf(url.c_str(), "ws://%[^:/]/%s", host, path) == 2) { port = 80; }
        else if (sscanf(url.c_str(), "ws://%[^:/]:%d", host, &port) == 2) {}
        else if (sscanf(url.c_str(), "ws://%[^:/]", host) == 1) { port = 80; }
        else {
            Log("ERROR: Could not parse WebSocket url: %s", url.c_str());
            return nullptr;
        }

        // Decide whether to use TLS directly or try ws:// first
        bool use_tls_first = explicit_wss || host_requires_tls(host);

        if (use_tls_first) {
            // archipelago.gg or explicit wss:// go straight to TLS, no fallback
            int tls_port = (port == 80) ? 443 : port;
            Log("easywsclient: connecting wss:// to %s:%d", host, tls_port);
            return connect_wss(host, tls_port, path, useMask, origin);
        }

        // ── Try plain ws:// first ────────────────────────────────────────────
        Log("easywsclient: trying ws:// to %s:%d", host, port);
        socket_t sockfd = hostname_connect(host, port);
        if (sockfd != INVALID_SOCKET) {
            char line[1024];
            int status = 0, i;
            snprintf(line, 1024, "GET /%s HTTP/1.1\r\n", path); ::send(sockfd, line, strlen(line), 0);
            if (port == 80)
                snprintf(line, 1024, "Host: %s\r\n", host);
            else
                snprintf(line, 1024, "Host: %s:%d\r\n", host, port);
            ::send(sockfd, line, strlen(line), 0);
            snprintf(line, 1024, "Upgrade: websocket\r\n");        ::send(sockfd, line, strlen(line), 0);
            snprintf(line, 1024, "Connection: Upgrade\r\n");       ::send(sockfd, line, strlen(line), 0);
            if (!origin.empty()) {
                snprintf(line, 1024, "Origin: %s\r\n", origin.c_str()); ::send(sockfd, line, strlen(line), 0);
            }
            snprintf(line, 1024, "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"); ::send(sockfd, line, strlen(line), 0);
            snprintf(line, 1024, "Sec-WebSocket-Version: 13\r\n");     ::send(sockfd, line, strlen(line), 0);
            snprintf(line, 1024, "\r\n");                               ::send(sockfd, line, strlen(line), 0);

            for (i = 0; i < 2 || (i < 1023 && line[i - 2] != '\r' && line[i - 1] != '\n'); ++i) {
                if (recv(sockfd, line + i, 1, 0) == 0) { closesocket(sockfd); sockfd = INVALID_SOCKET; break; }
            }
            if (sockfd != INVALID_SOCKET) {
                line[i] = 0;
                if (i < 1023 && sscanf(line, "HTTP/1.1 %d", &status) == 1 && status == 101) {
                    // ws:// upgrade accepted
                    while (true) {
                        for (i = 0; i < 2 || (i < 1023 && line[i - 2] != '\r' && line[i - 1] != '\n'); ++i) {
                            if (recv(sockfd, line + i, 1, 0) == 0) { closesocket(sockfd); sockfd = INVALID_SOCKET; break; }
                        }
                        if (sockfd == INVALID_SOCKET) break;
                        if (line[0] == '\r' && line[1] == '\n') break;
                    }
                    if (sockfd != INVALID_SOCKET) {
                        int flag = 1;
                        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
#ifdef _WIN32
                        u_long on = 1; ioctlsocket(sockfd, FIONBIO, &on);
#else
                        fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif
                        Log("easywsclient: ws:// connected to %s:%d", host, port);
                        return easywsclient::WebSocket::pointer(new _RealWebSocket(sockfd, useMask));
                    }
                }
                else {
                    Log("easywsclient: ws:// rejected (status %d), trying wss://", status);
                    closesocket(sockfd);
                }
            }
        }
        else {
            Log("easywsclient: ws:// connect failed, trying wss://");
        }

        // ── ws:// failed or rejected — fall back to wss:// ───────────────────
        int tls_port = (port == 80) ? 443 : port;
        Log("easywsclient: falling back to wss:// on %s:%d", host, tls_port);
        return connect_wss(host, tls_port, path, useMask, origin);
    }

}

namespace easywsclient {

    WebSocket::pointer WebSocket::create_dummy() {
        static pointer dummy = pointer(new _DummyWebSocket);
        return dummy;
    }

    WebSocket::pointer WebSocket::from_url(const std::string& url, const std::string& origin) {
        return ::from_url(url, true, origin);
    }

    WebSocket::pointer WebSocket::from_url_no_mask(const std::string& url, const std::string& origin) {
        return ::from_url(url, false, origin);
    }

}