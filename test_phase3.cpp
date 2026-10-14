// Phase 3 tests: end-to-end menu sessions driven by scripted input, checking output and the data file.
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <string>

#include "cli.h"

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
        std::printf("  失败：%s\n", label.c_str());
    }
}

// Runs one group of checks and prints a one-line pass/fail summary for it.
// name: group title shown in the output; body: function that performs the checks.
void run_group(const std::string& name, const std::function<void()>& body) {
    const int checks_before = g_checks;
    const int failures_before = g_failures;
    body();
    const int failures = g_failures - failures_before;
    std::printf("[%s] %s（%d 项检查）\n", failures == 0 ? "通过" : "失败", name.c_str(), g_checks - checks_before);
}

// Reads a whole file as raw bytes.
// p: file path; returns the file content.
std::string read_raw(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Writes raw bytes to a file, replacing previous content.
// p: file path; content: exact bytes to write.
void write_raw(const fs::path& p, const std::string& content) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << content;
}

// Runs one menu session on scripted input.
// input: everything the user types, one answer per line; path: data file. Returns all printed output.
std::string run_cli(const std::string& input, const fs::path& path) {
    std::istringstream in(input);
    std::ostringstream out;
    Cli cli(in, out, path.string());
    cli.run();
    return out.str();
}

// Returns true if text contains part.
bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// Counts non-overlapping occurrences of part in text.
std::size_t occurrences(const std::string& text, const std::string& part) {
    std::size_t n = 0;
    for (std::size_t pos = text.find(part); pos != std::string::npos; pos = text.find(part, pos + part.size())) ++n;
    return n;
}

// Init, add, save, exit; then a second session loads the file and queries, lists and counts.
// dir: scratch directory.
void test_full_flow(const fs::path& dir) {
    const fs::path file = dir / "flow.txt";
    const std::string out1 = run_cli(
        "1\ny\n"
        "4\n12\n张三\n85.55\n"
        "4\n7\nTom Li\n100\n"
        "3\n0\n",
        file);
    expect(contains(out1, "初始化完成，当前为空表。"), "flow: init message");
    expect(contains(out1, "添加成功：12 | 85.6 | 张三"), "flow: add rounds 85.55 to 85.6");
    expect(contains(out1, "添加成功：7 | 100.0 | Tom Li"), "flow: add 100 shows 100.0");
    expect(contains(out1, "已保存 2 条记录"), "flow: save message");
    expect(contains(out1, "再见。"), "flow: normal exit");
    expect(read_raw(file) == "STUDENTS_V1\n12\t85.6\t张三\n7\t100.0\tTom Li\n", "flow: file content");

    const std::string out2 = run_cli("2\n5\n12\n7\n8\n0\n", file);
    expect(contains(out2, "加载成功，共 2 条记录。"), "flow: load message");
    expect(contains(out2, "学号 | 成绩 | 姓名\n12 | 85.6 | 张三\n"), "flow: query result");
    expect(contains(out2, "共 2 条记录（按添加顺序）：\n学号 | 成绩 | 姓名\n12 | 85.6 | 张三\n7 | 100.0 | Tom Li\n"),
           "flow: list in file order");
    expect(contains(out2, "学生总数：2"), "flow: count");
}

// Every data operation before init or load reports NOT_INITIALIZED without asking for input.
// dir: scratch directory.
void test_before_init(const fs::path& dir) {
    const std::string out = run_cli("4\n5\n6\n7\n8\n3\n0\n", dir / "never.txt");
    expect(occurrences(out, error_message(ErrorCode::NOT_INITIALIZED)) == 6, "before init: six rejections");
    expect(contains(out, "保存失败："), "before init: save rejected");
    expect(!contains(out, "学号："), "before init: no field prompt");
    expect(contains(out, "状态：未初始化"), "before init: status line");
    expect(!fs::exists(dir / "never.txt"), "before init: no file created");
}

// Invalid menu choices are reported and the menu keeps running; choices are trimmed.
// dir: scratch directory.
void test_invalid_menu(const fs::path& dir) {
    const std::string out = run_cli("abc\n\n99\n 8 \n0\n", dir / "menu.txt");
    expect(occurrences(out, "无效选项") == 3, "menu: three invalid choices");
    expect(occurrences(out, error_message(ErrorCode::NOT_INITIALIZED)) == 1, "menu: trimmed ' 8 ' runs count");
    expect(contains(out, "再见。"), "menu: exit still works");
}

