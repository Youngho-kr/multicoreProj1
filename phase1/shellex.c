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
void removeEnter(char *cmdline);

void writeHistory(char **argv, char *cmdline);
void openHistory(int index);
void callHistory(int index, char *argv);

void checkBuiltin(int bg, char **argv, char *cmdline);
void checkcmdline(char *cmdline);

void changeStr(char *cmdline, int i, int j, char *str);

int checkPipe(int bg, char **argv, char *cmdline);

void call_pipe(int bg, char **argv, char *cmdline, int pipe_index);

void sigint_handler(int sg);
void sigchld_handler(int sg);
void sigtstp_handler(int sg);

void printJobs();
void printJob(int pid);

/*
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

/*
num = 0
next = NULL
*/
process *bg_process;

void addProcess(process *new_process);
void deleteProcess(process *prev, process *cur);

void foreground(char **argv);
void background(char **argv);
void killProcessIndex(char **argv);
void killProcessPID(int pid);

char HISTORY_PATH[MAXLINE];

volatile pid_t pid;
int main_pid;
int fg_pid;
char fg_processName[MAXLINE];

/*
    new_process를 process linked_list 마지막에 추가
*/
void addProcess(process *new_process) {
    int index;
    process *tmp_process = bg_process;
    while(1) {
        index = tmp_process -> num;
        if(tmp_process -> next == NULL)
            break;
        tmp_process = tmp_process -> next;
    }

    new_process->num = index + 1;
    tmp_process -> next = new_process;
}

/* cur process를 linked list에서 삭제 */
void deleteProcess(process *prev, process *cur)
{
    prev->next = cur->next;
    free(cur);
}

void sigtstp_handler(int sig)
{
    if(main_pid == getpid()){
        if(fg_pid != main_pid) {
            process *tmp_process = bg_process;
            tmp_process = tmp_process -> next;
            int flag = 0;
            while(1) {
                if(tmp_process == NULL)
                    break;
                if(tmp_process -> pid == fg_pid){
                    flag = 1;
                    break;
                }
                tmp_process = tmp_process -> next;
            }

            if(flag == 0){

            process *new_process = (process *)malloc(sizeof(process));
            new_process->pid = fg_pid;
            strcpy(new_process->processName, fg_processName);
            new_process->status = 0;
            new_process->next = NULL;

            addProcess(new_process);
            }
            else {
                tmp_process->status = 0;
            }
            
            kill(fg_pid, SIGTSTP);
        }
    }
}

void sigint_handler(int sig)
{
    // ctrl + c handler
    if (main_pid == getpid())
    {
        if(fg_pid != main_pid) {
            int index;
            process *tmp_process = bg_process;
            tmp_process = tmp_process -> next;
            process *prev_process = bg_process;

            while(1) {
                if(tmp_process == NULL)
                    break;

                if(tmp_process->pid == fg_pid){
                    deleteProcess(prev_process, tmp_process);
                }

                prev_process = tmp_process;
                tmp_process = tmp_process -> next;
            }

            kill(fg_pid, SIGKILL);
        }
    }
}

void sigchld_handler(int sig)
{
    // int olderrno = errno;
    pid_t pid_chld;
    int status;
    if(main_pid == getpid()) {
    while ((pid_chld = waitpid(-1, &status, WNOHANG)) > 0)
    {
        int index = 0;
        process *tmp_process = bg_process;
        tmp_process = tmp_process -> next;
        process *prev_process = bg_process;
        while(1){
            if(tmp_process == NULL)
                break;

            if(tmp_process -> pid == pid_chld) {
                deleteProcess(prev_process, tmp_process);
            }

            prev_process = tmp_process;
            tmp_process = tmp_process -> next;
        }
        //killProcessPID(pid_chld);

        //kill(pid_chld, SIGTERM);
        // sio_puts("Handler repead child ");
        // sio_putl((long)pid_chld);
        // sio_puts("\n");

    }
    // if (errno != ECHILD)
    //     sio_error("wait error");
    // errno = olderrno;
    }
}

int parseBackground(char *num)
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

