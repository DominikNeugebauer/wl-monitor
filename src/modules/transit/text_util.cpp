#include "text_util.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

namespace text {

void copy(char *dst, size_t n, const char *src) {
    if (!dst || n == 0) return;
    if (!src) { dst[0] = 0; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = 0;
}

void normalizeLine(const char *in, char *out, size_t n) {
    size_t o = 0;
    for (; in && *in && o + 1 < n; ++in) {
        if (*in == ' ') continue;
        out[o++] = (char)toupper((unsigned char)*in);
    }
    if (n) out[o] = 0;
}

bool lineEquals(const char *a, const char *b) {
    char na[16], nb[16];
    normalizeLine(a, na, sizeof(na));
    normalizeLine(b, nb, sizeof(nb));
    return strcmp(na, nb) == 0;
}

// UTF-8 helpers
static size_t putUtf8(char *out, size_t o, size_t n, uint32_t cp) {
    if (cp < 0x80) {
        if (o + 1 < n) out[o++] = (char)cp;
    } else if (cp < 0x800) {
        if (o + 2 < n) { out[o++] = (char)(0xC0 | (cp >> 6)); out[o++] = (char)(0x80 | (cp & 0x3F)); }
    } else if (cp < 0x10000) {
        if (o + 3 < n) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    return o;
}

// Length of a valid UTF-8 sequence at p, 0 if invalid
static int utf8SeqLen(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    int len = (p[0] & 0xE0) == 0xC0 ? 2 : (p[0] & 0xF0) == 0xE0 ? 3 : (p[0] & 0xF8) == 0xF0 ? 4 : 0;
    for (int i = 1; i < len; i++) if ((p[i] & 0xC0) != 0x80) return 0;
    return len;
}

static uint32_t decodeUtf8(const unsigned char *&p) {
    int len = utf8SeqLen(p);
    uint32_t cp;
    if (len == 0) { cp = *p++; return cp; } // treat as Latin-1
    if (len == 1) return *p++;
    cp = p[0] & (0xFF >> (len + 1));
    for (int i = 1; i < len; i++) cp = (cp << 6) | (p[i] & 0x3F);
    p += len;
    return cp;
}

void oebbToUtf8(const char *in, char *out, size_t n) {
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)in;
    while (p && *p && o + 1 < n) {
        if (*p == '<') { // strip HTML tags
            const char *e = strchr((const char *)p, '>');
            if (e) { p = (const unsigned char *)e + 1; continue; }
        }
        if (*p == '&') {
            const char *s = (const char *)p;
            const char *semi = strchr(s, ';');
            if (semi && semi - s < 10) {
                uint32_t cp = 0;
                if (s[1] == '#') cp = (s[2] == 'x' || s[2] == 'X') ? strtoul(s + 3, nullptr, 16) : strtoul(s + 2, nullptr, 10);
                else if (!strncmp(s, "&amp;", 5))  cp = '&';
                else if (!strncmp(s, "&quot;", 6)) cp = '"';
                else if (!strncmp(s, "&lt;", 4))   cp = '<';
                else if (!strncmp(s, "&gt;", 4))   cp = '>';
                else if (!strncmp(s, "&apos;", 6)) cp = '\'';
                if (cp) { o = putUtf8(out, o, n, cp); p = (const unsigned char *)semi + 1; continue; }
            }
        }
        if (*p >= 0x80) {
            int len = utf8SeqLen(p);
            if (len > 1) { // already UTF-8
                if (o + len < n) { memcpy(out + o, p, len); o += len; }
                p += len;
            } else {       // ISO-8859-1
                o = putUtf8(out, o, n, *p++);
            }
            continue;
        }
        out[o++] = (char)*p++;
    }
    if (n) out[o] = 0;
}

static void stripPrefix(char *s, const char *pre) {
    size_t l = strlen(pre);
    if (strncmp(s, pre, l) == 0) memmove(s, s + l, strlen(s + l) + 1);
}

static bool stripSuffix(char *s, const char *suf) {
    size_t ls = strlen(s), l = strlen(suf);
    if (ls > l && strcmp(s + ls - l, suf) == 0) { s[ls - l] = 0; return true; }
    return false;
}

void cleanStationName(char *s) {
    stripPrefix(s, "Wien ");
    // Remove parts in brackets, e.g. "(Josef-Holaubek-Platz)"
    char *br = strstr(s, " (");
    if (br) *br = 0;
    const char *suffixes[] = {" Bahnhst", " Bahnhof", " Hbf", " S U", " S+U", " U", " S"};
    bool again = true;
    while (again) {
        again = false;
        for (const char *suf : suffixes) if (stripSuffix(s, suf)) again = true;
    }
    // Keep ÖBB names like "Bhf. Heiligenstadt" as they are
}

static void fold(const char *in, char *out, size_t n) {
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)in;
    while (p && *p && o + 3 < n) {
        uint32_t cp = decodeUtf8(p);
        const char *rep = nullptr;
        switch (cp) {
            case 0xE4: case 0xC4: rep = "ae"; break;
            case 0xF6: case 0xD6: rep = "oe"; break;
            case 0xFC: case 0xDC: rep = "ue"; break;
            case 0xDF: rep = "ss"; break;
            default: break;
        }
        if (rep) { out[o++] = rep[0]; out[o++] = rep[1]; }
        else if (cp < 0x80) out[o++] = (char)tolower((int)cp);
    }
    out[o] = 0;
}

bool containsFolded(const char *hay, const char *needle) {
    if (!needle || !*needle) return true;
    if (!hay) return false;
    char h[96], nd[48];
    fold(hay, h, sizeof(h));
    fold(needle, nd, sizeof(nd));
    return strstr(h, nd) != nullptr;
}

void keyword(const char *in, char *out, size_t n) {
    if (!n) return;
    out[0] = 0;
    if (!in) return;
    const char *best = nullptr;
    size_t bestLen = 0;
    const char *p = in;
    while (*p) {
        while (*p && strchr(" /.,-()", *p)) p++;
        const char *start = p;
        while (*p && !strchr(" /.,-()", *p)) p++;
        size_t len = (size_t)(p - start);
        if (len > bestLen) { best = start; bestLen = len; }
    }
    if (!best) return;
    if (bestLen >= n) bestLen = n - 1;
    memcpy(out, best, bestLen);
    out[bestLen] = 0;
}

void utf8ToCp437(const char *in, char *out, size_t n) {
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)in;
    while (p && *p && o + 1 < n) {
        uint32_t cp = decodeUtf8(p);
        char c;
        switch (cp) {
            case 0xE4: c = (char)0x84; break; // ä
            case 0xF6: c = (char)0x94; break; // ö
            case 0xFC: c = (char)0x81; break; // ü
            case 0xC4: c = (char)0x8E; break; // Ä
            case 0xD6: c = (char)0x99; break; // Ö
            case 0xDC: c = (char)0x9A; break; // Ü
            case 0xDF: c = (char)0xE1; break; // ß
            case 0xE9: c = (char)0x82; break; // é
            case 0xE8: c = (char)0x8A; break; // è
            case 0xE1: c = (char)0xA0; break; // á
            case 0x2013: case 0x2014: c = '-'; break;
            default: c = cp < 0x80 ? (char)cp : '?'; break;
        }
        out[o++] = c;
    }
    if (n) out[o] = 0;
}

static bool replaceOnce(char *s, const char *from, const char *to) {
    char *pos = strstr(s, from);
    if (!pos) return false;
    size_t lf = strlen(from), lt = strlen(to);
    memmove(pos + lt, pos + lf, strlen(pos + lf) + 1);
    memcpy(pos, to, lt);
    return true;
}

void fitCp437(char *s, size_t maxChars) {
    if (strlen(s) <= maxChars) return;
    // Abbreviations (CP437: ß is 0xE1)
    replaceOnce(s, "stra\xE1" "e", "str.");
    if (strlen(s) <= maxChars) return;
    replaceOnce(s, "Stra\xE1" "e", "Str.");
    if (strlen(s) <= maxChars) return;
    replaceOnce(s, "gasse", "g.");
    if (strlen(s) <= maxChars) return;
    replaceOnce(s, "platz", "pl.");
    if (strlen(s) <= maxChars) return;
    replaceOnce(s, "Bahnhof", "Bhf.");
    if (strlen(s) <= maxChars) return;
    if (maxChars >= 2) {
        s[maxChars - 1] = '.';
        s[maxChars] = 0;
    } else if (maxChars < strlen(s)) {
        s[maxChars] = 0;
    }
}

} // namespace text
