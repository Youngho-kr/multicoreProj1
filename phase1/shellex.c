/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128

#include <stdio.h>

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

/* My function */
void removeEnter(char *cmdline);                                        /* 문자열의 마지막 '\n' 제거 */

void openHistory();                                            /* histor명령어 입력 */    
void writeHistory(char *cmdline);                                       /* history에 저장 */ 
void callHistory(int index, char *argv);                                /* history읽어옴 */

void checkBuiltin(int bg, char **argv, char *cmdline);                  /* command 명령어 처리 */
void checkcmdline(char *cmdline);                                       /* !! !# replace */
void changeStr(char *cmdline, int i, int j, char *str);                 /* !! !# 에 맞게 문자열 변경 */

int checkPipe(int bg, char **argv, char *cmdline);                      /* | 입력 있는 지 확인 */

void call_pipe(int bg, char **argv, char *cmdline, int pipe_index);     /* | 입력 시 처리 */

void sigint_handler(int sg);                                            /* sigint_handler */
void sigchld_handler(int sg);                                           /* sigchld_handler */
void sigtstp_handler(int sg);                                           /* sigtstp_handler */

void printJobs();                                                       /* background에 실행 중인 process 출력 */
void printJob(int pid);                                                 /* 해당 process 출력 */

/*
실행중인 process 정보를 linked list로 저장
num : index
pid : pidnumber
status : 0 -> suspended
         1 -> running
processName : processname
next : next list
*/
typedef struct process
{
    int num;
    int pid;
    int status;
    char processName[FILENAME_MAX];
    struct process *next;
} process;

process *bg_process;                                                    /* background process head */

void addProcess(process *new_process);                                  /* bg_process의 마지막에 process 추가 */
void deleteProcess(process *prev, process *cur);                        /* bg_process의 cur제거 */

void foreground(char **argv);                                           /* fg %# 입력 시 호출 */
void background(char **argv);                                           /* bg %# 입력 시 호출 */
void killProcessIndex(char **argv);                                     /* process->Index에 해당하는 process kill */
void killProcessPID(int pid);                                           /* process->pid에 해당하는 process kill */

char HISTORY_PATH[MAXLINE];                                             /* .history 파일의 경로 저장 */

//volatile pid_t pid;
int main_pid;                                                           /* main process */
volatile int fg_pid;                                                    /* current foreground process */
volatile char fg_processName[MAXLINE];                                  /* current foreground processName */

/* new_process를 process linked_list 마지막에 추가 */
void addProcess(process *new_process)
{
    int index;
    process *tmp_process = bg_process;
    while (1)
    {
        index = tmp_process->num;
        if (tmp_process->next == NULL)
            break;
        tmp_process = tmp_process->next;
    }

    new_process->num = index + 1;
    tmp_process->next = new_process;
}

/* cur process를 linked list에서 삭제 */
void deleteProcess(process *prev, process *cur)
{
    prev->next = cur->next;
    free(cur);
}

/* ctrl + z 입력 시 handler */
void sigtstp_handler(int sig)
{
    if (main_pid == getpid())                                           /* main process */
    {
        if (fg_pid != main_pid)                                         /* main process가 foreground가 아닐 때 */
        {
            process *tmp_process = bg_process;
            tmp_process = tmp_process->next;
            int flag = 0;
            while (1)
            {
                if (tmp_process == NULL)
                    break;
                if (tmp_process->pid == fg_pid)
                {
                    flag = 1;
                    break;
                }
                tmp_process = tmp_process->next;
            }
            if (flag == 0)
            {
                process *new_process = (process *)malloc(sizeof(process));
                new_process->pid = fg_pid;
                strcpy(new_process->processName, fg_processName);
                new_process->status = 0;
                new_process->next = NULL;

                addProcess(new_process);

                kill(fg_pid, SIGTSTP);
                fg_pid = main_pid;
            }
            else
            {
                tmp_process->status = 0;

                kill(fg_pid, SIGTSTP);
                fg_pid = main_pid;
            }
        }
    }
}

