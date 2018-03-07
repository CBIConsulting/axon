#include <exception>
#include <string>

class BasicException : public std::exception
{
 public:
  BasicException(const std::string &message, int code=0): _msg(message), _code(code)
  {
  }

  BasicException(const char* message, int code=0):_msg(message), _code(code)
  {
  }

  virtual ~BasicException() throw ()
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

class CapidAuthException : public BasicException
{
public:
  /**
   * CapidAuthException
   *
   * @param code      Error code
   * @param message   Error message
   *
   */
  CapidAuthException(const int& code, const std::string &message): BasicException(message, code)
  {
  }

  virtual ~CapidAuthException() throw ()
  {
  }
};

class DatabaseException : public BasicException
{
public:
  /**
   * DatabaseException
   *
   * @param code      Error code
   * @param message   Error message
   *
   */
  DatabaseException(const int& code, const std::string &message): BasicException(message, code)
  {
  }

  virtual ~DatabaseException() throw ()
  {
  }
};

class SQLiteException : public BasicException
{
public:
  /**
   * SQLiteException
   *
   * @param code      Error code
   * @param message   Error message
   *
   */
  SQLiteException(const int& code, const std::string &message): BasicException(message, code)
  {
  }

  virtual ~SQLiteException() throw ()
  {
  }
};

class CapidModuleException : public BasicException
{
public:
  /**
   * CapidModuleException
   *
   * @param code      Error code
   * @param message   Error message
   *
   */
  CapidModuleException(const int& code, const std::string &message): BasicException(message, code)
  {
  }

  virtual ~CapidModuleException() throw ()
  {
  }
};
