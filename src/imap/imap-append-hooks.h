#ifndef IMAP_APPEND_HOOKS_H
#define IMAP_APPEND_HOOKS_H

struct client_command_context;
struct istream;

typedef int imap_append_pre_save_hook_t(struct client_command_context *cmd,
                                        struct istream *input);

struct imap_append_hooks {
	imap_append_pre_save_hook_t *imap_append_pre_save;
};

ARRAY_DEFINE_TYPE(imap_append_hooks, const struct imap_append_hooks *);
extern ARRAY_TYPE(imap_append_hooks) imap_append_hooks;

void imap_append_hooks_add(const struct imap_append_hooks *hooks);
void imap_append_hooks_remove(const struct imap_append_hooks *hooks);

#endif
