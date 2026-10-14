// Phase 2 tests: both StudentStore implementations through the common interface.
// Run with "--bench" to also time 100000 random-order inserts and finds.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "bst_store.h"
#include "error.h"
#include "list_store.h"
#include "score.h"
#include "store.h"
#include "student.h"

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

// Creates an empty store of the given kind: 0 = linked list, 1 = BST.
// kind: store kind; returns the new store.
std::unique_ptr<StudentStore> make_store(int kind) {
    if (kind == 0) return std::make_unique<ListStore>();
    return std::make_unique<BstStore>();
}

// Returns the display name of a store kind.
// kind: 0 for list, 1 for BST.
std::string kind_name(int kind) {
    return kind == 0 ? "list" : "bst";
}

// Returns the Chinese label of a store kind for the test report.
// kind: 0 for list, 1 for BST.
std::string kind_label(int kind) {
    return kind == 0 ? "单链表" : "BST";
}

// Returns the number of records in a store, or -1 if count fails.
// st: store to query.
long long count_of(const StudentStore& st) {
    std::size_t n = 0;
    if (st.count(n) != ErrorCode::OK) return -1;
    return static_cast<long long>(n);
}

// Returns every record of a store in traversal order (empty if the store is uninitialized).
// st: store to read.
std::vector<Student> records_of(const StudentStore& st) {
    std::vector<Student> v;
    st.traverse(v);
    return v;
}

// Returns the ids of a store in traversal order.
// st: store to read.
std::vector<std::string> ids_of(const StudentStore& st) {
    std::vector<std::string> ids;
    for (const Student& s : records_of(st)) ids.push_back(s.id);
    return ids;
}

// Returns the ids of a store sorted, so contents can be compared regardless of order.
// st: store to read.
std::vector<std::string> sorted_ids_of(const StudentStore& st) {
    std::vector<std::string> ids = ids_of(st);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// Parses a score literal that the test expects to be valid.
// text: score literal such as "85.5"; returns the parsed score.
double score_of(const std::string& text) {
    double v = -1.0;
    expect(parse_input_score(text, v) == ErrorCode::OK, "test score literal " + text);
    return v;
}

// Checks that two record lists hold the same students (id, name and score), ignoring order.
// a, b: record lists to compare.
bool same_records(std::vector<Student> a, std::vector<Student> b) {
    if (a.size() != b.size()) return false;
    const auto by_id = [](const Student& x, const Student& y) { return x.id < y.id; };
    std::sort(a.begin(), a.end(), by_id);
    std::sort(b.begin(), b.end(), by_id);
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].name != b[i].name ||
            compare_score(a[i].score, b[i].score) != 0) {
            return false;
        }
    }
    return true;
}

// Every operation except init and load must return NOT_INITIALIZED before a successful init or load.
// kind: store kind to test; dir: scratch directory.
void test_before_init(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " before init";
    auto st = make_store(kind);
    const fs::path missing = dir / "missing.txt";
    fs::remove(missing);

    Student s;
    std::vector<Student> v;
    std::size_t n = 0;
    std::size_t line = 0;
    expect(st->add("1", "a", 1.0) == ErrorCode::NOT_INITIALIZED, tag + ": add");
    expect(st->find("1", s) == ErrorCode::NOT_INITIALIZED, tag + ": find");
    expect(st->remove("1") == ErrorCode::NOT_INITIALIZED, tag + ": remove");
    expect(st->traverse(v) == ErrorCode::NOT_INITIALIZED, tag + ": traverse");
    expect(st->count(n) == ErrorCode::NOT_INITIALIZED, tag + ": count");
    expect(st->save() == ErrorCode::NOT_INITIALIZED, tag + ": save");
    expect(st->load(missing.string(), line) == ErrorCode::FILE_NOT_FOUND, tag + ": load missing file");
    expect(st->add("1", "a", 1.0) == ErrorCode::NOT_INITIALIZED, tag + ": still uninitialized after failed load");
}

