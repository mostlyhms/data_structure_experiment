#include "cli.h"

#include <cstddef>
#include <istream>
#include <ostream>
#include <utility>
#include <vector>

#include "bst_store.h"
#include "list_store.h"
#include "score.h"
#include "validate.h"

namespace {

// Removes leading and trailing whitespace (space, tab, CR, LF, VT, FF).
// text: raw input; returns the trimmed copy.
std::string trim(const std::string& text) {
    const char* const whitespace = " \t\r\n\v\f";
    const std::size_t begin = text.find_first_not_of(whitespace);
    if (begin == std::string::npos) return "";
    const std::size_t end = text.find_last_not_of(whitespace);
    return text.substr(begin, end - begin + 1);
}

}  // namespace

// Maps every error code to the text shown in the menu.
// code: the error code; returns a Chinese message.
const char* error_message(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "操作成功";
        case ErrorCode::NOT_INITIALIZED: return "尚未初始化，请先选择“1. 初始化”或“2. 从文件加载”";
        case ErrorCode::ID_EMPTY: return "学号不能为空";
        case ErrorCode::ID_TOO_LONG: return "学号不能超过 20 位";
        case ErrorCode::ID_INVALID_CHAR: return "学号只能由数字组成";
        case ErrorCode::ID_LEADING_ZERO: return "学号不能以 0 开头";
        case ErrorCode::ID_DUPLICATE: return "学号已存在";
        case ErrorCode::ID_NOT_FOUND: return "未找到该学号";
        case ErrorCode::NAME_EMPTY: return "姓名不能为空，也不能全是空格";
        case ErrorCode::NAME_TOO_LONG: return "姓名不能超过 50 字节（约 16 个汉字）";
        case ErrorCode::NAME_INVALID_CHAR: return "姓名不能包含制表符等控制字符";
        case ErrorCode::SCORE_INVALID_FORMAT:
            return "成绩格式错误，应为 0~100 的数字（如 85、85.5），不带正负号、不以 0 开头、最多 6 位小数";
        case ErrorCode::SCORE_OUT_OF_RANGE: return "成绩超出范围，四舍五入到一位小数后必须在 0~100 之间";
        case ErrorCode::FILE_NOT_FOUND: return "数据文件不存在";
        case ErrorCode::FILE_FORMAT_ERROR: return "数据文件格式错误";
        case ErrorCode::FILE_IO_ERROR: return "文件读写失败";
    }
    return "未知错误";
}

// Binds the menu to its streams and data file and starts with an uninitialized linked list.
// in, out: user input and output; data_path: file used by init, load and save.
Cli::Cli(std::istream& in, std::ostream& out, std::string data_path)
    : in_(in), out_(out), path_(std::move(data_path)), store_(make_store(Kind::List)) {}

// Shows the menu and dispatches choices until the user exits or the input ends.
void Cli::run() {
    out_ << "学生信息管理系统（数据文件：" << path_ << "）\n";
    while (true) {
        print_menu();
        std::string choice;
        if (!ask("请选择 [0-9]：", choice)) break;
        choice = trim(choice);

        if (choice == "1") {
            do_init();
        } else if (choice == "2") {
            do_load();
        } else if (choice == "3") {
            do_save();
        } else if (choice == "4") {
            do_add();
        } else if (choice == "5") {
            do_find();
        } else if (choice == "6") {
            do_remove();
        } else if (choice == "7") {
            do_list();
        } else if (choice == "8") {
            do_count();
        } else if (choice == "9") {
            do_switch();
        } else if (choice == "0") {
            if (do_exit()) break;
        } else {
            out_ << "无效选项，请输入 0~9 之间的数字。\n";
        }
        if (input_ended_) break;
    }

    if (input_ended_) {
        out_ << "\n输入已结束，程序退出。";
        if (dirty_) out_ << "未保存的修改已丢弃。";
        out_ << "\n";
    } else {
        out_ << "再见。\n";
    }
}

// Returns the display name of an implementation kind.
// kind: implementation kind; returns "单链表" or "BST".
const char* Cli::kind_name(Kind kind) {
    return kind == Kind::List ? "单链表" : "BST";
}

// Creates an empty, uninitialized store of the given kind.
// kind: implementation kind; returns the new store.
std::unique_ptr<StudentStore> Cli::make_store(Kind kind) {
    if (kind == Kind::List) return std::make_unique<ListStore>();
    return std::make_unique<BstStore>();
}