void foreground(char **argv) {
    int index = parseBackground(argv[1]);

    int flag = 0;
    process *tmp_process;
    if(index > 0) {
        tmp_process = bg_process;
        tmp_process = tmp_process -> next;

        while(1) {
            if(tmp_process == NULL)
                break;

            if(tmp_process->num == index) {
                flag = 1;
                break;
            }

            tmp_process = tmp_process->next;
        }
    }

    if(flag == 1) {
        tmp_process->status = 1;
        kill(tmp_process->pid, SIGCONT);

        int status;
        if(waitpid(pid, &status, WUNTRACED) < 0) {
            unix_error("waitfg: waitpid error" );
        }
    }
    else {
        printf("No Such Job\n");
    }
}
void background(char **argv) {
    int index = parseBackground(argv[1]);

    int flag = 0;
    process *tmp_process;
    if(index > 0) {
        tmp_process = bg_process;
        tmp_process = tmp_process -> next;

        while(1) {
            if(tmp_process == NULL)
                break;

            if(tmp_process->num == index) {
                flag = 1;
                break;
            }

            tmp_process = tmp_process->next;
        }
    }

    if(flag == 1) {
        tmp_process->status = 1;
        kill(tmp_process->pid, SIGCONT);
    }
    else {
        printf("No Such Job\n");
    }
}

void killProcessIndex(char **argv)
{
    // kill(main_pid, SIGTERM);
    int index = parseBackground(argv[1]);

    if (index <= 0)
    {
        printf("No Such Job\n");
    }

    printf("index: %d\n", index);

    int flag = 0;
    process *tmp_process = bg_process;
    process *prev_process = bg_process;

    tmp_process = tmp_process->next;
    while (1)
    {
        if(tmp_process == NULL)             // linked list 끝에 도달
            break;
        if(tmp_process->num >index)         // num 없음
            break;

        if (tmp_process->num == index)
        {
            flag = 1;
            break;
        }

        prev_process = tmp_process;
        tmp_process = tmp_process->num;
    }

    if (flag == 1)
    {
        printf("killprocess: %d\n", tmp_process->pid);
        kill(tmp_process->pid, SIGKILL);
        waitpid(tmp_process->pid, 0, WUNTRACED);

        deleteProcess(prev_process, tmp_process);
    }
    else
    {
        printf("No Such Job\n");
    }
}

void killProcessPID(int pid) {
    process *tmp_process = bg_process;
    tmp_process = tmp_process -> next;
    process *prev_process = bg_process;

    while(1) {
        if(tmp_process == NULL)
            break;
        if(tmp_process->pid == pid) {
            kill(tmp_process->pid, SIGKILL);
            waitpid(tmp_process->pid, 0, 0);

            deleteProcess(prev_process, tmp_process);
        }

        prev_process = tmp_process;
        tmp_process = tmp_process->num;
    }
}

void printJobs()
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

void printJob(int pid)
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