/* ctrl + c 입력 시 handler*/
void sigint_handler(int sig)
{
    if (main_pid == getpid())                                           /* main process */
    {
        if (fg_pid != main_pid)
        {
            kill(fg_pid, SIGKILL);
            fg_pid = main_pid;
        }
    }
}

/* sigchld handler */
void sigchld_handler(int sig)
{
    pid_t pid_chld;
    int status = 0;
    if (main_pid == getpid())                                           /* main process */
    {
        int i = 0;
        process *cur_process = bg_process;
        cur_process = cur_process -> next;
        process *prev_process = bg_process;

        while (1)                                                       /* linked list에 저장되어 있는 process 전부 확인 */
        {
            if (cur_process == NULL)
                break;

            // pid_chld > 0 -> 종료된 pid num가 저장됨
            // pid_chld == -1 -> 해당 pid num가 이미 종료되어 있음
            pid_chld = waitpid(cur_process->pid, status, WNOHANG);
            if(pid_chld > 0 || pid_chld == -1)
            {
                deleteProcess(prev_process, cur_process);
                cur_process = prev_process;
            }
            prev_process = cur_process;
            cur_process = cur_process->next;
        }
    }
}

int parseBackground(char *num)                                          /* fg %#, bg %# parsing */
{
    if (num == NULL)
        return -2;
    if (num[0] != '%')
        return -1;
    else
    {
        if (num[1] == '\0')
            return -1;
        int index = atoi(&(num[1]));
        return index;
    }
}

void foreground(char **argv)                                            /* fg %# 입력 시 호출 */
{
    int index = parseBackground(argv[1]);                               /* %# parsing */

    int flag = 0;
    process *tmp_process;
    if (index > 0)
    {
        tmp_process = bg_process;
        tmp_process = tmp_process->next;

        while (1)
        {
            if (tmp_process == NULL)
                break;

            if (tmp_process->num == index)
            {
                flag = 1;
                break;
            }

            tmp_process = tmp_process->next;
        }
    }

    if (flag == 1)                                                      /* bg_process에 저장되어 있는 process */
    {
        tmp_process->status = 1;
        fg_pid = tmp_process->pid;
        kill(fg_pid, SIGCONT);                                          /* SIGCONT siganl 전송 */

        int status;
        if (waitpid(fg_pid, &status, WUNTRACED) < 0)
        {
            unix_error("waitfg: waitpid error");
        }
        fg_pid = main_pid;
    }
    else
    {
        printf("No Such Job\n");
    }
}
void background(char **argv)                                            /* bg %# 입력 시 호출 */
{
    int index = parseBackground(argv[1]);                               /* %# parsing */

    int flag = 0;
    process *tmp_process;
    if (index > 0)
    {
        tmp_process = bg_process;
        tmp_process = tmp_process->next;

        while (1)
        {
            if (tmp_process == NULL)
                break;

            if (tmp_process->num == index)
            {
                flag = 1;
                break;
            }

            tmp_process = tmp_process->next;
        }
    }

    if (flag == 1)                                                      /* bg_process에 저장되어 있는 process */
    {
        tmp_process->status = 1;
        kill(tmp_process->pid, SIGCONT);                                /* SIGCONT signal 전송 */
    }
    else
    {
        printf("No Such Job\n");
    }
}

void killProcessIndex(char **argv)                                      /* Index에 해당하는 process 종료 */
{
    int index = parseBackground(argv[1]);                               /* %# parsing */

    if (index <= 0)
    {
        printf("No Such Job\n");
        return;
    }

    int flag = 0;
    process *tmp_process = bg_process;
    process *prev_process = bg_process;

    tmp_process = tmp_process->next;
    while (1)
    {
        if (tmp_process == NULL)
            break;

        if (tmp_process->num == index)
        {
            flag = 1;
            break;
        }

        prev_process = tmp_process;
        tmp_process = tmp_process->next;
    }

    if (flag == 1)                                                      /* bg_process에 저장되어 있는 process */
    {
        int tmp_pid = tmp_process->pid;
        kill(tmp_process->pid, SIGKILL);                                /* SIGKILL signal 전송 */
        waitpid(tmp_pid, 0, 0);                                         /* process leaping */
    }
    else
    {
        printf("No Such Job\n");
    }
}

