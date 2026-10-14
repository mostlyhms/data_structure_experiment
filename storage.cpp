#include "storage.h"

#include <filesystem>
#include <fstream>
#include <set>

#include "score.h"
#include "validate.h"

namespace {

const char* const kHeader = "STUDENTS_V1";

// Removes one trailing '\r' from a line so CRLF files load correctly.
// line: the line to fix in place.
void strip_cr(std::string& line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
}

// Splits a line on TAB, keeping empty fields.
// line: one data line without its line break; returns the fields in order.
std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (char c : line) {
        if (c == '\t') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    fields.push_back(current);
    return fields;
}

}  // namespace

// Writes the header and one line per record, then renames the temp file over the target.
// path: target file; records: records written in the given order, assumed already valid.
// Returns OK or FILE_IO_ERROR; on failure the original file is left unchanged.
ErrorCode save_records(const std::string& path, const std::vector<Student>& records) {
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out) return ErrorCode::FILE_IO_ERROR;

        out << kHeader << '\n';
        for (const Student& s : records) {
            out << s.id << '\t' << format_score(s.score) << '\t' << s.name << '\n';
        }
        out.close();
        if (!out) {
            std::error_code ec;
            std::filesystem::remove(tmp_path, ec);
            return ErrorCode::FILE_IO_ERROR;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::filesystem::remove(tmp_path, ec);
        return ErrorCode::FILE_IO_ERROR;
    }
    return ErrorCode::OK;
}

// Reads the whole file, validating every line before replacing out.
// path: file to read; out: receives records only on success; error_line: 1-based line of the first bad line.
// Returns OK, FILE_NOT_FOUND, FILE_FORMAT_ERROR or FILE_IO_ERROR.
ErrorCode load_records(const std::string& path, std::vector<Student>& out, std::size_t& error_line) {
    error_line = 0;

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        std::error_code ec;
        return std::filesystem::exists(path, ec) ? ErrorCode::FILE_IO_ERROR
                                                 : ErrorCode::FILE_NOT_FOUND;
    }

    std::vector<Student> loaded;
    std::set<std::string> seen_ids;
    std::string line;
    std::size_t line_no = 0;

    // Records the failing line number and reports a format error.
    auto fail = [&]() {
        error_line = line_no;
        return ErrorCode::FILE_FORMAT_ERROR;
    };

    while (std::getline(in, line)) {
        ++line_no;
        strip_cr(line);

        if (line_no == 1) {
            if (line != kHeader) return fail();
            continue;
        }
        if (line.empty()) return fail();

        const std::vector<std::string> fields = split_tab(line);
        if (fields.size() != 3) return fail();

        Student s;
        s.id = fields[0];
        s.name = fields[2];
        if (validate_student_id(s.id) != ErrorCode::OK) return fail();
        if (parse_file_score(fields[1], s.score) != ErrorCode::OK) return fail();
        if (validate_student_name(s.name) != ErrorCode::OK) return fail();
        if (!seen_ids.insert(s.id).second) return fail();

        loaded.push_back(s);
    }

    if (in.bad()) return ErrorCode::FILE_IO_ERROR;
    if (line_no == 0) {
        // An empty file has no header.
        error_line = 1;
        return ErrorCode::FILE_FORMAT_ERROR;
    }

    out = std::move(loaded);
    return ErrorCode::OK;
}