// Checks an empty table and a single record, including re-adding an id after it was deleted.
// kind: store kind to test; dir: scratch directory.
void test_empty_and_single(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " empty/single";
    auto st = make_store(kind);
    const fs::path file = dir / (kind_name(kind) + "_empty.txt");

    expect(st->init(file.string()) == ErrorCode::OK, tag + ": init");
    expect(read_raw(file) == "STUDENTS_V1\n", tag + ": init writes header only");
    expect(count_of(*st) == 0, tag + ": empty count is 0");

    Student s;
    expect(st->find("12", s) == ErrorCode::ID_NOT_FOUND, tag + ": find on empty");
    expect(st->remove("12") == ErrorCode::ID_NOT_FOUND, tag + ": remove on empty");
    expect(records_of(*st).empty(), tag + ": traverse empty");

    expect(st->add("12", "张三", score_of("85.5")) == ErrorCode::OK, tag + ": add single");
    expect(count_of(*st) == 1, tag + ": count 1");
    expect(st->remove("12") == ErrorCode::OK, tag + ": remove only record");
    expect(count_of(*st) == 0, tag + ": count 0 after removing only record");
    expect(st->find("12", s) == ErrorCode::ID_NOT_FOUND, tag + ": find after delete");
    expect(st->add("12", "李四", score_of("60")) == ErrorCode::OK, tag + ": re-add after delete");
    expect(st->find("12", s) == ErrorCode::OK && s.name == "李四", tag + ": re-added record found");
}

// Invalid inputs are rejected with the right code and leave the store unchanged.
// kind: store kind to test; dir: scratch directory.
void test_invalid_inputs(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " invalid";
    auto st = make_store(kind);
    expect(st->init((dir / (kind_name(kind) + "_invalid.txt")).string()) == ErrorCode::OK, tag + ": init");

    expect(st->add("0", "a", 1.0) == ErrorCode::ID_LEADING_ZERO, tag + ": add leading zero");
    expect(st->add("12", "", 1.0) == ErrorCode::NAME_EMPTY, tag + ": add empty name");
    expect(st->add("12", "a", 100.5) == ErrorCode::SCORE_OUT_OF_RANGE, tag + ": add score out of range");
    expect(st->add("1a", "a", 1.0) == ErrorCode::ID_INVALID_CHAR, tag + ": add invalid id char");
    expect(st->add("12", "a", 1.0) == ErrorCode::OK, tag + ": add valid");
    expect(st->add("12", "b", 2.0) == ErrorCode::ID_DUPLICATE, tag + ": add duplicate");

    Student s;
    expect(st->find("0", s) == ErrorCode::ID_LEADING_ZERO, tag + ": find leading zero");
    expect(st->find("", s) == ErrorCode::ID_EMPTY, tag + ": find empty id");
    expect(st->remove("abc") == ErrorCode::ID_INVALID_CHAR, tag + ": remove invalid id");
    expect(count_of(*st) == 1, tag + ": count unchanged by rejected inputs");
    expect(st->find("12", s) == ErrorCode::OK && s.name == "a", tag + ": duplicate did not overwrite");
}

// Returns true if x is bit-for-bit the canonical stored form tenths / 10.0.
// x: score to inspect.
bool is_canonical(double x) {
    const double canonical = static_cast<double>(to_tenths(x)) / 10.0;
    return std::memcmp(&canonical, &x, sizeof x) == 0;
}

// add rounds the score to one decimal, range-checks the rounded value, and stores the rounded form.
// kind: store kind to test; dir: scratch directory.
void test_add_rounding(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " add rounding";
    auto st = make_store(kind);
    expect(st->init((dir / (kind_name(kind) + "_round.txt")).string()) == ErrorCode::OK, tag + ": init");

    struct Case {
        std::string id;
        double score;
        ErrorCode code;
        std::string stored;
    };
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<Case> cases = {
        {"1", 85.04, ErrorCode::OK, "85.0"},
        {"2", 85.06, ErrorCode::OK, "85.1"},
        {"3", 85.55, ErrorCode::OK, "85.6"},
        {"4", 99.96, ErrorCode::OK, "100.0"},
        {"5", 100.04, ErrorCode::OK, "100.0"},
        {"6", -0.04, ErrorCode::OK, "0.0"},
        {"7", 100.06, ErrorCode::SCORE_OUT_OF_RANGE, ""},
        {"8", -0.06, ErrorCode::SCORE_OUT_OF_RANGE, ""},
        {"9", nan, ErrorCode::SCORE_OUT_OF_RANGE, ""},
        {"10", inf, ErrorCode::SCORE_OUT_OF_RANGE, ""},
        {"11", -inf, ErrorCode::SCORE_OUT_OF_RANGE, ""},
    };
    for (const Case& c : cases) {
        expect(st->add(c.id, "a", c.score) == c.code, tag + ": add id " + c.id);
        Student s;
        if (c.code == ErrorCode::OK) {
            expect(st->find(c.id, s) == ErrorCode::OK && format_score(s.score) == c.stored &&
                       is_canonical(s.score),
                   tag + ": stored value for id " + c.id);
        } else {
            expect(st->find(c.id, s) == ErrorCode::ID_NOT_FOUND, tag + ": rejected id " + c.id + " absent");
        }
    }
}

