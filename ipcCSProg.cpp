#include "ipcCSProg.h"

Client_Server_Program::Client_Server_Program(ProgMode mode)
  :_progMode(mode)
{
  setServerIsDown(true);
}


Client_Server_Program::~Client_Server_Program()
{
  std::cout << "Client_Server_Program - Pure virtual destructor called";
}


// Critical Section
// Between main thread (process user commands)
// and worker thread (process client requests)
void Client_Server_Program::setServerIsDown(bool state)
{
  std::lock_guard<std::mutex> lock(_serverDownMutex);
  _serverDown = state;
}


// Critical Section
// Between main thread (process user commands)
// and worker thread (process client requests)
bool Client_Server_Program::serverIsDown()
{
  std::lock_guard<std::mutex> lock(_serverDownMutex);
  return _serverDown;
}


ProgMode Client_Server_Program::get_progMode()
{
  return _progMode;
}


int Client_Server_Program::run()
{
  if (ipc_setup_request(_progMode) != EXIT_SUCCESS)
    {
      return EXIT_FAILURE;
    }

  if (ipc_launch_request_handler() != EXIT_SUCCESS)
    {
      return EXIT_FAILURE;
    }
  
  process_user_cmds();
  
  return EXIT_SUCCESS;
}


int Client_Server_Program::ipc_setup_request()
{
  std::cout << "Client_Server_Program: Pure virtual ipc_setup_request called";
}


int Client_Server_Program::ipc_setup_result()
{
  std::cout << "Client_Server_Program: Pure virtual ipc_setup_result called";
}


int Client_Server_Program::ipc_launch_request_handler()
{
  std::cout << "Client_Server_Program: Pure virtual ipc_launch_request called";
}


int Client_Server_Program::ipc_launch_result_handler()
{
  std::cout << "Client_Server_Program: Pure virtual ipc_launch_result called";
}


void Client_Server_Program::process_user_cmds()
{
  std::cout << "Client_Server_Program:  Pure virtual process_user_cmds called";
}


void Client_Server_Program::ipc_cleanup_request()
{
  std::cout << "Client_Server_Program:  Pure virtual ipc_cleanup_request called";
}


void Client_Server_Program::ipc_cleanup_result()
{
  std::cout << "Client_Server_Program:  Pure virtual ipc_cleanup_result called";
}
