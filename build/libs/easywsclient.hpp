#pragma once
// Interface for easywsclient (dhbaird/easywsclient, minimal WebSocket client).
// easywsclient.cpp implements this against the pattern below; the wss:// (TLS)
// fallback added there doesn't change this interface — from_url() already
// takes a plain ws:// or wss:// URL string and picks the right path itself.

#include <cstdint>
#include <string>
#include <vector>

namespace easywsclient {

    // Callback base classes. These are structs (not std::function) so callers can
    // derive from them — e.g. _RealWebSocket::_dispatch builds a CallbackAdapter
    // that inherits BytesCallback_Imp. The dispatch<>() templates below wrap any
    // plain callable/lambda into one of these.
    struct Callback_Imp { virtual void operator()(const std::string& message) = 0;               virtual ~Callback_Imp() {} };
    struct BytesCallback_Imp { virtual void operator()(const std::vector<uint8_t>& message) = 0;      virtual ~BytesCallback_Imp() {} };

    class WebSocket {
    public:
        typedef WebSocket* pointer;
        enum readyStateValues { CLOSING, CLOSED, CONNECTING, OPEN };

        virtual ~WebSocket() {}
        virtual void poll(int timeout = 0) = 0;   // milliseconds; 0 = don't block
        virtual void send(const std::string& message) = 0;
        virtual void sendBinary(const std::string& message) = 0;
        virtual void sendBinary(const std::vector<uint8_t>& message) = 0;
        virtual void sendPing() = 0;
        virtual void close() = 0;
        virtual readyStateValues getReadyState() const = 0;

        // Wrap a plain callable/lambda into the matching *Callback_Imp and dispatch.
        template<class Callable>
        void dispatch(Callable callable) {           // received text messages
            struct _Cb : public Callback_Imp {
                Callable& c; _Cb(Callable& c) : c(c) {}
                void operator()(const std::string& m) { c(m); }
            } cb(callable);
            _dispatch(cb);
        }
        template<class Callable>
        void dispatchBinary(Callable callable) {     // received binary messages
            struct _Cb : public BytesCallback_Imp {
                Callable& c; _Cb(Callable& c) : c(c) {}
                void operator()(const std::vector<uint8_t>& m) { c(m); }
            } cb(callable);
            _dispatchBinary(cb);
        }

        static pointer create_dummy();
        static pointer from_url(const std::string& url, const std::string& origin = std::string());
        static pointer from_url_no_mask(const std::string& url, const std::string& origin = std::string());

    protected:
        virtual void _dispatch(Callback_Imp& callable) = 0;
        virtual void _dispatchBinary(BytesCallback_Imp& callable) = 0;
    };

} // namespace easywsclient