#include "clar_libgit2.h"
#include "tree.h"

static git_repository *repo;

static const char *blob_oid = "3d0970ec547fc41ef8a5882dde99c6adce65b021";
static const char *tree_oid  = "1b05fdaa881ee45b48cbaa5e9b037d667a47745e";

void test_object_tree_attributes__initialize(void)
{
	repo = cl_git_sandbox_init("deprecated-mode.git");
}

void test_object_tree_attributes__cleanup(void)
{
   cl_git_sandbox_cleanup();
}

void test_object_tree_attributes__ensure_correctness_of_attributes_on_insertion(void)
{
	git_treebuilder *builder;
	git_oid oid;

	cl_git_pass(git_oid_from_string(&oid, blob_oid, GIT_OID_SHA1));

	cl_git_pass(git_treebuilder_new(&builder, repo, NULL));

	cl_git_fail(git_treebuilder_insert(NULL, builder, "one.txt", &oid, (git_filemode_t)0777777));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "one.txt", &oid, (git_filemode_t)0100666));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "one.txt", &oid, (git_filemode_t)0000001));

	git_treebuilder_free(builder);
}

void test_object_tree_attributes__group_writable_tree_entries_created_with_an_antique_git_version_can_still_be_accessed(void)
{
	git_oid tid;
	git_tree *tree;
	const git_tree_entry *entry;


	cl_git_pass(git_oid_from_string(&tid, tree_oid, GIT_OID_SHA1));
	cl_git_pass(git_tree_lookup(&tree, repo, &tid));

	entry = git_tree_entry_byname(tree, "old_mode.txt");
	cl_assert_equal_i(
		GIT_FILEMODE_BLOB,
		git_tree_entry_filemode(entry));

	git_tree_free(tree);
}

void test_object_tree_attributes__treebuilder_reject_invalid_filemode(void)
{
	git_treebuilder *builder;
	git_oid bid;
	const git_tree_entry *entry;

	cl_git_pass(git_oid_from_string(&bid, blob_oid, GIT_OID_SHA1));
	cl_git_pass(git_treebuilder_new(&builder, repo, NULL));

	cl_git_fail(git_treebuilder_insert(
		&entry,
		builder,
		"normalized.txt",
		&bid,
		GIT_FILEMODE_BLOB_GROUP_WRITABLE));

	git_treebuilder_free(builder);
}

void test_object_tree_attributes__normalize_attributes_when_creating_a_tree_from_an_existing_one(void)
{
	git_treebuilder *builder;
	git_oid tid, tid2;
	git_tree *tree;
	const git_tree_entry *entry;

	cl_git_pass(git_oid_from_string(&tid, tree_oid, GIT_OID_SHA1));
	cl_git_pass(git_tree_lookup(&tree, repo, &tid));

	cl_git_pass(git_treebuilder_new(&builder, repo, tree));
	
	entry = git_treebuilder_get(builder, "old_mode.txt");
	cl_assert(entry != NULL);
	cl_assert_equal_i(
		GIT_FILEMODE_BLOB,
		git_tree_entry_filemode(entry));

	cl_git_pass(git_treebuilder_write(&tid2, builder));
	git_treebuilder_free(builder);
	git_tree_free(tree);

	cl_git_pass(git_tree_lookup(&tree, repo, &tid2));
	entry = git_tree_entry_byname(tree, "old_mode.txt");
	cl_assert(entry != NULL);
	cl_assert_equal_i(
		GIT_FILEMODE_BLOB,
		git_tree_entry_filemode(entry));

	git_tree_free(tree);
}

void test_object_tree_attributes__normalize_600(void)
{
	git_oid id;
	git_tree *tree;
	const git_tree_entry *entry;

	git_oid_from_string(&id, "0810fb7818088ff5ac41ee49199b51473b1bd6c7", GIT_OID_SHA1);
	cl_git_pass(git_tree_lookup(&tree, repo, &id));

	entry = git_tree_entry_byname(tree, "ListaTeste.xml");
	cl_assert_equal_i(git_tree_entry_filemode(entry), GIT_FILEMODE_BLOB);
	cl_assert_equal_i(git_tree_entry_filemode_raw(entry), 0100600);

	git_tree_free(tree);
}
