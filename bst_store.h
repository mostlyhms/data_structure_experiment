#ifndef BST_STORE_H
#define BST_STORE_H

#include <cstddef>
#include <string>
#include <vector>

#include "store.h"

// Binary search tree implementation keyed by student id (lexicographic order).
class BstStore : public StudentStore {
public:
    BstStore() = default;

    // Frees every node.
    ~BstStore() override;

    BstStore(const BstStore&) = delete;
    BstStore& operator=(const BstStore&) = delete;

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
        Node* left;
        Node* right;
    };

    // Returns the node holding this id, or nullptr if the id is absent.
    const Node* find_node(const std::string& id) const;

    // Inserts a record as a leaf; returns false without changing the tree if the id already exists.
    bool insert_node(const Student& s);

    // Builds a height-balanced subtree from sorted[lo, hi); returns its root or nullptr when empty.
    static Node* build_balanced(const std::vector<Student>& sorted, std::size_t lo, std::size_t hi);

    // Deletes every node iteratively and resets the tree to empty.
    void clear_nodes();

    Node* root_ = nullptr;
    std::size_t size_ = 0;
    bool initialized_ = false;
    std::string path_;
};

#endif