// import_records validates all-or-nothing, initializes the store, rounds scores, and never touches the file.
// kind: store kind to test; dir: scratch directory.
void test_import(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " import";
    const fs::path file = dir / (kind_name(kind) + "_import.txt");
    fs::remove(file);
    auto st = make_store(kind);

    const std::vector<Student> good = {{"12", "张三", 85.5}, {"7", "Tom Li", 100.04}, {"100", "c", 0.0}};
    expect(st->import_records(good, file.string()) == ErrorCode::OK, tag + ": import into uninitialized store");
    expect(!fs::exists(file), tag + ": import does not create the file");
    expect(count_of(*st) == 3, tag + ": count after import");
    Student s;
    expect(st->find("7", s) == ErrorCode::OK && format_score(s.score) == "100.0" && is_canonical(s.score),
           tag + ": imported score rounded");
    if (kind == 0) {
        expect(ids_of(*st) == std::vector<std::string>{"12", "7", "100"}, tag + ": list keeps given order");
    } else {
        expect(ids_of(*st) == std::vector<std::string>{"100", "12", "7"}, tag + ": bst sorted by id");
    }

    const std::vector<Student> before = records_of(*st);
    const std::vector<Student> dup = {{"1", "a", 1.0}, {"1", "b", 2.0}};
    expect(st->import_records(dup, file.string()) == ErrorCode::ID_DUPLICATE, tag + ": duplicate rejected");
    const std::vector<Student> bad_name = {{"1", "a", 1.0}, {"2", "", 2.0}};
    expect(st->import_records(bad_name, file.string()) == ErrorCode::NAME_EMPTY, tag + ": bad name rejected");
    const std::vector<Student> bad_score = {{"1", "a", 100.5}};
    expect(st->import_records(bad_score, file.string()) == ErrorCode::SCORE_OUT_OF_RANGE,
           tag + ": bad score rejected");
    expect(same_records(records_of(*st), before), tag + ": data untouched after failed imports");

    expect(st->save() == ErrorCode::OK && fs::exists(file), tag + ": save writes to the bound path");
    expect(st->import_records({}, file.string()) == ErrorCode::OK && count_of(*st) == 0,
           tag + ": import empty list");
}

// Init must clear both memory and the file, even when the store already holds data.
// kind: store kind to test; dir: scratch directory.
void test_init_clears(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " init clears";
    auto st = make_store(kind);
    const fs::path file = dir / (kind_name(kind) + "_clear.txt");
    expect(st->init(file.string()) == ErrorCode::OK, tag + ": first init");
    expect(st->add("1", "a", 1.0) == ErrorCode::OK, tag + ": add 1");
    expect(st->add("2", "b", 2.0) == ErrorCode::OK, tag + ": add 2");
    expect(st->save() == ErrorCode::OK, tag + ": save");

    expect(st->init(file.string()) == ErrorCode::OK, tag + ": second init");
    expect(count_of(*st) == 0, tag + ": memory cleared");
    expect(read_raw(file) == "STUDENTS_V1\n", tag + ": file cleared to header only");
    Student s;
    expect(st->find("1", s) == ErrorCode::ID_NOT_FOUND, tag + ": cleared record not found");
}

// Traversal order: list keeps insertion order, BST keeps ascending lexicographic id order.
// kind: store kind to test; dir: scratch directory.
void test_order(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " order";
    auto st = make_store(kind);
    expect(st->init((dir / (kind_name(kind) + "_order.txt")).string()) == ErrorCode::OK, tag + ": init");
    expect(st->add("9", "a", 1.0) == ErrorCode::OK, tag + ": add 9");
    expect(st->add("10", "b", 1.0) == ErrorCode::OK, tag + ": add 10");
    expect(st->add("100", "c", 1.0) == ErrorCode::OK, tag + ": add 100");

    const std::vector<std::string> ids = ids_of(*st);
    if (kind == 0) {
        expect(ids == std::vector<std::string>{"9", "10", "100"}, tag + ": insertion order");
    } else {
        expect(ids == std::vector<std::string>{"10", "100", "9"}, tag + ": lexicographic order");
    }
}

