#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

int nloop = 50;

/**********************************************************\
 * Function: increment a counter by some amount one by one *
 * argument: ptr (address of the counter), increment       *
 * output  : nothing                                       *
 **********************************************************/
void add_n(int *ptr, int increment){
  int i,j;
  for (i=0; i < increment; i++){
    *ptr = *ptr + 1;
    for (j=0; j < 1000000;j++);
  }
}

int main(){
  int pid;        /* Process ID                     */

  int *countptr;  /* pointer to the counter         */

  int *turn;  /* turn pointer - made by me */

  bool *interested[2];    /* pointer array - made by me */


  int fd;     /* file descriptor to the file "containing" my counter */
  int fd1;    /* file descriptor to the file containing interested[0] - made by me*/
  int fd2;    /* file descriptor to the file containing interested[1] - made by me*/
  int fdt;    /* file descriptor to the file containing turn - made by me*/
  int none = 0; /* a variable containing 0 - made by me */

  system("rm -f counter");
  system("rm -f turn"); /* made by me */
  system("rm -f interested[0]"); /* made by me */
  system("rm -f interested[1]"); /* made by me */

  /* create a file which will "contain" my shared variable */
  fd = open("counter",O_RDWR | O_CREAT);
  write(fd,&none,sizeof(int));

  fd1 = open("interested[0]",O_RDWR | O_CREAT); /* made by me */
  write(fd1,&none,sizeof(bool)); /* made by me */

  fd2 = open("interested[1]",O_RDWR | O_CREAT); /* made by me */
  write(fd2,&none,sizeof(bool)); /* made by me */

  fdt = open("turn",O_RDWR | O_CREAT); /* made by me */
  write(fdt,&none,sizeof(int)); /* made by me */

  /* map my file to memory */
  countptr = (int *) mmap(NULL, sizeof(int),PROT_READ | PROT_WRITE, MAP_SHARED, fd,0);

  turn = (int *) mmap(NULL, sizeof(int),PROT_READ | PROT_WRITE, MAP_SHARED, fdt,0); /* made by me */

  interested[0] = (bool *) mmap(NULL, sizeof(bool),PROT_READ | PROT_WRITE, MAP_SHARED, fd1,0); /* made by me */

  interested[1] = (bool *) mmap(NULL, sizeof(bool),PROT_READ | PROT_WRITE, MAP_SHARED, fd2,0); /* made by me */
 


 
  if (!countptr) {
    printf("Mapping failed\n");
    exit(1);
  }
  *countptr = 0;

  close(fd);

  if (!turn) {   /* made by me */
    printf("Mapping failed\n");  /* made by me */
    exit(1);  /* made by me */
  }
  *turn = 0;  /* made by me */

  close(fdt);  /* made by me */


  if (!interested[0]) {   /* made by me */
    printf("Mapping failed\n");   /* made by me */
    exit(1);   /* made by me */
  }
  *interested[0] = false; /* made by me */
  close(fd1);   /* made by me */

   if (!interested[1]) {  /* made by me */
    printf("Mapping failed\n");  /* made by me */
    exit(1);  /* made by me */
  }
  *interested[1] = false;  /* made by me */
  close(fd2);  /* made by me */

  setbuf(stdout,NULL);

  pid = fork();
  if (pid < 0){
    printf("Unable to fork a process\n");
    exit(1);
  }

  if (pid == 0) {
    /* The child increments the counter by two's */

    while(*countptr < nloop) {

      *interested[0] = true; /* made by me */
      *turn = 1; /* made by me */ 

      while ((*turn == 1) && (*interested[1]));
        if (*countptr < nloop){

          add_n(countptr,2);
          printf("Child process -->> counter= %d\n",*countptr);
        
          *interested[0] = false;  /* made by me */
          close(fd);
        }
    }
  }
  else {
    /* The parent increments the counter by twenty's */

    while(*countptr < nloop) {

      *interested[1] = true;  /* made by me */
      *turn = 0;  /* made by me */

      while((*turn == 0) && (*interested[0]));
        if (*countptr < nloop){

          add_n(countptr,20);
          printf("Parent process -->> counter = %d\n",*countptr);
        
          *interested[1] = false;
          close(fd);
        }
    }
  }
}









