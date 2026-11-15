#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H

#include <stddef.h>
#include <stdint.h>

namespace text {

// strncpy that always null-terminates
void copy(char *dst, size_t n, const char *src);

// Normalizes line names, e.g. "S 45" to "S45" and "35a" to "35A"
void normalizeLine(const char *in, char *out, size_t n);
bool lineEquals(const char *a, const char *b);

// Converts ÖBB text (ISO-8859-1, HTML entities, <b> tags) to UTF-8
void oebbToUtf8(const char *in, char *out, size_t n);

// Shortens station names, e.g. "Wien Hütteldorf Bahnhof" to "Hütteldorf"
void cleanStationName(char *s);

// Case-insensitive substring match. Umlauts are folded (ü = ue, ß = ss).
bool containsFolded(const char *hay, const char *needle);

// Longest word of a name, e.g. "Bhf. Heiligenstadt" gives "Heiligenstadt".
// Used as search term because WL and ÖBB spell names slightly differently.
void keyword(const char *in, char *out, size_t n);

// Converts UTF-8 to code page 437 for the default Adafruit GFX font
void utf8ToCp437(const char *in, char *out, size_t n);

// Shortens a CP437 string to maxChars, e.g. "straße" to "str." and "gasse" to "g."
void fitCp437(char *s, size_t maxChars);

} // namespace text

#endif
