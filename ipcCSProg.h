#ifndef IPC_CSPROG_H
#define IPC_CSPROG_H

#include <mutex>
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
  // Set ServerIsDown
  void setServerIsDown(bool state);
  
  // Get ServerIsDown
  bool serverIsDown();

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

  ProgMode _progMode;
  bool _serverDown;       // Initally true, false once server is up and running
  std::mutex _serverDown_mutex;

};

#endif // IPC_CSPROG_H
