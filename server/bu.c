#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#define maxline_length 1024
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

int profile_data_nitems = 0;

struct data{
  int year;
  int month;
  int day;
};

struct profile{
  int id;
  char school_name[70];
  struct data p1;
  char address[70];
  char *remarks;
};

struct profile profile_data_store[10000];

int subst(char *str, char c1, char c2);
int split(char *str, char *ret[], char sep, int max);
void parse_line(char *line, int c_s);
void exec_command(char *cmd, char *param, int c_s);
void new_profile(struct profile *p, char *line, int c_s);
void cmd_check(int c_s);
void cmd_print(int nitems, int c_s);
void print_profile(struct profile p, int c_s);
void cmd_write(char *filename, int c_s);
void csv_send(int c_s, struct profile *w);

int main(int argc, char* argv[])
{
  pid_t pid;
  int s_s, c_s, PORT_NO, x;
  struct sockaddr_in sa;
  char buf[1024];
  socklen_t clitLen;
  
  if ((s_s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("server socket error\n");
    return 1;
  }
  PORT_NO = atoi(argv[1]);
  memset((char *)&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET; //インターネットドメイン
  sa.sin_addr.s_addr = htonl(INADDR_ANY); //どのIPアドレスでも接続OK
  sa.sin_port = htons(PORT_NO); //接続待ちのポート番号を設定
  
  if (bind(s_s, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    printf("bind error\n");
    return 1;
  }
  
  if (listen(s_s, 5) == -1) {
    printf("listen error\n");
    return 1;
  }

  while (1) {
    clitLen = sizeof(sa);
    if ((c_s = accept(s_s, (struct sockaddr *)&sa, &clitLen)) == -1) {
      printf("accept error\n");
      continue;
    }
    
    pid = fork();
    if (pid == -1){
      printf("can not fork\n");
      break;
    }

    else if (pid == 0){
      while(1){
	memset((char *)&buf, '\0', sizeof(buf));
	if ((x = recv(c_s, buf, sizeof(buf), 0)) == -1) {
	  printf("recv error\n");
	  break;
	} else if (x == 0) {
	  //printf("end\n");
	  break;
	}
	printf("recv message : %s\n", buf);
	parse_line(buf, c_s);
      }
      close(c_s);
      kill(getpid(),SIGINT); 
    }

    /*else printf("parents : %d\n", pid);*/
  }

  close(s_s);
  return 0;
}

int subst(char *str, char c1, char c2)
{
  int i=0;
  for (; *str != '\0'; str++ ){
    if (*str == c1){
      *str = c2;
      i++;
    }
  }
  return i;
}
int split(char *str, char *ret[], char sep, int max)
{
  int n = 0;

  ret[n] = str;
  while (*str != '\0' && n<max){
    if (*str == sep) {
      *str = '\0';
      n++;
      ret[n] = str + 1;
    }
    str++;
  }
  return n;
}

void parse_line(char *line, int c_s)
{
  int n;
  char *ret[2] = {0};

  if (*line == '%') {
    n = split(line + 1, ret, ' ', 2);
    if(n == 0){
      return exec_command(ret[0], '\0', c_s);
    }
    return exec_command(ret[0], ret[1], c_s);
  } else {
    if (profile_data_nitems == 10000) {
      printf("You can't add data any more.\n");
      return;
    }
    new_profile(&profile_data_store[profile_data_nitems++], line, c_s);
  }
}

void new_profile(struct profile *p, char *line, int c_s)
{
  int tmp;
  char *ret[5], *bret[3];
  char res[1024];
  memset((char *)&res, '\0', sizeof(res));

  tmp = split(line, ret, ',', 5);
  if (tmp <= 3) {
    profile_data_nitems--;
    sprintf(res, "profile error");  
    if ((send(c_s, res, strlen(res), 0)) == -1) {
      printf("send error\n");
    }
    return;
  }
  
  tmp = split(ret[2], bret, '-', 3);
  if (tmp <= 1) {
    profile_data_nitems--;
    sprintf(res, "profile error");
   if ((send(c_s, res, strlen(res), 0)) == -1) {
      printf("send error\n");
    }
    return;
  }
  tmp = strlen(ret[1]);
  if (tmp > 70) {
    profile_data_nitems--;
    sprintf(res, "profile error");
    if ((send(c_s, res, strlen(res), 0)) == -1) {
      printf("send error\n");
    }
    return;
  }
  tmp = strlen(ret[3]);
  if (tmp > 70) {
    profile_data_nitems--;
    sprintf(res, "profile error");
    if ((send(c_s, res, strlen(res), 0)) == -1) {
     printf("send error\n");
   }
    return;
  }

  p->p1.year = atoi(bret[0]);
  p->p1.month = atoi(bret[1]);
  p->p1.day = atoi(bret[2]);
  p->remarks = (char *)malloc(sizeof(char) *(strlen(ret[4])+1));

  p->id = atoi(ret[0]);
  strncpy(p->school_name,ret[1],70);
  strncpy(p->address,ret[3],70);
  strcpy(p->remarks,ret[4]);
  
  sprintf(res, "200");
  if ((send(c_s, res, strlen(res), 0)) == -1) {
    printf("send error\n");
    return;
  }
}

void exec_command(char *cmd, char *param, int c_s)
{
  int i;
  char list[10][3] = {{"C"}, {"P"}, {"W"}, {"R"}};
  char res[1024];
  memset((char *)&res, '\0', sizeof(res));

  for (i = 0; i < 4; i++) {
    if(strcmp(cmd, list[i]) == 0)break;
  }
    switch(i) {
    case 0: cmd_check(c_s); break;
    case 1: cmd_print(atoi(param), c_s); break;
    case 2: cmd_write(param, c_s); break;
    case 3: break;
    default:
      sprintf(res, "Invalid command : %s.", cmd);
      if ((send(c_s, res, sizeof(res), 0)) == -1) {
	printf("send error\n");
	return;
      }
      break;
  }
}

void cmd_check(int c_s)
{
  char res[1024];
  memset((char *)&res, '\0', sizeof(res));
  sprintf(res, "%d profile(s)", profile_data_nitems);
  if ((send(c_s, res, sizeof(res), 0)) == -1) {
    printf("send error\n");
    return;
  }
}

void cmd_print(int nitems, int c_s)
{
  int i, start = 0, end = 0;
  char res[1024];
  memset((char *)&res, '\0', sizeof(res));

  if (profile_data_nitems == 0) {
    sprintf(res, "# %d", end-start);
    if ((send(c_s, res, sizeof(res), 0)) == -1) {
      printf("send error\n");
      return;
    }
    return;
  }
  
  if (nitems > 0) {
    start = 0;
    end = min(nitems, profile_data_nitems);
    sprintf(res, "# %d", end - start);
    if ((send(c_s, res, sizeof(res), 0)) == -1) {
      printf("send error\n");
      return;
    }
    for (i = start; i < end; i++) {
      print_profile(profile_data_store[i], c_s);
    }
  } else if (nitems < 0) {
    start = max(0, profile_data_nitems + nitems);
    end = profile_data_nitems - 1;
    sprintf(res, "# %d", end - start + 1);
    if ((send(c_s, res, sizeof(res), 0)) == -1) {
      printf("send error\n");
      return;
    }
    for (i = end; i >= start; i--) {
      print_profile(profile_data_store[i], c_s);
    }
  } else {
    start = 0;
    end = profile_data_nitems;
    sprintf(res, "# %d", end - start);
    if ((send(c_s, res, sizeof(res), 0)) == -1) {
      printf("send error\n");
      return;
    }
    for (i = start; i < end; i++) {
      print_profile(profile_data_store[i], c_s);
    }
  }
}

void print_profile(struct profile p, int c_s)
{
  char res[1024];
  memset((char *)&res, '\0', sizeof(res));

  sprintf(res, "Id    : %d\nName  : %s\nBirth : %d-%02d-%02d\nAddr  : %s\nCom.  : %s\n ", 
	  p.id, p.school_name, p.p1.year, p.p1.month, p.p1.day, p.address, p.remarks);

  if ((send(c_s, res, sizeof(res), 0)) == -1) {
    printf("send error\n");
    return;
  }
}
void cmd_write(char *filename, int c_s)
{
  int i;
  char buf[10] = "\a";
  for (i = 0; i < profile_data_nitems; i++) {
    csv_send(c_s, &profile_data_store[i]);
  }
  if (send(c_s, buf, strlen(buf), 0) == -1) {
    printf("send error\n");
    return;
  }
}

void csv_send(int c_s, struct profile *w)
{
  char buf[1024];
  int i;
  sprintf(buf, "%d,%s,%04d-%02d-%02d,%s,%s\n",
	  w->id, w->school_name,w->p1.year,w->p1.month,w->p1.day,
	  w->address,w->remarks); 
  
  if (send(c_s, buf, strlen(buf), 0) == -1) {
    printf("send error\n");
    return;
  }
}
