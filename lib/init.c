#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/sockio.h>
#include <machine/endian.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/netinet/in.h>
#include <sys/net/if.h>
#include <errno.h>


#define perror(...)	\
	printf("%s", ##__VA_ARGS__); while (1);

#define PRINT_VER()	\
	do {	printf("Hi, buddy!\nWelcome!\nCopyright (C) Shaun Yuan 2012-2015\nshaun at shaunos dot com\nwww.shaunos.com");	\
		fflush(NULL);	\
	} while (0)	

#define PRINT_PROMT()	\
	do {	printf("\nBUDDY# ");	\
		fflush(NULL);	\
	} while (0)


void help();
void ifconfig();
void ls();
void loglevel(char *buf);
void dash();
void makedir(char *buf);
void exec(char *buf);
void cd(char *buf);
char curr_dir[256] = {'/'};



struct	sockaddr_in netmask;

void pwd(char *buf)
{
	printf("%s\n", curr_dir);
}

void cd(char *buf)
{
	if (strcmp(buf, "") == 0)
		return;
	if (strcmp(buf, ".") == 0)
		return;
	if (strcmp(buf, "~") == 0){
		if (chdir("/") == 0) {
			strcpy(curr_dir, "/");
			curr_dir[1] = '\0';
			return;	
		} else 
			goto err_out;
	}
	if (chdir(buf) == 0){
		memset(curr_dir, 0, sizeof(curr_dir));
		strcpy(curr_dir, buf);
		return;
	}
err_out:
	printf("change to directory %s error %s.", buf, strerror(errno));
}

void loglevel(char *buf)
{
	int level = atoi((buf));
	printf("set log level to %d\n", level);
	setloglevel(level);
}



void dash()
{
	extern char **environ;
	char *const *envp = environ;
	char *argv[] = {NULL,0};
	execve("/bin/dash",(char *const *)argv, envp);
	wait(NULL);
}
void makedir(char *buf)
{
	int ret;
	ret = mkdir(buf, S_IRWXU | S_IFDIR);
	if (ret < 0)
		printf("mkdir error:%s\n", strerror(errno));

}

void date(char *buf)
{
	time_t tm;
	time(&tm);
	printf("%s\n", ctime(&tm));
}



void exec(char *buf)
{
	char name[32] = {0};
	extern char **environ;
	char *const *envp = environ;
	char *p;
	char *argv;
	if (strcmp(buf, "") == 0)
		return;
	p  = buf;
	while  (*p == ' ')
		p++;
	argv = strchr(p, ' ');
	if (argv)
		*argv++ = 0;
	if (*p == '/'){
		strcpy(name, p);
	} else {
		strcpy(name, "/bin/");
		strcat(name, p);
	}
	char *argvs[] = {argv, 0};// this should be in a while
	if (execve(name, (char *const *)argvs, envp) < 0)
		return;
	wait(NULL);
}


void ls(char *buf)
{
	char *path = buf;
	DIR *dirp;
	struct dirent *dp;
	struct stat st = {0};
	int ret;
	

	if (strcmp(buf, "") == 0) {
		ret = stat(curr_dir, &st);
		path = curr_dir;
	 } else {
		ret = stat(buf, &st);
		path = buf;
	}
	if (ret < 0) {
		printf("stat error:%s\n", strerror(errno));
		return;
	}

	switch(st.st_mode & S_IFMT) {
	case S_IFDIR:
		goto ls_dir;
	case S_IFBLK:
		printf("b mode:%x size:%d  %s\n", (unsigned long)st.st_mode, st.st_size, buf);
		break;
	case S_IFREG:
		printf("mode:%x size:%d  %s\n", (unsigned long)st.st_mode, st.st_size, buf);
		break;
	case S_IFIFO:
		printf("f mode: %x size:%d  %s\n", (unsigned long)st.st_mode, st.st_size, buf);
		break;
	case S_IFSOCK:
		printf("s mode:%x size:%d  %s\n", (unsigned long)st.st_mode, st.st_size, buf);
		break;
	default:
		printf("mode:%x size:%d  %s\n", (unsigned long)st.st_mode, st.st_size, buf);
		break;
	}

	return;
ls_dir:
	dirp = opendir(path);
	if (dirp == NULL){
		printf("open dir %s faild, %s\n", path, strerror(errno));
		return;
	}
	dp = readdir(dirp);
	while (dp != NULL) 
	{
		dp->d_name[dp->d_namlen] = 0;
		printf("%s ", dp->d_name);
		dp = readdir(dirp);
	}
	printf("\n");
	fflush(NULL);
	closedir(dirp);
	
}
void in_status()
{
	struct sockaddr_in *sin;
	int s, flags;
	struct	ifreq		ifr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("ifconfig: socket");
		exit(1);
	}

	memset(&ifr, 0, sizeof(ifr));

	memcpy(ifr.ifr_name, "eth0", 5);
	printf("name:%s", ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		perror("ioctl (SIOCGIFFLAGS)");
		exit(1);
	}

	flags = ifr.ifr_flags;


	fflush(NULL);
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		} else
			perror("ioctl (SIOCGIFADDR)");
	}
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	printf("\tinet %s ", inet_ntoa(sin->sin_addr));
	//strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
		if (errno != EADDRNOTAVAIL)
			perror("ioctl (SIOCGIFNETMASK)");
		bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
	} else
		netmask.sin_addr =
		    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

	printf("netmask 0x%x ", ntohl(netmask.sin_addr.s_addr));
	if (flags & IFF_BROADCAST) {
		if (ioctl(s, SIOCGIFBRDADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    perror("ioctl (SIOCGIFADDR)");
		}
		//strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		if (sin->sin_addr.s_addr != 0)
			printf("broadcast %s", inet_ntoa(sin->sin_addr));
	}
	putchar('\n');
	fflush(NULL);
	close(s);
}

