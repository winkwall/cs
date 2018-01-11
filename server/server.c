#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <dirent.h>

#include "../t_net.h"

typedef struct _user{
    char name[32];
    char passwd[16];
} User;

typedef struct _info{
    int id;
    char name[32];
    char hobby[32];
} Info;

User users[4] = {{"蔡勇", "1111"}, {"周沅沅", "2222"}, {"赵庆涛", "6666"}, {"夏聪", "7777"}};

int prepareServer(){
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t slen = sizeof(struct sockaddr_in);
    bind(sfd, (struct sockaddr *)&server, slen);

    listen(sfd, 10);
    return sfd;
}

int getClient(int sfd){
    struct sockaddr_in client;
    socklen_t clen = sizeof(struct sockaddr_in);
    int cfd = accept(sfd, (struct sockaddr *)&client, &clen);
    char IP[32];
    printf("%s\n", inet_ntop(AF_INET, &client.sin_addr, IP, 32));
    return cfd;
}

int checkUser(User *user){
    int size = sizeof(users) / sizeof(User);
    int i;
    for(i = 0; i < size; i++){
        if(strcmp(users[i].name, user->name) == 0 &&
                strcmp(users[i].passwd, user->passwd) == 0){
            return 1;
        }
    }
    return 0;
}

void responseLog(int cfd){
    User user;
    read(cfd, &user, sizeof(User));

    int status = checkUser(&user);
    write(cfd, &status, sizeof(int));
}

char recvCmd(int cfd){
    char cmd;
    read(cfd, &cmd, sizeof(char));
    return cmd;
}

void insert(int cfd){
    Info info;
    read(cfd, &info, sizeof(Info));
    int fd = open("./data.txt", O_WRONLY | O_CREAT | O_APPEND, 0664);
    write(fd, &info, sizeof(Info));
    close(fd);
    int ifd = open("./items", O_RDWR | O_CREAT, 0664);
    int num;
    int r = read(ifd, &num, sizeof(int));
    if(r == 0){
        num = 0;
    }
    num++;
    printf("num:%d\n", num);
    lseek(ifd, 0, SEEK_SET);
    write(ifd, &num, sizeof(int));
    close(ifd);
}

void display(int cfd){
    int fd = open("./data.txt", O_RDONLY | O_CREAT);
    int ifd = open("./items", O_RDONLY | O_CREAT);
    int num;
    read(ifd, &num, sizeof(int));
    printf("num:%d\n", num);
    write(cfd, &num, sizeof(int));
    int r;
    Info info;
    while((r = read(fd, &info, sizeof(Info))) != 0){
        //printf("info:%d %s %s\n", info.id, info.name, info.hobby);
        int w = write(cfd, &info, sizeof(Info));
    }
    close(ifd);
    close(fd);
}

void modify(int cfd){
    Info tmp;
    read(cfd, &tmp, sizeof(Info));
    int fd = open("./data.txt", O_RDWR | O_CREAT);
    int r;
    Info info;
    int found;
    while(1){
        r = read(fd, &info, sizeof(Info));
        if(r == 0){
            found = 0;
            write(cfd, &found, sizeof(int));
            break;
        }
        if(tmp.id == info.id){
            found = 1;
            write(cfd, &found, sizeof(int));
            strcpy(info.hobby, tmp.hobby);
            lseek(fd, -r ,SEEK_CUR);
            write(fd, &info, sizeof(info));
            break;
        }
    }
    close(fd);
}

void delete(int cfd){
    Info tmp;
    read(cfd, &tmp, sizeof(Info));
    int fd = open("./data.txt", O_RDWR | O_CREAT);
    int r;
    Info info;
    int found;
    int count = 0;
    while(1){
        r = read(fd, &info, sizeof(Info));
        count += r;
        if(r == 0){
            found = 0;
            write(cfd, &found, sizeof(int));
            break;
        }
        if(tmp.id == info.id){
            Info info1;
            found = 1;
            write(cfd, &found, sizeof(int));
            int r1 = read(fd, &info1, sizeof(Info));
            while(r1 != 0){
                lseek(fd, -(r1 + r), SEEK_CUR);
                write(fd, &info1, sizeof(Info));
                lseek(fd, r1, SEEK_CUR);
                r = r1;
                count += r;
                r1 = read(fd, &info1, sizeof(Info));
            }
            count -= r;
            ftruncate(fd, count);
            int ifd = open("./items", O_RDWR | O_CREAT);
            int num;
            int ir = read(ifd, &num, sizeof(int));
            if(ir > 0){
                num--;
                printf("num:%d\n", num);
                lseek(ifd, 0, SEEK_SET);
                write(ifd, &num, sizeof(int));
            }
            close(ifd);
            break;
        }
    }
    close(fd);
}

void download(int cfd){
    char file[64];
    struct stat buf;
    read(cfd, file, 64);
    int found;
    DIR *dirp = opendir(".");
    struct dirent *dp;
    while(1){
        dp = readdir(dirp);
        if(dp == NULL){
            found = 0;
            break;
        }
        if(strcmp(file, dp->d_name) == 0){
            stat(file, &buf);
            found = 1;
            break;
        }
    }
    write(cfd, &found, sizeof(int));
    if(found == 1){
        int size = buf.st_size;
        write(cfd, &size, sizeof(int));
        int fd = open(file, O_RDONLY);
        char tmp[32];
        int r;
        while((r = read(fd, tmp, 32)) > 0){
            write(cfd, tmp, r);
        }
        close(fd);
    }
}

typedef void (*fptrCmd)(int);
fptrCmd cmds[128];

void initialize(){
    cmds['z'] = insert;
    cmds['c'] = display;
    cmds['g'] = modify;
    cmds['s'] = delete;
    cmds['d'] = download;
}

void responseInfo(int cfd, char cmd){
    fptrCmd cmdFun = cmds[cmd];
    cmdFun(cfd);
}

int main(void){
    initialize();
    int sfd = prepareServer();
    while(1){
        int cfd = getClient(sfd);
        pid_t pid = fork();

        if(pid == 0){
            close(sfd);

            responseLog(cfd);
            char cmd = recvCmd(cfd);
            while(1){
                printf("收到cmd:%c\n", cmd);
                if(cmd == 'q'){
                    break;
                }
                responseInfo(cfd, cmd);
                cmd = recvCmd(cfd);
            }

            close(cfd);
            exit(0);
        }else{
            close(cfd);
            waitpid(-1, NULL, WNOHANG);
        }

    }

    return 0;
}
