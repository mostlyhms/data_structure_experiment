#ifndef VALIDATE_H
#define VALIDATE_H

#include <string>

#include "error.h"

// Checks a student id: 1~20 ASCII digits, no leading zero (so "0" is rejected); no trimming.
ErrorCode validate_student_id(const std::string& id);

// Checks a student name: 1~50 bytes, not empty or all spaces, no control characters; duplicates allowed.
ErrorCode validate_student_name(const std::string& name);

#endif
