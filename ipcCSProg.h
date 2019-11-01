#ifndef IPC_CSPROG_H
#define IPC_CSPROG_H

#include "ipcCommon.h"

//  Client_Server_Program - Abstract Class
class Client_Server_Program
{
 public:
  // Constructor
  Client_Server_Program(ProgMode id);
  
  // Destructor
  virtual ~Client_Server_Program();

  // Run
  int run();
  
 protected:
  // Init
  void init();

  // Setup
  virtual int ipc_setup_request()=0;
  virtual int ipc_setup_result()=0;

  // Launch IPC
  virtual int ipc_launch_request_handler()=0;
  virtual int ipc_launch_result_handler()=0;
    
  // Process User Commands
  virtual void process_user_cmds()=0;
  
  // Cleanup
  virtual void ipc_cleanup_request()=0;
  virtual void ipc_cleanup_result()=0;

  ProgMode _progModeId;
  bool _serverDown;       // Initally server is down, false once server is up and running

};

#endif // IPC_CSPROG_H
