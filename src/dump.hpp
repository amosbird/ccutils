#pragma once
#include <iostream>
#include <string>

struct Trunk {
    Trunk* prev;
    std::string str;

    Trunk(Trunk* prev, std::string str) {
        this->prev = prev;
        this->str = str;
    }
};

// Helper function to print branches of the binary tree
void showTrunks(std::ostream& os, Trunk* p, int first = 1) {
    if (p == nullptr)
        return;
    showTrunks(os, p->prev, 0);
    os << p->str.substr(0, p->str.size() - first);
}

template <typename T>
void dump(std::ostream& os, T* root, Trunk* prev = nullptr, bool left = true) {
    if (root == nullptr)
        return;
    std::string prev_str = "    ";
    Trunk* trunk = new Trunk(prev, prev_str);
    dump(os, root->left, trunk, true);
    if (!prev)
        trunk->str = "----";
    else if (left) {
        trunk->str = ".---";
        prev_str = "   |";
    } else {
        trunk->str = "`---";
        prev->str = prev_str;
    }
    showTrunks(os, trunk);
    std::cerr << root->val << std::endl;
    if (prev)
        prev->str = prev_str;
    trunk->str = "   |";
    dump(os, root->right, trunk, false);
}

template <typename T> inline static std::ostream& operator<<(std::ostream& os, T* root) noexcept {
    dump(os, root);
    return os;
}
