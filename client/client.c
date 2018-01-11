#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

int connectServer(char *sName){
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if(cfd == -1){
        perror("socket");
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    inet_pton(AF_INET, sName, &server.sin_addr);
    socklen_t slen = sizeof(struct sockaddr_in);
    int c = connect(cfd, (struct sockaddr *)&server, slen);
    if(c == -1){
        perror("connect");
    }

    return cfd;
}

char *logInfo[2] = {"用户名或密码错误", "登录成功"};

void requestLog(int cfd){
    User user;
    printf("请输入用户名：");
    scanf("%s", user.name);
    printf("请输入密码：");
    scanf("%s", user.passwd);

    write(cfd, &user, sizeof(User));

    int status;
    read(cfd, &status, sizeof(int));
    printf("%s\n", logInfo[status]);
}

void requestInsert(int cfd){
    printf("请输入要插入的学号、姓名和爱好：");
    Info info;
    scanf("%d %s %s", &info.id, info.name, info.hobby);
    write(cfd, &info, sizeof(Info));
}

void requestDisplay(int cfd){
    Info info;
    int num;
    read(cfd, &num, sizeof(int));
    printf("num:%d\n", num);
    while(num > 0){
        read(cfd, &info, sizeof(Info));
        printf("%d %s %s\n", info.id, info.name, info.hobby);
        num--;
    }
}

void requestModify(cfd){
    Info info;
    printf("请输入要修改的学号：");
    scanf("%d", &info.id);
    printf("请输入要修改的爱好：");
    scanf("%s", info.hobby);
    write(cfd, &info, sizeof(Info));
    int found;
    read(cfd, &found, sizeof(int));
    if(!found){
        printf("要修改的没找到\n");
    }else{
        printf("修改成功\n");
    }
}

void requestDelete(int cfd){
    Info info;
    printf("请输入要删除的条目学号：");
    scanf("%d", &info.id);
    write(cfd, &info, sizeof(Info));
    int found;
    read(cfd, &found, sizeof(int));
    if(!found){
        printf("要删除的没找到\n");
    }else{
        printf("删除成功\n");
    }
}

void requestDownload(int cfd){
    printf("请输入要下载的文件名:");
    char file[64];
    scanf("%s", file);
    write(cfd, file, strlen(file) + 1);
    int found;
    read(cfd, &found, sizeof(int));
    if(!found){
        printf("没有找到要下载的文件\n");
    }else{
        int size;
        read(cfd, &size, sizeof(int));
        printf("size:%d\n", size);
        int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        printf("开始下载\n");
        int total = 0;
        int r;
        char tmp[32];
        while(total < size){
            r = read(cfd, tmp, 32);
            write(fd, tmp, r);
            total += r;
        }
        printf("total:%d\n", total);
        printf("下载完成\n");
        close(fd);
    }
}

void requestExit(int a){
    _exit(0);
}

typedef void (*fptrCmd)(int);
fptrCmd cmds[128];

void initialize(){
    cmds['z'] = requestInsert;
    cmds['c'] = requestDisplay;
    cmds['g'] = requestModify;
    cmds['s'] = requestDelete;
    cmds['d'] = requestDownload;
    cmds['q'] = requestExit;
}

void sendCmd(int cfd){
    printf("请输入命令：z（增加），s（删除），g（修改），c（查看），d（下载），q（退出）:");
    char cmd;
    scanf("%*[^\n]");
    scanf("%*c");
    scanf("%c", &cmd);
    write(cfd, &cmd, sizeof(char));
    fptrCmd cmdFun = cmds[cmd];
    cmdFun(cfd);
}


int main(int argc, char *argv[]){
    initialize();
    int cfd = connectServer(argv[1]);

    requestLog(cfd);

    while(1){
        sendCmd(cfd);
    }

    close(cfd);

    return 0;
}
