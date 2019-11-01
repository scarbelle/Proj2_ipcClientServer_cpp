/*
 *
 * CSE 691 Adv Systems Programming
 * Project #2 - IPC Terminal
 *
 * Scarlett J. Davidson
 * SUID:  43342-9524
 *
 * 4/22/2014
 *
 */


#define _GNU_SOURCE               // asprintf
#include <fcntl.h>                // open
#include <stdlib.h>               // EXIT_FAILURE
#include <stdio.h>
#include <unistd.h>               // STDIN, STDOUT, execlp
#include <sys/stat.h>             // mkfifo
#include <sys/types.h>            // mkfifo
#include <errno.h>
#include <limits.h>               // PATH_MAX
#include <stdarg.h>               // va_list
#include <pthread.h>              // Threads

#include <sys/mman.h>
#include <string.h>
#include <semaphore.h>


/*********************************************************************/
// Following example from:
//        http://man7.org/linux/man-pages/man3/pthread_create.3.html

#define handle_error_en(en, msg)				\
  do { errno=en; perror(msg); exit(EXIT_FAILURE);} while (0)

/*********************************************************************/


#define TRUE 1
#define FALSE 0
#define ERROR -1

//
//  Program 
//
#define SERVER (serverflg == TRUE)
#define CLIENT (serverflg == FALSE)

#define UNSET -1                                // unset program mode/option
#define MAXLINE 4096                            // max line read/write
#define OPT_INTERRUPT 1                         // server->client msg to inform of task interruption
#define OPT_SHUTDOWN 2                          // server->client msg to request client shutdown
#define ACTIVECLIENT_TASKQ struct shmem_struct  // server's task queue for active client task requests

//
// Shared Memory
// - receives initial client request
//
#define shmem_name "/sjdavids_SERVER"         // shared memory name & location /dev/shm/sjdavids_SERVER
#define shmem_structsz sizeof(shmem_struct_t)


//
// FIFO
// - task output communication server->client 
// - per client FIFO name:  /tmp/sjdavids_FIFO[clientid]
//
#define MYFIFO "/tmp/sjdavids_FIFO"            // FIFO filename & location  
#define SEND_INTERRUPT "\n---sjdavidsINTERRUPTsjdavids---\n"
#define READ_INTERRUPT "---sjdavidsINTERRUPTsjdavids---\n"
#define SEND_SHUTDOWN "\n---sjdavidsSHUTDOWNsjdavids---\n"
#define READ_SHUTDOWN "---sjdavidsSHUTDOWNsjdavids---\n"


//---------------------------------------------------------------------------------------------------------
//  GLOBALS

int serverflg=UNSET;             // program mode - SERVER=1, CLIENT=0
int serverdown=TRUE;             // Initally server is down, false once server is up and running
int fd_ServerConnect;            // fd for client-server shared memory connection point
int SERVER_STOP = FALSE;         // Server reads clients msg indefinitely, stop at error, exit, shutdown
int CLIENT_SHUTDOWN = FALSE;     // Set TRUE when server requests client shutdown

//
//  Client Message Request Holder 
//    -- This struct type is used to:
//           1) receive initial client task requests in shared memory IPC (server connection point)
//           2) holding active client task requests in server's active client task queue
//
typedef struct shmem_struct
{
  pid_t client_id;              // Client Id
  size_t textlen;               // Client Request - text length
  char text[MAXLINE];           // Client Request - text
  sem_t sem;                    // Semaphore for Shared Memory Exclusivity - server receipt of client msg
  char fifoname[PATH_MAX];      // Unique Fifoname for server->client command output
  int fifo_fd;                  // Fifo fd
  pthread_t tid;                // Thread id for this client request processing
  int tresult;                  // Holds thread status for client request processing - not currently checked
  ACTIVECLIENT_TASKQ* next;     // Ptr to next server active client task queue entry
  ACTIVECLIENT_TASKQ* prev;     // Ptr to previous server active client task queue entry
} shmem_struct_t;
shmem_struct_t* mem_ServerConnect;   // client-server shared memory connection info


ACTIVECLIENT_TASKQ *client_request;  // task queue node ptr - holds client request info
ACTIVECLIENT_TASKQ *HEAD_taskq=NULL; // head of task queue list containing all active client requests



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

  char *outstr="";
  int count=0;
  int size=0;

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



