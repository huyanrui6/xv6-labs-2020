#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x) {
  return x+3;
}

int f(int x) {
  return g(x);
}

void main(void) {
  // RISC V is little endian
  // from low addr 72=r 6c=l 64=d 00=end 
  unsigned int i = 0x00646c72; 
	printf("H%x Wo%s\n", 57616, &i); // HE110 World
  int c1 = 0x64; // d
  int c2 = 0x6c; // l
  int c3 = 0x72; // r
  printf("%c%c%c\n",c1,c2,c3); // dlr
  printf("%d %d\n", f(8)+1, 13);
  // if big endian
  // from high addr
  // unsigned int j = 0x726c6400; 
	// printf("H%x Wo%s\n", 57616, &j);
  exit(0);
}
