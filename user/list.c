// list file names in current directory

#include "kernel/types.h"
#include "user/user.h"

struct dirent
{
  ushort inum;
  char name[14];
};

int main()
{
  int fd;
  struct dirent de;

  fd = open(".",0);
  
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.name[0] != '\0'){
      printf("%s\n", de.name);
    }
  }
  exit(0);
}