//
//  Source from text - Linux Programming - M. Kerrisk, example pg 1201
//
//  Reads a line of input terminated by '\n'
//    - max length preserved is n
//    - input beyond length n is dropped
//    - add '\0' to end of input string
//
ssize_t readLine(int fd, void *buffer, size_t n)
{
  ssize_t numRead;
  size_t totalRead;
  char *buf;
  char ch;

  if (n <=0 || buffer == NULL){
    errno = EINVAL;
    return ERROR;
  }

  buf = buffer;

  totalRead = 0;
  numRead = 0;
  for(;;) {

    numRead = read(fd, &ch, 1);

    if (numRead == ERROR) {
      if (errno == EINTR){         // Interrupted, restart read
	continue;
      }
      else {
	return ERROR;              // Other error
      }

    } else if (numRead == 0) {     // EOF
      if (totalRead == 0){
	return 0;
      } else {
	break;
      }

    } else {
      if (totalRead < n-1) {
	totalRead++;
	*buf++ = ch;
      }
      if (ch == '\n'){
	break;
      }
    }
  }
  *buf = '\0';

  return totalRead;
}



//---------------------------------------------------------------------------------------------------------
//     CLIENT, SERVER Program -
//        function prefix indicates component usage:
//          cs - client, server 
//          s  - server
//          c  - client
//
//



//----------------------------------------------------------------------------------------------------------
//     CLIENT, SERVER - Request Connection Access Point - Shared Memory IPC 
//
 

//
//     Cleanup Shared Memory IPC - (client,server) 
//
static void cs_cleanup_shm(void)
{
  int status=EXIT_SUCCESS;

  if( (SERVER) || 
      ((CLIENT)  && (serverdown == TRUE)))
    {
      // Clean up shared memory
      if (shm_unlink(shmem_name) == ERROR)
	{
	  // perror("shm_unlink - server access");
	  status = EXIT_FAILURE;
	}
    }

  // Unmap connection shared memory
  if (munmap(mem_ServerConnect, shmem_structsz) == ERROR) 
    {
      perror("munmap");
      status = EXIT_FAILURE;
    }

  // Close shared memory fd_ServerConnect;
  if (close(fd_ServerConnect) == ERROR)
    {
      // perror commented out - cannot unregister atexit functions, after proper cleanup
      // so gives multiple warnings, w/ multiple client commands
      // perror("close fd_ServerConnect - cleanup_shm");  
      status = EXIT_FAILURE;
    }
}



//
//     Cleanup shared memory semaphore only once (server) 
//
static void s_cleanup_shm_sema(void)
{
  if (SERVER){
    // Clean up semaphore before unlink
    if (sem_destroy(&mem_ServerConnect->sem) == ERROR)
      {
	perror("sem_destroy");
      }
  }
}



