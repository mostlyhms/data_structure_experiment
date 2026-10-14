#ifndef STORAGE_H
#define STORAGE_H

#include <cstddef>
#include <string>
#include <vector>

#include "error.h"
#include "student.h"

// Writes records in the given order to path via a temp file and rename; the old file survives on failure.
ErrorCode save_records(const std::string& path, const std::vector<Student>& records);

// Loads records from path all-or-nothing; on failure out is untouched and error_line holds the bad line (1-based).
ErrorCode load_records(const std::string& path, std::vector<Student>& out, std::size_t& error_line);

#endif
