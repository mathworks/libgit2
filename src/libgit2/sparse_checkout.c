/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "sparse_checkout.h"
#include "hashmap_str.h"
#include "parse.h"

typedef struct {
	char *path;
} git_sparse_checkout_map_entry;

GIT_HASHMAP_STR_SETUP(
        git_sparse_checkout_parentmap,
        git_sparse_checkout_map_entry *);

GIT_HASHMAP_STR_SETUP(
        git_sparse_checkout_recursivemap,
        git_sparse_checkout_map_entry *);

typedef struct {
	git_repository *repo;
	int ignore_case;

	int full_cone;
	git_sparse_checkout_parentmap parentmap;
	git_sparse_checkout_recursivemap recursivemap;
} git_sparse_checkout;

static int git_sparse_checkout__add_parentmap_entry(
        git_sparse_checkout *sparse_checkout,
        const char *key)
{
	int error = 0;
	git_sparse_checkout_map_entry *entry;

	entry = git__calloc(1, sizeof(*entry));
	GIT_ERROR_CHECK_ALLOC(entry);
	entry->path = git__strdup(key);

	git_sparse_checkout_parentmap_put(
	        &sparse_checkout->parentmap, entry->path, entry);

	printf("Added to parent map: %s\n", key);

	return error;
}

static int git_sparse_checkout__remove_parentmap_entry(
        git_sparse_checkout *sparse_checkout,
        const char *key)
{
	int error = 0;
	git_sparse_checkout_map_entry *entry;

	if (git_sparse_checkout_parentmap_get(
	            &entry, &sparse_checkout->parentmap, key) != 0)
		return GIT_ENOTFOUND;

	if ((error = git_sparse_checkout_parentmap_remove(
	             &sparse_checkout->parentmap, key)) < 0)
		return error;

	printf("Remove from parent map: %s\n", key);

	git__free(entry->path);
	git__free(entry);
	return error;
}

static int git_sparse_checkout__add_recursivemap_entry(
        git_sparse_checkout *sparse_checkout,
        const char *key)
{
	int error = 0;
	git_sparse_checkout_map_entry *entry;

	entry = git__calloc(1, sizeof(*entry));
	GIT_ERROR_CHECK_ALLOC(entry);
	entry->path = git__strdup(key);

	git_sparse_checkout_recursivemap_put(
	        &sparse_checkout->recursivemap, entry->path, entry);

	printf("Added to recursive map: %s\n", key);

	return error;
}

static int git_sparse_checkout__remove_recursivemap_entry(
        git_sparse_checkout *sparse_checkout,
        const char *key)
{
	int error = 0;
	git_sparse_checkout_map_entry *entry;

	if (git_sparse_checkout_recursivemap_get(
	            &entry, &sparse_checkout->recursivemap, key) != 0)
		return GIT_ENOTFOUND;

	if ((error = git_sparse_checkout_recursivemap_remove(
	             &sparse_checkout->recursivemap, key)) < 0)
		return error;

	printf("Remove from recursive map: %s\n", key);

	git__free(entry->path);
	git__free(entry);
	return error;
}

static int git_sparse_checkout__init(
        git_sparse_checkout *sparse_checkout,
        git_repository *repo)
{
	int error = 0;

	memset(sparse_checkout, 0, sizeof(*sparse_checkout));

	sparse_checkout->repo = repo;

	/* Need to check ignore case */
	error = git_repository__configmap_lookup(
	        &sparse_checkout->ignore_case, repo, GIT_CONFIGMAP_IGNORECASE);

	return error;
}

static void git_sparse_checkout__dispose(git_sparse_checkout *sparse_checkout)
{
	git_sparse_checkout_map_entry *e;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;

	while (git_sparse_checkout_parentmap_iterate(
	               &iter, NULL, &e, &sparse_checkout->parentmap) == 0)
		git__free(e);
	git_sparse_checkout_parentmap_dispose(&sparse_checkout->parentmap);

	while (git_sparse_checkout_recursivemap_iterate(
	               &iter, NULL, &e, &sparse_checkout->recursivemap) == 0)
		git__free(e);
	git_sparse_checkout_recursivemap_dispose(
	        &sparse_checkout->recursivemap);
}

