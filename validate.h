#ifndef VALIDATE_H
#define VALIDATE_H

#include <string>
#include <vector>

#include "error.h"
#include "student.h"

// Checks a student id: 1~20 ASCII digits, no leading zero (so "0" is rejected); no trimming.
ErrorCode validate_student_id(const std::string& id);

// Checks a student name: 1~50 bytes, not empty or all spaces, no control characters; duplicates allowed.
ErrorCode validate_student_name(const std::string& name);

// Checks every record (id, name, score range) and that no id appears twice; returns the first failure.
ErrorCode validate_student_records(const std::vector<Student>& records);

#endif