void if_setaddr(in_addr_t addr)
{
	struct sockaddr_in *sin;
	int fd, ret;
	struct	ifreq	ifr;
	
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		printf("socket error:%d\n", errno);
		return;
	}
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = addr;
	//sin->sin_len = sizeof(struct sockaddr_in);
	
	ret = ioctl(fd, SIOCSIFADDR, (caddr_t)&ifr);
	if (ret < 0) {
		printf("ioctl error:%d\n", errno);
	}
	close(fd);
	return;
}


void ifconfig(char *buf)
{
	char *p = (char *)(buf);
	in_addr_t addr;
	if (*p != '\0') {
		addr = inet_addr(p);
		if (addr == INADDR_NONE) {
			printf("invalid addrress\n");
			return;
		}

		if_setaddr(addr);
		return ;
	}


	in_status();
}

void echo(char *buf)
{
	printf("%s\n", buf);
}

void rm(char *buf)
{
	unlink(buf);
}

void touch(char *buf)
{

}

struct cmd {
	char *c_name;
	void (*c_func)();
	char *c_desc;
} cmds[] = {
		{"help",  help, "print this"},
		{"ifconfig", ifconfig, "get && set interface ip address"},
		{"ls", ls, "list directory or file"},
		{"loglevel", loglevel, "change kernel log level."},
		{"dash", dash, "a dash shell ported by shaun for buddy os"},
		{"mkdir", makedir, "make directory"},
		{"exec", exec, "run a binary file"},
		{"pwd", pwd, "print current working directory"},
		{"cd", cd, "change current working directory"},
		{"echo", echo, "echo "},
		{"date", date, "print date and time"},
		{"rm", rm, "remove file"},
		{"touch", touch, "create a file"},
		{0, 0},
};


void help()
{
	struct cmd *p;
	PRINT_VER();
	printf("command supported:\n");
	for (p=cmds; p->c_name; p++)
		printf("%s 		%s\n", p->c_name, p->c_desc);
}

int main(int argc, char **argv, char **env)
{
	int ret;
	char buf[32] = {0};
	struct cmd *p;
	int fd = open("/dev/tty", O_RDWR, 0);
	if (fd < 0){
		//printf("open tty error:%s\n",strerror(errno));
		return -1;
	}
	dup2(fd, 1);
	dup2(fd, 2);
	//fcntl(fd, F_SETFD, 1);
	do {
		printf("Identification Code:");
		fflush(NULL);
		ret = read(fd, buf, 31);
		if (ret > 0){
			if (strncmp(buf, "icui4cu", 7) != 0){
				printf("error\n");
				fflush(NULL);
				continue;
			}
			//printf("Login OK!\n");
			break;
		}
	}while (1);

	PRINT_VER();

	PRINT_PROMT();
	memset(buf, 0, sizeof(buf));
	while ((ret = read(fd, buf, 31)) > 0) {
		buf[ret-1] = 0;
		for (p=cmds; p->c_name; p++) {
			if (strncmp(p->c_name, buf, strlen(p->c_name)) == 0)
				break;
		}

		if (p->c_func)
			(*p->c_func)(buf + strlen(p->c_name) + 1);
		else {
			exec(buf);
		}

		 next:
		 memset(buf, 0, sizeof(buf));
		 printf("BUDDY# ");	
		 fflush(NULL);
	}
	
	return 0;
}