// Id and score are trimmed by the input layer; the name is kept exactly as typed.
// dir: scratch directory.
void test_input_trimming(const fs::path& dir) {
    const fs::path file = dir / "trim.txt";
    run_cli("1\ny\n4\n 12 \n 张 三 \n 85 \n3\n0\n", file);
    expect(read_raw(file) == "STUDENTS_V1\n12\t85.0\t 张 三 \n", "trim: id and score trimmed, name kept");
}

// Each field is checked as soon as it is entered and a failure returns to the menu.
// dir: scratch directory.
void test_add_errors(const fs::path& dir) {
    const fs::path file = dir / "add_errors.txt";
    const std::string out = run_cli(
        "1\ny\n"
        "4\n12\n张三\n85\n"
        "4\n12\n"             // duplicate id: rejected before the name prompt
        "4\n012\n"            // leading zero
        "4\n1a\n"             // non-digit
        "4\n13\n   \n"        // name of spaces only
        "4\n13\nLi\nabc\n"    // bad score format
        "4\n13\nLi\n+50\n"    // signed score
        "4\n13\nLi\n100.05\n" // rounds to 100.1, out of range
        "8\n3\n0\n",
        file);
    expect(contains(out, std::string("添加失败：") + error_message(ErrorCode::ID_DUPLICATE)), "add: duplicate");
    expect(contains(out, std::string("添加失败：") + error_message(ErrorCode::ID_LEADING_ZERO)), "add: leading zero");
    expect(contains(out, std::string("添加失败：") + error_message(ErrorCode::ID_INVALID_CHAR)), "add: invalid char");
    expect(contains(out, std::string("添加失败：") + error_message(ErrorCode::NAME_EMPTY)), "add: blank name");
    expect(occurrences(out, std::string("添加失败：") + error_message(ErrorCode::SCORE_INVALID_FORMAT)) == 2,
           "add: two bad score formats");
    expect(contains(out, std::string("添加失败：") + error_message(ErrorCode::SCORE_OUT_OF_RANGE)), "add: out of range");
    expect(contains(out, "学生总数：1"), "add: only the first record stored");
    expect(read_raw(file) == "STUDENTS_V1\n12\t85.0\t张三\n", "add: file holds one record");
}

// Delete shows the removed record, a second delete fails, and exit asks about unsaved changes.
// dir: scratch directory.
void test_delete_and_exit_prompt(const fs::path& dir) {
    const fs::path file = dir / "delete.txt";
    const std::string out = run_cli(
        "1\ny\n"
        "4\n12\na\n60\n"
        "4\n13\nb\n70\n"
        "6\n12\n"
        "6\n12\n"
        "8\n"
        "0\nn\n"  // unsaved changes: stay
        "3\n"
        "0\n",    // saved: exits without asking
        file);
    expect(contains(out, "已删除：12 | 60.0 | a"), "delete: removed record shown");
    expect(contains(out, std::string("删除失败：") + error_message(ErrorCode::ID_NOT_FOUND)), "delete: second delete fails");
    expect(contains(out, "学生总数：1"), "delete: count after delete");
    expect(occurrences(out, "退出后将丢失") == 1, "exit: asked once while unsaved");
    expect(contains(out, "再见。"), "exit: exits after save");
    expect(read_raw(file) == "STUDENTS_V1\n13\t70.0\tb\n", "delete: file after save");
}

// Switching moves the data between implementations, changes the listing order, and keeps the file.
// dir: scratch directory.
void test_switch(const fs::path& dir) {
    const fs::path file = dir / "switch.txt";
    const std::string out = run_cli(
        "1\ny\n"
        "4\n9\na\n1\n"
        "4\n10\nb\n1\n"
        "4\n100\nc\n1\n"
        "7\n"
        "9\n7\n"
        "9\n8\n"
        "3\n0\n",
        file);
    expect(contains(out, "（按添加顺序）：\n学号 | 成绩 | 姓名\n9 | 1.0 | a\n10 | 1.0 | b\n100 | 1.0 | c\n"),
           "switch: list order before switching");
    expect(contains(out, "已切换到 BST 实现，迁移了 3 条记录。"), "switch: to BST");
    expect(contains(out, "（按学号字典序）：\n学号 | 成绩 | 姓名\n10 | 1.0 | b\n100 | 1.0 | c\n9 | 1.0 | a\n"),
           "switch: BST order");
    expect(contains(out, "已切换到 单链表 实现，迁移了 3 条记录。"), "switch: back to list");
    expect(contains(out, "学生总数：3"), "switch: count kept");
    expect(read_raw(file) == "STUDENTS_V1\n10\t1.0\tb\n100\t1.0\tc\n9\t1.0\ta\n",
           "switch: list saves in the order it imported from the BST");

    const std::string idle = run_cli("9\n0\n", dir / "switch_idle.txt");
    expect(contains(idle, "已切换到 BST 实现（当前未初始化，无数据迁移）。"), "switch: uninitialized");
    expect(contains(idle, "实现：BST"), "switch: status shows BST");
}

