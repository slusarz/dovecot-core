#ifndef MAILBOX_LIST_ITER_PRIVATE_H
#define MAILBOX_LIST_ITER_PRIVATE_H

#include "mailbox-list-private.h"
#include "mailbox-list-iter.h"
#include "mailbox-list-delete.h"

static inline bool
mailbox_list_iter_try_delete_noselect(struct mailbox_list_iterate_context *ctx,
				      const struct mailbox_info *info,
				      const char *storage_name)
{
	if ((info->flags & (MAILBOX_NOSELECT|MAILBOX_NOCHILDREN)) ==
	    (MAILBOX_NOSELECT|MAILBOX_NOCHILDREN) &&
	    ctx->list->set.no_noselect) {
		/* Try to rmdir() all \NoSelect mailbox leafs and
		   afterwards their parents. */
		mailbox_list_delete_mailbox_until_root(ctx->list, storage_name);
		return TRUE;
	}
	return FALSE;
}

#endif
