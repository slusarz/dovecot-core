#ifndef MASTER_SETTINGS_H
#define MASTER_SETTINGS_H

#include "service-settings.h"

struct master_settings {
	pool_t pool;
	const char *base_dir;
	const char *state_dir;
	const char *libexec_dir;
	const char *instance_name;
	ARRAY_TYPE(const_string) protocols;
	ARRAY_TYPE(const_string) listen;
	const char *ssl;
	const char *default_internal_user;
	const char *default_internal_group;
	const char *default_login_user;
	unsigned int default_process_limit;
	unsigned int default_client_limit;
	unsigned int default_idle_kill_interval;
	uoff_t default_vsz_limit;

	bool version_ignore;

	unsigned int first_valid_uid, last_valid_uid;
	unsigned int first_valid_gid, last_valid_gid;

	ARRAY_TYPE(const_string) services;

	ARRAY_TYPE(service_settings) parsed_services;

	/* message snippet settings */
	const char *message_snippet_driver;
	const char *message_snippet_ollama_url;
	const char *message_snippet_ollama_model;
	const char *message_snippet_ollama_prompt;
	uoff_t message_snippet_ollama_max_input_size;
	unsigned int message_snippet_ollama_timeout;
};

extern const struct setting_parser_info master_setting_parser_info;

void master_settings_do_fixes(const struct master_settings *set);

#endif
