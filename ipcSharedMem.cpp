
#include <fcntl.h>          // shm_open - file access modes
#include <sys/mman.h>       // shm_open()
#include <errno.h>          // errno
#include <unistd.h>         // ftruncate()
#include <string.h>         // memset()
#include <pthread.h>        // pthread_create()

#include "ipcSharedMem.h"

IpcSharedMem::IpcSharedMem()
  :_progMode(ProgMode::UNSET),
   _serverDown(true),
   _fdServerConnect(ERROR),
   _memServerConnect(nullptr)
{
}


IpcSharedMem::~IpcSharedMem()
{
  cleanup();
}


void IpcSharedMem::init(ProgMode mode,
                        void (*setServerIsDownFunc)(bool state),
                        bool (*serverIsDownFunc)(void)
                        )
{
  _progMode = mode;
  _setServerIsDown = setServerIsDownFunc;
  _serverIsDown = serverIsDownFunc;
}


int IpcSharedMem::setupSharedMemAccess()
{
  int msgSize;

  // 
  //  Attempt to establish ONE EXCLUSIVE shared memory(POSIX)
  //    --failure implies server already up
  //  _fdServerConnect=shm_open(shmem_name, O_RDWR | O_CREAT | O_EXCL,
  //                            S_IRUSR | S_IWUSR);
  _fdServerConnect=shm_open(SHMEM_NAME, O_RDWR | O_CREAT | O_EXCL,
                            S_IRUSR | S_IWUSR);

  //  Server
  //    - exclusive open - multiple servers not allowed
  if (_progMode == ProgMode::SERVER){
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
          _fdServerConnect=shm_open(SHMEM_NAME, O_RDWR, S_IRUSR | S_IWUSR);
          if (_fdServerConnect == ERROR)  // Error neither open succeeded
            {
              perror("CLIENT shm_open");
              return EXIT_FAILURE;
            }
        }
      else {
        fprintf(stderr,"\nCLIENT connect attempt - no server up\n");
        _serverDown = TRUE;
        cleanup_shm();
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
  _memServerConnect = static_cast<ClientRequestMsg_t *>(mmap(NULL, msgSize, PROT_READ | PROT_WRITE, MAP_SHARED, _fdServerConnect, 0));
  if (_memServerConnect == MAP_FAILED)
    {
      perror("mmap");
      if (close(_fdServerConnect) == ERROR)
        {
          perror("_fdServerConnect - mmap");
        }
      return EXIT_FAILURE;
    }

  // // Register SERVER shared memory for cleanup at server termination
  // if (atexit(cleanup_shm) != 0)     // reg cleanup - server access - shared mem
  //   {
  //     perror("setupServerConnect - atexit cs_cleanup_shm");
  //     return EXIT_FAILURE;
  //   }

  // Server - initialize shared memory semaphore and register its cleanup with exit
  if (_progMode == ProgMode::SERVER) 
    {
      memset(_memServerConnect, 0, msgSize);
      if (sem_init(&_memServerConnect->sem, (int)_memServerConnect, 0) == ERROR)
        {
          perror ("sem_init");
          return EXIT_FAILURE;
        }
      // if (atexit(cleanup_shm_sema) != 0)  // reg cleanup - server access - shared mem sema
      //   {
      //     perror("setupServerConnect - atexit s_cleanup_shm_sema");
      //     return EXIT_FAILURE;
      //   }
    }
  // memServerConnect = _memServerConnect;
  _serverDown = FALSE;
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
void* IpcSharedMem::unloadClientRequests(void* (*processFunction)(void *clientReq))
{
  ClientRequestMsg_t *clientRequest;

  while (!(_serverIsDown()))
    {
      // make client req hold for transfer of client req info from shared memory
      clientRequest = (ClientRequestMsg_t *)malloc(ClientRequestMsgSz);
      if (clientRequest == NULL){
        perror("ERROR: client_request dropped");
        _setServerIsDown(true);
        break;
      }
      memset(clientRequest, 0, ClientRequestMsgSz);

      // block until there is a client request to process 
      // then lock shared memory prior to reading contents
      if (sem_wait(&_memServerConnect->sem) == ERROR)
        {
          perror("sem_wait");
          _setServerIsDown(true);
          break;
        }

      // Save copy of client id, textlen, text from shared memory 
      clientRequest->client_id = _memServerConnect->client_id;
      clientRequest->textlen = _memServerConnect->textlen;
      memset(clientRequest->text, '\0', MAXLINE);
      strncpy(clientRequest->text, _memServerConnect->text, (MAXLINE-1));

      // Process Client Request - Thread
      int s;
      pthread_t taskreqThread;
      s = pthread_create(&taskreqThread, NULL, processFunction, (void *)clientRequest);
      if (s != 0){
        handle_error_en(s,"pthread_create doCliReq");
      }
    }
  return NULL;
}

//    Cleanup Shared Memory IPC - (client,server) 
int IpcSharedMem::cleanup_shm(void)
{
  int status=EXIT_SUCCESS;

  if( (_progMode == ProgMode::SERVER) || 
      ((_progMode == ProgMode::CLIENT)  && (_serverIsDown())))
    {
      // Clean up shared memory
      if (shm_unlink(SHMEM_NAME) == ERROR)
        {
          // perror("shm_unlink - server access");
          status = EXIT_FAILURE;
        }
    }

  // Unmap connection shared memory
  if (munmap(_memServerConnect, sizeof(_memServerConnect)) == ERROR) 
    {
      perror("munmap");
      status = EXIT_FAILURE;
    }

  // Close shared memory fd_ServerConnect;
  if (close(_fdServerConnect) == ERROR)
    {
      // perror commented out - cannot unregister atexit functions, after proper cleanup
      // so gives multiple warnings, w/ multiple client commands
      // perror("close fd_ServerConnect - cleanup_shm");  
      status = EXIT_FAILURE;
    }

  return status;
}




//    Cleanup shared memory semaphore only once (server) 
//    before shared memory unlink
int IpcSharedMem::cleanup_shm_sema_server(void)
{
  int status = EXIT_SUCCESS;
  
  if (_progMode == ProgMode::SERVER){
    status = sem_destroy(&_memServerConnect->sem);
    if (status == ERROR)
      {
        perror("sem_destroy");
      }
  }
  return status;
}


int IpcSharedMem::cleanup()
{
  int status = EXIT_SUCCESS;
  
  if (cleanup_shm_sema_server() == EXIT_FAILURE)
    {
      status = EXIT_FAILURE;
    }
  
  if (cleanup_shm() == EXIT_FAILURE)
    {
      status = EXIT_FAILURE;
    }
  
  return status;
}


