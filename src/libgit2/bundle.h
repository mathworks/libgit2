/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_bundle_h__
#define INCLUDE_bundle_h__

#include "common.h"
#include "vector.h"

typedef struct {
	int version;
	git_oid_t oid_type;
	git_vector prerequisites;
	git_vector refs;
} git_bundle_header;

#define GIT_BUNDLE_HEADER_INIT { 0, GIT_OID_SHA1, GIT_VECTOR_INIT, GIT_VECTOR_INIT }

void git_bundle_header_dispose(git_bundle_header *bundle);

int git_bundle__read_header(git_bundle_header *header, const char *url);

int git_bundle__read_pack(
        git_repository *repo,
        const char *url,
        git_indexer_progress *stats);

int git_bundle__is_bundle(const char *url);

#endif