//
//     Setup Server-Client IPC Connection  - POSIX shared memory (client, server)
//       - Server  "listens" to this shared memory IPC for initial client requests.
//       - Clients  establishs connection with the server through this shared memory IPC
//                  to deliver task request.
// 
int cs_setupConnectionAccess_shmIPC()
{
  // 
  //  Attempt to establish ONE EXCLUSIVE shared memory(POSIX)
  //    --failure implies server already up
  fd_ServerConnect=shm_open(shmem_name, O_RDWR | O_CREAT | O_EXCL,
			    S_IRUSR | S_IWUSR);

  if (SERVER){
    if (fd_ServerConnect == ERROR)   // exclusive open ERROR - multiple servers not allowed
      {
	fprintf(stderr,"\nSERVER Already Up\n\n");
	return EXIT_FAILURE;
      }
  }

  else  //CLIENT (requires SERVER UP prior, client shared memory NOT EXCLUSIVE open)
    {
      // Verify SERVER Shared Memory UP - expect exclusive open failure due to file already exists
      if ((fd_ServerConnect== ERROR) && (errno == EEXIST))  // SERVER up, OK to connect shmem
	{
	  fd_ServerConnect=shm_open(shmem_name, O_RDWR, S_IRUSR | S_IWUSR);
	  if (fd_ServerConnect == ERROR)  // Error neither open succeeded
	    {
	      perror("CLIENT shm_open");
	      return EXIT_FAILURE;
	    }
	}
      else {
	fprintf(stderr,"\nCLIENT connect attempt - no server up\n");
	serverdown = TRUE;
	cs_cleanup_shm();
	return EXIT_FAILURE;
      }
    }

  // Size shared mem file to client request holder
  if (ftruncate(fd_ServerConnect, shmem_structsz) == ERROR)  
    {
      perror("ftruncate");
      return EXIT_FAILURE;
    }

  // Map shared mem file into memory 
  mem_ServerConnect = mmap(NULL, shmem_structsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd_ServerConnect, 0);
  if (mem_ServerConnect == MAP_FAILED)
    {
      perror("mmap");
      if (close(fd_ServerConnect) == ERROR)
	{
	  perror("fd_ServerConnect - mmap");
	}
      return EXIT_FAILURE;
    }

  // Register SERVER shared memory for cleanup at server termination
  if (atexit(cs_cleanup_shm) != 0)     // reg cleanup - server access - shared mem
    {
      perror("setupServerConnect - atexit cs_cleanup_shm");
      return EXIT_FAILURE;
    }

  // Server - initialize shared memory semaphore and register its cleanup with exit
  if (SERVER) 
    {
      memset(mem_ServerConnect, 0, shmem_structsz);
      if (sem_init(&mem_ServerConnect->sem, (int)mem_ServerConnect, 0) == ERROR)
	{
	  perror ("sem_init");
	  return EXIT_FAILURE;
	}
      if (atexit(s_cleanup_shm_sema) != 0)  // reg cleanup - server access - shared mem sema
	{
	  perror("setupServerConnect - atexit s_cleanup_shm_sema");
	  return EXIT_FAILURE;
	}
    }
  serverdown = FALSE;
  return 0;
}



//----------------------------------------------------------------------------------------------------------
//     SERVER - Active Client Task Queue
//
//      -managed by server secondary task processing threads
//      -includes functions:
//          s_taskqAdd (ACTIVECLIENT_TASKQ *cli)
//          s_taskqFind (int clientid)
//          s_taskqRemove (ACTIVECLIENT_TASKQ *node)
//          s_taskqShow ()
//
//      *** WARNING - UNPROTECTED MULTITHREAD SHARED DATA STRUCT - ADD MUTEX PROTECTION


//
//     s_taskqAdd - add client request to the task queue (server)
//
//
ACTIVECLIENT_TASKQ*  s_taskqAdd(ACTIVECLIENT_TASKQ *cli)
{
  if (!cli) 
    {
      fprintf(stderr,"\ns_taskqAdd - null add");
      return NULL;
    }

  //make new node for task queue entry
  ACTIVECLIENT_TASKQ *node = malloc(sizeof(shmem_struct_t));
  if (node == NULL){
    perror("s_taskqAdd: Bad malloc");
    return NULL;
  }
  memset(node, 0, shmem_structsz);

  if (!node)
    {
      perror("s_taskqAdd - malloc");
      return NULL;
    }

  //  Copy client request info into new task queue node
  else
    {
      node->client_id = cli->client_id;
      node->textlen = cli->textlen;
      memset(node->text, '\0', MAXLINE);
      strcpy(node->text, cli->text);
      node->sem = cli->sem;
      memset(node->fifoname, '\0', MAXLINE);
      strcpy(node->fifoname, cli->fifoname);
      node->fifo_fd = cli->fifo_fd;
      node->tid = cli->tid;
      node->prev = NULL;
      node->next = NULL;
    }

  // Add node to watchlist head
  if (HEAD_taskq)
    {
      HEAD_taskq->prev = node;
      node->next = HEAD_taskq;
      HEAD_taskq = node;
    }
  else
    {
      HEAD_taskq = node;
    }

  return node;
}



//
//     s_taskqFind - find a clientid in the active task queue list (server)
//  
//
ACTIVECLIENT_TASKQ* s_taskqFind(int clientid)
{
  ACTIVECLIENT_TASKQ *curr;

  curr = HEAD_taskq;

  //Find node 
  while(curr)
    {
      if (curr->client_id == clientid)
	{
	  return curr;
	}
      else
	{
	  curr=curr->next;
	}
    }
  return NULL;
}



