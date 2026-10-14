#include "score.h"

#include <cmath>
#include <cstddef>

namespace {

// Returns true if s is non-empty and every character is an ASCII digit.
bool all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// Returns true if s is 1~3 digits with no leading zero (a lone "0" is allowed).
bool is_valid_int_part(const std::string& s) {
    if (s.size() < 1 || s.size() > 3 || !all_digits(s)) return false;
    return s.size() == 1 || s[0] != '0';
}

// Returns the numeric value of an integer part that has already passed is_valid_int_part.
int int_value(const std::string& s) {
    int v = 0;
    for (char c : s) v = v * 10 + (c - '0');
    return v;
}

// Builds a stored score from its integer tenths value; the only place a score double is produced.
double from_tenths(long long tenths) {
    return static_cast<double>(tenths) / 10.0;
}

}  // namespace

// Parses a user-typed score with half-up rounding on the second decimal digit, then range-checks it.
// text: raw input; out: receives the score on success. Returns OK, SCORE_INVALID_FORMAT or SCORE_OUT_OF_RANGE.
ErrorCode parse_input_score(const std::string& text, double& out) {
    // A second '.' ends up inside frac_part and fails the digit check below.
    const std::size_t dot = text.find('.');
    const std::string int_part = dot == std::string::npos ? text : text.substr(0, dot);
    const std::string frac_part = dot == std::string::npos ? "" : text.substr(dot + 1);

    if (!is_valid_int_part(int_part)) return ErrorCode::SCORE_INVALID_FORMAT;
    if (dot != std::string::npos &&
        (frac_part.empty() || frac_part.size() > 6 || !all_digits(frac_part))) {
        return ErrorCode::SCORE_INVALID_FORMAT;
    }

    // Only the second decimal digit decides the rounding; later digits cannot change the result.
    const long long first = frac_part.size() >= 1 ? frac_part[0] - '0' : 0;
    const long long second = frac_part.size() >= 2 ? frac_part[1] - '0' : 0;
    const long long tenths = int_value(int_part) * 10LL + first + (second >= 5 ? 1 : 0);

    if (tenths > 1000) return ErrorCode::SCORE_OUT_OF_RANGE;
    out = from_tenths(tenths);
    return ErrorCode::OK;
}

// Parses a score from the data file, accepting only the canonical "I.d" form.
// text: one score field from the file; out: receives the score on success.
// Returns OK, SCORE_INVALID_FORMAT or SCORE_OUT_OF_RANGE.
ErrorCode parse_file_score(const std::string& text, double& out) {
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos) return ErrorCode::SCORE_INVALID_FORMAT;

    const std::string int_part = text.substr(0, dot);
    const std::string frac_part = text.substr(dot + 1);
    if (!is_valid_int_part(int_part) || frac_part.size() != 1 || !all_digits(frac_part)) {
        return ErrorCode::SCORE_INVALID_FORMAT;
    }

    const long long tenths = int_value(int_part) * 10LL + (frac_part[0] - '0');
    if (tenths > 1000) return ErrorCode::SCORE_OUT_OF_RANGE;
    out = from_tenths(tenths);
    return ErrorCode::OK;
}

// Converts a score to its integer tenths value using lround.
// score: a score value; returns round(score * 10) as a long long.
long long to_tenths(double score) {
    return static_cast<long long>(std::lround(score * 10.0));
}

// Checks that a score is within the allowed range, comparing in tenths.
// score: value to check; returns true if 0 <= tenths <= 1000.
bool is_score_in_range(double score) {
    const long long t = to_tenths(score);
    return t >= 0 && t <= 1000;
}

// Compares two scores by their tenths values.
// a, b: scores to compare; returns -1 if a < b, 1 if a > b, 0 if equal.
int compare_score(double a, double b) {
    const long long ta = to_tenths(a);
    const long long tb = to_tenths(b);
    if (ta < tb) return -1;
    if (ta > tb) return 1;
    return 0;
}

// Formats a score with exactly one decimal place.
// score: a valid score in [0.0, 100.0]; returns the text such as "85.5" or "90.0".
std::string format_score(double score) {
    const long long t = to_tenths(score);
    return std::to_string(t / 10) + "." + std::to_string(t % 10);
}
