#include "VecTree.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <utility>

namespace
{
    template<class T, class S>
    void dump_tree_to_stream(
        const VecTree<T>& tree,
        S& outstream)
    {
        tree.traverse_depthfirst([&](const T& node, size_t index, size_t level)
            {
                auto [nbr_children, branch_stride, parent_ofs] = tree.get_node_info(node);

                for (int i = 0; i < level; i++) outstream << "  ";
                outstream << node
                    << " (children " << nbr_children
                    << ", stride " << branch_stride
                    << ", parent ofs " << parent_ofs << ")\n";
            });
    }
}

TEST(VecTreeBasicTest, EmptyTree) {
    VecTree<std::string> tree;
    EXPECT_EQ(tree.size(), 0u);
    EXPECT_FALSE(tree.contains("A"));
}

TEST(VecTreeBasicTest, InsertAndContains) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    EXPECT_EQ(tree.size(), 1u);
    EXPECT_TRUE(tree.contains("A"));
    EXPECT_TRUE(tree.is_root("A"));
    EXPECT_TRUE(tree.is_leaf("A"));
}

TEST(VecTreeInsertTest, InsertChildren) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    EXPECT_TRUE(tree.insert("B", "A"));
    EXPECT_TRUE(tree.insert("C", "A"));
    EXPECT_EQ(tree.size(), 3u);

    auto [nbrA, bsA, poA] = tree.get_node_info("A");
    EXPECT_EQ(nbrA, 2u);
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 3u);
    EXPECT_FALSE(tree.is_leaf("A"));

    EXPECT_EQ(tree.get_parent("B"), "A");
    EXPECT_EQ(tree.get_parent("C"), "A");
    EXPECT_TRUE(tree.is_leaf("B"));
    EXPECT_TRUE(tree.is_leaf("C"));
}

TEST(VecTreeNestedTest, NestedInsertionAndRelationships) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    EXPECT_EQ(tree.size(), 4u);
    EXPECT_EQ(tree.get_nbr_children("B"), 1u);
    EXPECT_EQ(tree.get_branch_size("B"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 4u);

    EXPECT_TRUE(tree.is_descendant_of("D", "A"));
    EXPECT_TRUE(tree.is_descendant_of("D", "B"));
    EXPECT_FALSE(tree.is_descendant_of("C", "B"));
    EXPECT_EQ(tree.get_parent("D"), "B");
}

TEST(VecTreeEraseTest, EraseBranch) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    EXPECT_TRUE(tree.erase_branch("B"));
    EXPECT_EQ(tree.size(), 2u);
    EXPECT_FALSE(tree.contains("B"));
    EXPECT_FALSE(tree.contains("D"));
    EXPECT_EQ(tree.get_nbr_children("A"), 1u);
    EXPECT_EQ(tree.get_branch_size("A"), 2u);
}

TEST(VecTreeEraseTest, EraseRootBranch) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    EXPECT_EQ(tree.size(), 4u);
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 4u);
    
    EXPECT_TRUE(tree.erase_branch("A"));

    EXPECT_EQ(tree.size(), 0u);
    EXPECT_FALSE(tree.contains("A"));
    EXPECT_FALSE(tree.contains("B"));
    EXPECT_FALSE(tree.contains("C"));
}

TEST(VecTreeReparentTest, ReparentNode) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    // Move C under B
    tree.reparent("C", "B");
    EXPECT_EQ(tree.get_nbr_children("A"), 1u);
    EXPECT_EQ(tree.get_nbr_children("B"), 2u);
    EXPECT_TRUE(tree.is_descendant_of("C", "B"));
    EXPECT_EQ(tree.get_parent("C"), "B");
}

TEST(VecTreeUnparentTest, UnparentNode) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "B");

    EXPECT_EQ(tree.get_nbr_children("B"), 1u);
    tree.unparent("C");
    EXPECT_TRUE(tree.is_root("C"));
    EXPECT_FALSE(tree.is_descendant_of("C", "A"));
}

