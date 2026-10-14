#ifndef STORE_H
#define STORE_H

#include <cstddef>
#include <string>
#include <vector>

#include "error.h"
#include "student.h"

// Common interface of the two storage implementations (linked list and BST).
// Until init, load or import_records succeeds, every other operation returns NOT_INITIALIZED.
class StudentStore {
public:
    virtual ~StudentStore() = default;

    // Writes an empty table to path and clears memory; the store is initialized on success.
    virtual ErrorCode init(const std::string& path) = 0;

    // Loads path all-or-nothing and replaces memory; error_line receives the bad line on format errors.
    virtual ErrorCode load(const std::string& path, std::size_t& error_line) = 0;

    // Replaces memory with records (validated all-or-nothing) and binds path; the file itself is not touched.
    virtual ErrorCode import_records(const std::vector<Student>& records, const std::string& path) = 0;

    // Writes all records to the file given to init, load or import_records.
    virtual ErrorCode save() = 0;

    // Adds a record; the score is rounded to one decimal, range-checked, then stored. ID_DUPLICATE if the id exists.
    virtual ErrorCode add(const std::string& id, const std::string& name, double score) = 0;

    // Looks up a record by id and copies it into out.
    virtual ErrorCode find(const std::string& id, Student& out) const = 0;

    // Removes the record with this id.
    virtual ErrorCode remove(const std::string& id) = 0;

    // Replaces out with every record in the implementation's traversal order.
    virtual ErrorCode traverse(std::vector<Student>& out) const = 0;

    // Stores the number of records in out.
    virtual ErrorCode count(std::size_t& out) const = 0;
};

#endif
