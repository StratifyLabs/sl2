#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <var/String.hpp>

class Process {
public:
  Process();

  int execute();

private:
  API_AC(Process, var::String, path);
  API_AC(Process, var::StringList, environment);
};

#endif // PROCESS_HPP
