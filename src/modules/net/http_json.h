#ifndef HTTP_JSON_H
#define HTTP_JSON_H

#include <Arduino.h>
#include <ArduinoJson.h>

namespace http {

// Loads a URL over HTTPS and parses the JSON straight from the stream.
// skipToBrace: skip a JS prefix before the first brace (ÖBB sends "journeysObj = {...}")
// filter: optional ArduinoJson filter, nullptr for none
bool getJson(const String &url, JsonDocument &doc, JsonDocument *filter, bool skipToBrace, String &error);

// URL encoding for query parameters
String urlEncode(const String &s);

} // namespace http

#endif
