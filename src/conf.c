#include "burp.h"
#include "strlist.h"
#include "conf.h"
#include "log.h"
#include "handy.h"

#include <assert.h>

#define CONF_FLAG_CC_OVERRIDE	0x01
#define CONF_FLAG_OVERRIDDEN	0x02
#define CONF_FLAG_INCEXC	0x04

static enum burp_mode str_to_burp_mode(const char *str)
{
	if(!strcmp(str, "server"))
		return BURP_MODE_SERVER;
	else if(!strcmp(str, "client"))
		return BURP_MODE_CLIENT;
	logp("Unknown mode setting: %s\n", str);
	return BURP_MODE_UNSET;
}

static const char *burp_mode_to_str(enum burp_mode bm)
{
	switch(bm)
	{
		case BURP_MODE_UNSET: return "unset";
		case BURP_MODE_SERVER: return "server";
		case BURP_MODE_CLIENT: return "client";
		default: return "unknown";
	}
}

static enum recovery_method str_to_recovery_method(const char *str)
{
	if(!strcmp(str, "delete"))
		return RECOVERY_METHOD_DELETE;
	else if(!strcmp(str, "resume"))
		return RECOVERY_METHOD_RESUME;
	else if(!strcmp(str, "use"))
		return RECOVERY_METHOD_USE;
	logp("Unknown working_dir_recovery_method setting: %s\n", str);
	return RECOVERY_METHOD_UNSET;
}

static const char *recovery_method_to_str(enum recovery_method r)
{
	switch(r)
	{
		case RECOVERY_METHOD_DELETE: return "delete";
		case RECOVERY_METHOD_RESUME: return "resume";
		case RECOVERY_METHOD_USE: return "use";
		default: return "unknown";
	}
}

struct strlist *get_strlist(struct conf *conf)
{
	assert(conf->conf_type==CT_STRLIST);
	return conf->data.sl;
}

char *get_string(struct conf *conf)
{
	assert(conf->conf_type==CT_STRING);
	return conf->data.s;
}

int get_int(struct conf *conf)
{
	assert(conf->conf_type==CT_UINT);
	return conf->data.i;
}

ssize_t get_ssize_t(struct conf *conf)
{
	assert(conf->conf_type==CT_SSIZE_T);
	return conf->data.ssizet;
}

float get_float(struct conf *conf)
{
	assert(conf->conf_type==CT_FLOAT);
	return conf->data.f;
}

mode_t get_mode_t(struct conf *conf)
{
	assert(conf->conf_type==CT_MODE_T);
	return conf->data.mode;
}

enum burp_mode get_e_burp_mode(struct conf *conf)
{
	assert(conf->conf_type==CT_E_BURP_MODE);
	return conf->data.burp_mode;
}

enum protocol get_e_protocol(struct conf *conf)
{
	assert(conf->conf_type==CT_E_PROTOCOL);
	return conf->data.protocol;
}

enum recovery_method get_e_recovery_method(struct conf *conf)
{
	assert(conf->conf_type==CT_E_RECOVERY_METHOD);
	return conf->data.recovery_method;
}

static int set_string(struct conf *conf, const char *s)
{
	assert(conf->conf_type==CT_STRING);
	if(conf->data.s) free(conf->data.s);
	if(s && !(conf->data.s=strdup_w(s, __func__)))
		return -1;
	return 0;
}

static int set_int(struct conf *conf, unsigned int i)
{
	assert(conf->conf_type==CT_UINT);
	conf->data.i=i;
	return 0;
}

static int set_strlist(struct conf *conf, struct strlist *s)
{
	assert(conf->conf_type==CT_STRLIST);
	// FIX THIS
	//if(conf->data.sl) strlists_free(&conf->data.sl);
	conf->data.sl=s;
	return 0;
}

static int set_float(struct conf *conf, float f)
{
	assert(conf->conf_type==CT_FLOAT);
	conf->data.f=f;
	return 0;
}

static int set_e_burp_mode(struct conf *conf, enum burp_mode bm)
{
	assert(conf->conf_type==CT_E_BURP_MODE);
	conf->data.burp_mode=bm;
	return 0;
}

