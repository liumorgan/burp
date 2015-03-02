/* Experiment to start the sanitising of conf.c, so that we can eventually do
   things like dumping the current configuration. */

enum burp_mode
{
	MODE_UNSET=0,
	MODE_SERVER,
	MODE_CLIENT
};

enum protocol
{
	PROTO_AUTO=0,
	PROTO_1,
	PROTO_2
};

enum recovery_method
{
	RECOVERY_METHOD_DELETE=0,
	RECOVERY_METHOD_RESUME,
	RECOVERY_METHOD_USE
};

enum conf_type
{
	CT_STRING=0,
	CT_UINT,
	CT_FLOAT,
	CT_MODE_T,
	CT_SSIZE_T,
	CT_E_BURP_MODE,
	CT_E_PROTOCOL,
	CT_E_RECOVERY_METHOD,
	CT_STRLIST,
};

struct conf
{
        enum conf_type conf_type;
        const char *field;
	union
	{
		char *s;
		float f;
		enum burp_mode burp_mode;
		enum recovery_method recovery_method;
		enum protocol protocol;
		mode_t mode;
		ssize_t ssizet;
		unsigned int i;
		struct strlist *sl;
		struct cntr *cntr;
	} data;
	uint8_t flags;
};

enum conf_opt
{
	OPT_CONFFILE=0,
	OPT_MODE,
	OPT_LOCKFILE,
	OPT_SYSLOG,
	OPT_STDOUT,
	OPT_PROGRESS_COUNTER,
	OPT_SSL_CERT_CA,
	OPT_SSL_CERT,
	OPT_SSL_KEY,
	OPT_SSL_KEY_PASSWORD,
	OPT_SSL_PEER_CN,
	OPT_SSL_CIPHERS,
	OPT_SSL_COMPRESSION,
	OPT_USER,
	OPT_GROUP,
	OPT_RATELIMIT,
	OPT_NETWORK_TIMEOUT,
	OPT_CLIENT_IS_WINDOWS,
	OPT_PEER_VERSION,
	OPT_PROTOCOL,

	// Server options.
	OPT_ADDRESS,
	OPT_PORT,
	OPT_STATUS_ADDRESS,
	OPT_STATUS_PORT,
	OPT_DIRECTORY,
	OPT_TIMESTAMP_FORMAT,
	OPT_CLIENTCONFDIR,
	OPT_SSL_DHFILE,
	OPT_MAX_CHILDREN,
	OPT_MAX_STATUS_CHILDREN,
	OPT_CLIENT_LOCKDIR,
	OPT_UMASK,
	OPT_MAX_HARDLINKS,
	OPT_MAX_STORAGE_SUBDIRS,
	OPT_FORK,
	OPT_DAEMON,
	OPT_DIRECTORY_TREE,
	OPT_CA_CONF,
	OPT_CA_NAME,
	OPT_CA_SERVER_NAME,
	OPT_CA_BURP_CA,
	OPT_PASSWORD_CHECK,
	OPT_MANUAL_DELETE,
	OPT_MONITOR_LOGFILE, // An ncurses client option, from command line.
	OPT_MONITOR_BROWSE_CACHE,

	// Client options.
	OPT_CNAME, // set on the server when client connects
	OPT_PASSWORD, // also a clientconfdir option
	OPT_PASSWD, // also a clientconfdir option
	OPT_SERVER,
	OPT_ENCRYPTION_PASSWORD,
	OPT_AUTOUPGRADE_OS,
	OPT_AUTOUPGRADE_DIR, // also a server option
	OPT_CA_CSR_DIR,
	OPT_RANDOMISE,

