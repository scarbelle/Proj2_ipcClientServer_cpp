
#include "common.h"
#include "ipcSharedMem.h"

IpcSharedMem::IpcSharedMem()
  :_progModeId(ProgMode::UNSET),
   _serverDown(True),
   _fdServerConnect(ERROR)
{
}


IpcSharedMem::~IpcSharedMem()
{
}


void IpcSharedMem::init(ProgMode id)
{
  progModeId = id;
}

int IpcSharedMem::setupSharedMemAccess(ClientRequestMsg_t*& memServerConnect)
{
  int msgSize;
  
  // 
  //  Attempt to establish ONE EXCLUSIVE shared memory(POSIX)
  //    --failure implies server already up
  _fdServerConnect=shm_open(shmem_name, O_RDWR | O_CREAT | O_EXCL,
			    S_IRUSR | S_IWUSR);

  //  Server
  //    - exclusive open - multiple servers not allowed
  if (_progModeId == ProgMode::SERVER){
    if (_fdServerConnect == ERROR)   
      {
	fprintf(stderr,"\nSERVER Already Up\n\n");
	return EXIT_FAILURE;
      }
  }

  //  CLIENT
  //    - requires SERVER UP prior
  //    - client shared memory NOT EXCLUSIVE open
  else  
    {
      // Verify SERVER Shared Memory UP - expect exclusive open failure due to file already exists
      if ((_fdServerConnect== ERROR) && (errno == EEXIST))  // SERVER up, OK to connect shmem
	{
	  _fdServerConnect=shm_open(shmem_name, O_RDWR, S_IRUSR | S_IWUSR);
	  if (_fdServerConnect == ERROR)  // Error neither open succeeded
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

  msgSize = sizeof(*_memServerConnect);
  
  // Size shared mem file to client request holder
  if (ftruncate(_fdServerConnect, msgSize) == ERROR)  
    {
      perror("ftruncate");
      return EXIT_FAILURE;
    }

  // Map shared mem file into memory 
  _memServerConnect = mmap(NULL, msgSize, PROT_READ | PROT_WRITE, MAP_SHARED, _fdServerConnect, 0);
  if (_memServerConnect == MAP_FAILED)
    {
      perror("mmap");
      if (close(_fdServerConnect) == ERROR)
	{
	  perror("_fdServerConnect - mmap");
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
      memset(_memServerConnect, 0, msgSize);
      if (sem_init(&_memServerConnect->sem, (int)_memServerConnect, 0) == ERROR)
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
  memServerConnect = _memServerConnect;
  serverdown = FALSE;
  return 0;  
}



//
//     s_unloadRequest_shm (server) 
//       Continually watch shared memory IPC for incoming client requests.
//       Use semaphore to safely lock shared memory while retrieving request.
//       Server - get client request from shared memory, 
//                make copy to hold, 
//                initiate processing in separate thread
//
void* IpcSharedMem::unloadClientRequests(void (*processFunction)())
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
      //s = pthread_create(&taskreqThread, NULL, s_processClientReq, (void *)client_request);
      s = pthread_create(&taskreqThread, NULL, processFunction, (void *)client_request);
      if (s != 0){
	handle_error_en(s,"pthread_create doCliReq");
      }
    }
  return NULL;
}


//
//     Cleanup Shared Memory IPC - (client,server) 
//
void cleanup_shm(void)
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

  return status;
}



//
//     Cleanup shared memory semaphore only once (server) 
//
int server_cleanup_shm_sema(void)
{
  int status = EXIT_SUCCESS;
  
  if (SERVER){
    // Clean up semaphore before unlink
    status = sem_destroy(&_mem_ServerConnect->sem);
    if (status == ERROR)
      {
	perror("sem_destroy");
      }
  }
  return status;
}



int IpcSharedMem::cleanup_server()
{
  int status = EXIT_SUCCESS;
  
  if ( (server_cleanup_shm_sema() == EXIT_FAILURE) ||
       (cleanup_shm(void) == EXIT_FAILURE) )
    {
      status = EXIT_FAILURE;
    }
  
  return status;
}


//IpcSharedMem::
