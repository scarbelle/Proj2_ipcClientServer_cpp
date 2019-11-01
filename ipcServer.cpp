/*
 * C++ Code Sample
 *
 * Refactored C++ Version of
 * Syracuse University 4/22/2014
 * CSE 691 Adv Systems Programming
 * Project #2 - IPC Terminal
 *
 * Scarlett J. Davidson
 *
 * 7/31/2019
 *
 */

#include <ipcServer.h>

// Forward Declarations
void*IpcServer::processClientRequest(void *clientInfo);


char* shmem_name="/sjdavids_SERVER"; // shared memory name, location: /dev/shm/sjdavids_SERVER


// Constructor
IpcServer::IpcServer()
  : Client_Server_Program(ProgMode::SERVER)
{
}
  
// Destructor
virtual IpcServer::~IpcServer()
{
}

// Init
void IpcServer::init();
{
}
  
// Setup
int IpcServer::ipc_setup_request()
{
  return (_ipcRequest.setupSharedMemAccess(_mem_ServerConnect)) ;
}
  
// Launch
int IpcServer::ipc_launch_request_handler()
{
  pthread_t tid_doClientRequest;
  int status;

  // Always Listen and capture client request
  //status = pthread_create(&tid_doClientRequest, NULL, s_unloadClientRequest_shm, NULL);
  status = pthread_create(&tid_doClientRequest, NULL,
			  _ipcRequest.unloadClientRequests, &processClientRequest);
  if (status != 0){
    handle_error(status,"pthread_create doClientRequest");
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}

int IpcServer::ipc_launch_result_handler
{
}
  
// Process_cmds
void IpcServer::process_user_cmds()
{
  bool serverStop = False;
  int option = STOP_INTERRUPT;

  displayServerCmdList();
  
  while (!serverStop)
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
	  serverStop = True;
	  option = STOP_INTERRUPT;
	  printf("\n\t*** Server Exit ***\n\t- all active tasks cancelled\n");
	}

      else if (strcmp(msg, "shutdownall\n") == 0)
	{
	  serverStop = True;
	  option = STOP_SHUTDOWN;
	  printf("\n\t*** Shutdown All ***\n\t- server and all active clients\n");
	}

      else if (strcmp(msg, "list\n") == 0)
	{
	  taskqShow();
	}

      else if (strncmp(msg, "cancel", strlen("cancel")) == 0)
	{
	  char cancel_str[strlen("cancel")+1];   // +1 to include "\0"
	  char id_str[LINE_MAX];
	  memset(id_str, '\0', LINE_MAX);
	  int id;
	  int numread = sscanf(msg, "%s %s", cancel_str, id_str);
	  if ((numread == 2) && ((id=atoi(id_str)) != 0))
	    {
	      printf("\n\t*** Cancel Request ***");
	      cancelClient(id, STOP_INTERRUPT);
	    }
	  else
	    {
	      printf("\n\t Invalid Client Id: %s\n", id_str);
	    }
	}
      else if (strncmp(msg, "shutdown", strlen("shutdown")) == 0)
	{
	  char cancel_str[strlen("shutdown")+1];   // +1 to include "\0"
	  char id_str[LINE_MAX];
	  memset(id_str, '\0', LINE_MAX);
	  int id;
	  int numread = sscanf(msg, "%s %s", cancel_str, id_str);
	  if ((numread == 2) && ((id=atoi(id_str)) != 0))
	    {
	      printf("\n\t*** Shutdown Request ***");
	      cancelClient(id, STOP_SHUTDOWN);
	    }
	  else
	    {
	      printf("\n\t Invalid Client Id: %s\n", id_str);
	    }
	}
      else 
	{
	  displayServerCmdList();
	}
      free(msg);
    }
     
  // Cleanup Server - Everything
  cancelAllActiveClients(option);
  //  s_cleanup_shm_sema();          // not invoked here, registered w/ atexit
  //  cs_cleanup_shm();              // not invoked here, registered w/ atexit

}
  
// Cleanup
void IpcServer::ipc_cleanup_request()
{
}
void IpcServer::ipc_cleanup_result()
{
}


// Helper Functions