static int set_e_protocol(struct conf *conf, enum protocol p)
{
	assert(conf->conf_type==CT_E_PROTOCOL);
	conf->data.protocol=p;
	return 0;
}

static int set_e_recovery_method(struct conf *conf, enum recovery_method r)
{
	assert(conf->conf_type==CT_E_RECOVERY_METHOD);
	conf->data.recovery_method=r;
	return 0;
}

static int set_mode_t(struct conf *conf, mode_t m)
{
	assert(conf->conf_type==CT_MODE_T);
	conf->data.mode=m;
	return 0;
}

static int set_ssize_t(struct conf *conf, ssize_t s)
{
	assert(conf->conf_type==CT_SSIZE_T);
	conf->data.ssizet=s;
	return 0;
}

static void conf_free_content(struct conf *c)
{
	if(!c) return;
	switch(c->conf_type)
	{
		case CT_STRING:
			if(c->data.s)
			{
				free(c->data.s);
				c->data.s=NULL;
			}
			break;
		case CT_STRLIST:
			// FIX THIS.
			//strlists_free(&c->data.sl);
			break;
		case CT_FLOAT:
		case CT_E_BURP_MODE:
		case CT_E_PROTOCOL:
		case CT_E_RECOVERY_METHOD:
		case CT_UINT:
		case CT_MODE_T:
		case CT_SSIZE_T:
			memset(&c->data, 0, sizeof(c->data));
			break;
	}
}

void confs_free_content(struct conf **confs)
{
	int i=0;
	if(!confs) return;
	for(i=0; i<OPT_MAX; i++) conf_free_content(confs[i]);
}

/* Free only stuff related to includes/excludes.
   This is so that the server can override them all on the client. */
void free_incexcs(struct conf **confs)
{
	int i=0;
	if(!confs) return;
	for(i=0; i<OPT_MAX; i++)
		if(confs[i]->flags && CONF_FLAG_INCEXC)
			conf_free_content(confs[i]);
}

static void sc(struct conf *conf, uint8_t flags,
	enum conf_type conf_type, const char *field)
{
	conf->conf_type=conf_type;
	conf->field=field;
	conf->flags=flags;
	memset(&conf->data, 0, sizeof(conf->data));
}

static int sc_str(struct conf *conf, const char *def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_STRING, field);
	return set_string(conf, def);
}

static int sc_int(struct conf *conf, unsigned int def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_UINT, field);
	return set_int(conf, def);
}

static int sc_lst(struct conf *conf, struct strlist *def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_STRLIST, field);
	return set_strlist(conf, def);
}

static int sc_flt(struct conf *conf, float def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_FLOAT, field);
	return set_float(conf, def);
}

static int sc_ebm(struct conf *conf, enum burp_mode def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_E_BURP_MODE, field);
	return set_e_burp_mode(conf, def);
}

static int sc_epr(struct conf *conf, enum protocol def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_E_PROTOCOL, field);
	return set_e_protocol(conf, def);
}

static int sc_rec(struct conf *conf, enum recovery_method def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_E_RECOVERY_METHOD, field);
	return set_e_recovery_method(conf, def);
}

static int sc_mod(struct conf *conf, mode_t def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_MODE_T, field);
	return set_mode_t(conf, def);
}

static int sc_szt(struct conf *conf, ssize_t def,
	uint8_t flags, const char *field)
{
	sc(conf, flags, CT_SSIZE_T, field);
	return set_ssize_t(conf, def);
}

