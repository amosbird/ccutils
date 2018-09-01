#pragma once
g
#include <iomanip>
#include <iostream>

struct Trunk {
    Trunk* prev;
    string str;

    Trunk(Trunk* prev, string str) {
        this->prev = prev;
        this->str = str;
    }
};

// Helper function to print branches of the binary tree
void showTrunks(ostream& os, Trunk* p) {
    if (p == nullptr)
        return;
    showTrunks(os, p->prev);
    os << p->str;
}

template <typename T> void dump(ostream& os, T* root, Trunk* prev = nullptr, bool left = true) {
    if (root == nullptr)
        return;
    string prev_str = "    ";
    Trunk* trunk = new Trunk(prev, prev_str);
    dump(os, root->left, trunk, true);
    if (!prev)
        trunk->str = "---";
    else if (left) {
        trunk->str = ".---";
        prev_str = "   |";
    } else {
        trunk->str = "`---";
        prev->str = prev_str;
    }
    showTrunks(os, trunk);
    cout << root->val << endl;
    if (prev)
        prev->str = prev_str;
    trunk->str = "   |";
    dump(os, root->right, trunk, false);
}

template <typename T> inline static ostream& operator<<(ostream& os, T* root) noexcept {
    dump(os, root);
    return os;
}