//
//     s_taskqRemove - remove client task request from active queue list (server)
//
//
int s_taskqRemove(ACTIVECLIENT_TASKQ *node)
{
  int removed = FALSE;

  if (node != NULL) 
    {
      //      printf("\ns_taskqRemove: node->fifoname: %s\n node->fifo_fd\n",
      //               node->fifoname, node->fifo_fd);

      if(node == HEAD_taskq)
	{
	  HEAD_taskq = node->next;
	  if(node->next) {node->prev = NULL;}
	}
      else 
	{
	  node->prev->next = node->next;
	  if (node->next) {node->next->prev = node->prev;}
	}
      free(node);
      removed = TRUE;
    }
  return removed;
}



//
//     s_taskqShow - show the entries in the active client task queue (server)
//
//
void s_taskqShow()
{
  ACTIVECLIENT_TASKQ *curr = HEAD_taskq;

  printf("\n\t*** Active Client List ***");
  printf("\n\n\tClient Pid\tCommand");
  printf("\n\t__________\t_______");

  if (!curr)
    {
      printf("\n\t--- no requests in progress ---\n\n");
    }
  else
    {
      while(curr)
	{
	  printf("\n\t%d\t\t%s", curr->client_id, curr->text);
	  curr=curr->next;
	}
    }
}



//----------------------------------------------------------------------------------------------------------
//     CLIENT, SERVER - FIFO Management
//       used to send request results from server to client


//
//     cs_makeFIFO -
//       inputs - clientid(to create unique fifoname), read_end or write_end
//       tracks - clientfifo name for cleanup in server later
//       returns - fifo_fd
//
//
int cs_makeFIFO (int *fifo_fd, int clientid, char *clientfifo)
{
  *fifo_fd = UNSET;
  char c_fifoname[PATH_MAX];

  snprintf(c_fifoname, sizeof(c_fifoname), "%s%d", MYFIFO, clientid);

  // Create FIFO     - ignore error if already exists
  umask(0);               
  if ((mkfifo(c_fifoname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |S_IROTH | S_IWOTH) == ERROR)
      && errno != EEXIST){
    perror("mkfifo c_fifoname");
    return EXIT_FAILURE;
  }

  strcpy(clientfifo, c_fifoname);     //  Save clientfifo name for server cleanup

  if (SERVER)     // WRITE_END            //  Server writes to client
    {
      *fifo_fd = open(c_fifoname, O_WRONLY);  
      if (*fifo_fd == ERROR){
	perror("server open fifo");
	return EXIT_FAILURE;
      }
    }

  else //Client  // READ_END 
    {
      *fifo_fd = open(c_fifoname, O_RDONLY);
      if (*fifo_fd == ERROR){
	perror("client open fifo");
	return EXIT_FAILURE;
      }
    }
  return 0;
}
 


//
//    Remove Fifo (client,server) 
//      No problem if it does not exist, it was already cleaned
//
void cs_removeFifo(char *pname)
{
  if (unlink(pname) == ERROR)
    {
      // perror("FIFO unlink");   // Debug only
    }
}


//----------------------------------------------------------------------------------------------------------
//     SERVER - Request Processing
//       - server - setup server->client FIFO for communication of request output
//       - server main thread listens for admin commands from standard input
//       - server processes client requests on separate task processing thread
//       - client retrieves server results from FIFO line by line
//       - client sends server request and waits for completion prior to subsequent request


