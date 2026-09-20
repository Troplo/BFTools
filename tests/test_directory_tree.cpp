#include "test_utils.hpp"
#include "Nomad/Archives/DirectoryTree.hpp"
#include "Nomad/Archives/Types.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include <gtest/gtest.h>

namespace Nomad::Archives::Test {

class DirectoryTreeTest : public ::testing::Test {};

TEST_F(DirectoryTreeTest, BuildHierarchyAndRowInParent) {
    std::vector<ArchiveEntry> entries;
    entries.push_back({.path = "engine/settings/default.xml"});
    entries.push_back({.path = "engine/settings/render.xml"});
    entries.push_back({.path = "engine/audio/sound.bnk"});
    entries.push_back({.path = "root_file.txt"});

    auto root = DirectoryTreeBuilder::Build(entries);
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->isDirectory);
    EXPECT_EQ(root->name, "/");

    
    ASSERT_EQ(root->children.size(), 2);
    auto engineNode = root->children[0];
    EXPECT_EQ(engineNode->name, "engine");
    EXPECT_TRUE(engineNode->isDirectory);
    EXPECT_EQ(engineNode->parent, root.get());
    EXPECT_EQ(engineNode->RowInParent(), 0);

    auto fileNode = root->children[1];
    EXPECT_EQ(fileNode->name, "root_file.txt");
    EXPECT_FALSE(fileNode->isDirectory);
    EXPECT_EQ(fileNode->RowInParent(), 1);

    
    ASSERT_EQ(engineNode->children.size(), 2);
    for (size_t i = 0; i < engineNode->children.size(); ++i) {
        EXPECT_EQ(engineNode->children[i]->RowInParent(), i);
        EXPECT_EQ(engineNode->children[i]->parent, engineNode.get());
    }
}

TEST_F(DirectoryTreeTest, WildcardMatching) {
    EXPECT_TRUE(HasWildcards("*.xml"));
    EXPECT_TRUE(HasWildcards("worlds/*"));
    EXPECT_TRUE(HasWildcards("test?.txt"));
    EXPECT_TRUE(HasWildcards("file[0-9].bin"));
    EXPECT_FALSE(HasWildcards("worlds/oasis/oasis.xml"));

    
    EXPECT_TRUE(MatchGlob("worlds/*.xml", "worlds/oasis.xml"));
    EXPECT_FALSE(MatchGlob("worlds/*.xml", "worlds/sub/oasis.xml"));
    EXPECT_TRUE(MatchGlob("worlds/*/*.xml", "worlds/sub/oasis.xml"));
    EXPECT_FALSE(MatchGlob("worlds/*/*.xml", "worlds/sub/extra/oasis.xml"));

    
    EXPECT_TRUE(MatchGlob("worlds/**/*.xml", "worlds/oasis.xml"));
    EXPECT_TRUE(MatchGlob("worlds/**/*.xml", "worlds/sub/oasis.xml"));
    EXPECT_TRUE(MatchGlob("worlds/**/*.xml", "worlds/a/b/c/oasis.xml"));
    EXPECT_TRUE(MatchGlob("worlds/**", "worlds/sub/oasis.xml"));

    
    EXPECT_TRUE(MatchGlob("worlds/*", "worlds/oasis.xml"));
    EXPECT_FALSE(MatchGlob("worlds/*", "worlds/sub/oasis.xml"));

    
    EXPECT_TRUE(MatchGlob("world?.xml", "world1.xml"));
    EXPECT_FALSE(MatchGlob("world?.xml", "world12.xml"));
    EXPECT_FALSE(MatchGlob("world?.xml", "world/.xml"));

    
    EXPECT_TRUE(MatchGlob("world[0-9].xml", "world5.xml"));
    EXPECT_FALSE(MatchGlob("world[0-9].xml", "worldA.xml"));
    EXPECT_TRUE(MatchGlob("world[!0-9].xml", "worldA.xml"));
    EXPECT_FALSE(MatchGlob("world[!0-9].xml", "world5.xml"));

    
    EXPECT_TRUE(MatchGlob("WORLDS/*.XML", "worlds/oasis.xml"));
    EXPECT_TRUE(MatchGlob("worlds/*.xml", "WORLDS/OASIS.XML"));
}

TEST_F(DirectoryTreeTest, PathNormalization) {
    EXPECT_EQ(NormalizePath("a\\b\\c.txt"), "a/b/c.txt");
    EXPECT_EQ(NormalizePath("\\a\\b\\c.txt"), "a/b/c.txt");

    EXPECT_EQ(NormalizePathForNfo("a/b/c.txt"), "a\\b\\c.txt");
    EXPECT_EQ(NormalizePathForHash("a/b/c.txt"), "a\\b\\c.txt");
}

TEST_F(DirectoryTreeTest, HashConsistency) {
    auto v2 = ArchiveFactory::Create(ArchiveVersion::FATv2);
    auto v3 = ArchiveFactory::Create(ArchiveVersion::FATv3);
    auto v5 = ArchiveFactory::Create(ArchiveVersion::FATv5);

    std::string testPath = "engine/settings/test.xml";
    uint64_t h2 = v2->ComputeNameHash(testPath);
    uint64_t h3 = v3->ComputeNameHash(testPath);
    uint64_t h5 = v5->ComputeNameHash(testPath);

    EXPECT_NE(h2, 0);
    EXPECT_NE(h3, 0);
    EXPECT_NE(h5, 0);

    
    EXPECT_EQ(v2->ComputeNameHash(testPath), h2);
    EXPECT_EQ(v3->ComputeNameHash(testPath), h3);
    EXPECT_EQ(v5->ComputeNameHash(testPath), h5);
}

} 
