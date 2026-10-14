#include "bst_store.h"

#include <algorithm>

#include "score.h"
#include "storage.h"
#include "validate.h"

// Frees all nodes when the store is destroyed.
BstStore::~BstStore() {
    clear_nodes();
}

// Writes an empty table to path, then clears memory; memory is untouched if the write fails.
// path: file to rewrite. Returns OK or FILE_IO_ERROR.
ErrorCode BstStore::init(const std::string& path) {
    const ErrorCode code = save_records(path, {});
    if (code != ErrorCode::OK) return code;
    clear_nodes();
    path_ = path;
    initialized_ = true;
    return ErrorCode::OK;
}

// Loads path all-or-nothing and replaces the tree with the loaded records.
// path: file to read; error_line: receives the bad line on format errors. Returns the load result.
ErrorCode BstStore::load(const std::string& path, std::size_t& error_line) {
    std::vector<Student> records;
    const ErrorCode code = load_records(path, records, error_line);
    if (code != ErrorCode::OK) return code;
    return import_records(records, path);
}

// Validates all records first, then rebuilds a balanced tree from them sorted by id (scores rounded).
// Building from sorted data avoids the degenerate tree that inserting a saved (sorted) file would create.
// records: new contents; path: file bound for later saves, not touched here. Returns OK or the first failure.
ErrorCode BstStore::import_records(const std::vector<Student>& records, const std::string& path) {
    const ErrorCode code = validate_student_records(records);
    if (code != ErrorCode::OK) return code;

    std::vector<Student> sorted;
    sorted.reserve(records.size());
    for (const Student& s : records) sorted.push_back(Student{s.id, s.name, round_score(s.score)});
    std::sort(sorted.begin(), sorted.end(),
              [](const Student& a, const Student& b) { return a.id < b.id; });

    clear_nodes();
    root_ = build_balanced(sorted, 0, sorted.size());
    size_ = sorted.size();
    path_ = path;
    initialized_ = true;
    return ErrorCode::OK;
}

// Writes all records, in ascending id order, to the file bound by init, load or import_records.
// Returns NOT_INITIALIZED, or the result of save_records.
ErrorCode BstStore::save() {
    std::vector<Student> records;
    const ErrorCode code = traverse(records);
    if (code != ErrorCode::OK) return code;
    return save_records(path_, records);
}

// Adds a record after validating id, name and score, and rejects duplicate ids.
// The score is range-checked in tenths (i.e. after rounding) and stored rounded to one decimal.
// id, name, score: fields of the new record. Returns OK or the first failing ErrorCode.
ErrorCode BstStore::add(const std::string& id, const std::string& name, double score) {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    ErrorCode code = validate_student_id(id);
    if (code != ErrorCode::OK) return code;
    code = validate_student_name(name);
    if (code != ErrorCode::OK) return code;
    if (!is_score_in_range(score)) return ErrorCode::SCORE_OUT_OF_RANGE;
    if (!insert_node(Student{id, name, round_score(score)})) return ErrorCode::ID_DUPLICATE;
    return ErrorCode::OK;
}

// Looks up a record by id and copies it into out.
// id: student id to find; out: receives the record on success. Returns OK or the failing ErrorCode.
ErrorCode BstStore::find(const std::string& id, Student& out) const {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    const ErrorCode code = validate_student_id(id);
    if (code != ErrorCode::OK) return code;
    const Node* node = find_node(id);
    if (node == nullptr) return ErrorCode::ID_NOT_FOUND;
    out = node->data;
    return ErrorCode::OK;
}

// Removes the record with this id. A node with two children takes the data of its in-order
// successor (the leftmost node of its right subtree), and that successor node is unlinked.
// id: student id to remove. Returns OK or the failing ErrorCode.
ErrorCode BstStore::remove(const std::string& id) {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    const ErrorCode code = validate_student_id(id);
    if (code != ErrorCode::OK) return code;

    // link points at the pointer that refers to the target node, so the parent needs no separate variable.
    Node** link = &root_;
    while (*link != nullptr && (*link)->data.id != id) {
        link = id < (*link)->data.id ? &(*link)->left : &(*link)->right;
    }
    if (*link == nullptr) return ErrorCode::ID_NOT_FOUND;

    Node* target = *link;
    if (target->left != nullptr && target->right != nullptr) {
        Node** succ_link = &target->right;
        while ((*succ_link)->left != nullptr) succ_link = &(*succ_link)->left;
        Node* succ = *succ_link;
        *succ_link = succ->right;  // succ has no left child, so only its right subtree moves up
        target->data = succ->data;
        delete succ;
    } else {
        *link = target->left != nullptr ? target->left : target->right;
        delete target;
    }
    --size_;
    return ErrorCode::OK;
}

// Copies every record into out in ascending id order, using an explicit stack (no recursion).
// out: replaced with the records. Returns NOT_INITIALIZED or OK.
ErrorCode BstStore::traverse(std::vector<Student>& out) const {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    out.clear();
    out.reserve(size_);

    std::vector<const Node*> stack;
    const Node* cur = root_;
    while (cur != nullptr || !stack.empty()) {
        while (cur != nullptr) {
            stack.push_back(cur);
            cur = cur->left;
        }
        cur = stack.back();
        stack.pop_back();
        out.push_back(cur->data);
        cur = cur->right;
    }
    return ErrorCode::OK;
}

// Reports the number of records.
// out: receives the count. Returns NOT_INITIALIZED or OK.
ErrorCode BstStore::count(std::size_t& out) const {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    out = size_;
    return ErrorCode::OK;
}

// Descends from the root by id order and returns the node with this id, or nullptr.
// id: student id to search for.
const BstStore::Node* BstStore::find_node(const std::string& id) const {
    const Node* node = root_;
    while (node != nullptr && node->data.id != id) {
        node = id < node->data.id ? node->left : node->right;
    }
    return node;
}

// Inserts a record as a new leaf in id order.
// s: record to insert. Returns false if the id already exists (tree unchanged), true otherwise.
bool BstStore::insert_node(const Student& s) {
    Node** link = &root_;
    while (*link != nullptr) {
        if (s.id == (*link)->data.id) return false;
        link = s.id < (*link)->data.id ? &(*link)->left : &(*link)->right;
    }
    *link = new Node{s, nullptr, nullptr};
    ++size_;
    return true;
}

// Builds a balanced subtree from the middle element of sorted[lo, hi), recursing on each half.
// sorted: records sorted by id with no duplicates; lo, hi: half-open index range.
// Returns the subtree root, or nullptr for an empty range.
BstStore::Node* BstStore::build_balanced(const std::vector<Student>& sorted, std::size_t lo, std::size_t hi) {
    if (lo >= hi) return nullptr;
    const std::size_t mid = lo + (hi - lo) / 2;
    Node* node = new Node{sorted[mid], nullptr, nullptr};
    node->left = build_balanced(sorted, lo, mid);
    node->right = build_balanced(sorted, mid + 1, hi);
    return node;
}

// Deletes every node with an explicit stack and resets the tree to empty.
void BstStore::clear_nodes() {
    std::vector<Node*> stack;
    if (root_ != nullptr) stack.push_back(root_);
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        if (node->left != nullptr) stack.push_back(node->left);
        if (node->right != nullptr) stack.push_back(node->right);
        delete node;
    }
    root_ = nullptr;
    size_ = 0;
}
