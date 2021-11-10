//server_chat
#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<unistd.h>
#include<string.h>
#include<linux/stat.h>
#define FIFO_FILE "MYFIFO"
int main(void)
{
   FILE *fp;
   char readbuf[80],a[40];
   umask(0);
   mknod(FIFO_FILE,S_IFIFO|0666,1);
   start:
      fp=fopen(FIFO_FILE,"r");
      fgets(readbuf,80,fp);
      printf("Him :%s\n",readbuf);
      fclose(fp);
      sleep(1);
      printf("Me : ");
      gets(a);
      fp=fopen(FIFO_FILE,"r+");
      if(a[strlen(a)-1]=='.')
      {
         fputs(a,fp);
         fclose(fp);
         return 0;
       }
      fputs(a,fp);
      fclose(fp);
      sleep(1);
      goto start;
   return 0;
}