void killProcessPID(int pid)                                            /* PID에 해당하는 process kill */
{
    process *tmp_process = bg_process;
    tmp_process = tmp_process->next;
    process *prev_process = bg_process;

    while (1)
    {
        if (tmp_process == NULL)
            break;
        if (tmp_process->pid == pid)
        {
            int tmp_pid = tmp_process->pid;
            kill(tmp_process->pid, SIGKILL);                            /* SIGKILL signal 전송 */
            waitpid(tmp_pid, 0, 0);                                     /* process leaping */

            break;
        }

        prev_process = tmp_process;
        tmp_process = tmp_process->next;
    }
}

void printJobs()                                                        /* bg_process에 저장되어 있는 process 출력 */
{
    process *tmp_process = bg_process;
    tmp_process = tmp_process->next;

    while (1)
    {
        if (tmp_process == NULL)
            break;

        if (tmp_process->status == 0)
        {
            printf("[%d] suspended %s\n", tmp_process->num, tmp_process->processName);
        }
        if (tmp_process->status == 1)
        {
            printf("[%d] running %s\n", tmp_process->num, tmp_process->processName);
        }

        tmp_process = tmp_process->next;
    }
}

void printJob(int pid)                                                  /* PID process 출력 */
{
    process *tmp_process = bg_process;

    while (tmp_process->next != NULL)
    {
        if (tmp_process->pid == pid)
            break;

        tmp_process = tmp_process->next;
    }

    printf("[%d] %d\n", tmp_process->num, tmp_process->pid);
}

void writeJob(int pid, char *cmdline)                                   /* bg_process에 process추가 */
{
    process *tmp_process = bg_process;

    int index = 0;
    while (1)
    {
        if (tmp_process->next == NULL)
            break;

        tmp_process= tmp_process->next;
        index = tmp_process->num;
    }

    process *new_process = (process *)malloc(sizeof(process));
    new_process->num = index + 1;
    new_process->pid = pid;
    strcpy(new_process->processName, cmdline);
    new_process->status = 1;
    new_process->next = NULL;

    tmp_process->next = new_process;
}

