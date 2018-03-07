#include "os.hpp"
#include "basic_exception.hpp"
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include <vector>
#include <iostream>

void OS::setuser(std::string userName)
{
	struct passwd* pw = getpwnam(userName.c_str());
	if (pw == NULL)
		throw BasicException("User "+userName+" not found", 200);
	if (setuid(pw->pw_uid) !=0)
		throw BasicException("Cant set UID to user "+userName+" ("+std::to_string(pw->pw_uid)+")", 201);
}

void OS::setgroup(std::string groupName)
{
	struct group* grp = getgrnam(groupName.c_str());
	if (grp == NULL)
		throw BasicException("Group "+groupName+" not found", 202);
	if (setgid(grp->gr_gid) !=0)
		throw BasicException("Cant set GID to group "+groupName+" ("+std::to_string(grp->gr_gid)+")", 203);	
}

OS::ExecData OS::execute(std::string executable, std::list<std::string> arguments, std::string stdin)
{
	ExecData ed;
	ed.status = -1;
	std::vector<char*> av;
	av.push_back((char*)executable.c_str());
	for (auto a : arguments)
		av.push_back((char*)a.c_str());
	av.push_back(0);
	int inpipe[2], outpipe[2], errpipe[2];
	if (!stdin.empty())
		{
			if (pipe(inpipe)<0)
				throw BasicException("Could not initialize input pipe", 206);
		}
	if (pipe(outpipe)<0)
		throw BasicException("Could not initialize output pipe", 207);
	if (pipe(errpipe)<0)
		throw BasicException("Could not initialize error pipe", 208);
	
	auto son = fork();
	if (son<0)
		return ed;

	else if (son>0)
		{
			int status;
			if (!stdin.empty())
				{
					if (close(inpipe[0])<0)
						throw BasicException("Error in input pipe", 209);
					if (write (inpipe[1], stdin.c_str(), stdin.length()+1)<0)
						throw BasicException("Error writing to pipe", 209);
					if (close(inpipe[1])<0)
						throw BasicException("Error in input pipe", 209);
				}
			close(outpipe[1]);
			close(errpipe[1]);
			std::string buffer(1024, '\0');
			ssize_t size;
			while ((size = read(outpipe[0], &buffer[0], 1023))>0)
				{
					buffer[size]='\0';
					ed.stdout+=&buffer[0];
				}
			close(outpipe[0]);
			while ((size = read(errpipe[0], &buffer[0], 1023))>0)
				{
					buffer[size]='\0';
					ed.stderr+=&buffer[0];
				}
			close(errpipe[0]);
			auto _son = ::wait(&status);
			if (WIFEXITED(status))
				{
					ed.status=0;
					ed.exitCode=WEXITSTATUS(status);
				}
			else if (WIFSIGNALED(status))
				{
					ed.status=-2;
					ed.exitCode = WTERMSIG(status);
				}
			/* Father */
			return ed;
		}
	else
		{
			/* Child */
			if (!stdin.empty())
				{
					if (close(inpipe[1])<0)
						std::abort();
					if (close(STDIN_FILENO)<0)
						std::abort();
					if (dup(inpipe[0])<0)
						std::abort();
					if (close(inpipe[0])<0)
						std::abort();
				}
			if (close(STDOUT_FILENO)<0)
				std::abort();
			if (dup(outpipe[1])<0)
				std::abort();
			if (close(outpipe[1])<0)
				std::abort();
			if (close(STDERR_FILENO)<0)
				std::abort();
			if (dup(errpipe[1])<0)
				std::abort();
			if (close(errpipe[1])<0)
				std::abort();

			if (execvp(executable.c_str(), &av[0])<0)
				{
					std::terminate();
				}
		}
}
