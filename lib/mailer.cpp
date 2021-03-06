#include "mailer.hpp"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/random.h>
#include <sys/ioctl.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <deque>
#include "glove/utils.hpp"
#include "cutils.hpp"

#define RANDOM_DEVICE "/dev/urandom"

namespace 
{
	const char dirSlash = '/';

  int generate_random(unsigned char *buffer, int len)
  {
    int rfd = open(RANDOM_DEVICE, O_RDONLY);

		if (rfd == -1)
			return 0;

    int generated = 0;
    int entropy;
    int total;

    if (ioctl(rfd, RNDGETENTCNT, &entropy) == -1)
      return 0, close(rfd);

    while ( (generated<len) && ((total=read(rfd, (buffer+generated), len-generated)) > 0) )
      {
				generated+=total;
      }

		close(rfd);
    if (total == -1)
      return 0; 

    return 1;
  }

	std::string pathSlash(std::string& path)
	{
		trim(path);
                
		if (path.empty())
			return path;

		if (path.back()!=dirSlash)
			path+=dirSlash;

		return path;
	}

	std::deque<std::string> tokenize(const std::string& str, char delimiter)
	{
		std::deque<std::string> ret; 
		std::string::const_iterator i = str.begin();

		while (i != str.end())
			{
				i = find_if(i, str.end(), [delimiter](char c)   {
						return (c!=delimiter);
					}); 
				std::string::const_iterator j = find_if(i, str.end(), [delimiter](char c)   {
						return (c==delimiter);
					}); 
				if (i != str.end())         
					ret.push_back(std::string(i, j)); i = j;
			}
		return ret;

	}

  bool findSendMail(std::string &smFile)
  {
		static char* _envpath = getenv("PATH");
		std::string envpath = (_envpath == NULL)?"":_envpath;
		std::list<std::string> paths;
		
		if (envpath.empty())
			{
				paths.insert(paths.end(), "/usr/local/bin/sendmail");
				paths.insert(paths.end(), "/usr/local/sbin/sendmail");
				paths.insert(paths.end(), "/usr/bin/sendmail");
				paths.insert(paths.end(), "/usr/sbin/sendmail");
				paths.insert(paths.end(), "/bin/sendmail");
				paths.insert(paths.end(), "/sbin/sendmail");
			}
		else
			{
				auto tokens = tokenize(envpath, (char)':');
				for (auto token: tokens)
					paths.insert(paths.end(), pathSlash(token)+"sendmail");
			}

		return CUtils::findFile(smFile, paths, {});
  }
	
	std::string sendMailPath;
	std::string getSendMail()
	{
		if (!sendMailPath.empty())
			return sendMailPath;

		if (findSendMail(sendMailPath))
			return sendMailPath;
		
		return "";
	}
}
namespace Mailer
{
  int sendmail(const char *from, const char* to, const char* subject, char* body, const char* headers)
  {
    char messageId[200];
    char fromcopy[200];
    unsigned char randoms[10];
    time_t t;
    struct tm *tmp;
    char* stmp;

    strcpy(fromcopy, from);
    char* at = (char*)strchr(to, '@');
    if (at == NULL)				/* no @ in to */
      return -1;

    at = strchr(fromcopy, '@');
    if (at == NULL)				/* no @ in from */
      return -2;

    stmp = strchr(at, '>');
    if (stmp!=NULL)
      *stmp='\0';

    if (!generate_random(randoms, 10)) /* Generate random numbers for Message-ID */
      return -3;

    t = time(NULL);
    tmp = localtime(&t);

    sprintf(messageId, "%02x%02x%02x%02x%02x%02x.%d%d%d%d%d%d.%02x%02x%02x%02x%s", 
	    randoms[0], randoms[1],randoms[2],randoms[3],randoms[4],randoms[5],
	    1900+tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
	    randoms[6],randoms[7],randoms[8],randoms[9], at);

		std::string sendMailExec = getSendMail();
		if (sendMailExec.empty())
			return -6;								/* No sendmail exec found */

    FILE* sendmail = popen (sendMailExec.c_str(), "w");
    if (!sendmail)
      return -4;

    fprintf (sendmail,"To: %s\nFrom: %s\nSubject: %s\nMessage-Id: <%s>\n%s", to, from, subject, messageId, headers);

    if ( (strlen(headers)>0) && (headers[strlen(headers)-1]!='\n') )
      fprintf (sendmail,"\n");

    fprintf (sendmail,"\n%s\n", body);

    int res = pclose(sendmail);
    if (res == -1)
      return -5;

    return 1;
  }

  int sendmail(const std::string &from, const std::string& to, const std::string& subject, const std::string& body)
  {
    return (sendmail(from.c_str(), to.c_str(), subject.c_str(), (char*) body.c_str(), (char*)"User-agent: Axond 0.5Beta\nX-Priority: 1 (Highest)\nImportance: High\n"));
  }

}
