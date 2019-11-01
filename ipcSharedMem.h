#ifndef IPC_SHARED_MEM_H
#define IPC_SHARED_MEM_H

#include "common.h"
#include "ipcServer.h"

class IpcSharedMem
{
 public:
  IpcSharedMem();
  ~IpcSharedMem();

  int setupSharedMemAccess(ClientRequestMsg_t*& memServerConnect);
  void* unloadClientRequests(void (*processFunction)());
  int loadClientRequest();
  void cleanup_server();
  void cleanup_client();

  // Private
  static void server_cleanup_shm_sema(void);
  static void cleanup_shm(void);

  
  ProgMode _progModeId;
  bool _serverDown;
  int _fdServerConnect;
  ClientRequestMsg_t* _memServerConnect;   //
};


#endif //IPC_SHARED_MEM_H