#if 0
TEST(VecTreeTraversalTest, DepthFirstTraversal) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    vector<std::string> order;
    tree.traverse_depthfirst([&](std::string& payload, size_t idx) {
        order.push_back(payload);
        });

    vector<std::string> expected = { "A", "B", "D", "C" };
    EXPECT_EQ(order, expected);
}

TEST(VecTreeTraversalTest, BreadthFirstTraversal) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    vector<std::string> order;
    tree.traverse_breadthfirst("A", [&](std::string& payload, size_t idx) {
        order.push_back(payload);
        });

    vector<std::string> expected = { "A", "B", "C", "D" };
    EXPECT_EQ(order, expected);
}

TEST(VecTreeTraversalTest, AscendTraversal) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "B");

    vector<std::string> order;
    tree.ascend("C", [&](std::string& payload, size_t idx) {
        order.push_back(payload);
        });

    vector<std::string> expected = { "C", "B", "A" };
    EXPECT_EQ(order, expected);
}

TEST(VecTreeTraversalTest, DepthFirstWithLevel) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    vector<pair<std::string, size_t>> results;
    tree.traverse_depthfirst([&](std::string& payload, size_t idx, size_t level) {
        results.emplace_back(payload, level);
        });

    vector<pair<std::string, size_t>> expected = { {"A", 0}, {"B", 1}, {"D", 2}, {"C", 1} };
    EXPECT_EQ(results, expected);
}

TEST(VecTreeTraversalTest, ProgressiveTraversal) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    tree.insert("D", "B");

    vector<pair<std::string, std::string>> calls;
    tree.traverse_progressive([&](std::string* node, std::string* parent, size_t idx, size_t pidx) {
        calls.emplace_back(
            node ? *node : std::string(""),
            parent ? *parent : std::string("(null)")
        );
        });

    vector<pair<std::string, std::string>> expected = {
        {"A", "(null)"},
        {"B", "A"},
        {"C", "A"},
        {"D", "B"}
    };
    EXPECT_EQ(calls, expected);
}


// Test erasing a leaf and adjusting sibling offsets
TEST(VecTreeEraseSiblingTest, EraseSiblingAndAdjustOffsets) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "A");
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 3u);
    EXPECT_TRUE(tree.erase_branch("B"));
    EXPECT_FALSE(tree.contains("B"));
    EXPECT_TRUE(tree.contains("C"));
    EXPECT_EQ(tree.get_nbr_children("A"), 1u);
    EXPECT_EQ(tree.get_branch_size("A"), 2u);
    auto [nbrC, bsC, parentOfsC] = tree.get_node_info("C");
    EXPECT_EQ(parentOfsC, 1u);
}

// Test deep-tree reparenting
TEST(VecTreeReparentDeepTest, ReparentMidSubtree) {
    VecTree<std::string> tree;
    tree.insert_as_root("A");
    tree.insert("B", "A");
    tree.insert("C", "B");
    tree.insert("D", "C");
    EXPECT_EQ(tree.get_branch_size("A"), 4u);
    EXPECT_EQ(tree.get_branch_size("B"), 3u);
    EXPECT_EQ(tree.get_branch_size("C"), 2u);
    tree.reparent("C", "A");
    EXPECT_EQ(tree.get_nbr_children("A"), 2u);
    EXPECT_EQ(tree.get_branch_size("A"), 4u);
    EXPECT_EQ(tree.get_branch_size("B"), 1u);
    EXPECT_TRUE(tree.is_leaf("B"));
    EXPECT_EQ(tree.get_nbr_children("C"), 1u);
    EXPECT_EQ(tree.get_parent("C"), "A");
    EXPECT_EQ(tree.get_parent("D"), "C");
    EXPECT_TRUE(tree.is_leaf("D"));
}
#endif

// main() can be omitted if linked with GTest's main library
