#include "lib/daemon.hpp"
#include <iostream>
#include <thread>

using namespace std;

int main()
{
	Daemon daemon;

  while(1)
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
	return 1;
}