//     Server Process Client Request
//
//       Do this work (in thread)
//          1. setup fifo server_stdout->client_fifo_in->client_stdout; 
//          2. patch cmd string to send cmd stderr to cmd stdout 
//          3. add client id to active task list - this is a multi-thread shared data struct needs protection
//          4. exec cmd with popen
//          5. read popen output and write line to fifo
//          6. cleanup
//
void* s_processClientReq(void *arg)
{
  shmem_struct_t *client = (shmem_struct_t*)(arg);
  int fifo_fd;
  int status = EXIT_FAILURE;

  //debug
  //  fprintf(stdout, "got message from %d: %d %s",
  //	  client->client_id, client->textlen, client->text);     //print to server stdout

  // Make Server-Client FIFO 
  if ((status = cs_makeFIFO(&fifo_fd, client->client_id, client->fifoname) == EXIT_SUCCESS))
    {
      FILE *fp;
      char *line;
      size_t linelen;
      ssize_t cmdout;
      ACTIVECLIENT_TASKQ* node;

      client->fifo_fd = fifo_fd;
      client->tid = pthread_self();             // save self thread id

      node = s_taskqAdd(client);                  // Add to Server's Client Work Queue List

      // Command stderr to same as stdout
      client->text[(client->textlen)-1]='\0';
      strcat(client->text, " 2>&1\n");

      // Open pipe to process and read client text command
      fp = popen(client->text, "r");
      if (!fp)
	{
	  perror("SERVER error - popen)");
	  client->tresult = status;
	  pthread_exit(NULL);
	}

      line = NULL;
      linelen = 0;
      // Read one line of client command output from pipe above.
      while ((cmdout = getline(&line, &linelen, fp)) != ERROR)
	{
	  // Write the readin line to the server-client fifo.
	  if (write(fifo_fd, line, (strlen(line)+1)) == ERROR){  // +1 to include newline char
	    perror("s_processClientReq - write error");
	  }
	  line=NULL;
	  linelen=0;
	}
      free(line);

      if (pclose(fp) == ERROR){                     // close - server end
	perror("SERVER - pclose");
	client->tresult = EXIT_FAILURE;
	pthread_exit(NULL);
      }

      s_taskqRemove(node);     // Remove from Server's Client Work Queue List

      if (close(fifo_fd) == ERROR){                     // close - server end
	perror("SERVER - myfifo");
	client->tresult = EXIT_FAILURE;
	pthread_exit(NULL);
      }

      cs_removeFifo(client->fifoname);
    }

  client->tresult = EXIT_FAILURE;
  pthread_exit(NULL);
}



//
//     s_unloadRequest_shm (server) 
//       Continually watch shared memory IPC for incoming client requests.
//       Use semaphore to safely lock shared memory while retrieving request.
//       Server - get client request from shared memory, 
//                make copy to hold, 
//                initiate processing in separate thread
//
void* s_unloadClientRequest_shm()
{
  int status;

  SERVER_STOP = FALSE;
  while (SERVER_STOP == FALSE)
    {
      // make client req hold for transfer of client req info from shared memory
      client_request = (ACTIVECLIENT_TASKQ *)malloc(shmem_structsz);
      if (client_request == NULL){
	perror("ERROR: client_request dropped");
	SERVER_STOP = TRUE;
	break;
      }
      memset(client_request, 0, shmem_structsz);

      // block until there is a client request to process 
      // then lock shared memory prior to reading contents
      if (sem_wait(&mem_ServerConnect->sem) == ERROR)
	{
	  perror("sem_wait");
	  SERVER_STOP = TRUE;
	  break;
	}

      // Save copy of client id, textlen, text from shared memory 
      client_request->client_id = mem_ServerConnect->client_id;
      client_request->textlen = mem_ServerConnect->textlen;
      memset(client_request->text, '\0', MAXLINE);
      strncpy(client_request->text, mem_ServerConnect->text, (MAXLINE-1));

      // Process Client Request - Thread
      int s;
      pthread_t taskreqThread;
      s = pthread_create(&taskreqThread, NULL, s_processClientReq, (void *)client_request);
      if (s != 0){
	handle_error_en(s,"pthread_create doCliReq");
      }
    }
  return NULL;
}



//-------------------------------------------------------------------------------------------------------------
//     SERVER main thread - User Input / Admin Command Processing
//