static int reset_conf(struct conf **c, enum conf_opt o)
{
	// Do this with a switch statement, so that we get compiler warnings
	// if anything is missed.
	switch(o)
	{
	case OPT_BURP_MODE:
	  return sc_ebm(c[o], BURP_MODE_UNSET, 0, "mode");
	case OPT_LOCKFILE:
	  // FIX THIS: synonym: pidfile
	  return sc_str(c[o], 0, 0, "lockfile");
	case OPT_SSL_CERT_CA:
	  return sc_str(c[o], 0, 0, "ssl_cert_ca");
	case OPT_SSL_CERT:
	  return sc_str(c[o], 0, 0, "ssl_cert");
	case OPT_SSL_KEY:
	  return sc_str(c[o], 0, 0, "ssl_key");
	case OPT_SSL_KEY_PASSWORD:
	  // FIX THIS: synonym: ssl_cert_password
	  return sc_str(c[o], 0, 0, "ssl_key_password");
	case OPT_SSL_PEER_CN:
	  return sc_str(c[o], 0, 0, "ssl_peer_cn");
	case OPT_SSL_CIPHERS:
	  return sc_str(c[o], 0, 0, "ssl_ciphers");
	case OPT_SSL_COMPRESSION:
	  return sc_int(c[o], 5, 0, "ssl_compression");
	case OPT_RATELIMIT:
	  return sc_flt(c[o], 0, 0, "ratelimit");
	case OPT_NETWORK_TIMEOUT:
	  return sc_int(c[o], 60*60*2, 0, "network_timeout");
	case OPT_CLIENT_IS_WINDOWS:
	  return sc_int(c[o], 0, 0, "client_is_windows");
	case OPT_PEER_VERSION:
	  return sc_str(c[o], 0, 0, "peer_version");
	case OPT_ADDRESS:
	  return sc_str(c[o], 0, 0, "address");
	case OPT_PORT:
	  return sc_str(c[o], 0, 0, "port");
	case OPT_STATUS_ADDRESS:
	  return sc_str(c[o], 0, 0, "status_address");
	case OPT_STATUS_PORT:
	  return sc_str(c[o], 0, 0, "status_port");
	case OPT_SSL_DHFILE:
	  return sc_str(c[o], 0, 0, "ssl_dhfile");
	case OPT_MAX_CHILDREN:
	  return sc_int(c[o], 5, 0, "max_children");
	case OPT_MAX_STATUS_CHILDREN:
	  return sc_int(c[o], 5, 0, "max_status_children");
	case OPT_CLIENT_LOCKDIR:
	  return sc_str(c[o], 0, 0, "client_lockdir");
	case OPT_UMASK:
	  return sc_mod(c[o], 0022, 0, "umask");
	case OPT_MAX_HARDLINKS:
	  return sc_int(c[o], 10000, 0, "max_hardlinks");
	case OPT_MAX_STORAGE_SUBDIRS:
	  // ext3 maximum number of subdirs is 32000, so leave a little room.
	  return sc_int(c[o], 30000, 0, "max_storage_subdirs");
	case OPT_DAEMON:
	  return sc_int(c[o], 1, 0, "daemon");
	case OPT_CA_CONF:
	  return sc_str(c[o], 0, 0, "ca_conf");
	case OPT_CA_NAME:
	  return sc_str(c[o], 0, 0, "ca_name");
	case OPT_CA_SERVER_NAME:
	  return sc_str(c[o], 0, 0, "ca_server_name");
	case OPT_CA_BURP_CA:
	  return sc_str(c[o], 0, 0, "ca_burp_ca");
	case OPT_MONITOR_LOGFILE:
	  return sc_str(c[o], 0, 0, "monitor_logfile");
	case OPT_CNAME:
	  return sc_str(c[o], 0, 0, "cname");
	case OPT_PASSWORD:
	  return sc_str(c[o], 0, 0, "password");
	case OPT_PASSWD:
	  return sc_str(c[o], 0, 0, "passwd");
	case OPT_SERVER:
	  return sc_str(c[o], 0, 0, "server");
	case OPT_ENCRYPTION_PASSWORD:
	  return sc_str(c[o], 0, 0, "encryption_password");
	case OPT_AUTOUPGRADE_OS:
	  return sc_str(c[o], 0, 0, "autoupgrade_os");
	case OPT_AUTOUPGRADE_DIR:
	  return sc_str(c[o], 0, 0, "autoupgrade_dir");
	case OPT_CA_CSR_DIR:
	  return sc_str(c[o], 0, 0, "ca_csr_dir");
	case OPT_RANDOMISE:
	  return sc_int(c[o], 0, 0, "randomise");
	case OPT_STARTDIR:
	  return sc_lst(c[o], 0, 0, "startdir");
	case OPT_BACKUP:
	  return sc_str(c[o], 0, 0, "backup");
	case OPT_BACKUP2:
	  return sc_str(c[o], 0, 0, "backup2");
	case OPT_RESTOREPREFIX:
	  return sc_str(c[o], 0, 0, "restoreprefix");
	case OPT_RESTORE_SPOOL:
	  return sc_str(c[o], 0, 0, "restore_spool");
	case OPT_BROWSEFILE:
	  return sc_str(c[o], 0, 0, "browsefile");
	case OPT_BROWSEDIR:
	  return sc_str(c[o], 0, 0, "browsedir");
	case OPT_B_SCRIPT_PRE:
	  return sc_str(c[o], 0, 0, "backup_script_pre");
	case OPT_B_SCRIPT_PRE_ARG:
	  return sc_lst(c[o], 0, 0, "backup_script_pre_arg");
	case OPT_B_SCRIPT_POST:
	  return sc_str(c[o], 0, 0, "backup_script_post");
	case OPT_B_SCRIPT_POST_ARG:
	  return sc_lst(c[o], 0, 0, "backup_script_post_arg");
	case OPT_B_SCRIPT_POST_RUN_ON_FAIL:
	  return sc_int(c[o], 0, 0, "backup_script_post_run_on_fail");
	case OPT_R_SCRIPT_PRE:
	  return sc_str(c[o], 0, 0, "restore_script_pre");
	case OPT_R_SCRIPT_PRE_ARG:
	  return sc_lst(c[o], 0, 0, "restore_script_pre_arg");
	case OPT_R_SCRIPT_POST:
	  return sc_str(c[o], 0, 0, "restore_script_post");
	case OPT_R_SCRIPT_POST_ARG:
	  return sc_lst(c[o], 0, 0, "restore_script_post_arg");
	case OPT_R_SCRIPT_POST_RUN_ON_FAIL:
	  return sc_int(c[o], 0, 0, "restore_script_run_on_fail");
	case OPT_B_SCRIPT:
	  return sc_str(c[o], 0, 0, "backup_script");
	case OPT_B_SCRIPT_ARG:
	  return sc_lst(c[o], 0, 0, "backup_script_arg");
	case OPT_R_SCRIPT:
	  return sc_str(c[o], 0, 0, "restore_script");
	case OPT_R_SCRIPT_ARG:
	  return sc_lst(c[o], 0, 0, "restore_script_arg");
	case OPT_SEND_CLIENT_CNTR:
	  return sc_int(c[o], 0, 0, "send_client_cntr");
	case OPT_RESTORE_CLIENT:
	  return sc_str(c[o], 0, 0, "restore_client");
	case OPT_RESTORE_PATH:
	  return sc_str(c[o], 0, 0, "restore_path");
	case OPT_ORIG_CLIENT:
	  return sc_str(c[o], 0, 0, "orig_client");
	case OPT_CNTR:
	  return sc_str(c[o], 0, 0, "cntr");
	case OPT_BREAKPOINT:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "breakpoint");
	case OPT_CONFFILE:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "conffile");
	case OPT_SYSLOG:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "syslog");
	case OPT_STDOUT:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "stdout");
	case OPT_PROGRESS_COUNTER:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "progress_counter");
	case OPT_USER:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "user");
	case OPT_GROUP:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "group");
	case OPT_PROTOCOL:
	  return sc_epr(c[o], PROTO_AUTO,
		CONF_FLAG_CC_OVERRIDE, "protocol");
	case OPT_DIRECTORY:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "directory");
	case OPT_TIMESTAMP_FORMAT:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "timestamp_format");
	case OPT_CLIENTCONFDIR:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "clientconfdir");
	case OPT_FORK:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "fork");
	case OPT_DIRECTORY_TREE:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "directory_tree");
	case OPT_PASSWORD_CHECK:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "password_check");
	case OPT_MANUAL_DELETE:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "manual_delete");
	case OPT_MONITOR_BROWSE_CACHE:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "monitor_browse_cache");
	case OPT_S_SCRIPT_PRE:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_pre");
	case OPT_S_SCRIPT_PRE_ARG:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_pre_arg");
	case OPT_S_SCRIPT_PRE_NOTIFY:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_pre_notify");
	case OPT_S_SCRIPT_POST:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_post");
	case OPT_S_SCRIPT_POST_ARG:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_post_arg");
	case OPT_S_SCRIPT_POST_RUN_ON_FAIL:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_post_run_on_fail");
	case OPT_S_SCRIPT_POST_NOTIFY:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_post_notify");
	case OPT_S_SCRIPT:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script");
	case OPT_S_SCRIPT_ARG:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_arg");
	case OPT_S_SCRIPT_NOTIFY:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "server_script_notify");
	case OPT_HARDLINKED_ARCHIVE:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "hardlinked_archive");
	case OPT_KEEP:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "keep");
	case OPT_LIBRSYNC:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "librsync");
	case OPT_COMPRESSION:
	  return sc_int(c[o], 9,
		CONF_FLAG_CC_OVERRIDE, "compression");
	case OPT_VERSION_WARN:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "version_warn");
	case OPT_PATH_LENGTH_WARN:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "path_length_warn");
	case OPT_HARD_QUOTA:
	  return sc_szt(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "hard_quota");
	case OPT_SOFT_QUOTA:
	  return sc_szt(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "soft_quota");
	case OPT_TIMER_SCRIPT:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "timer_script");
	case OPT_TIMER_ARG:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "timer_arg");
	case OPT_N_SUCCESS_SCRIPT:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "notify_success_script");
	case OPT_N_SUCCESS_ARG:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "notify_success_arg");
	case OPT_N_SUCCESS_WARNINGS_ONLY:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "notify_success_warnings_only");
	case OPT_N_SUCCESS_CHANGES_ONLY:
	  return sc_int(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "notify_success_changes_only");
	case OPT_N_FAILURE_SCRIPT:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "notify_failure_script");
	case OPT_N_FAILURE_ARG:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "notify_failure_arg");
	case OPT_RESTORE_CLIENTS:
	  return sc_lst(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "restore_clients");
	case OPT_DEDUP_GROUP:
	  return sc_str(c[o], 0,
		CONF_FLAG_CC_OVERRIDE, "dedup_group");
	case OPT_CLIENT_CAN_DELETE:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "client_can_delete");
	case OPT_CLIENT_CAN_DIFF:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "client_can_diff");
	case OPT_CLIENT_CAN_FORCE_BACKUP:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "client_can_force_backup");
	case OPT_CLIENT_CAN_LIST:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "client_can_list");
	case OPT_CLIENT_CAN_RESTORE:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "client_can_restore");
	case OPT_CLIENT_CAN_VERIFY:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "client_can_verify");
	case OPT_SERVER_CAN_RESTORE:
	  return sc_int(c[o], 1,
		CONF_FLAG_CC_OVERRIDE, "server_can_restore");
	case OPT_WORKING_DIR_RECOVERY_METHOD:
	  return sc_rec(c[o], RECOVERY_METHOD_DELETE,
		CONF_FLAG_CC_OVERRIDE, "working_dir_recovery_method");
	case OPT_INCEXCDIR:
	  // FIX THIS: Also need "exclude" in the same option.
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "include");
	case OPT_FSCHGDIR:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "cross_filesystem");
	case OPT_NOBACKUP:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "nobackup");
	case OPT_INCEXT:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "include_ext");
	case OPT_EXCEXT:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "exclude_ext");
	case OPT_INCREG:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "include_regex");
	case OPT_EXCREG:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "exclude_regex");
	case OPT_EXCFS:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "exclude_fs");
	case OPT_EXCOM:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "exclude_comp");
	case OPT_INCGLOB:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "include_glob");
	case OPT_CROSS_ALL_FILESYSTEMS:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "cross_all_filesystems");
	case OPT_READ_ALL_FIFOS:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "read_all_fifos");
	case OPT_FIFOS:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "read_fifo");
	case OPT_READ_ALL_BLOCKDEVS:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "read_all_blockdevs");
	case OPT_BLOCKDEVS:
	  return sc_lst(c[o], 0, CONF_FLAG_INCEXC, "read_blockdev");
	case OPT_MIN_FILE_SIZE:
	  return sc_szt(c[o], 0, CONF_FLAG_INCEXC, "min_file_size");
	case OPT_MAX_FILE_SIZE:
	  return sc_szt(c[o], 0, CONF_FLAG_INCEXC, "max_file_size");
	case OPT_SPLIT_VSS:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "split_vss");
	case OPT_STRIP_VSS:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "strip_vss");
	case OPT_VSS_DRIVES:
	  return sc_str(c[o], 0, CONF_FLAG_INCEXC, "vss_drives");
	case OPT_ATIME:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "atime");
	case OPT_OVERWRITE:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "overwrite");
	case OPT_STRIP:
	  return sc_int(c[o], 0, CONF_FLAG_INCEXC, "strip");
	case OPT_REGEX:
	  return sc_str(c[o], 0, CONF_FLAG_INCEXC, "regex");
	case OPT_MAX:
	  return 0;
	// No default, so we get compiler warnings if something was missed.
	}
	return -1;
}

