#ifndef CLI_H
#define CLI_H

#include <iosfwd>
#include <memory>
#include <string>

#include "error.h"
#include "store.h"
#include "student.h"

// Returns the Chinese message shown to the user for an error code.
const char* error_message(ErrorCode code);

// Interactive menu over a StudentStore that can switch between the linked-list and BST implementations.
// It reads from in and writes to out, so tests can drive it with string streams.
class Cli {
public:
    // Creates the menu bound to data_path; starts with the linked-list implementation, uninitialized.
    Cli(std::istream& in, std::ostream& out, std::string data_path);

    // Runs the menu loop until the user exits or the input ends.
    void run();

private:
    enum class Kind { List, Bst };

    // Returns the display name of an implementation kind.
    static const char* kind_name(Kind kind);

    // Creates an empty, uninitialized store of the given kind.
    static std::unique_ptr<StudentStore> make_store(Kind kind);

    // Reads one line into line, dropping a trailing '\r'; returns false at end of input.
    bool read_line(std::string& line);

    // Prints text, then reads one line into line; returns false at end of input.
    bool ask(const std::string& text, std::string& line);

    // Asks a yes/no question; returns true only for the answer y or Y.
    bool confirm(const std::string& text);

    // Returns true if the store is initialized; otherwise prints the NOT_INITIALIZED message.
    bool require_initialized();

    // Prints "<action>失败：<message>" for a failed operation.
    void report_failure(const char* action, ErrorCode code);

    // Prints one record as "学号 | 成绩 | 姓名".
    void print_record(const Student& s);

    // Prints the status line and the menu options.
    void print_menu();

    // Menu actions; each one prints its own result.
    void do_init();
    void do_load();
    void do_save();
    void do_add();
    void do_find();
    void do_remove();
    void do_list();
    void do_count();
    void do_switch();

    // Handles the exit option; returns true if the program should end.
    bool do_exit();

    std::istream& in_;
    std::ostream& out_;
    std::string path_;
    Kind kind_ = Kind::List;
    std::unique_ptr<StudentStore> store_;
    bool dirty_ = false;        // memory differs from the file since the last init, load or save
    bool input_ended_ = false;  // set once the input stream reaches its end
};

#endif