int main()
{
    main_pid = getpid();                                        /* main process pid */
    fg_pid = main_pid;                                          /* fg_pid 초기화 */

    if (getcwd(HISTORY_PATH, MAXLINE) == NULL)                  /* .history path 저장*/
    {
        printf("fail to read path\n");
        exit(1);
    }

    strcat(HISTORY_PATH, "/.history");

    FILE *file = fopen(HISTORY_PATH, "a");                      /* .history 파일 생성 */
    fclose(file);

    /* signal handler */
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        perror("signal error");

    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
        perror("signal error");

    if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR)
        perror("signal error");

    bg_process = (process *)malloc(sizeof(process));            /* bg_process 초기화 */
    bg_process->next = NULL;
    bg_process->num = 0;

    char cmdline[MAXLINE];                                      /* Command line */

    while (1)
    {
        /* Read */
        printf("CSE4100-MP-P1> ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */


    /* parsing line */
    removeEnter(cmdline);
    checkcmdline(cmdline);

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    if(bg == -1)                        /* 잘못된 입력 */
        return;

    writeHistory(cmdline);              /* history에 저장*/

    if (argv[0] == NULL)
        return; /* Ignore empty lines */
    
    checkBuiltin(bg, argv, cmdline);    /* 명령어 실행 */

    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "exit"))               /* exit 입력 시 실행중인 background process 종료 */
    { /* quit command */
        // free all process
        process *cur = bg_process;
        cur = cur->next;
        process *prev = bg_process;

        while (1)
        {
            if (cur == NULL)
                break;
            kill(cur->pid, SIGKILL);

            free(prev);
            prev = cur;
            cur = cur->next;
        }
        free(prev);

        exit(0);
    }
    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;
    if (!strcmp(argv[0], "cd"))
    {
        chdir(argv[1]);
        return 1;
    }
    if (!strcmp(argv[0], "history"))
    {
        openHistory();
        return 1;
    }
    if (!strcmp(argv[0], "jobs"))
    {
        printJobs();
        return 1;
    }
    if (!strcmp(argv[0], "fg"))
    {
        foreground(argv);
        return 1;
    }
    if (!strcmp(argv[0], "bg"))
    {
        background(argv);
        return 1;
    }
    if (!strcmp(argv[0], "kill"))
    {
        killProcessIndex(argv);
        return 1;
    }
    return 0; /* Not a builtin command */
}
/* $end eval */
/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
/*
    1) " or ' 입력 시 -> 다음 " or ' 까지 하나의 명령어로 인식
    2) | 입력 시 하나의 명령어로 인식
    3) 마지막에 & 입력 시 & 제거 후 return 1, 없을 때 return 0
    4) 올바르지 않은 값 입력 시 return -1
*/
int parseline(char *buf, char **argv) {
    int bg = 0;
    int argc = 0;
    char str[MAXLINE];
    // str[0] = '\0';

    int i = 0, j = 0;
    while(1) {
        if(*buf == NULL)
            break;
        while(*buf == ' '){
            *buf = '\0';
            buf++;
        }
        if (*buf == '\"') {                     // 쌍따옴표
            i = 1;
            while(1) {
                if(buf[i] == NULL)
                    break;
                if(buf[i] == '\0')
                    break;
                if(buf[i] == '\"'){
                    break;
                }
                i++;
            }
            if(buf[i] == NULL)
                return -1;
            if(buf[i] == '\0')
                return -1;
            if(buf[i] == '\"') {
                buf[i] = '\0';
                argv[argc++] = buf + 1;
            }
            buf += i;
        }
        else if(buf[i] == '\'') {                // 따옴표
            i = 1;
            while(1) {
                if(buf[i] == NULL)
                    break;
                if(buf[i] == '\0')
                    break;
                if(buf[i] == '\''){
                    break;
                }
                i++;
            }
            if(buf[i] == NULL)
                return -1;
            if(buf[i] == '\0')
                return -1;
            if(buf[i] == '\'') {
                buf[i] = '\0';
                argv[argc++] = buf + 1;
            }
            buf += i;
        } 
        else {
            i = 0;
            int flag = 0;
            while(1) {
                if(buf[i] == NULL) {
                    buf[i] = '\0';
                    argv[argc++] = buf;
                    buf += i;
                    flag = 1;
                    break;
                }
                else if(buf[i] == '\0') {
                    argv[argc++] = buf;
                    buf += i;
                    flag = 1;
                    break;
                }
                else if(buf[i] == ' ') {
                    buf[i] = '\0';
                    argv[argc++] = buf;
                    buf += i;
                    break;
                }
                else if(buf[i] == '|') {
                    buf[i] = '\0';
                    if(i == 0) {
                        argv[argc++] = "|";
                        // buf += i;
                        break;
                    }
                    else {
                        argv[argc++] = buf;
                        argv[argc++] = "|";
                        buf += i;
                        break;
                    }
                }
                i++;
            }
            if(flag == 1)
                break;
        }

        buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 0;

    /* background check & 입력 확인 */
    if(!strcmp(argv[argc - 1], "&")){
        if(argc == 1)
            return -1;
        argv[argc - 1] = NULL;
        bg = 1;
    }
    else if(argv[argc - 1][strlen(argv[argc - 1]) - 1] == '&') {
        argv[argc - 1][strlen(argv[argc - 1]) - 1] = '\0';
        bg = 1;
    }    

    return bg;
}

void removeEnter(char *cmdline)                 /* cmdline 마지막의 '\n'제거 */
{
    int i = 0;
    while (1)
    {
        if (cmdline[i] == '\n')
        {
            cmdline[i] = '\0';
            break;
        }
        i++;
    }
}

void checkBuiltin(int bg, char **argv, char *cmdline)
{
    pid_t pid;
    strcpy(fg_processName, cmdline);

    if (!builtin_command(argv))
    { 
        // quit -> exit(0), & -> ignore, other -> run
        // pipe가 있는 경우 checkPipe함수에서 처리
        // pipe가 없는 경우 이 함수에서 처리

        int pipe_index = checkPipe(bg, argv, cmdline); // argv[] = "|"인 가장 가까운 index, pipe_index = -1 --> '|' 존재하지않음
        if (pipe_index > 0)                            // pipe가 존재
        {
            if ((pid = fork()) == 0)
            {
                call_pipe(bg, argv, cmdline, pipe_index);

                exit(0);
            }
        }
        else if (pipe_index == 0) // argv[0] = "|"
        {
            printf("bash: syntax error near unexpected token `\\'\n");
            return;
        }
        else if (pipe_index = -1) // pipe 존재하지 않음
        {
            if ((pid = fork()) == 0)
            {
                /* child process group id를 parent group id와 다르게 함 */
                setpgid(getpid(), getpid());

                /* signal ignore */
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGCHLD, SIG_IGN);

                if (execvpe(argv[0], argv, environ) < 0)
                { // ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[0]);
                }

                exit(0);
            }
        }

        /* Parent waits for foreground job to terminate */
        if (!bg)
        {
            fg_pid = pid;
            int status;

            if (waitpid(pid, &status, WUNTRACED) < 0)
            {
                unix_error("waitfg: waitpid error");
            }
        }
        else                                                        /* background 실행 시 linked list에 추가 */
        { // when there is backgrount process!
            writeJob(pid, cmdline);
            printJob(pid);
        }
    }
}