static int set_conf(struct conf *c, const char *field, const char *value)
{
	switch(c->conf_type)
	{
		case CT_STRING:
			if(set_string(c, value)) return 1;
			break;
		case CT_FLOAT:
			if(set_float(c, atof(value))) return 1;
			break;
		case CT_E_BURP_MODE:
		{
			enum burp_mode bm;
			bm=str_to_burp_mode(value);
			if(bm==BURP_MODE_UNSET
			  || set_e_burp_mode(c, bm))
				return 1;
			break;
		}
		case CT_E_RECOVERY_METHOD:
		{
			enum recovery_method rm;
			rm=str_to_recovery_method(value);
			if(rm==RECOVERY_METHOD_UNSET
			  || set_e_recovery_method(c, rm))
				return 1;
			break;
		}
	// FIX THIS
		case CT_UINT:
		case CT_MODE_T:
		case CT_SSIZE_T:
		case CT_E_PROTOCOL:
		case CT_STRLIST:
			break;
	}
	return 0;
}

int conf_set(struct conf **confs, const char *field, const char *value)
{
	int i=0;
	int r=0;
	for(i=0; i<OPT_MAX; i++)
	{
		if(strcmp(confs[i]->field, field)) continue;
		r+=set_conf(confs[i], field, value);
	}
	return r;
}

