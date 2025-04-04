/******************************************************************************** 

  Copyright (C) 2017-2018 Intel Corporation 
  Lantiq Beteiligungs-GmbH & Co. KG 
  Lilienthalstrasse 15, 85579 Neubiberg, Germany  

  For licensing information, see the file 'LICENSE' in the root folder of 
  this software module. 

********************************************************************************/ 

  

/*========================Includes============================*/ 

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include <ulogging.h>
#include <ltq_api_include.h>

/*========================Defines=============================*/ 




/*====================Implementation==========================*/ 

/* 
 ** =============================================================================
 **   Function Name    :scapi_spawn
 **
 **   Description      :Forks and execs a child process. Whether parent will block
 **						Or unblocks till child exits depends on the nBlockingFlag option
 **			
 **
 **   Parameters       :pcBuf[IN] --> exec program for child along with args
 **						nBlockingFlag[IN] --> SCAPI_BLOCK/SCAPI_DONTBLOCK
 **						pnChildExitStatus[OUT] --> status
 **						NOTE: If int pointed by pnChildExitStatus is greater than 128, That it possibly means
 **						the executed process exited due to an unhandled signal. It is expected that programs
 **						spawned by this API doesn't do exit() with a value greater than or equal to 128
 **
 **
 **   Return Value     :Success --> EXIT_SUCCESS
 **						Failure --> Different -ve error values 
 ** 
 **   Notes            : Only in blocking mode the API will wait for the child to exit and collects it's 
 **						exit info.(Its exit value or signal due to which it exited).
 **		
 **			--USAGE INSTRUCTIONS--
 **			1. Spawn daemons only in non-blocking mode.
 **			2. Spawn utilities only in blocking mode. Every utility which is executed in non-blocking 
 **				it is assumed that parent handles the SIGCHLD signal and does wait() or waitpid() in that
 **				signal handler. If not, then the child will stay as a zombie until parent exits
 **
 ** ============================================================================
 */
int scapi_spawn(char *pcBuf, int nBlockingFlag, int* pnChildExitStatus)
{
	int nStatus = 0;
	int nRet = 0;
	pid_t pid = -1;
	char sLDLibPath[255]={0};
	char *envp[1];

	if (sprintf_s(sLDLibPath, sizeof(sLDLibPath), "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:%s/usr/lib:%s/lib", VENDOR_PATH, VENDOR_PATH) <= 0) 
		return -1;

	envp[0] = sLDLibPath;


	if(pnChildExitStatus == NULL || pcBuf == NULL)
	{
		nRet = -EINVAL;
		LOGF_LOG_ERROR("ERROR = %d\n", nRet);
		goto returnHandler;
	}
	*pnChildExitStatus = 0;
	pid = fork();
	switch(pid)
	{
		case -1:
			nRet = -errno;                           //can't fork
			LOGF_LOG_ERROR( "ERROR = %d -> %s, PROCESS = %s\n", nRet, strerror(-nRet), scapi_get_process_name(getpid()));
			goto returnHandler;
			break;
		case 0: //child
			/* Doesn't return on success. Text, stack, heap, data segments of the forked process over written. use vfork() instead??*/
			nRet = execl("/bin/sh","sh","-c", pcBuf, envp,NULL);
			if(-1 == nRet) {
				nRet = -errno;
				LOGF_LOG_ERROR("ERROR = %d\n", nRet);
				goto returnHandler;
			}
			break;
		default: //parent
			if(nBlockingFlag == SCAPI_BLOCK) //block parent till child exits
			{
				/*After this call 'nStatus' is an encoded exit value. WIF macros will extract how it exited*/
				if( waitpid(pid, &nStatus, 0) < 0 ) 
				{
					nRet = -errno;
					*pnChildExitStatus = errno;
					LOGF_LOG_ERROR("ERROR = %d\n", nRet);
				}
				//use only exit with +ve values in child as WEXITSTATUS() macro will only consider LSB 8 bits
				if(WIFEXITED(nStatus))
				{
					nRet = EXIT_SUCCESS;
					*pnChildExitStatus = WEXITSTATUS(nStatus);
				}
				goto returnHandler;
			}
			break;
	}
returnHandler:
	return nRet;
}

