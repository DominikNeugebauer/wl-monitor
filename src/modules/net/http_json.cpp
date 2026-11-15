#include "http_json.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace http {

// Stream reader that puts back the '{' that was already read
struct PrefixedReader {
    Stream &s;
    bool pending;
    int read() {
        if (pending) { pending = false; return '{'; }
        char c;
        return s.readBytes(&c, 1) == 1 ? (uint8_t)c : -1;
    }
    size_t readBytes(char *buf, size_t n) {
        if (n == 0) return 0;
        if (pending) {
            pending = false;
            buf[0] = '{';
            return 1 + s.readBytes(buf + 1, n - 1);
        }
        return s.readBytes(buf, n);
    }
};

bool getJson(const String &url, JsonDocument &doc, JsonDocument *filter, bool skipToBrace, String &error) {
    if (WiFi.status() != WL_CONNECTED) {
        error = "no WiFi";
        return false;
    }

    // Resolve the host first to get a clearer error than "connection refused"
    int hs = url.indexOf("://");
    String host = url.substring(hs < 0 ? 0 : hs + 3);
    int slash = host.indexOf('/');
    if (slash >= 0) host = host.substring(0, slash);
    IPAddress ip;
    if (!WiFi.hostByName(host.c_str(), ip) || ip == IPAddress(0, 0, 0, 0)) {
        error = "DNS: " + host + " could not be resolved";
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();   // public timetable data, no certificate check

    HTTPClient http;
    http.useHTTP10(true);   // avoid chunked encoding so the stream can be parsed directly
    http.setTimeout(12000);
    http.setConnectTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
        error = "invalid URL";
        return false;
    }
    http.addHeader("Accept", "application/json, text/javascript, */*");
    http.setUserAgent("wl-monitor-esp32/1.0");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        if (code > 0) {
            error = "HTTP " + String(code);
        } else {
            // Include the IP and the mbedTLS error
            char tls[100] = "";
            int tlsErr = client.lastError(tls, sizeof(tls));
            error = http.errorToString(code) + " (" + host + " = " + ip.toString();
            if (tlsErr) error += ", TLS " + String(tlsErr) + ": " + tls;
            error += ")";
        }
        http.end();
        return false;
    }

    // WL responses are nested deeper than the default limit of 10.
    const auto NESTING = DeserializationOption::NestingLimit(32);

    Stream &stream = http.getStream();
    stream.setTimeout(12000);
    DeserializationError err;
    if (skipToBrace) {
        if (!stream.find((char *)"{")) {
            error = "no JSON in response";
            http.end();
            return false;
        }
        PrefixedReader reader{stream, true};
        err = filter ? deserializeJson(doc, reader, DeserializationOption::Filter(*filter), NESTING)
                     : deserializeJson(doc, reader, NESTING);
    } else {
        err = filter ? deserializeJson(doc, stream, DeserializationOption::Filter(*filter), NESTING)
                     : deserializeJson(doc, stream, NESTING);
    }
    http.end();

    if (err) {
        error = String("JSON: ") + err.c_str();
        return false;
    }
    return true;
}

String urlEncode(const String &s) {
    static const char hex[] = "0123456789ABCDEF";
    String out;
    out.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); i++) {
        uint8_t c = (uint8_t)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

} // namespace http