//
//     s_cancelClient - cancel task of active client id
//       opt indicates:
//         - interrupt task
//         - request client shutdown
//
void s_cancelClient(int clientid, int opt)
{
  int s;
  pthread_t killme;

  ACTIVECLIENT_TASKQ *node = s_taskqFind(clientid);

  // Check Invalid Input
  if (node == NULL)
    {
      printf("\n\t Invalid Client Id: %d\n", clientid);
      return;
    }

  killme = node->tid;
  s=pthread_cancel(killme);
  if (s !=0)
    handle_error_en(s, "s_cancelClient");

  s = pthread_join(killme, NULL);
  if (s != 0){
    handle_error_en(s,"pthread_join doCliReq");
  }

  char *line = NULL;
  int linelen = 0;

  switch (opt)
    {
    case OPT_INTERRUPT:
      line = SEND_INTERRUPT;
      break;

    case OPT_SHUTDOWN:
      line = SEND_SHUTDOWN;
      break;

    default:
      printf("\ns_cancelClient - Bad Option\n");
      return;
    }

  linelen = strlen(line);

  // Send EOF Transmission Signal String
  if (write(node->fifo_fd, line, ((linelen))) == ERROR){
    perror("s_cancelClient - write error");
  }

  if (close(node->fifo_fd) == ERROR){                     // close - server end
    perror("client - close myfifo");
  }

  cs_removeFifo(node->fifoname);

  printf("\n\tJob Cancelled - \tClientID: %d\tCommand:  %s", node->client_id, node->text);

  s_taskqRemove(node);
}



//
//     s_CancelAllActiveClient server
//       Cancel all tasks for client request in active task queue
//         opt indicates:
//           - interrupt task
//           - request client shutdown
//
void s_CancelAllActiveClients(int opt)
{
  ACTIVECLIENT_TASKQ *curr = HEAD_taskq;
  while(curr)
    {
      s_cancelClient(curr->client_id, opt );
      curr=curr->next;
    }
  printf("\n\n");
}



//
//  s_displayServerCmdList (server)
//
void s_displayServerCmdList()
{
  char *str1="\n  Server Command Options";
  char *str2="\n  ___________________";
  char *str3="\n    help\t\t- show this server command list";
  char *str4="\n    list\t\t- list all attached active client ids and tasks";
  char *str5="\n    cancel [clientid]   - cancel active client task";
  char *str6="\n    shutdown [clientid] - request shutdown of active client";
  char *str7="\n\n    exit\t\t- cancel all active client tasks, exit server";
  char *str8="\n    shutdownall\t\t- request shutdown of all active clients, exit server";
  char *str9="\n\n    **Note** - inactive clients(no current running task) are not managed";

  myprint(9, str1, str2, str3, str4, str5, str6, str7, str8, str9);
}



//
//     s_catchServerUI (server)
//       Catch and process all server console user input
//
int s_catchServerUI()
{
  SERVER_STOP = FALSE;
  int option = OPT_INTERRUPT;

  s_displayServerCmdList();
  while (SERVER_STOP == FALSE)
    {
      char* msg = NULL;
      size_t msglen = 0;
 
      // Get User Input
      printf("\n\n\n    To exit, type \"exit\", press [ENTER]\n\n>> "); // prompt user
      if (getline(&msg, &msglen, stdin) == ERROR)
      	{
      	  perror("s_catchServerUI");
      	}

      if (strcmp(msg, "exit\n") == 0)
	{
	  SERVER_STOP = TRUE;
	  option = OPT_INTERRUPT;
	  printf("\n\t*** Server Exit ***\n\t- all active tasks cancelled\n");
	}

      else if (strcmp(msg, "shutdownall\n") == 0)
	{
	  SERVER_STOP = TRUE;
	  option = OPT_SHUTDOWN;
	  printf("\n\t*** Shutdown All ***\n\t- server and all active clients\n");
	}

      else if (strcmp(msg, "list\n") == 0)
	{
	  s_taskqShow();
	}

      else if (strncmp(msg, "cancel", strlen("cancel")) == 0)
	{
	  char cancel_str[strlen("cancel")+1];   // +1 to include "\0"
	  char id_str[MAXLINE];
	  memset(id_str, '\0', MAXLINE);
	  int id;
	  int numread = sscanf(msg, "%s %s", cancel_str, id_str);
	  if ((numread == 2) && ((id=atoi(id_str)) != 0))
	    {
	      printf("\n\t*** Cancel Request ***");
	      s_cancelClient(id, OPT_INTERRUPT);
	    }
	  else
	    {
	      printf("\n\t Invalid Client Id: %s\n", id_str);
	    }
	}
      else if (strncmp(msg, "shutdown", strlen("shutdown")) == 0)
	{
	  char cancel_str[strlen("shutdown")+1];   // +1 to include "\0"
	  char id_str[MAXLINE];
	  memset(id_str, '\0', MAXLINE);
	  int id;
	  int numread = sscanf(msg, "%s %s", cancel_str, id_str);
	  if ((numread == 2) && ((id=atoi(id_str)) != 0))
	    {
	      printf("\n\t*** Shutdown Request ***");
	      s_cancelClient(id, OPT_SHUTDOWN);
	    }
	  else
	    {
	      printf("\n\t Invalid Client Id: %s\n", id_str);
	    }
	}
      else 
	{
	  s_displayServerCmdList();
	}
      free(msg);
    }
     
  // Cleanup Server - Everything
  s_CancelAllActiveClients(option);
  //  s_cleanup_shm_sema();          // not invoked here, registered w/ atexit
  //  cs_cleanup_shm();              // not invoked here, registered w/ atexit

  return 0;
}