static int parse_sparse_checkout_buffer(
        git_sparse_checkout *sparse_checkout,
        const char *buf,
        size_t len)
{
	int error = 0;
	const char *scan = buf;
	git_pool pool = GIT_POOL_INIT;
	git_attr_fnmatch *match = NULL;
	git_str truncated_match = GIT_STR_INIT;

	if (git_pool_init(&pool, 1) < 0)
		goto cleanup;

	// printf("Buffer:\n%.*s\n", (int)len, buf);

	while (*scan) {
		match = git__calloc(1, sizeof(git_attr_fnmatch));
		if (!match)
			return -1;

		match->flags = GIT_ATTR_FNMATCH_ALLOWSPACE |
		               GIT_ATTR_FNMATCH_ALLOWNEG;

		/*
		 * context not needed as it's always the repository root
		 */
		if ((error = git_attr_fnmatch__parse(
		             match, &pool, NULL, &scan)) < 0) {
			if (error != GIT_ENOTFOUND) {
				goto cleanup;
			}
			error = 0;
			continue;
		}

		scan = git__next_line(scan);

		printf("%.*s ", (int)match->length, match->pattern);
		printf("Negated = %d, ",
		       (match->flags & GIT_ATTR_FNMATCH_NEGATIVE) != 0);
		printf("Directory = %d, ",
		       (match->flags & GIT_ATTR_FNMATCH_DIRECTORY) != 0);
		printf("Full Path = %d, ",
		       (match->flags & GIT_ATTR_FNMATCH_FULLPATH) != 0);
		printf("Has Wild = %d\n",
		       (match->flags & GIT_ATTR_FNMATCH_HASWILD) != 0);

		if (git__strncmp("*", match->pattern, match->length) == 0 &&
		    (match->flags & GIT_ATTR_FNMATCH_NEGATIVE) == 0 &&
		    (match->flags & GIT_ATTR_FNMATCH_DIRECTORY) == 0) {
			sparse_checkout->full_cone = 1;
			git__free(match);
			match = NULL;
			continue;
		}

		if (git__strncmp("*", match->pattern, match->length) == 0 &&
		    (match->flags & GIT_ATTR_FNMATCH_NEGATIVE) != 0 &&
		    (match->flags & GIT_ATTR_FNMATCH_DIRECTORY) != 0) {
			sparse_checkout->full_cone = 0;
			git__free(match);
			match = NULL;
			continue;
		}

		if (((match->flags & GIT_ATTR_FNMATCH_DIRECTORY) == 0 ||
		     (match->flags & GIT_ATTR_FNMATCH_FULLPATH) == 0) &&
		    git__strncmp("*", match->pattern, match->length) != 0) {
			error = GIT_EINVALID;
			git_error_set(
			        GIT_ERROR_INVALID,
			        "Invalid cone-mode pattern: %.*s",
			        (int)match->length, match->pattern);
			goto cleanup;
		}

		/* Negative matches must come after the identical non-negated
		 * match. It must exist in the recursive hash map. Suffix must
		 * equal "\*".
		 */
		if ((match->flags & GIT_ATTR_FNMATCH_NEGATIVE) != 0) {
			if (match->length < 3 &&
			    git__suffixcmp(match->pattern, "/*") != 0) {
				error = GIT_EINVALID;
				git_error_set(
				        GIT_ERROR_INVALID,
				        "Invalid cone-mode pattern: %.*s",
				        (int)match->length, match->pattern);
				goto cleanup;
			}

			git_str_sets(&truncated_match, match->pattern);
			git_str_shorten(&truncated_match, 2);

			// If in recursive hash map, remove and add to parent
			// hash map.

			if (!git_sparse_checkout_recursivemap_contains(
			            &sparse_checkout->recursivemap,
			            truncated_match.ptr)) {
				error = GIT_EINVALID;
				git_error_set(
				        GIT_ERROR_INVALID,
				        "Invalid cone-mode pattern: %.*s",
				        (int)match->length, match->pattern);
				goto cleanup;
			}

			git_sparse_checkout__remove_recursivemap_entry(
			        sparse_checkout, truncated_match.ptr);
			git_sparse_checkout__add_parentmap_entry(
			        sparse_checkout, truncated_match.ptr);
		} else {
			// Add to recursive hash map
			git_sparse_checkout__add_recursivemap_entry(
			        sparse_checkout, match->pattern);
		}

		git__free(match);
		match = NULL;
	}

cleanup:
	git_str_dispose(&truncated_match);
	if (match)
		git__free(match);
	return error;
}

static int read_sparse_checkout_file(
        git_sparse_checkout *sparse_checkout,
        git_repository *repo)
{
	int error = 0;
	git_str contents = GIT_STR_INIT;
	git_str info_path = GIT_STR_INIT;
	git_str sparse_checkout_file_path = GIT_STR_INIT;

	if ((error = git_repository__item_path(
	             &info_path, repo, GIT_REPOSITORY_ITEM_INFO)) < 0 ||
	    (error = git_str_joinpath(
	             &sparse_checkout_file_path, info_path.ptr,
	             GIT_SPARSE_CHECKOUT_FILE)) < 0 ||
	    (error = git_futils_readbuffer(
	             &contents, sparse_checkout_file_path.ptr)) < 0 ||
	    (error = parse_sparse_checkout_buffer(
	             sparse_checkout, contents.ptr, contents.size)) < 0) {
		goto cleanup;
	}

cleanup:
	git_str_dispose(&contents);
	git_str_dispose(&info_path);
	git_str_dispose(&sparse_checkout_file_path);
	return error;
}

static int make_dummy_file_from_directory(git_str *path)
{
	if (path->ptr[path->size - 1] == '/') {
		return git_str_putc(path, '-');
	}
	return 0;
}

