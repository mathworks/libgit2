/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sparse_checkout_h__
#define INCLUDE_sparse_checkout_h__

#include "common.h"

#include "repository.h"

#define GIT_SPARSE_CHECKOUT_FILE "sparse-checkout"

extern int git_sparse_checkout__path_is_tracked(
        int *is_tracked,
        git_repository *repo,
        const char *pathname);

#endif
