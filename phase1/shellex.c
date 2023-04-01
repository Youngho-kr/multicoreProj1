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

char HISTORY_PATH[MAXLINE];

int main()
{   
    // HISTORY_PATH에 history경로 저장
    if(getcwd(HISTORY_PATH, MAXLINE) == NULL) {
        printf("fail to read path\n");
        exit(1);
    }

    strcat(HISTORY_PATH, "/history");

    //  history 파일 생성
    FILE *file = fopen(HISTORY_PATH, "a");
    fclose(file);

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
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

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
    if (!strcmp(argv[0], "exit")) /* quit command */
        exit(0);
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
    return 0; /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */
    int bg;      /* Background job? */

    // buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' ')))
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
void removeEnter(char *cmdline){
    int i = 0;
    while(1) {
        if(cmdline[i] == '\n') {
            cmdline[i] = ' ';
            cmdline[i+1] = '\0';
            break;
        }
        i++;
    }
}

void checkBuiltin(int bg, char **argv, char *cmdline)
{
    pid_t pid;

    if (!builtin_command(argv))
    { // quit -> exit(0), & -> ignore, other -> run
        if ((pid = fork()) == 0)
        {
            if (execvpe(argv[0], argv, environ) < 0)
            { // ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }

        /* Parent waits for foreground job to terminate */
        if (!bg)
        {
            int status;
            if (waitpid(pid, &status, 0) < 0)
                unix_error("waitfg: waitpid error");
        }
        else // when there is backgrount process!
            printf("%d %s", pid, cmdline);
    }
}

void checkcmdline(char *cmdline)
{
    char str[MAXLINE];
    int i;
    int flag = 0;

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
                    break;
                }
                int num = atoi(&(cmdline[i + 1]));
                if(num == 0) {
                    flag = 1;
                    printf("event not found");
                }
                else {
                    callHistory(num, str);

                    int digits = 0;
                    while(num != 0) {
                        num = num / 10;
                        digits++;
                    }

                    changeStr(cmdline, i, i + digits, str);

                    break;
                }
            }
        }
        if(flag == 1)
            break;
        if (i == strlen(cmdline))
            break;
    }

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
    while(!feof(file)) {
        fgets(str, MAXLINE, file);
    }
    fclose(file);

    removeEnter(str);

    // 가장 최근 명령어와 같은 지 확인
    char *history_argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */

    strcpy(buf, str);
    bg = parseline(buf, history_argv);

    int i = 0;
    int flag = 0;
    while(1) {
        if(argv[i] == NULL)
            break;
        if(history_argv[i] == NULL)
            break;

        if(strncmp(argv[i], history_argv[i], MAXLINE)){
            flag = 1;
            break;
        }
        i++;
    }

    // 가장 최근 명령어와 같으면 저장하지 않음
    if(flag == 0)
        return;

    // history에 저장
    file = fopen(HISTORY_PATH, "a");
    i = 0;
    while (1)
    {
        fprintf(file, argv[i]);

        if (argv[i+1] == NULL)
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
            if(i == index)
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