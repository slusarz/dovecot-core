#include "lib.h"
#include "array.h"
#include "imap-append-hooks.h"

ARRAY_TYPE(imap_append_hooks) imap_append_hooks;

void imap_append_hooks_add(const struct imap_append_hooks *hooks)
{
	if (!array_is_created(&imap_append_hooks))
		i_array_init(&imap_append_hooks, 4);
	array_push_back(&imap_append_hooks, &hooks);
}

void imap_append_hooks_remove(const struct imap_append_hooks *hooks)
{
	const struct imap_append_hooks *const *p;

	array_foreach(&imap_append_hooks, p) {
		if (*p == hooks) {
			array_delete(&imap_append_hooks,
				     array_foreach_idx(&imap_append_hooks), 1);
			break;
		}
	}
}