// Reads one line from the input, dropping a trailing '\r' left by CRLF input.
// line: receives the line. Returns false (and marks the input as ended) at end of input.
bool Cli::read_line(std::string& line) {
    if (!std::getline(in_, line)) {
        input_ended_ = true;
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

// Prints a prompt and reads the answer.
// text: prompt; line: receives the answer. Returns false at end of input.
bool Cli::ask(const std::string& text, std::string& line) {
    out_ << text << std::flush;
    return read_line(line);
}

// Asks a yes/no question; anything other than y or Y (including end of input) means no.
// text: the question. Returns true for yes.
bool Cli::confirm(const std::string& text) {
    std::string answer;
    if (!ask(text, answer)) return false;
    answer = trim(answer);
    return answer == "y" || answer == "Y";
}

// Checks that the store is ready before an operation asks for input.
// Returns true if initialized; otherwise prints the NOT_INITIALIZED message and returns false.
bool Cli::require_initialized() {
    std::size_t n = 0;
    if (store_->count(n) == ErrorCode::OK) return true;
    out_ << error_message(ErrorCode::NOT_INITIALIZED) << "。\n";
    return false;
}

// Prints a failure line for an operation.
// action: operation name such as "添加"; code: the failure reason.
void Cli::report_failure(const char* action, ErrorCode code) {
    out_ << action << "失败：" << error_message(code) << "。\n";
}

// Prints one record in the fixed output format.
// s: record to print.
void Cli::print_record(const Student& s) {
    out_ << s.id << " | " << format_score(s.score) << " | " << s.name << '\n';
}

// Prints the current implementation, data file and state, followed by the options.
void Cli::print_menu() {
    std::size_t n = 0;
    const bool ready = store_->count(n) == ErrorCode::OK;

    out_ << "\n========== 学生信息管理 ==========\n"
         << "实现：" << kind_name(kind_) << "  |  文件：" << path_ << "  |  状态：";
    if (ready) {
        out_ << "已就绪，" << n << " 条记录";
        if (dirty_) out_ << "（有未保存的修改）";
    } else {
        out_ << "未初始化";
    }
    out_ << "\n"
         << "  1. 初始化（清空内存和数据文件）\n"
         << "  2. 从文件加载\n"
         << "  3. 保存到文件\n"
         << "  4. 添加学生\n"
         << "  5. 按学号查询\n"
         << "  6. 按学号删除\n"
         << "  7. 显示全部学生\n"
         << "  8. 统计学生总数\n"
         << "  9. 切换实现（当前：" << kind_name(kind_) << "）\n"
         << "  0. 退出\n";
}

// Clears memory and rewrites the data file as an empty table after the user confirms.
void Cli::do_init() {
    if (!confirm("初始化将清空内存和文件 " + path_ + " 中的全部记录，确认吗？(y/N)：")) {
        if (!input_ended_) out_ << "已取消初始化。\n";
        return;
    }
    const ErrorCode code = store_->init(path_);
    if (code != ErrorCode::OK) {
        report_failure("初始化", code);
        return;
    }
    dirty_ = false;
    out_ << "初始化完成，当前为空表。\n";
}

// Loads the data file, asking first if unsaved changes would be discarded.
void Cli::do_load() {
    if (dirty_ && !confirm("有未保存的修改，加载将丢弃它们，确认吗？(y/N)：")) {
        if (!input_ended_) out_ << "已取消加载。\n";
        return;
    }
    std::size_t line = 0;
    const ErrorCode code = store_->load(path_, line);
    if (code != ErrorCode::OK) {
        out_ << "加载失败：" << error_message(code);
        if (code == ErrorCode::FILE_FORMAT_ERROR) out_ << "（第 " << line << " 行）";
        out_ << "。内存中的数据未改变。\n";
        return;
    }
    std::size_t n = 0;
    store_->count(n);
    dirty_ = false;
    out_ << "加载成功，共 " << n << " 条记录。\n";
}

// Writes all records to the data file.
void Cli::do_save() {
    const ErrorCode code = store_->save();
    if (code != ErrorCode::OK) {
        report_failure("保存", code);
        return;
    }
    std::size_t n = 0;
    store_->count(n);
    dirty_ = false;
    out_ << "已保存 " << n << " 条记录到 " << path_ << "。\n";
}

// Asks for id, name and score one by one, checking each field as soon as it is entered.
// The id and score are trimmed here; the name is stored exactly as typed.
void Cli::do_add() {
    if (!require_initialized()) return;

    std::string id;
    if (!ask("学号：", id)) return;
    id = trim(id);
    ErrorCode code = validate_student_id(id);
    Student existing;
    if (code == ErrorCode::OK && store_->find(id, existing) == ErrorCode::OK) code = ErrorCode::ID_DUPLICATE;
    if (code != ErrorCode::OK) {
        report_failure("添加", code);
        return;
    }

    std::string name;
    if (!ask("姓名：", name)) return;
    code = validate_student_name(name);
    if (code != ErrorCode::OK) {
        report_failure("添加", code);
        return;
    }

    std::string score_text;
    if (!ask("成绩（0~100，自动四舍五入到一位小数）：", score_text)) return;
    double score = 0.0;
    code = parse_input_score(trim(score_text), score);
    if (code != ErrorCode::OK) {
        report_failure("添加", code);
        return;
    }

    code = store_->add(id, name, score);
    if (code != ErrorCode::OK) {
        report_failure("添加", code);
        return;
    }
    dirty_ = true;
    Student added;
    store_->find(id, added);
    out_ << "添加成功：";
    print_record(added);
}

// Asks for an id and prints the matching record.
void Cli::do_find() {
    if (!require_initialized()) return;
    std::string id;
    if (!ask("要查询的学号：", id)) return;
    Student s;
    const ErrorCode code = store_->find(trim(id), s);
    if (code != ErrorCode::OK) {
        report_failure("查询", code);
        return;
    }
    out_ << "学号 | 成绩 | 姓名\n";
    print_record(s);
}

// Asks for an id, removes that record and prints what was removed.
void Cli::do_remove() {
    if (!require_initialized()) return;
    std::string id;
    if (!ask("要删除的学号：", id)) return;
    id = trim(id);
    Student s;
    ErrorCode code = store_->find(id, s);
    if (code == ErrorCode::OK) code = store_->remove(id);
    if (code != ErrorCode::OK) {
        report_failure("删除", code);
        return;
    }
    dirty_ = true;
    out_ << "已删除：";
    print_record(s);
}

// Prints every record in the current implementation's traversal order.
void Cli::do_list() {
    if (!require_initialized()) return;
    std::vector<Student> records;
    const ErrorCode code = store_->traverse(records);
    if (code != ErrorCode::OK) {
        report_failure("显示", code);
        return;
    }
    if (records.empty()) {
        out_ << "（暂无学生记录）\n";
        return;
    }
    out_ << "共 " << records.size() << " 条记录（" << (kind_ == Kind::List ? "按添加顺序" : "按学号字典序")
         << "）：\n"
         << "学号 | 成绩 | 姓名\n";
    for (const Student& s : records) print_record(s);
}

// Prints the number of records.
void Cli::do_count() {
    if (!require_initialized()) return;
    std::size_t n = 0;
    store_->count(n);
    out_ << "学生总数：" << n << "\n";
}

// Switches between the linked list and the BST; current records move to the new store,
// which stays bound to the same data file. An uninitialized store switches without data.
void Cli::do_switch() {
    const Kind next_kind = kind_ == Kind::List ? Kind::Bst : Kind::List;
    std::unique_ptr<StudentStore> next = make_store(next_kind);

    std::vector<Student> records;
    const bool initialized = store_->traverse(records) == ErrorCode::OK;
    if (initialized) {
        const ErrorCode code = next->import_records(records, path_);
        if (code != ErrorCode::OK) {
            report_failure("切换", code);
            return;
        }
    }
    store_ = std::move(next);
    kind_ = next_kind;

    out_ << "已切换到 " << kind_name(kind_) << " 实现";
    if (initialized) {
        out_ << "，迁移了 " << records.size() << " 条记录。\n";
    } else {
        out_ << "（当前未初始化，无数据迁移）。\n";
    }
}

// Ends the program, asking first if there are unsaved changes.
// Returns true if the program should end.
bool Cli::do_exit() {
    if (!dirty_) return true;
    return confirm("有未保存的修改，退出后将丢失，确认退出吗？(y/N)：");
}