void writeJob(int pid, char *cmdline)
{
    process *tmp_process = bg_process;

    int index = 0;
    while (1)
    {
        if (tmp_process->next == NULL)
            break;
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
    // main process의 pid num
    main_pid = getpid();
    fg_pid = getpid();

    // HISTORY_PATH에 history경로 저장
    if (getcwd(HISTORY_PATH, MAXLINE) == NULL)
    {
        printf("fail to read path\n");
        exit(1);
    }

    strcat(HISTORY_PATH, "/.history");

    //  history 파일 생성
    FILE *file = fopen(HISTORY_PATH, "a");
    fclose(file);

    // SIGNAL
    // SIGINT, SIGCHLD, SIGTSTP handler
    sigset_t mask, prev_mask;

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        perror("signal error");

    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
        perror("signal error");

    if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR)
        perror("signal error");

    bg_process = (process *)malloc(sizeof(process));
    bg_process->next = NULL;
    bg_process->num = 0;

    char cmdline[MAXLINE]; /* Command line */

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

    free(bg_process);
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    // pid_t pid;           /* Process id */

    removeEnter(cmdline);

    checkcmdline(cmdline);

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    writeHistory(argv, cmdline);

    if (argv[0] == NULL)
        return; /* Ignore empty lines */
    checkBuiltin(bg, argv, cmdline);

    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "exit")) { /* quit command */
        // free all process
        process* cur = bg_process;
        cur = cur->next;
        process* prev = bg_process;

        while(1) {
            if(cur == NULL)
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
        openHistory(-1);
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
int parseline(char *buf, char **argv)
{
    char *delim_space; /* Points to first space delimiter */
    char *delim_pipe;
    int argc; /* Number of args */
    int bg;   /* Background job? */

    char str[MAXLINE];

    // buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;

    int i = 0, j = 0; // index for ' ', '|'
    int flag;         // flag for pipe
    while (1)
    {
        if (buf[i] == NULL)
            break;

        flag = 0;
        while (1)
        {
            if (buf[j] == NULL)
            {
                buf[j + 1] = NULL;
                break;
            }

            if (buf[j] == ' ')
            {
                break;
            }
            if (buf[j] == '|')
            {
                flag = 1;
                break;
            }
            j++;
        }

        if (flag == 1)
        { // buf[j] = '|'
            if (j != 0)
            {
                buf[j] = '\0';
                argv[argc++] = buf;
            }

            argv[argc++] = "|";
        }
        else
        {
            buf[j] = '\0';
            argv[argc++] = buf;
        }

        buf = &(buf[j]) + 1;

        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;

        i = 0, j = 0;
    }
    argv[argc] = NULL;

    i = 0;
    while (1)
    {
        if (argv[i] == NULL)
            break;

        i++;
    }

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
void removeEnter(char *cmdline)
{
    int i = 0;
    while (1)
    {
        if (cmdline[i] == '\n')
        {
            cmdline[i] = ' ';
            cmdline[i + 1] = '\0';
            break;
        }
        i++;
    }
}

void checkBuiltin(int bg, char **argv, char *cmdline)
{
    // pid_t pid;
    strcpy(fg_processName, cmdline);

    if (!builtin_command(argv))
    { // quit -> exit(0), & -> ignore, other -> run
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
                if (!bg)
                {
                }
                else
                {
                    //signal(SIGINT, SIG_IGN);
                    //signal(SIGTSTP, SIG_IGN);
                    // ignore sigint, sigstsp
                }

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
            if(waitpid(pid, &status, WUNTRACED) < 0) {
                unix_error("waitfg: waitpid error" );
            }
        }
        else
        { // when there is backgrount process!
            // printf("[%d] %s\n", pid, cmdline);
            writeJob(pid, cmdline);
            printJob(pid);
        }
    }
}

int checkPipe(int bg, char **argv, char *cmdline)
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

    ///////////////////////////////////////////////////////////////
    /* new_argv 동적할당                                            */
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
    //////////////////////////////////////////////////////////////

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

    // pid_t pid;
    int fds[2];
    pipe(fds);

    pid = fork();
    if (pid == 0)
    {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);

        // execvp(argv[0], argv);

        checkBuiltin(bg, argv, cmdline);

        exit(0);
    }
    else
    {
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);

        waitpid(pid, NULL, 0);

        pipe_index = checkPipe(bg, new_argv, cmdline);

        if (pipe_index == -1)
            execvp(new_argv[0], new_argv);
        else
            call_pipe(bg, new_argv, cmdline, checkPipe(bg, new_argv, cmdline));
    }

    // free new_argv
    for (j = 0; j < i; j++)
    {
        free(new_argv[j]);
    }
    free(new_argv);
}

void checkcmdline(char *cmdline)
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

void changeStr(char *cmdline, int i, int j, char *str)
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

// history에 저장
// 입력한 명령어 저장
void writeHistory(char **argv, char *cmdline)
{
    if (argv[0] == NULL)
        return;

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

    // 가장 최근 명령어와 같은 지 확인
    char *history_argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];           /* Holds modified command line */
    int bg;                      /* Should the job run in bg or fg? */

    strcpy(buf, str);
    bg = parseline(buf, history_argv);

    int i = 0;
    int flag = 0;
    while (1)
    {
        if (argv[i] == NULL)
        {
            if (history_argv[i] != NULL)
                flag = 1;
            break;
        }
        if (history_argv[i] == NULL)
        {
            if (argv[i] != NULL)
                flag = 1;
            break;
        }

        if (strncmp(argv[i], history_argv[i], MAXLINE))
        {
            flag = 1;
            break;
        }
        i++;
    }

    // 가장 최근 명령어와 같으면 저장하지 않음
    if (flag == 0)
        return;

    // history에 저장
    file = fopen(HISTORY_PATH, "a");
    i = 0;
    while (1)
    {
        fprintf(file, argv[i]);

        if (argv[i + 1] == NULL)
            break;
        fprintf(file, " ");

        i++;
    }
    fprintf(file, "\n");

    fclose(file);
}

// history읽기
// index에 해당하는 history를 출력
// index = 0 -> 가장 최근
// index = -1 -> 모든 history 출력
void openHistory(int index)
{
    FILE *file = fopen(HISTORY_PATH, "r");

    // history 명령어로 이 함수 호출하므로 파일이 없는 경우는 없음

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

// index번호에 맞는 명령어 argv[0]에 저장
// index = -1일 때는 가장 최근 명령어 argv[0]에 저장
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