// Saves with one kind and loads with another; the records must survive the round trip.
// from_kind: store kind that saves; to_kind: store kind that loads; dir: scratch directory.
void test_save_load(int from_kind, int to_kind, const fs::path& dir) {
    const std::string tag = "save " + kind_name(from_kind) + " -> load " + kind_name(to_kind);
    const fs::path file = dir / ("roundtrip_" + kind_name(from_kind) + "_" + kind_name(to_kind) + ".txt");

    auto a = make_store(from_kind);
    expect(a->init(file.string()) == ErrorCode::OK, tag + ": init");
    expect(a->add("12", "张三", score_of("85.5")) == ErrorCode::OK, tag + ": add 12");
    expect(a->add("7", "Tom Li", score_of("100")) == ErrorCode::OK, tag + ": add 7");
    expect(a->add("100", " 空格 ", score_of("0")) == ErrorCode::OK, tag + ": add 100");
    expect(a->save() == ErrorCode::OK, tag + ": save");
    const std::vector<Student> expected = records_of(*a);

    auto b = make_store(to_kind);
    std::size_t line = 99;
    expect(b->load(file.string(), line) == ErrorCode::OK && line == 0, tag + ": load");
    expect(same_records(records_of(*b), expected), tag + ": same records after load");
    expect(b->add("999", "new", 1.0) == ErrorCode::OK, tag + ": loaded store accepts add");
}

// A corrupt file must fail with the bad line number and leave the current data and state untouched.
// kind: store kind to test; dir: scratch directory.
void test_corrupt_keeps_state(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " corrupt load";
    const fs::path file = dir / (kind_name(kind) + "_corrupt.txt");
    auto st = make_store(kind);
    expect(st->init(file.string()) == ErrorCode::OK, tag + ": init");
    expect(st->add("1", "a", 1.0) == ErrorCode::OK, tag + ": add 1");
    expect(st->add("2", "b", 2.0) == ErrorCode::OK, tag + ": add 2");
    expect(st->save() == ErrorCode::OK, tag + ": save");
    const std::vector<Student> before = records_of(*st);

    write_raw(file, "STUDENTS_V1\n12\t85.5\t张三\n12\t90.0\tLi\n");
    std::size_t line = 0;
    expect(st->load(file.string(), line) == ErrorCode::FILE_FORMAT_ERROR, tag + ": format error");
    expect(line == 3, tag + ": error line is 3");
    expect(same_records(records_of(*st), before), tag + ": data untouched");
    expect(st->add("3", "c", 3.0) == ErrorCode::OK, tag + ": store still usable");
}

// Deletion across tree shapes: root with two children whose successor has a right child,
// leaves, nodes with two children, and emptying the store. Both kinds must agree.
// kind: store kind to test; dir: scratch directory.
void test_delete_shapes(int kind, const fs::path& dir) {
    const std::string tag = kind_name(kind) + " delete shapes";
    auto st = make_store(kind);
    expect(st->init((dir / (kind_name(kind) + "_shapes.txt")).string()) == ErrorCode::OK, tag + ": init");

    std::vector<std::string> live;
    for (const char* id : {"50", "30", "70", "20", "40", "60", "80", "55", "65", "57"}) {
        expect(st->add(id, std::string("n") + id, 80.0) == ErrorCode::OK, tag + ": add " + id);
        live.push_back(id);
    }

    for (const char* id : {"50", "20", "70", "60", "40", "30", "55", "57", "65", "80"}) {
        expect(st->remove(id) == ErrorCode::OK, tag + ": remove " + id);
        live.erase(std::find(live.begin(), live.end(), std::string(id)));

        Student s;
        expect(st->find(id, s) == ErrorCode::ID_NOT_FOUND, tag + ": " + id + " gone");
        std::vector<std::string> want = live;
        std::sort(want.begin(), want.end());
        expect(sorted_ids_of(*st) == want, tag + ": contents after removing " + id);
        expect(count_of(*st) == static_cast<long long>(live.size()), tag + ": count after removing " + id);
    }
    expect(st->remove("99") == ErrorCode::ID_NOT_FOUND, tag + ": remove missing id");
}