static char *conf_data_to_str(struct conf *conf)
{
	static char ret[256]="";
	*ret='\0';
	switch(conf->conf_type)
	{
		case CT_STRING:
			snprintf(ret, sizeof(ret), "%s",
				get_string(conf)?get_string(conf):"");
			break;
		case CT_FLOAT:
			snprintf(ret, sizeof(ret), "%g", get_float(conf));
			break;
		case CT_E_BURP_MODE:
			snprintf(ret, sizeof(ret), "%s",
				burp_mode_to_str(get_e_burp_mode(conf)));
			break;
		case CT_E_PROTOCOL:
			snprintf(ret, sizeof(ret), "%d", get_e_protocol(conf));
			break;
		case CT_E_RECOVERY_METHOD:
			snprintf(ret, sizeof(ret), "%s",
				recovery_method_to_str(get_e_recovery_method(conf)));
			break;
		case CT_UINT:
			snprintf(ret, sizeof(ret), "%u", get_int(conf));
			break;
		case CT_STRLIST:
			break;
		case CT_MODE_T:
			snprintf(ret, sizeof(ret), "%o", get_mode_t(conf));
			break;
		case CT_SSIZE_T:
			break;
	}
	return ret;

}

struct conf **confs_alloc(void)
{
	int i=0;
	struct conf **confs=NULL;
	if(!(confs=(struct conf **)
		calloc_w(OPT_MAX, sizeof(struct conf *), __func__)))
			return NULL;
	for(i=0; i<OPT_MAX; i++)
	{
		struct conf *c;
		if(!(c=(struct conf *)
			calloc_w(1, sizeof(struct conf), __func__)))
				return NULL;
		confs[i]=c;
	}
	return confs;
};

void confs_free(struct conf ***confs)
{
	if(!confs || !*confs) return;
	free(*confs);
	*confs=NULL;
}

int confs_init(struct conf **confs)
{
	int i=0;
	for(i=0; i<OPT_MAX; i++)
		if(reset_conf(confs, (enum conf_opt)i))
			return -1;
	return 0;
}

int confs_dump(struct conf **confs)
{
	int i=0;
	for(i=0; i<OPT_MAX; i++)
	{
		printf("%32s: %s\n", confs[i]->field,
			conf_data_to_str(confs[i]));
	}
	return 0;
}