int checkPipe(int bg, char **argv, char *cmdline)                   /* | index 반환 */
{
    int pipe_index = 0;
    while (1)
    {
        if (argv[pipe_index] == NULL)
        {
            pipe_index = -1;
            break;
        }

        if (!strcmp(argv[pipe_index], "|"))
            break;

        pipe_index++;
    }

    if (pipe_index > 0)
    {
        int i, j;

        i = 0;
        while (1)
        {
            if (argv[i] == NULL)
                break;
            i++;
        }
    }

    return pipe_index;
}

// 명령어에 pipe가 있을 때 call
void call_pipe(int bg, char **argv, char *cmdline, int pipe_index)
{
    /*
    ex) ls | grep c | sort
    argv
    -> ls
    new_argv
    -> grep c | sort
    */
    int i, j;

    /* new_argv -> argv 나누기 */
    i = 0;
    while (1)
    {
        if (argv[i] == NULL)
            break;
        i++;
    }

    char **new_argv = (char **)malloc(sizeof(char *) * i);
    for (j = 0; j < i; j++)
    {
        new_argv[j] = (char *)malloc(sizeof(char) * MAXLINE);
    }

    // argv -> | 오른쪽 명령어 저장
    // new_argv -> | 왼쪽 명령어 저장
    j = 0;
    while (1)
    {
        if (argv[pipe_index + j + 1] == NULL)
            break;

        strcpy(new_argv[j], argv[pipe_index + j + 1]);
        j++;
    }
    new_argv[j] = NULL;
    argv[pipe_index] = NULL;

    // pipe open
    pid_t pid;
    int fds[2];
    pipe(fds);

    pid = fork();
    if (pid == 0)                                   // child process | 왼쪽 명령어 실행
    {   
        // stdout을 pipe in에 연결
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);

        checkBuiltin(bg, argv, cmdline);

        exit(0);
    }
    else                                            // parent process | 오른쪽 명령어 실행
    {
        // pipe out을 stdout에 연결
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);

        waitpid(pid, NULL, 0);                                                  // | 왼쪽 명령어가 종료될 때까지 wait

        pipe_index = checkPipe(bg, new_argv, cmdline);

        if (pipe_index == -1)                                                   // 더이상 | 가 없는 경우
        {
            /* child process group id를 parent group id와 다르게 함 */
            setpgid(getpid(), getpid());

            /* signal ignore */
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGCHLD, SIG_IGN);

            execvp(new_argv[0], new_argv);
        }
        else
            call_pipe(bg, new_argv, cmdline, checkPipe(bg, new_argv, cmdline)); // pipe가 | 가 있는 경우
    }

    // free new_argv
    for (j = 0; j < i; j++)
    {
        free(new_argv[j]);
    }
    free(new_argv);
}