// Load failures leave the state untouched; loading over unsaved changes asks first.
// dir: scratch directory.
void test_load_errors(const fs::path& dir) {
    const std::string missing = run_cli("2\n8\n0\n", dir / "no_such_file.txt");
    expect(contains(missing, std::string("加载失败：") + error_message(ErrorCode::FILE_NOT_FOUND)), "load: missing file");
    expect(contains(missing, error_message(ErrorCode::NOT_INITIALIZED)), "load: still uninitialized");
    expect(!fs::exists(dir / "no_such_file.txt"), "load: missing file not created");

    const fs::path corrupt = dir / "corrupt.txt";
    write_raw(corrupt, "STUDENTS_V1\n12\t85.5\t张三\n12\t90.0\tLi\n");
    const std::string bad = run_cli("2\n0\n", corrupt);
    expect(contains(bad, "加载失败：数据文件格式错误（第 3 行）。内存中的数据未改变。"), "load: corrupt line reported");

    const fs::path file = dir / "reload.txt";
    const std::string out = run_cli(
        "1\ny\n"
        "4\n12\na\n60\n"
        "2\nn\n"   // keep unsaved changes
        "8\n"
        "2\ny\n"   // discard them: file is still an empty table
        "8\n0\n",
        file);
    expect(contains(out, "已取消加载。"), "load: cancelled over unsaved changes");
    expect(contains(out, "学生总数：1"), "load: data kept after cancel");
    expect(contains(out, "加载成功，共 0 条记录。"), "load: confirmed load replaces memory");
    expect(contains(out, "学生总数：0"), "load: count after reload");
}

// Declining init leaves the file untouched.
// dir: scratch directory.
void test_init_cancel(const fs::path& dir) {
    const fs::path file = dir / "keep.txt";
    write_raw(file, "STUDENTS_V1\n12\t85.5\t张三\n");
    const std::string out = run_cli("1\nn\n2\n8\n0\n", file);
    expect(contains(out, "已取消初始化。"), "init cancel: message");
    expect(read_raw(file) == "STUDENTS_V1\n12\t85.5\t张三\n", "init cancel: file unchanged");
    expect(contains(out, "学生总数：1"), "init cancel: data still loadable");
}

// Input that ends in the middle of a prompt or with unsaved changes ends the program cleanly.
// dir: scratch directory.
void test_end_of_input(const fs::path& dir) {
    const std::string mid = run_cli("1\ny\n4\n12\n", dir / "eof1.txt");
    expect(contains(mid, "输入已结束，程序退出。"), "eof: during a field prompt");

    const std::string dirty = run_cli("1\ny\n4\n12\na\n1\n", dir / "eof2.txt");
    expect(contains(dirty, "输入已结束，程序退出。未保存的修改已丢弃。"), "eof: unsaved changes reported");

    run_cli("1\r\ny\r\n4\r\n12\r\na\r\n1\r\n3\r\n0\r\n", dir / "crlf.txt");
    expect(read_raw(dir / "crlf.txt") == "STUDENTS_V1\n12\t1.0\ta\n", "CRLF input accepted");
}

}  // namespace

// Runs all phase 3 tests and prints a summary.
// Returns 0 when every check passes, 1 otherwise.
int main() {
    std::printf("阶段三集成测试：命令行菜单端到端流程\n");
    const fs::path dir = fs::temp_directory_path() / "phase3_tests";
    fs::remove_all(dir);
    fs::create_directories(dir);

    run_group("完整流程：初始化 → 添加 → 保存 → 加载 → 查询 → 统计", [&] { test_full_flow(dir); });
    run_group("未初始化时的操作被拒绝", [&] { test_before_init(dir); });
    run_group("非法菜单输入", [&] { test_invalid_menu(dir); });
    run_group("学号、成绩去除首尾空白，姓名原样保存", [&] { test_input_trimming(dir); });
    run_group("添加时逐项校验", [&] { test_add_errors(dir); });
    run_group("删除与退出确认", [&] { test_delete_and_exit_prompt(dir); });
    run_group("菜单切换实现（数据迁移、遍历顺序）", [&] { test_switch(dir); });
    run_group("加载失败与加载确认", [&] { test_load_errors(dir); });
    run_group("取消初始化", [&] { test_init_cancel(dir); });
    run_group("输入意外结束与 CRLF 输入", [&] { test_end_of_input(dir); });

    fs::remove_all(dir);
    std::printf("合计：%d 项检查，%d 项失败\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