int do_serverMain()
{
  //
  // SERVER - setup for listening to recieve initial client requests
  //   Uses shared memory & semaphore for interprocess communication.
  //
  if (cs_setupConnectionAccess_shmIPC() == EXIT_FAILURE)
    {
      return EXIT_FAILURE;
    }

  pthread_t tid_doClientRequest;
  int s;

  // Always Listen and capture client request
  s = pthread_create(&tid_doClientRequest, NULL, s_unloadClientRequest_shm, NULL);
  if (s != 0){
    handle_error_en(s,"pthread_create doClientRequest");
  }

  s_catchServerUI();

  return 0;
}



//-------------------------------------------------------------------------------------------------------------
//     CLIENT main thread - User Input Command Processing
//
//       - client - setup server->client FIFO for communication of request output
//       - client retrieves server results from FIFO line by line
//       - client sends server request and waits for completion prior to subsequent request


//
//     c_loadRequest_shm  (client)
//       Client -
//         First verify server is up
//         Establish Server Connection Access - Shared Memory IPC
//         Use semaphore to safely lock shared memory prior to putting client request in shared memory.
//         Put client request in Server Connection - Shared Memory IPC
int c_loadRequest_shm(char *msg)
{
  // Verify Server Shared Memory Connection is Up
  if (cs_setupConnectionAccess_shmIPC() == EXIT_FAILURE)
    {
      return EXIT_FAILURE;
    }

  // Build client request into shared memory message
  mem_ServerConnect->client_id = getpid();
  memset(mem_ServerConnect->text, '\0', MAXLINE);
  strncpy(mem_ServerConnect->text, msg, (MAXLINE-1));
  mem_ServerConnect->textlen = strlen(msg); 

  // Wait, load client request into shared memory
  //
  sem_trywait(&mem_ServerConnect->sem);               // class example -> no error check, we don't care
  if (sem_post(&mem_ServerConnect->sem) == ERROR)     // unlock the shared memory semaphore
    {
      perror("Client send failure - sem_post");
      return EXIT_FAILURE;
    }

  return 0;
}



//
//  c_getResult_FIFO() - client
//     - opens FIFO 
//     - forever:
//          reads in line of command output from FIFO 
//          checks for EOF (exit), interrupt from server, shutdown request from server
//          writes line to stdout
//     - closes FIFO 
//
int c_getResult_FIFO()
{
  int fifo_fd;
  char fifoname[MAXLINE];
  char cbuf[MAXLINE];
  size_t num_in, num_out;
  int status;

  char *interruptClient = READ_INTERRUPT;
  int interruptClient_len = strlen(READ_INTERRUPT);
  char *shutdownClient = READ_SHUTDOWN;
  int shutdownClient_len = strlen(READ_SHUTDOWN);

  if ((status = cs_makeFIFO(&fifo_fd, getpid(), fifoname)) == EXIT_SUCCESS)
    {
      //      Read Fifo Contents
      for (;;)
	{
	  num_in = 0;
	  memset(cbuf, '\0', MAXLINE);

	  num_in = readLine(fifo_fd, cbuf, MAXLINE);
	  if (num_in == 0){            //  Read EOF - FIFO closed, time to exit
	    break;
	  }

	  else if (num_in == interruptClient_len) //  Server Cancelled with INTERRUPT string
	    { 
	      if (strcmp(cbuf, READ_INTERRUPT) == 0)
		{
		  printf("\n\t*** Error Warning *** - Command Terminated by Server\n\n");
		  break;
		}
	    }

	  else if (num_in == shutdownClient_len) //  Server requested client shutdown
	    { 
	      if (strcmp(cbuf, READ_SHUTDOWN) == 0)
		CLIENT_SHUTDOWN = TRUE;
	      {
		printf("\n\t*** SHUTDOWN *** - Request from Server\n\n");
		break;
	      }
	    }

	  else {                       //  writes line to client standout
	    if (write(STDOUT_FILENO, cbuf, num_in) != num_in){
	      perror("client write error");
	    }
	  }
	}

      // cleanup
      if (close(fifo_fd) == ERROR){                     // close - server end
	perror("client - close myfifo");
	status = EXIT_FAILURE;
      }
      cs_removeFifo(fifoname);
    }

  return status;
}



