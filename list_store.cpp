#include "list_store.h"

#include "score.h"
#include "storage.h"
#include "validate.h"

// Frees all nodes when the store is destroyed.
ListStore::~ListStore() {
    clear_nodes();
}

// Writes an empty table to path, then clears memory; memory is untouched if the write fails.
// path: file to rewrite. Returns OK or FILE_IO_ERROR.
ErrorCode ListStore::init(const std::string& path) {
    const ErrorCode code = save_records(path, {});
    if (code != ErrorCode::OK) return code;
    clear_nodes();
    path_ = path;
    initialized_ = true;
    return ErrorCode::OK;
}

// Loads path all-or-nothing and replaces the list with the loaded records.
// path: file to read; error_line: receives the bad line on format errors. Returns the load result.
ErrorCode ListStore::load(const std::string& path, std::size_t& error_line) {
    std::vector<Student> records;
    const ErrorCode code = load_records(path, records, error_line);
    if (code != ErrorCode::OK) return code;
    clear_nodes();
    for (const Student& s : records) append(s);
    path_ = path;
    initialized_ = true;
    return ErrorCode::OK;
}

// Writes all records, in list order, to the file chosen by init or load.
// Returns NOT_INITIALIZED, or the result of save_records.
ErrorCode ListStore::save() {
    std::vector<Student> records;
    const ErrorCode code = traverse(records);
    if (code != ErrorCode::OK) return code;
    return save_records(path_, records);
}

// Adds a record after validating id, name and score, and rejects duplicate ids.
// The score is range-checked in tenths (i.e. after rounding) and stored rounded to one decimal.
// id, name, score: fields of the new record. Returns OK or the first failing ErrorCode.
ErrorCode ListStore::add(const std::string& id, const std::string& name, double score) {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    ErrorCode code = validate_student_id(id);
    if (code != ErrorCode::OK) return code;
    code = validate_student_name(name);
    if (code != ErrorCode::OK) return code;
    if (!is_score_in_range(score)) return ErrorCode::SCORE_OUT_OF_RANGE;
    if (find_node(id) != nullptr) return ErrorCode::ID_DUPLICATE;
    append(Student{id, name, round_score(score)});
    return ErrorCode::OK;
}

// Looks up a record by id and copies it into out.
// id: student id to find; out: receives the record on success. Returns OK or the failing ErrorCode.
ErrorCode ListStore::find(const std::string& id, Student& out) const {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    const ErrorCode code = validate_student_id(id);
    if (code != ErrorCode::OK) return code;
    const Node* node = find_node(id);
    if (node == nullptr) return ErrorCode::ID_NOT_FOUND;
    out = node->data;
    return ErrorCode::OK;
}

// Removes the record with this id and unlinks its node, fixing head and tail when needed.
// id: student id to remove. Returns OK or the failing ErrorCode.
ErrorCode ListStore::remove(const std::string& id) {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    const ErrorCode code = validate_student_id(id);
    if (code != ErrorCode::OK) return code;

    Node* prev = nullptr;
    Node* cur = head_;
    while (cur != nullptr && cur->data.id != id) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == nullptr) return ErrorCode::ID_NOT_FOUND;

    if (prev != nullptr) {
        prev->next = cur->next;
    } else {
        head_ = cur->next;
    }
    if (cur == tail_) tail_ = prev;
    delete cur;
    --size_;
    return ErrorCode::OK;
}

// Copies every record, in list order, into out.
// out: replaced with the records. Returns NOT_INITIALIZED or OK.
ErrorCode ListStore::traverse(std::vector<Student>& out) const {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    out.clear();
    out.reserve(size_);
    for (const Node* node = head_; node != nullptr; node = node->next) {
        out.push_back(node->data);
    }
    return ErrorCode::OK;
}

// Reports the number of records.
// out: receives the count. Returns NOT_INITIALIZED or OK.
ErrorCode ListStore::count(std::size_t& out) const {
    if (!initialized_) return ErrorCode::NOT_INITIALIZED;
    out = size_;
    return ErrorCode::OK;
}

// Walks the list from the head and returns the node with this id, or nullptr.
// id: student id to search for.
const ListStore::Node* ListStore::find_node(const std::string& id) const {
    const Node* node = head_;
    while (node != nullptr && node->data.id != id) node = node->next;
    return node;
}

// Appends a record at the tail, which keeps insertion order; the caller ensures the id is unique.
// s: record to append.
void ListStore::append(const Student& s) {
    Node* node = new Node{s, nullptr};
    if (tail_ == nullptr) {
        head_ = node;
    } else {
        tail_->next = node;
    }
    tail_ = node;
    ++size_;
}

// Deletes every node and resets the list to empty.
void ListStore::clear_nodes() {
    Node* node = head_;
    while (node != nullptr) {
        Node* next = node->next;
        delete node;
        node = next;
    }
    head_ = nullptr;
    tail_ = nullptr;
    size_ = 0;
}