// Runs the same random operation sequence on both implementations and checks every result matches.
// dir: scratch directory.
void test_random_crosscheck(const fs::path& dir) {
    ListStore list;
    BstStore bst;
    expect(list.init((dir / "rand_list.txt").string()) == ErrorCode::OK, "random: list init");
    expect(bst.init((dir / "rand_bst.txt").string()) == ErrorCode::OK, "random: bst init");

    std::mt19937 rng(20260101);
    std::uniform_int_distribution<int> pick_op(0, 9);
    std::uniform_int_distribution<int> pick_num(1, 300);
    std::uniform_int_distribution<int> pick_tenths(0, 1005);

    int mismatches = 0;
    for (int step = 0; step < 5000; ++step) {
        std::string id = std::to_string(pick_num(rng));
        if (step % 97 == 0) id = "0" + id;  // leading-zero probe
        const int op = pick_op(rng);

        if (op <= 3) {  // add
            const int t = pick_tenths(rng);
            const std::string text = std::to_string(t / 10) + "." + std::to_string(t % 10);
            double v = -1.0;
            if (parse_input_score(text, v) != ErrorCode::OK) continue;
            const std::string name = "n" + id;
            if (list.add(id, name, v) != bst.add(id, name, v)) ++mismatches;
        } else if (op <= 5) {  // find
            Student a, b;
            const ErrorCode ca = list.find(id, a);
            const ErrorCode cb = bst.find(id, b);
            if (ca != cb) {
                ++mismatches;
            } else if (ca == ErrorCode::OK && (a.name != b.name || compare_score(a.score, b.score) != 0)) {
                ++mismatches;
            }
        } else if (op <= 7) {  // remove
            if (list.remove(id) != bst.remove(id)) ++mismatches;
        } else if (op == 8) {  // count
            if (count_of(list) != count_of(bst)) ++mismatches;
        } else {  // full content comparison
            if (!same_records(records_of(list), records_of(bst))) ++mismatches;
        }
    }
    expect(mismatches == 0, "random crosscheck mismatches: " + std::to_string(mismatches));
    expect(same_records(records_of(list), records_of(bst)), "random crosscheck final contents");

    const std::vector<std::string> ids = ids_of(bst);
    const bool ascending = std::is_sorted(ids.begin(), ids.end()) &&
                           std::adjacent_find(ids.begin(), ids.end()) == ids.end();
    expect(ascending, "bst traversal strictly ascending");
}

// Times 100000 random-order adds and finds on each implementation.
// dir: scratch directory. Only called with --bench.
void run_bench(const fs::path& dir) {
    using clock = std::chrono::steady_clock;
    std::vector<std::string> ids;
    for (int i = 1; i <= 100000; ++i) ids.push_back(std::to_string(i));
    std::mt19937 rng(7);
    std::shuffle(ids.begin(), ids.end(), rng);

    for (int kind = 0; kind < 2; ++kind) {
        auto st = make_store(kind);
        expect(st->init((dir / ("bench_" + kind_name(kind) + ".txt")).string()) == ErrorCode::OK,
               "bench init " + kind_name(kind));

        const auto t0 = clock::now();
        for (const std::string& id : ids) st->add(id, "stu" + id, 80.0);
        const auto t1 = clock::now();
        Student s;
        for (const std::string& id : ids) st->find(id, s);
        const auto t2 = clock::now();

        std::printf("性能 · %s：%zu 条记录，添加 %lld ms，查找 %lld ms\n", kind_label(kind).c_str(),
                    static_cast<std::size_t>(count_of(*st)),
                    static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()),
                    static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()));
    }
}

}  // namespace

// Runs all phase 2 tests and prints a summary; "--bench" also runs the timing.
// Returns 0 when every check passes, 1 otherwise.
int main(int argc, char** argv) {
    std::printf("阶段二单元测试：单链表与 BST 两套存储实现\n");
    const fs::path dir = fs::temp_directory_path() / "phase2_tests";
    fs::remove_all(dir);
    fs::create_directories(dir);

    for (int kind = 0; kind < 2; ++kind) {
        const std::string k = kind_label(kind) + " · ";
        run_group(k + "未初始化时拒绝操作", [&] { test_before_init(kind, dir); });
        run_group(k + "空表与单条记录", [&] { test_empty_and_single(kind, dir); });
        run_group(k + "非法输入拒绝", [&] { test_invalid_inputs(kind, dir); });
        run_group(k + "添加时成绩舍入", [&] { test_add_rounding(kind, dir); });
        run_group(k + "批量导入", [&] { test_import(kind, dir); });
        run_group(k + "初始化清空内存与文件", [&] { test_init_clears(kind, dir); });
        run_group(k + "遍历顺序", [&] { test_order(kind, dir); });
        run_group(k + "损坏文件加载不改变数据", [&] { test_corrupt_keeps_state(kind, dir); });
        run_group(k + "各种形态的删除", [&] { test_delete_shapes(kind, dir); });
    }
    for (int from = 0; from < 2; ++from) {
        for (int to = 0; to < 2; ++to) {
            run_group("保存/加载往返 · " + kind_label(from) + " 保存 → " + kind_label(to) + " 加载",
                      [&] { test_save_load(from, to, dir); });
        }
    }
    run_group("随机操作交叉比对（5000 步，两套实现结果一致）", [&] { test_random_crosscheck(dir); });

    if (argc > 1 && std::string(argv[1]) == "--bench") run_bench(dir);

    fs::remove_all(dir);
    std::printf("合计：%d 项检查，%d 项失败\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