//
//  displayServerCmdList
//
void void IpcServer::displayServerCmdList()
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
void*IpcServer::processClientRequest(void *clientInfo)
{
  shmem_struct_t *client = (shmem_struct_t*)(clientInfo);
  int fifo_fd;
  int status = EXIT_FAILURE;

  //debug
  //  fprintf(stdout, "got message from %d: %d %s",
  //	  client->client_id, client->textlen, client->text);     //print to server stdout

  // Make Server-Client FIFO 
  //if ((status = cs_makeFIFO(&fifo_fd, client->client_id, client->fifoname) == EXIT_SUCCESS))
  if ((status = _ipcResult.makeFIFO(&fifo_fd, client->client_id, client->fifoname) == EXIT_SUCCESS))
    {
      FILE *fp;
      char *line;
      size_t linelen;
      ssize_t cmdout;
      ClientRequestMsg* node;

      client->fifo_fd = fifo_fd;
      client->tid = pthread_self();             // save self thread id

      node = taskqAdd(client);                  // Add to Server's Client Work Queue List

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
	    perror("processClientRequest - write error");
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

      taskqRemove(node);     // Remove from Server's Client Work Queue List

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
//     taskqAdd - add client request to the task queue (server)
//
//
ClientRequestMsg*  IpcServer::taskqAdd(ClientRequestMsg_t *cli)
{
  if (!cli) 
    {
      fprintf(stderr,"\ntaskqAdd - null add");
      return NULL;
    }

  //make new node for task queue entry
  ClientRequestMsg_t *node = malloc(sizeof(shmem_struct_t));
  if (node == NULL){
    perror("taskqAdd: Bad malloc");
    return NULL;
  }
  memset(node, 0, shmem_structsz);

  if (!node)
    {
      perror("taskqAdd - malloc");
      return NULL;
    }

  //  Copy client request info into new task queue node
  else
    {
      node->client_id = cli->client_id;
      node->textlen = cli->textlen;
      memset(node->text, '\0', LINE_MAX);
      strcpy(node->text, cli->text);
      node->sem = cli->sem;
      memset(node->fifoname, '\0', LINE_MAX);
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
//     taskqFind - find a clientid in the active task queue list (server)
//  
//
ClientRequestMsg_t* IpcServer::taskqFind(int clientid)
{
  ClientRequestMsg_t *curr;

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
//     taskqRemove - remove client task request from active queue list (server)
//
//
int IpcServer::taskqRemove(ClientRequestMsg_t *node)
{
  int removed = FALSE;

  if (node != NULL) 
    {
      //      printf("\ntaskqRemove: node->fifoname: %s\n node->fifo_fd\n",
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
//     taskqShow - show the entries in the active client task queue (server)
//
//
void IpcServer::taskqShow()
{
  ClientRequestMsg_t *curr = HEAD_taskq;

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

//
//     cancelClient - cancel task of active client id
//       opt indicates:
//         - interrupt task
//         - request client shutdown
//
void IpcServer::cancelClient(int clientid, int opt)
{
  int s;
  pthread_t killme;

  ClientRequestMsg_t *node = taskqFind(clientid);

  // Check Invalid Input
  if (node == NULL)
    {
      printf("\n\t Invalid Client Id: %d\n", clientid);
      return;
    }

  killme = node->tid;
  s=pthread_cancel(killme);
  if (s !=0)
    handle_error_en(s, "cancelClient");

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
      printf("\ncancelClient - Bad Option\n");
      return;
    }

  linelen = strlen(line);

  // Send EOF Transmission Signal String
  if (write(node->fifo_fd, line, ((linelen))) == ERROR){
    perror("cancelClient - write error");
  }

  if (close(node->fifo_fd) == ERROR){                     // close - server end
    perror("client - close myfifo");
  }

  cs_removeFifo(node->fifoname);

  printf("\n\tJob Cancelled - \tClientID: %d\tCommand:  %s", node->client_id, node->text);

  taskqRemove(node);
}


//
//     cancelAllActiveClient server
//       Cancel all tasks for client request in active task queue
//         opt indicates:
//           - interrupt task
//           - request client shutdown
//
void cancelAllActiveClients(int opt)
{
  ClientRequestMsg_t *curr = HEAD_taskq;
  while(curr)
    {
      s_cancelClient(curr->client_id, opt );
      curr=curr->next;
    }
  printf("\n\n");
}



