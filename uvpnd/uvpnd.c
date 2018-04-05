#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <syslog.h>
//#include <sys/mman.h>

#include "../util/log.h"
#include "pub.h"
#include "user.h"
#include "uvpnd.h"
#include "client.h"
#include "server.h"

extern int optind,opterr,optopt;
extern char *optarg;

struct uvpn_opts uvpn_opt;


static void usage(void)
{
    printf("ugly VPN ver %d.%d\n", UVPN_VER_MAJOR, UVPN_VER_MINOR);
    printf("Usage: \n");
    printf("  Server:\n");
    printf("\tuvpnd <-s> [-f cfg_file] [-l local address] [-p port] [-g]\n");
    printf("  Client:\n");
    printf("\tuvpnd [-f cfg_file] <server address>\n"); 
}

static int check_cmd_opt(void)
{
    FILE* fp = NULL;

    fp = fopen(uvpn_opt.cfg_file, "r");
    if (NULL == fp)
    {
        return -1;
    }
    fclose(fp);
 
    if (uvpn_opt.srv == CLIENT_MODE)
    {
        if(!uvpn_opt.bind_addr.ip){
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[], char *env[])
{
    int opt;
    pid_t pid;
    FILE* fp = NULL;

    openlog("uvpn", LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_DAEMON);
    
    /* default setting */
    uvpn_opt.srv = CLIENT_MODE;
    uvpn_opt.cfg_file = UVPN_DEFAULT_CFG_PATH;

    uvpn_opt.debug    = 0;
    uvpn_opt.daemonize= 1;
    uvpn_opt.bind_addr.name = "eth0";
    uvpn_opt.bind_addr.ip   = NULL;
    uvpn_opt.bind_addr.port = UVPN_DEFAULT_SERVER_PORT;
	
    /* startup from command line */
    while((opt=getopt(argc,argv,"sf:p:l:g")) != EOF ){
    switch(opt){
        case 's':
            uvpn_opt.srv = SERVER_MODE;
            break;
        case 'l':
            uvpn_opt.bind_addr.ip = strdup(optarg);
            break;
        case 'p':
            uvpn_opt.bind_addr.port = atoi(optarg);
            break;
        case 'f':
            uvpn_opt.cfg_file = strdup(optarg);
            break;
        case 'g':
            uvpn_opt.debug = 1;
            break;
        default:
            usage();
            exit(1);
        }   
    }  

    if (check_cmd_opt() < 0){
        usage();
        exit(1);
    }

    if(uvpn_opt.daemonize){
        pid = fork();
        if (pid > 0){
            exit(0);
        }
        else if(pid == 0){
            int fd = open("/dev/null", O_RDWR);
            close(0); dup(fd);
            close(1); dup(fd);
            close(2); dup(fd);
            close(fd);

            setsid();
            chdir("/");
        }
        else{

        }
    }

    //load_cfg_file();
    /* read config file */
	
    if (uvpn_opt.srv == SERVER_MODE){
        
        uvpn_opt.spec.srv_opt.tundev = strdup("tun0");
        uvpn_opt.spec.srv_opt.srv_ip = strdup("10.100.0.102");
        
        uvpn_server(&uvpn_opt.spec.srv_opt);
    }else{
    	uvpn_syslog(LOG_INFO, "ugly-vpnd client open !");
		uvpn_opt.spec.cli_opt.authmethod = AUTH_METHOD_PASSWORD;
        uvpn_opt.spec.cli_opt.userid = 123;
        uvpn_opt.spec.cli_opt.passwd = "test";
        uvpn_opt.spec.cli_opt.tundev = "tun0";
        uvpn_opt.spec.cli_opt.vport  = 6789;
		// uvpn_client(&user1);
		uvpn_client(&uvpn_opt.spec.cli_opt);
    }

    uvpn_syslog(LOG_INFO, "ugly-vpn close!");

    return 0;
}
