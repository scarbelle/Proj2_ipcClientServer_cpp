#include "ipcCSProg.h"

Client_Server_Program::Client_Server_Program(ProgMode id)
  :_progModeId(id), _serverDown(True)
{

}


Client_Server_Program::~Client_Server_Program()
{
  std::cout << "Client_Server_Program - Pure virtual destructor called";
}


bool Client_Server_Program::serverIsDown()
{
  return _serverDown;
}


ProgMode Client_Server_Program::get_progModeId()
{
  return _progModeId;
}


int Client_Server_Program::run()
{
  if (ipc_setup_request() != EXIT_SUCCESS)
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
