#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>

#define REAL_EXECUTABLE "/app/devices-bin/noflag.sh"

int main(int argc, char** argv, char** envp)
{
   setreuid(geteuid(), geteuid());
   setregid(getegid(), getegid());

   execvpe(REAL_EXECUTABLE, argv, envp);

   return 0;
}
