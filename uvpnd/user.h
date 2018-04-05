#ifndef _USER_H_
#define _USER_H_

#define PASSWORD_SIZE 16

#include "pub.h"

struct user{
	uint32_t userid;     
	uint8_t  sp_authmethod;  /*  supported authenticated method */
	uint8_t  ef_authmethod;  /*  effictive authenticated method */
	uint8_t  passwd[PASSWORD_MAXSIZE];         
	uint8_t  chanresp[CHALLENGE_RESPONSE_SIZE];
};

#endif