int do_clientMain()
{
  int exit = FALSE;

  while (CLIENT_SHUTDOWN == FALSE)
    {
      // Get User Input
      printf("\n\n\n   To exit, type \"exit\", press [ENTER]\n\n>> "); // prompt user

      char* msg = NULL;
      size_t msglen = 0;
      if (getline(&msg, &msglen, stdin) == ERROR)
      	{
      	  perror("do_clientMain - bad getline");
      	}

      if (strcmp(msg, "exit\n") == 0)
	{
	  exit = TRUE;
	  break;
	}

      // Get command from stdin and send to server
      if (c_loadRequest_shm(msg) == EXIT_FAILURE){
	break;
      }
      free(msg);

      // Catch & display command output
      if (c_getResult_FIFO() == EXIT_FAILURE)
	{
	  fprintf(stderr, "\nClient - getCmdFifo error");
	}

      //  Cleanup shared Memory
      cs_cleanup_shm();
    }
 
  return exit;
}



//
//     cs_progHelpInfo (client, server)
//       - display program usage information
//
void cs_progHelpInfo(char *prog)
{
  char *str1="\n  [ Usage: ]   ";
  char *str2=" -h -s -c -x \n";
  char *str3="\n  -h = usage help";
  char *str4="\n  -s = run as server";
  char *str5="\n  -c = run as client";
  char *str10="\n  -x = debug cleanup (cleans IPC remants on crashes)";

  char *str6="\n\n\n  **** NOTE 1:  All options and commands must be lowercase";
  char *str7="\n  **** NOTE 2:  Server or Client options are exclusive";
  char *str8="\n  **** NOTE 3:  Server must be activated prior to Client";
  char *str9="\n  **** NOTE 4:  To exit - type:  \"exit\"[enter]";
  myprint(11, str1, prog, str2, str3, str4, str5, str10,str6, str7, str8, str9);

  s_displayServerCmdList();
}



//
//     Main
//       1) process command line options
//       2) process client requests until further notice
//
int main(int argc, char* argv[])
{
  serverflg=UNSET;
  int helpflg=0;
  int opt;

  //--------------------------
  // 
  //  Task 1 Process Command Line options
  //
  while ((opt = getopt(argc, argv, "hscx")) != -1)     // -1 no more options to parse
    {
      int size;
      switch(opt) {

      case 'h':      // usage help
	helpflg=1;
	break;

      case 's':      // run as server
	if (serverflg == 0) {helpflg = 1;}
	else {serverflg = 1;}
	break;

      case 'c':      // run as client
	if (serverflg == 1) {helpflg = 1;}
	else {serverflg = 0;}
	break;

      case 'x':     // For Debug Cleanup - Not public to user
	serverflg=1;
	s_cleanup_shm_sema();
	cs_cleanup_shm();
	return 0;
	break;

      default:
	helpflg=1;
      }
    }

  
  //  Check for program usage help - no client or server, extra args
  if ( (serverflg == -1) || (argc > optind) ){
    helpflg=1;
  }

  /* Command Line Parsing Complete - Begin Task Processing */
  if (helpflg){
    cs_progHelpInfo(argv[0]);
    return 0;
  }
 

  //---------------------------
  //
  //  Task 2  - Server do Serverwork, 
  //            Client do Clientwork

  int status = (SERVER) ? do_serverMain() :
    do_clientMain();


  return status;
}
