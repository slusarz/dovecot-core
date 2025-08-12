/* Copyright (c) 2024 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "message-snippet-driver.h"
#include "lib-mail.h"

void lib_mail_init(void)
{
	message_snippet_drivers_register_all();
}

void lib_mail_deinit(void)
{
	message_snippet_drivers_unregister_all();
}
