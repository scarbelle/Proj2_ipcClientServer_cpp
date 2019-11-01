#ifndef IPC_COMMON_H
#define IPC_COMMON_H


/*********************************************************************/
// Following example from:
//        http://man7.org/linux/man-pages/man3/pthread_create.3.html

#define handle_error_en(en, msg)				\
  do { errno=en; perror(msg); exit(EXIT_FAILURE);} while (0)

/*********************************************************************/

enum class ProgMode
  {
    UNSET,   
    SERVER,
    CLIENT
  };

extern int const ERROR;
extern int const FALSE;
extern int const TRUE;

//
//  Client Request Message 
//    -- This struct type is used to:
//           1) send/receive client task requests in shared memory IPC (server connection point)
//           2) holding active client task requests in server's active client task queue
//
typedef struct ClientRequestMsg
{
  pid_t client_id;              // Client Id
  size_t textlen;               // Client Request - text length
  char text[MAXLINE];           // Client Request - text
  sem_t sem;                    // Semaphore for Shared Memory Exclusivity - server receipt of client msg
  char fifoname[PATH_MAX];      // Unique Fifoname for server->client command output
  int fifo_fd;                  // Fifo fd
  pthread_t tid;                // Thread id for this client request processing
  int tresult;                  // Holds thread status for client request processing - not currently checked
  ClientRequestMsg* next;       // Next server active client task queue entry
  ClientRequestMsg* prev;       // Previous server active client task queue entry
} ClientRequestMsg_t;

typedef sizeof(ClientRequestMsg_t) ClientRequestMsgSz;

#endif // IPC_COMMON_H
