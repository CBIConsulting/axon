#include <exception>
#include <string>

class ModuleException : public std::exception
{
 public:
  ModuleException(const std::string &message, int code=0): _msg(message), _code(code)
  {
  }

  ModuleException(const char* message, int code=0):_msg(message), _code(code)
  {
  }

  virtual ~ModuleException() throw ()
    {
    }

  virtual const char* what() const throw ()
    {
      return _msg.c_str();
    }

  int code() const
  {
    return _code;
  }

 protected:
  std::string _msg;
  int _code;
};
