#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include "ipcCommon.h"
#include "ipcCSProg.h"
#include "ipcSharedMem.h"
#include "ipcFifo.h"


extern const char* const SHMEM_NAME;      // shared memory name & location /dev/shm/sjdavids_SERVER

  
enum class StopOption
  {
    STOP_INTERRUPT,
    STOP_SHUTDOWN
  };


class IpcServer : public Client_Server_Program
{
 public:
  // Constructor
  IpcServer();
  
  // Destructor
  virtual ~IpcServer();
  
  // Process_cmds
  virtual void process_user_cmds();
  
  // Setup
  virtual int ipc_setup_request();
  virtual int ipc_setup_result();
  
  // Launch
  virtual int ipc_launch_request_handler();
  virtual int ipc_launch_result_handler();
  
  // Cleanup
  virtual void ipc_cleanup_request();  
  virtual void ipc_cleanup_result();

 private:
  void displayServerCmdList();
  void taskqShow();
  ClientRequestMsg_t* taskqAdd(ClientRequestMsg_t *cli);
  ClientRequestMsg_t* taskqFind(int clientid);
  int taskqRemove(ClientRequestMsg_t *node);

  void cancelClient(int clientid, int opt);
  void* processClientRequest(void *clientInfo);

  
  IpcSharedMem _ipcRequest;
  IpcFifo      _ipcResult;
 
  ClientRequestMsg_t* _memServerConnect;    // client-server shared memory connection info
  ClientRequestMsg_t* _client_request;      // task queue nodeptr - holds client request info
  ClientRequestMsg_t* _HEAD_taskq=nullptr;  // head task queue list containing all active client requests

};

#endif // IPC_SERVER_H