void checkcmdline(char *cmdline)                        /* cmdline의 !! !# replace */
{
    char str[MAXLINE];
    int i;
    int flag = 0;

    int history_flag = 0;

    while (1)
    {
        for (i = 0; i < strlen(cmdline); i++)
        {
            if (cmdline[i] == '!')
            {
                if (cmdline[i + 1] == '!')
                {
                    callHistory(-1, str);
                    changeStr(cmdline, i, i + 1, str);
                    history_flag = 1;
                    break;
                }
                int num = atoi(&(cmdline[i + 1]));
                if (num == 0)
                {
                    flag = 1;
                    printf("event not found");
                }
                else
                {
                    callHistory(num, str);

                    int digits = 0;
                    while (num != 0)
                    {
                        num = num / 10;
                        digits++;
                    }

                    changeStr(cmdline, i, i + digits, str);

                    history_flag = 1;
                    break;
                }
            }
        }
        if (flag == 1)
            break;
        if (i == strlen(cmdline))
            break;
    }

    if (history_flag)
        printf("%s\n", cmdline);
}

void changeStr(char *cmdline, int i, int j, char *str)          /* string replace */
{
    char newline[MAXLINE];
    for (int i = 0; i < MAXLINE; i++)
    {
        newline[i] = '\0';
    }
    strncpy(newline, cmdline, i);
    strcat(newline, str);
    strcat(newline, &(cmdline[j + 1]));

    strncpy(cmdline, newline, MAXLINE);
}

void writeHistory(char *cmdline) {                                  /* history에 명령어 저장 */
    FILE *file;

    // str에 history의 가장 최근 명령어 저장
    char str[MAXLINE];

    file = fopen(HISTORY_PATH, "r");
    while (!feof(file))
    {
        fgets(str, MAXLINE, file);
    }
    fclose(file);

    removeEnter(str);

    if(strcmp(str, cmdline)) {                                      /* 최근 명령어와 다를 경우만 저장 */
        file = fopen(HISTORY_PATH, "a");
        fprintf(file, cmdline);
        fprintf(file, "\n");

        fclose(file);
    }
}

void openHistory()                                                  /* history 출력 */
{
    FILE *file = fopen(HISTORY_PATH, "r");

    char str[MAXLINE];
    int i = 1;
    while (!feof(file))
    {
        if (fgets(str, MAXLINE, file) == NULL)
            break;
        printf("%d %s", i++, str);
    }

    fclose(file);
}

// index번호에 맞는 명령어 returnstring에 저장
// index = -1일 때는 가장 최근 명령어 저장
void callHistory(int index, char *returnstring)
{
    FILE *file = fopen(HISTORY_PATH, "r");

    char str[MAXLINE];

    if (index == -1)
    {
        while (!feof(file))
        {
            if (fgets(str, MAXLINE, file) == NULL)
                break;
        }
    }
    else
    {
        int i = 0;
        while (!feof(file))
        {
            if (i == index)
                break;
            if (fgets(str, MAXLINE, file) == NULL)
                break;
            i++;
        }
    }

    for (int i = 0; i < strlen(str); i++)
    {
        if (str[i] == '\n')
            str[i] = '\0';
    }

    strncpy(returnstring, str, MAXLINE);

    fclose(file);
}