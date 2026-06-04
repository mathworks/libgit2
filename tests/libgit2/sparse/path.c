#include "clar_libgit2.h"
#include "posix.h"
#include "sparse_checkout.h"
#include "futils.h"

static git_repository *g_repo = NULL;

void test_sparse_path__initialize(void)
{
	g_repo = cl_git_sandbox_init("sparse");
}

void test_sparse_path__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

static void assert_is_tracked_(
        bool expected,
        const char *filepath,
        const char *file,
        const char *func,
        int line)
{
	int is_tracked = 0;

	cl_git_expect(
	        git_sparse_checkout__path_is_tracked(
	                &is_tracked, g_repo, filepath),
	        0, file, func, line);

	clar__assert_equal(
	        file, func, line, "expected != is_tracked", 1, "%d",
	        (int)(expected != 0), (int)(is_tracked != 0));
}

#define assert_is_tracked(expected, filepath) \
	assert_is_tracked_(expected, filepath, __FILE__, __func__, __LINE__)

void test_sparse_path__basic(void)
{
	assert_is_tracked(true, "fileA.txt");
	assert_is_tracked(true, "fileB.txt");
	assert_is_tracked(true, "c");
	assert_is_tracked(true, "c/fileA.txt");
	assert_is_tracked(true, "c/d");
	assert_is_tracked(true, "c/d/fileA.txt");
	assert_is_tracked(true, "c/d/fileB.txt");
	assert_is_tracked(true, "c/d/e/fileB.txt");
	assert_is_tracked(false, "b/fileA.txt");
	assert_is_tracked(false, "b/d/fileA.txt");

	assert_is_tracked(true, "c/");
	assert_is_tracked(true, "c/d/");
	assert_is_tracked(true, "c/d/e/");
	assert_is_tracked(true, "c/d/e/f/");
	assert_is_tracked(true, "c/d/e/f/g/");

	assert_is_tracked(true, "b");
	assert_is_tracked(false, "b/");

	assert_is_tracked(true, "doesnotexist.txt");
	assert_is_tracked(false, "does/not/exist.txt");

	assert_is_tracked(false, "/");
}

void test_sparse_path__error_not_directory(void)
{
	int is_tracked = 0;

	cl_git_rewritefile(
	        "sparse/.git/info/sparse-checkout", "/a/b/c");
	cl_git_fail_with(
	        GIT_EINVALID, git_sparse_checkout__path_is_tracked(
	                              &is_tracked, g_repo, "fileA.txt"));
}

void test_sparse_path__error_space(void)
{
	int is_tracked = 0;

	cl_git_rewritefile("sparse/.git/info/sparse-checkout", " #comment");
	cl_git_fail_with(
	        GIT_EINVALID, git_sparse_checkout__path_is_tracked(
	                              &is_tracked, g_repo, "fileA.txt"));
}
