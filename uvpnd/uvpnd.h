#ifndef _UGLYVPND_H_
#define _UGLYVPND_H_

#define UVPN_VER_MAJOR 0
#define UVPN_VER_MINOR 1


#define SERVER_MODE    1
#define CLIENT_MODE    0

#define UVPN_DEFAULT_CFG_PATH "./uvpn.conf"
#define UVPN_DEFAULT_PID_PATH "/var/run/uvpn_pidfile"
#define UVPN_DEFAULT_SERVER_PORT 6000 

#define TUNDEV_SIZE 8
#define IPBUF_SIZE  16

struct uvpn_addr{
	char* name;
	char* ip;
	uint16_t port;
};

struct srv_opts{
    char* tundev;
    char* srv_ip; /* server virtual addr */
};

struct cli_opts{
	int authmethod;  /* supported authenticate method   */
    uint32_t userid;
	char* passwd;
    char* tundev;
    uint16_t vport;
};

struct uvpn_opts {
   int  srv;         /* 0-client, 1-server  */
   char* cfg_file;   /* configure file path   */
   int  daemonize;
   int  debug;
  
   struct uvpn_addr bind_addr;  /* server listen on this */
   union spec{
	   struct srv_opts srv_opt;
	   struct cli_opts cli_opt;
   }spec;
};

#endif