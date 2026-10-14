// Phase 1 tests: score parsing, id/name validation and file persistence boundary cases.
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "error.h"
#include "score.h"
#include "storage.h"
#include "student.h"
#include "validate.h"

namespace fs = std::filesystem;

namespace {

int g_checks = 0;
int g_failures = 0;

// Records one check and prints its label when it fails.
// ok: whether the check passed; label: text shown on failure.
void expect(bool ok, const std::string& label) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", label.c_str());
    }
}

// Returns a string of n Chinese characters, 3 bytes each in UTF-8.
// n: number of characters.
std::string repeat_zhong(std::size_t n) {
    std::string s;
    for (std::size_t i = 0; i < n; ++i) s += "中";
    return s;
}

// Writes raw bytes to a file, replacing any previous content.
// p: file path; content: exact bytes to write.
void write_raw(const fs::path& p, const std::string& content) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << content;
}

// Reads a whole file as raw bytes.
// p: file path; returns the file content.
std::string read_raw(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Checks parse_input_score and, when it is accepted, the stored tenths value.
// text: input to parse; want: expected code; want_tenths: expected tenths when accepted.
void expect_input(const std::string& text, ErrorCode want, long long want_tenths = -1) {
    double v = -1.0;
    const ErrorCode got = parse_input_score(text, v);
    const bool ok = got == want && (want != ErrorCode::OK || to_tenths(v) == want_tenths);
    expect(ok, "input score \"" + text + "\"");
}

// Checks parse_file_score and, when it is accepted, the stored tenths value.
// text: field to parse; want: expected code; want_tenths: expected tenths when accepted.
void expect_file(const std::string& text, ErrorCode want, long long want_tenths = -1) {
    double v = -1.0;
    const ErrorCode got = parse_file_score(text, v);
    const bool ok = got == want && (want != ErrorCode::OK || to_tenths(v) == want_tenths);
    expect(ok, "file score \"" + text + "\"");
}

// Tests user-input score rounding, range and format rules.
void test_input_score() {
    expect_input("100.04", ErrorCode::OK, 1000);
    expect_input("100.05", ErrorCode::SCORE_OUT_OF_RANGE);
    expect_input("100.5", ErrorCode::SCORE_OUT_OF_RANGE);
    expect_input("85.55", ErrorCode::OK, 856);
    expect_input("85.45", ErrorCode::OK, 855);
    expect_input("85.44", ErrorCode::OK, 854);
    expect_input("99.96", ErrorCode::OK, 1000);
    expect_input("99.9", ErrorCode::OK, 999);
    expect_input("90.123456", ErrorCode::OK, 901);
    expect_input("0", ErrorCode::OK, 0);
    expect_input("0.0", ErrorCode::OK, 0);
    expect_input("0.5", ErrorCode::OK, 5);
    expect_input("100", ErrorCode::OK, 1000);
    expect_input("100.0", ErrorCode::OK, 1000);

    expect_input("", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("-0", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("-1", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("+50", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input(".5", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("85.", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("1e2", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("00.5", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("05", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("1000", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("8 5", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input(" 85", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("85 ", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("85.1234567", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("abc", ErrorCode::SCORE_INVALID_FORMAT);
    expect_input("1.2.3", ErrorCode::SCORE_INVALID_FORMAT);
}

// Tests the strict score format used when loading the data file.
void test_file_score() {
    expect_file("85.5", ErrorCode::OK, 855);
    expect_file("100.0", ErrorCode::OK, 1000);
    expect_file("0.0", ErrorCode::OK, 0);
    expect_file("100.5", ErrorCode::SCORE_OUT_OF_RANGE);

    expect_file("", ErrorCode::SCORE_INVALID_FORMAT);
    expect_file("85", ErrorCode::SCORE_INVALID_FORMAT);
    expect_file("85.55", ErrorCode::SCORE_INVALID_FORMAT);
    expect_file("085.5", ErrorCode::SCORE_INVALID_FORMAT);
    expect_file("1000.0", ErrorCode::SCORE_INVALID_FORMAT);
    expect_file("-1.0", ErrorCode::SCORE_INVALID_FORMAT);
    expect_file("85.", ErrorCode::SCORE_INVALID_FORMAT);
}

// Tests tenths conversion, comparison and fixed one-decimal formatting.
void test_format_and_compare() {
    expect(format_score(90.0) == "90.0", "format 90.0");
    expect(format_score(85.5) == "85.5", "format 85.5");
    expect(format_score(0.0) == "0.0", "format 0.0");
    expect(format_score(100.0) == "100.0", "format 100.0");
    expect(to_tenths(85.5) == 855, "tenths of 85.5");
    expect(compare_score(85.5, 85.5) == 0, "compare equal scores");
    expect(compare_score(85.4, 85.5) == -1, "compare 85.4 < 85.5");
    expect(compare_score(100.0, 99.9) == 1, "compare 100.0 > 99.9");
    expect(is_score_in_range(0.0) && is_score_in_range(100.0), "range boundaries accepted");
    expect(!is_score_in_range(100.1) && !is_score_in_range(-0.1), "range outside rejected");
    expect(!is_score_in_range(std::numeric_limits<double>::quiet_NaN()), "NaN rejected");
    expect(!is_score_in_range(std::numeric_limits<double>::infinity()), "inf rejected");
    expect(!is_score_in_range(-std::numeric_limits<double>::infinity()), "-inf rejected");
    expect(!is_score_in_range(1e300), "huge value rejected");

    expect(format_score(round_score(85.04)) == "85.0", "round 85.04 -> 85.0");
    expect(format_score(round_score(85.06)) == "85.1", "round 85.06 -> 85.1");
    expect(format_score(round_score(85.55)) == "85.6", "round 85.55 -> 85.6");
    expect(format_score(round_score(99.96)) == "100.0", "round 99.96 -> 100.0");
}

// Tests student id validation rules.
void test_id() {
    expect(validate_student_id("") == ErrorCode::ID_EMPTY, "id empty");
    expect(validate_student_id("12") == ErrorCode::OK, "id 12");
    expect(validate_student_id(std::string(20, '1')) == ErrorCode::OK, "id 20 digits");
    expect(validate_student_id(std::string(21, '1')) == ErrorCode::ID_TOO_LONG, "id 21 digits");
    expect(validate_student_id("0") == ErrorCode::ID_LEADING_ZERO, "id 0");
    expect(validate_student_id("0012") == ErrorCode::ID_LEADING_ZERO, "id 0012");
    expect(validate_student_id("12a") == ErrorCode::ID_INVALID_CHAR, "id 12a");
    expect(validate_student_id("A12") == ErrorCode::ID_INVALID_CHAR, "id A12");
    expect(validate_student_id("-12") == ErrorCode::ID_INVALID_CHAR, "id -12");
    expect(validate_student_id(" 12") == ErrorCode::ID_INVALID_CHAR, "id leading space");
    expect(validate_student_id("12 ") == ErrorCode::ID_INVALID_CHAR, "id trailing space");
}

// Tests student name validation rules (length counted in bytes).
void test_name() {
    expect(validate_student_name("") == ErrorCode::NAME_EMPTY, "name empty");
    expect(validate_student_name("   ") == ErrorCode::NAME_EMPTY, "name all spaces");
    expect(validate_student_name("张三") == ErrorCode::OK, "name 张三");
    expect(validate_student_name(" 张 三 ") == ErrorCode::OK, "name keeps surrounding spaces");
    expect(validate_student_name("Tom Li") == ErrorCode::OK, "name with inner space");
    expect(validate_student_name(std::string(50, 'a')) == ErrorCode::OK, "name 50 bytes");
    expect(validate_student_name(std::string(51, 'a')) == ErrorCode::NAME_TOO_LONG, "name 51 bytes");
    expect(validate_student_name(repeat_zhong(16)) == ErrorCode::OK, "name 16 Chinese (48 bytes)");
    expect(validate_student_name(repeat_zhong(17)) == ErrorCode::NAME_TOO_LONG, "name 17 Chinese (51 bytes)");
    expect(validate_student_name("A\tB") == ErrorCode::NAME_INVALID_CHAR, "name with TAB");
    expect(validate_student_name("A\nB") == ErrorCode::NAME_INVALID_CHAR, "name with newline");
    expect(validate_student_name("A\x7F") == ErrorCode::NAME_INVALID_CHAR, "name with DEL");
}

// Tests save and load round trips and the all-or-nothing load rules.
// dir: scratch directory that exists and is empty.
void test_storage(const fs::path& dir) {
    const fs::path file = dir / "students.txt";
    const fs::path tmp = dir / "students.txt.tmp";

    // Round trip with mixed content.
    std::vector<Student> records = {{"12", "张三", 85.5}, {"100", "Tom Li", 100.0}, {"7", " 空格 ", 0.0}};
    expect(save_records(file.string(), records) == ErrorCode::OK, "save three records");
    expect(read_raw(file) == "STUDENTS_V1\n12\t85.5\t张三\n100\t100.0\tTom Li\n7\t0.0\t 空格 \n",
           "saved file content");
    expect(!fs::exists(tmp), "temp file removed after save");

    std::vector<Student> loaded;
    std::size_t line = 99;
    expect(load_records(file.string(), loaded, line) == ErrorCode::OK, "load three records");
    expect(loaded.size() == 3 && line == 0, "loaded count and error_line");
    if (loaded.size() == 3) {
        expect(loaded[0].id == "12" && loaded[0].name == "张三" && to_tenths(loaded[0].score) == 855,
               "loaded record 1");
        expect(loaded[2].name == " 空格 ", "loaded keeps name spaces");
    }

    // Empty table is a valid save and load.
    expect(save_records(file.string(), {}) == ErrorCode::OK, "save empty table");
    expect(read_raw(file) == "STUDENTS_V1\n", "empty table writes header only");
    expect(load_records(file.string(), loaded, line) == ErrorCode::OK && loaded.empty(),
           "load empty table");

    // Missing file.
    fs::remove(file);
    expect(load_records(file.string(), loaded, line) == ErrorCode::FILE_NOT_FOUND, "missing file");

    // CRLF line endings are tolerated.
    write_raw(file, "STUDENTS_V1\r\n12\t85.5\t张三\r\n");
    expect(load_records(file.string(), loaded, line) == ErrorCode::OK && loaded.size() == 1 &&
               loaded[0].name == "张三",
           "CRLF accepted");

    // Corrupt files must fail, report the line, and leave the previous data untouched.
    struct BadCase {
        std::string content;
        std::size_t bad_line;
        std::string label;
    };
    const std::vector<BadCase> bad_cases = {
        {"", 1, "empty file"},
        {"BAD\n", 1, "wrong header"},
        {"STUDENTS_V1\n12\t85.5\n", 2, "two columns"},
        {"STUDENTS_V1\n12\t85.5\t张三\tX\n", 2, "four columns"},
        {"STUDENTS_V1\n012\t85.5\t张三\n", 2, "leading zero id"},
        {"STUDENTS_V1\n12\t85\t张三\n", 2, "score without decimal"},
        {"STUDENTS_V1\n12\t85.55\t张三\n", 2, "score with two decimals"},
        {"STUDENTS_V1\n12\t85.5\t\n", 2, "empty name"},
        {"STUDENTS_V1\n12\t85.5\t张三\n12\t90.0\tLi\n", 3, "duplicate id"},
        {"STUDENTS_V1\n12\t85.5\t张三\n\n", 3, "blank line"},
        {"STUDENTS_V1\n12\t85.5\t张三\n13\t90.0\t\x01\n", 3, "control char in name"},
    };
    for (const BadCase& bc : bad_cases) {
        write_raw(file, bc.content);
        std::vector<Student> keep = {{"1", "x", 1.0}};
        std::size_t err_line = 0;
        const ErrorCode code = load_records(file.string(), keep, err_line);
        expect(code == ErrorCode::FILE_FORMAT_ERROR && err_line == bc.bad_line,
               "corrupt file: " + bc.label);
        expect(keep.size() == 1 && keep[0].id == "1", "data untouched after failed load: " + bc.label);
    }
}

}  // namespace

// Runs all phase 1 tests and prints a summary.
// Returns 0 when every check passes, 1 otherwise.
int main() {
    test_input_score();
    test_file_score();
    test_format_and_compare();
    test_id();
    test_name();

    const fs::path dir = fs::temp_directory_path() / "phase1_tests";
    fs::create_directories(dir);
    test_storage(dir);
    fs::remove_all(dir);

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
