#ifndef IPC_SHARED_MEM_H
#define IPC_SHARED_MEM_H

#include "ipcCommon.h"
//#include "ipcServer.h"

class IpcSharedMem
{
 public:
  IpcSharedMem();
  ~IpcSharedMem();

  void init(ProgMode mode,
            void (*setServerIsDownFunc)(bool state),
            bool (*serverIsDownFunc)(void));
  int setupSharedMemAccess();
  void* unloadClientRequests(void* (*processFunction)(void *clientReq));
  int loadClientRequest();
  int cleanup();
  //  void cleanup_client();

  // Private
  void (*_setServerIsDown)(bool state);
  bool (*_serverIsDown)(void);
  int cleanup_shm_sema_server(void);
  int cleanup_shm(void);

  
  ProgMode _progMode;
  bool _serverDown;
  int _fdServerConnect;
  ClientRequestMsg_t* _memServerConnect;   //
};


#endif //IPC_SHARED_MEM_H
