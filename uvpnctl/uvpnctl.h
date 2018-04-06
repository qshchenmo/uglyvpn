#ifndef _UVPNCTL_H_
#define _UVPNCTL_H_


#define CMD_HELP       0
#define CMD_TERMINATE  1

#define UVPNCTL_UNIX_PATH  "/tmp/uvpnctl_unixsocket_file"
#define UVPND_UNIX_PATH    "/tmp/uvpnd_unixsocket_file"

struct cmd_head{
	int type;
	int len;
	void* payload[0];
};


struct command
{
	char* cmd;
	int argc;
	int type;
	int (*recv)(void);
};

#endif