#include "validate.h"

namespace {

// Returns true if c is an ASCII digit.
bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// Returns true if c is a control character (0x00~0x1F or 0x7F).
bool is_control(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return u < 0x20 || u == 0x7F;
}

}  // namespace

// Validates a student id in the order: empty, too long, non-digit, leading zero.
// id: raw id string, used as-is. Returns OK or the first failing ErrorCode.
ErrorCode validate_student_id(const std::string& id) {
    if (id.empty()) return ErrorCode::ID_EMPTY;
    if (id.size() > 20) return ErrorCode::ID_TOO_LONG;
    for (char c : id) {
        if (!is_digit(c)) return ErrorCode::ID_INVALID_CHAR;
    }
    if (id[0] == '0') return ErrorCode::ID_LEADING_ZERO;
    return ErrorCode::OK;
}

// Validates a student name in the order: empty, control character, all spaces, too long.
// name: raw name bytes, stored unchanged. Returns OK or the first failing ErrorCode.
ErrorCode validate_student_name(const std::string& name) {
    if (name.empty()) return ErrorCode::NAME_EMPTY;
    for (char c : name) {
        if (is_control(c)) return ErrorCode::NAME_INVALID_CHAR;
    }
    if (name.find_first_not_of(' ') == std::string::npos) return ErrorCode::NAME_EMPTY;
    if (name.size() > 50) return ErrorCode::NAME_TOO_LONG;
    return ErrorCode::OK;
}