	// This block of client stuff is all to do with what files to backup.
	OPT_STARTDIR,
	OPT_INCEXCDIR,
	OPT_FSCHGDIR,
	OPT_NOBACKUP,
	OPT_INCEXT, // include extensions
	OPT_EXCEXT, // exclude extensions
	OPT_INCREG, // include (regular expression)
	OPT_EXCREG, // exclude (regular expression)
	OPT_EXCFS, // exclude filesystems
	OPT_EXCOM, // exclude from compression
	OPT_INCGLOB, // include (glob expression)
	OPT_CROSS_ALL_FILESYSTEMS,
	OPT_READ_ALL_FIFOS,
	OPT_FIFOS,
	OPT_READ_ALL_BLOCKDEVS,
	OPT_BLOCKDEVS,
	OPT_MIN_FILE_SIZE,
	OPT_MAX_FILE_SIZE,
	OPT_SPLIT_VSS,
	OPT_STRIP_VSS,
	OPT_VSS_DRIVES,
	OPT_ATIME,
	// These are to do with restore.
	OPT_OVERWRITE,
	OPT_STRIP,
	OPT_BACKUP,
	OPT_BACKUP2, // For diffs.
	OPT_RESTOREPREFIX,
	OPT_REGEX,
	OPT_RESTORE_SPOOL,
	// To do with listing.
	OPT_BROWSEFILE,
	OPT_BROWSEDIR,

	// Backup scripts.
	OPT_B_SCRIPT_PRE,
	OPT_B_SCRIPT_PRE_ARG,
	OPT_B_SCRIPT_POST,
	OPT_B_SCRIPT_POST_ARG,
	OPT_B_SCRIPT_POST_RUN_ON_FAIL,
	OPT_R_SCRIPT_PRE,
	OPT_R_SCRIPT_PRE_ARG,
	OPT_R_SCRIPT_POST,
	OPT_R_SCRIPT_POST_ARG,
	OPT_R_SCRIPT_POST_RUN_ON_FAIL,

	// Server scripts.
	OPT_S_SCRIPT_PRE,
	OPT_S_SCRIPT_PRE_ARG,
	OPT_S_SCRIPT_PRE_NOTIFY,
	OPT_S_SCRIPT_POST,
	OPT_S_SCRIPT_POST_ARG,
	OPT_S_SCRIPT_POST_RUN_ON_FAIL,
	OPT_S_SCRIPT_POST_NOTIFY,

	// Use these when you want to give the same args to both post and pre
	// scripts.
	OPT_B_SCRIPT,
	OPT_B_SCRIPT_ARG,
	OPT_R_SCRIPT,
	OPT_R_SCRIPT_ARG,

	OPT_S_SCRIPT,
	OPT_S_SCRIPT_ARG,
	OPT_S_SCRIPT_NOTIFY,

	// Client options on the server.
	// They can be set globally in the server config, or for each client.
	OPT_HARDLINKED_ARCHIVE,

	OPT_KEEP,

	OPT_WORKING_DIR_RECOVERY_METHOD,
	OPT_LIBRSYNC,

	OPT_COMPRESSION,
	OPT_VERSION_WARN,
	OPT_PATH_LENGTH_WARN,
	OPT_HARD_QUOTA,
	OPT_SOFT_QUOTA,

	OPT_TIMER_SCRIPT,
	OPT_TIMER_ARG,

	// Notify scripts
	OPT_N_SUCCESS_SCRIPT,
	OPT_N_SUCCESS_ARG,
	OPT_N_SUCCESS_WARNINGS_ONLY,
	OPT_N_SUCCESS_CHANGES_ONLY,

	OPT_N_FAILURE_SCRIPT,
	OPT_N_FAILURE_ARG,
	// List of clients that are permitted to restore the files from our client.
	OPT_RESTORE_CLIENTS,

	OPT_DEDUP_GROUP,

	OPT_CLIENT_CAN_DELETE,
	OPT_CLIENT_CAN_DIFF,
	OPT_CLIENT_CAN_FORCE_BACKUP,
	OPT_CLIENT_CAN_LIST,
	OPT_CLIENT_CAN_RESTORE,
	OPT_CLIENT_CAN_VERIFY,
	OPT_SERVER_CAN_RESTORE,

	// Set to 1 on both client and server when the server is able to send
	// counters on resume/verify/restore.
	OPT_SEND_CLIENT_CNTR,

	// Set on the server to the restore client name (the one that you
	// connected with) when the client has switched to a different set of
	// client backups.
	OPT_RESTORE_CLIENT,
	// Path to the server initiated restore file.
	OPT_RESTORE_PATH,

	// Original client that backed up. Used when doing a server initiated
	// restore to an alternative client,
	OPT_ORIG_CLIENT,

	OPT_CNTR,

	// For testing.
	OPT_BREAKPOINT,

	OPT_MAX
};

extern struct conf *conf_alloc(void);