static int path_is_tracked(
        int *is_tracked,
        git_sparse_checkout *sparse_checkout,
        const char *pathname)
{
	int error = 0;
	*is_tracked = 0;
	git_str path = GIT_STR_INIT;
	git_str dirname = GIT_STR_INIT;

	if (sparse_checkout->full_cone) {
		*is_tracked = 1;
		goto cleanup;
	}

	// pathname could be directory or file
	// If directory, append a random filename to the folder
	// This is what Git does...
	if ((error = git_str_sets(&path, pathname)) < 0 ||
	    (error = make_dummy_file_from_directory(&path)) < 0 ||
	    (error = git_fs_path_dirname_r(&dirname, path.ptr)) < 0) {
		goto cleanup;
	}
	error = 0;
	printf("dirname: %s\n", dirname.ptr);

	if (dirname.size == 1 && dirname.ptr[0] == '.') {
		*is_tracked = 1;
		goto cleanup;
	}

	if (git_sparse_checkout_recursivemap_contains(
	            &sparse_checkout->recursivemap, path.ptr) ||
	    git_sparse_checkout_parentmap_contains(
	            &sparse_checkout->parentmap, dirname.ptr)) {
		*is_tracked = 1;
		goto cleanup;
	}

	while (dirname.size != 1 && dirname.ptr[0] != '.') {
		if (git_sparse_checkout_recursivemap_contains(
		            &sparse_checkout->recursivemap, dirname.ptr)) {
			*is_tracked = 1;
			goto cleanup;
		}

		if ((error = git_fs_path_dirname_r(&dirname, dirname.ptr)) <
		    0) {
			goto cleanup;
		}
		error = 0;
		printf("dirname: %s\n", dirname.ptr);
	}

cleanup:
	git_str_dispose(&dirname);
	git_str_dispose(&path);
	return error;
}

int git_sparse_checkout__path_is_tracked(
        int *is_tracked,
        git_repository *repo,
        const char *pathname)
{
	/*
	 * git: init_sparse_checkout_patterns -> get_sparse_checkout_patterns
	 * update_working_directory(struct pattern_list *pl)
	 */
	int error = 0;
	int sparse_checkout_enabled = 0;
	int sparse_checkout_cone_enabled = 0;
	int ignore_case = 0;
	git_dir_flag dir_flag = GIT_DIR_FLAG_UNKNOWN;
	git_sparse_checkout sparse_checkout;

	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(is_tracked);
	GIT_ASSERT_ARG(pathname);

	if ((error = git_repository__configmap_lookup(
	             &sparse_checkout_enabled, repo,
	             GIT_CONFIGMAP_SPARSECHECKOUT)) < 0 ||
	    !sparse_checkout_enabled) {
		*is_tracked = 1;
		return error;
	}

	if ((error = git_repository__configmap_lookup(
	             &sparse_checkout_cone_enabled, repo,
	             GIT_CONFIGMAP_SPARSECHECKOUTCONE)) < 0) {
		*is_tracked = 1;
		return error;
	}

	if (!sparse_checkout_cone_enabled) {
		/*
		 * We only support cone mode as non-cone mode is deprecated in
		 * Git. Should we error? What's the plan for Git?
		 */
		*is_tracked = 1;
		git_error_set(
		        GIT_ERROR_CONFIG,
		        "non-cone mode for sparse checkout is not supported");
		error = GIT_ENOTSUPPORTED;
		return error;
	}

	/*
	 * https://git-scm.com/docs/git-sparse-checkout/2.42.0
	 * Unless core.sparseCheckoutCone is explicitly set to false, Git will
	 * parse the sparse-checkout file expecting patterns of these types. Git
	 * will warn if the patterns do not match. If the patterns do match the
	 * expected format, then Git will use faster hash-based algorithms to
	 * compute inclusion in the sparse-checkout. If they do not match, git
	 * will behave as though core.sparseCheckoutCone was false, regardless
	 * of its setting.
	 */

	/* In cone mode, .git/info/sparse-checkout file specifies directories to
	 * include.
	 * Parent pattern (include all files under "dir" but nothing below
	 * that): /dir/
	 *   !/dir/*\/
	 * Recursive pattern (include everything in this folder):
	 *   /dir/subdir/
	 */

	/* open and parse the file for the patterns, populating the parent and
	 * recursive hash maps */
	/* use attr_cache so gets cached on repository? Only real benefit
	 * appears to be reparsing when the file is updated. */

	printf("\nCheck path is tracked: %s\n", pathname);

	if ((error = git_sparse_checkout__init(&sparse_checkout, repo)) < 0 ||
	    (error = read_sparse_checkout_file(&sparse_checkout, repo)) < 0 ||
	    (error = path_is_tracked(is_tracked, &sparse_checkout, pathname)) <
	            0)
		goto cleanup;

cleanup:
	git_sparse_checkout__dispose(&sparse_checkout);
	return error;
}
