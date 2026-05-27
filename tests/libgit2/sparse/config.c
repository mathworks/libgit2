#include "clar_libgit2.h"
#include "repository.h"

static git_repository *g_repo = NULL;

void test_sparse_config__initialize(void)
{
	g_repo = cl_git_sandbox_init("sparse");
}

void test_sparse_config__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_sparse_config__cone(void)
{
	git_config *config, *worktree;
	int sparse_checkout = 0;
	int sparse_checkout_cone = 0;

	cl_git_pass(git_repository_config(&config, g_repo));
	cl_git_pass(git_config_open_level(
	        &worktree, config, GIT_CONFIG_LEVEL_WORKTREE));

	cl_git_pass(git_config_get_bool(
	        &sparse_checkout, worktree, "core.sparseCheckout"));
	cl_assert_equal_b(true, sparse_checkout);
	cl_git_pass(git_config_get_bool(
	        &sparse_checkout_cone, worktree, "core.sparseCheckoutCone"));
	cl_assert_equal_b(true, sparse_checkout_cone);

	sparse_checkout = 0;
	cl_git_pass(git_repository__configmap_lookup(
	        &sparse_checkout, g_repo,
	        GIT_CONFIGMAP_SPARSECHECKOUT));
	cl_assert_equal_b(true, sparse_checkout);
	sparse_checkout_cone = 0;
	cl_git_pass(git_repository__configmap_lookup(
	        &sparse_checkout_cone, g_repo,
	        GIT_CONFIGMAP_SPARSECHECKOUTCONE));
	cl_assert_equal_b(true, sparse_checkout_cone);

	git_config_free(worktree);
	git_config_free(config);
}
