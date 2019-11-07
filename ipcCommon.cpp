#include "ipcCommon.h"

const char* const SHMEM_NAME = "/sjdavids_SERVER";   // shared memory name & location /dev/shm/sjdavids_SERVER

//
// FIFO
// - task output communication server->client 
// - per client FIFO name:  /tmp/sjdavids_FIFO[clientid]
//
const char* const MYFIFO="/tmp/sjdavids_FIFO";       // FIFO filename & location  
const char* const SEND_INTERRUPT = "\n---sjdavidsINTERRUPTsjdavids---\n";
const char* const READ_INTERRUPT = "---sjdavidsINTERRUPTsjdavids---\n";
const char* const SEND_SHUTDOWN = "\n---sjdavidsSHUTDOWNsjdavids---\n";
const char* const READ_SHUTDOWN = "---sjdavidsSHUTDOWNsjdavids---\n";


int const ERROR = -1;
int const FALSE = 0;
int const TRUE = 1;
int const MAXLINE = 4096;

int ClientRequestMsgSz = sizeof(ClientRequestMsg_t);

//---------------------------------------------------------------------------------------------------------
//     General Utility Functions
//

//  myprint:
//    1) - builds single large string from variable number of string argument
//    2) - prints the big string to stdout
//
void myprint(int num, ...)
{
  va_list myargs;

  char *outstr = NULL;;
  int count = 0;
  int size = 0;

  if (num == 0){  // nothing to print
    return;
  }

  va_start (myargs, num);
  while (count++ < num)
    {
      char *s;
      s = va_arg(myargs, char *);
      size = asprintf(&outstr, "%s%s", outstr, s);
      if (size  == ERROR)
	{
	  perror("myprint:  asprintf failed");
	  return;
	}
    }
  va_end(myargs);

  printf("\n  %s\n\n", outstr);
  free(outstr);
  return;
}



