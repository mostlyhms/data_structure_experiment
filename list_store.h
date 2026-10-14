#ifndef LIST_STORE_H
#define LIST_STORE_H

#include <cstddef>
#include <string>

#include "store.h"

// Singly linked list implementation; records are traversed in insertion order.
class ListStore : public StudentStore {
public:
    ListStore() = default;

    // Frees every node.
    ~ListStore() override;

    ListStore(const ListStore&) = delete;
    ListStore& operator=(const ListStore&) = delete;

    ErrorCode init(const std::string& path) override;
    ErrorCode load(const std::string& path, std::size_t& error_line) override;
    ErrorCode import_records(const std::vector<Student>& records, const std::string& path) override;
    ErrorCode save() override;
    ErrorCode add(const std::string& id, const std::string& name, double score) override;
    ErrorCode find(const std::string& id, Student& out) const override;
    ErrorCode remove(const std::string& id) override;
    ErrorCode traverse(std::vector<Student>& out) const override;
    ErrorCode count(std::size_t& out) const override;

private:
    struct Node {
        Student data;
        Node* next;
    };

    // Returns the node holding this id, or nullptr if the id is absent.
    const Node* find_node(const std::string& id) const;

    // Appends a record at the tail; the caller guarantees the id is not already present.
    void append(const Student& s);

    // Deletes every node and resets the list to empty.
    void clear_nodes();

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
    bool initialized_ = false;
    std::string path_;
};

#endif
