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

//#define _GNU_SOURCE               // asprintf

#include "ipcCommon.h"
#include "ipcServer.h"
//#include "ipcClient.h"
#include <unistd.h>               // getopt, STDIN, STDOUT, execlp

/*
  #include <fcntl.h>                // open
  #include <stdlib.h>               // EXIT_FAILURE
  #include <stdio.h>
  //#include <sys/stat.h>             // mkfifo
  #include <errno.h>
  #include <limits.h>               // PATH_MAX
  #include <stdarg.h>               // va_list
  #include <pthread.h>              // Threads

  #include <sys/mman.h>
  #include <string.h>
*/

//#include <semaphore.h>



//
//  progHelp (progname)
//    - display program usage information
//
void progHelp(char *progname)
{
  char const *str1="\n  [ Usage: ]   ";
  char const *str2=" -h -s -c -x \n";
  char const *str3="\n  -h = usage help";
  char const *str4="\n  -s = run as server";
  char const *str5="\n  -c = run as client";
  char const *str10="\n  -x = debug cleanup (cleans IPC remants on crashes)";

  char const *str6="\n\n\n  **** NOTE 1:  All options and commands must be lowercase";
  char const *str7="\n  **** NOTE 2:  Server and Client options are exclusive";
  char const *str8="\n  **** NOTE 3:  Server must be activated prior to Client";
  char const *str9="\n  **** NOTE 4:  To exit - type:  \"exit\"[enter]";
  myprint(11, str1, progname, str2, str3, str4, str5, str10,str6, str7, str8, str9);

  // update to progCmdList (general, w/ Server, Client specialization)
  // s_displayServerCmdList();
  
}


//
//     Main
//       1) process command line options
//       2) process client requests until further notice
//
int main(int argc, char* argv[])
{
  int opt;
  bool helpflg=false;
  ProgMode progMode=ProgMode::UNSET;
  bool debugServerCleanup=false;

  //--------------------------
  // 
  //  Task 1 Process Command Line options
  //
  while ((opt = getopt(argc, argv, "hscx")) != -1)  // -1 end of options
    {
      int size;
      switch(opt) {

      case 'h':      // usage help
        helpflg=true;
        break;

      case 's':      // run as server
        if (progMode == ProgMode::CLIENT)
          {helpflg = true;}
        else
          {progMode = ProgMode::SERVER;}
        break;

      case 'c':      // run as client
        if (progMode == ProgMode::SERVER)
          {helpflg = true;}
        else
          {progMode = ProgMode::CLIENT;}
        break;

      case 'x':     // For Debug Cleanup - Not public to user
        progMode=ProgMode::SERVER;
        debugServerCleanup = true;
        //s_cleanup_shm_sema();
        //cs_cleanup_shm();
        //return 0;
        break;

      default:
        helpflg=1;
      }
    }

  
  //  Display program usage messgae if:
  //    More than one program argument specified
  //    ProgMode was not provided
  //    User requested usage message
  if ( (argc > optind)  ||
       (progMode == ProgMode::UNSET) ||
       helpflg )
    {
      progHelp(argv[0]);
      return 0;
    }
 

  //---------------------------
  //
  //  Task 2  - Server do Serverwork, 
  //            Client do Clientwork

  //int status = (SERVER) ? do_serverMain() :
  //  do_clientMain();

  int status;

  if (progMode == ProgMode::SERVER)
    {
      IpcServer ipcServer;
      if (debugServerCleanup)
        {
          ipcServer.ipc_cleanup_request();
          status = 0;
        }
      else
        {
          status = ipcServer.run();
        }
    }
  else
    {
      /*      IpcClient ipcClient;
              status = ipcClient.run();
      */
    }

  return status;
}
