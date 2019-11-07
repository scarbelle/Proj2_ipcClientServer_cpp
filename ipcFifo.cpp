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


// #define _GNU_SOURCE               // asprintf
// #include <fcntl.h>                // open
// #include <stdlib.h>               // EXIT_FAILURE
// #include <stdio.h>
// #include <unistd.h>               // STDIN, STDOUT, execlp
// #include <sys/stat.h>             // mkfifo
// #include <sys/types.h>            // mkfifo
// #include <errno.h>
// #include <limits.h>               // PATH_MAX
// #include <stdarg.h>               // va_list
// #include <pthread.h>              // Threads

// #include <sys/mman.h>
// #include <string.h>
// #include <semaphore.h>

// #include "ipcCommon.h"        // MAXLINE,



//---------------------------------------------------------------------------------------------------------
//  GLOBALS

// int serverflg=UNSET;             // program mode - SERVER=1, CLIENT=0
// int serverdown=TRUE;             // Initally server is down, false once server is up and running
// int fd_ServerConnect;            // fd for client-server shared memory connection point
// int SERVER_STOP = FALSE;         // Server reads clients msg indefinitely, stop at error, exit, shutdown
// int CLIENT_SHUTDOWN = FALSE;     // Set TRUE when server requests client shutdown
//----------------------------------------------------------------------------------------------------------
//     CLIENT, SERVER - Request Connection Access Point - Shared Memory IPC 
//
 

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
  *fifo_fd = -1;              // Unset
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



