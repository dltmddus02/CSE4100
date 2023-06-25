/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#define MAXARGS 128

#define NONE 0
#define FG 1
#define BG 2
#define SUSP 3
#define DONE 4
#define KILL 5

/* Function prototypes */
void eval(char *cmdline);
void eval_pipe(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
int parse_pipe(char **argv, char *argv_pipe[30][128]);
void insert_get_history(char *cmdline); // get_history 배열에 명령어 1개 추가
int count_history_inst();               // history에 줄 개수 세기
void read_history_file();               // history파일 읽어 get_history 배열에 명령어 전체 넣기
void history();
char path1[50];
char path2[50];
char get_history[MAXLINE][MAXLINE];
int history_inst_num = 0; // 현재 명령어 개수
FILE *fp;                 // 'a+'
char *pChar;

// signal func
void sigint_handler();
void sigtstp_handler();
void sigchld_handler();
int find_job_state(pid_t pid);
// signal variable
int cur_child_pid = -1;
int job_num = 1;
char add_cmdline[MAXLINE];
typedef struct job
{
    pid_t pid;
    int id;
    int state;
    char cmdline[MAXLINE];
} job;
job job_list[MAXARGS];
void print_job();
int add_job(job *job_list, pid_t pid, int state, char *cmdline);
void clear_job(int index);
void done_job();
// cmdline 마지막 '&' 제거해서 add_cmdline에 넣기
void remove_ampersand(char *cmdline, char *add_cmdline);

int main()
{
    fp = fopen("history.txt", "a+");
    if (!fp)
    {
        printf("No such file or directory\n");
        exit(1);
    }

    history_inst_num = count_history_inst();
    if (history_inst_num != 0)
        read_history_file(); // history가 존재한다면 get_history에 저장하기
    char cmdline[MAXLINE];   /* Command line */
    while (1)
    {
        Signal(SIGINT, sigint_handler);
        Signal(SIGTSTP, sigtstp_handler);
        Signal(SIGCHLD, sigchld_handler);

        /* Read */
        printf("> ");
        pChar = fgets(cmdline, MAXLINE, stdin);

        // get_history에 명령어 넣기
        insert_get_history(cmdline);
        if (feof(stdin))
            exit(0);
        /* Evaluate */
        if (strstr(cmdline, "|"))
        {
            eval_pipe(cmdline);
        }
        else
        {
            eval(cmdline);
        }
        done_job();
    }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS];    /* Argument list execve() */
    char buf[MAXLINE];      /* Holds modified command line */
    char add_char[MAXLINE]; /* Holds modified command line */
    int bg;                 /* Should the job run in bg or fg? */
    pid_t pid;              /* Process id */
    int state;

    strcpy(path1, "/bin/");
    strcpy(path2, "/usr/bin/");
    strcpy(buf, cmdline);
    strcpy(add_char, cmdline);
    remove_ampersand(cmdline, add_char);
    // printf("%s!\n", add_char);
    bg = parseline(buf, argv); // 스페이스로 구문된 인자들 argv로 파싱
    // printf("bg이면 1 : %d\n", bg);
    if (!bg)
        state = FG;
    else
        state = BG;
    if (strcmp(argv[0], "!!") == 0 || (argv[0][0] == '!' && isdigit(atoi(argv[0] + 1)) == 0))
    {
    }
    else
    {
        char buffer[MAXARGS];
        char *bufferChar;
        rewind(fp);
        while (fgets(buffer, MAXARGS, fp) != NULL)
        {
            bufferChar = buffer;
        }
        bufferChar[strlen(bufferChar) - 1] = '\0';
        if (strcmp(bufferChar, buf) != 0)
        {
            // history에 넣기
            fseek(fp, 0, SEEK_END);
            fputs(cmdline, fp);
        }
    }
    if (argv[0] == NULL)
        return; /* Ignore empty lines */
    if (!builtin_command(argv))
    {                            // quit -> exit(0), & -> ignore, other -> run
        if ((pid = Fork()) == 0) // Child process
        {
            if (!bg)
            {
                Signal(SIGINT, SIG_DFL); // fg였다면 default 값
            }
            else
            {
                Signal(SIGINT, SIG_IGN); // bg였다면 무시
            }
            if (execvp(argv[0], argv) < 0)
            { // ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        // job 추가
        add_job(job_list, pid, state, add_char);

        /* Parent waits for foreground job to terminate */
        if (!bg)
        { // parent가 fg인 경우 (zombie 방지 위함)
            int status;
            cur_child_pid = pid;
            strcpy(add_cmdline, cmdline);
            if (waitpid(pid, &status, WUNTRACED) < 0)
                unix_error("waitfg: waitpid error");
            if (WIFEXITED(status))
            { // 정상적으로 종료
                cur_child_pid = -1;
                for (int i = 0; i < MAXARGS; i++)
                {
                    if (job_list[i].state == FG)
                    {
                        // clear job
                        clear_job(i);
                    }
                }
            }
        }
        else // parent가 bg인 경우
        {
            // int index = find_job_state(pid);
            // if (job_list[index].state == BG)
            //     printf("[%d] Running\t %s\n", index, add_char);
            // else if (job_list[index].state == DONE)
            //     printf("[%d] Stopped\t %s", index, add_char);
        }
    }
    return;
}

void eval_pipe(char *cmdline)
{
    int pfd[30];
    char *argv[MAXARGS] = {0};
    char buf[MAXLINE];
    char buf_copy[MAXLINE];
    char add_char[MAXLINE]; // add_job에 넣을 배열
    int bg;
    pid_t pid;
    int state;

    char *argv_pipe[30][128] = {0};

    strcpy(path1, "/bin/");
    strcpy(path2, "/usr/bin/");

    strcpy(buf, cmdline);
    strcpy(buf_copy, cmdline);
    strcpy(add_char, cmdline);
    remove_ampersand(cmdline, add_char);

    bg = parseline(buf, argv); // 스페이스로 구문된 인자들 argv로 파싱
    if (!bg)
        state = FG;
    else
        state = BG;
    if (argv[0] == NULL)
        return;

    int argv_idx = parse_pipe(argv, argv_pipe);
    if (strcmp(argv[0], "!!") == 0 || (argv[0][0] == '!' && isdigit(atoi(argv[0] + 1)) == 0))
    {
    }
    else
    {
        char buffer[MAXARGS];
        char *bufferChar;

        rewind(fp);
        while (fgets(buffer, MAXARGS, fp) != NULL)
        {
            bufferChar = buffer;
        }
        bufferChar[strlen(bufferChar) - 1] = '\0';
        if (strcmp(bufferChar, buf) != 0)
        {
            // history에 넣기
            fseek(fp, 0, SEEK_END);
            fputs(cmdline, fp);
        }
    }

    if (!builtin_command(argv))
    { // quit -> exit(0), & -> ignore, other -> run
        for (int i = 0; i < 15; i++)
        {
            if (pipe(pfd + 2 * i) < 0)
            {
                unix_error("pipe error");
            }
        }

        for (int i = 0; i <= argv_idx; i++)
        {
            if ((pid = Fork()) == 0)
            {
                // file pointer 열기
                if (i != 0) // not the first command
                {
                    Dup2(pfd[2 * (i - 1)], STDIN_FILENO);
                }
                if (i != argv_idx) // not the last command
                {
                    Dup2(pfd[2 * i + 1], STDOUT_FILENO);
                }
                if (execvp(argv_pipe[i][0], argv_pipe[i]) < 0)
                { // ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv_pipe[i][0]);
                    exit(0);
                }
            }
            // file pointer 닫기
            if (i != 0)
                close(pfd[2 * (i - 1)]);
            if (i != argv_idx)
                close(pfd[2 * i + 1]);
            if (i == 0)
            {
                printf("%s!\n", add_char);
                // job 추가
                add_job(job_list, pid, state, add_char);
            }
            /* Parent waits for foreground job to terminate */
            if (!bg)
            { // child가 foreground process인 경우

                int status;
                if (waitpid(pid, &status, WUNTRACED) < 0)
                    unix_error("waitfg: waitpid error");
                if (WIFEXITED(status))
                { // 정상적으로 종료
                    cur_child_pid = -1;
                    for (int i = 0; i < MAXARGS; i++)
                    {
                        if (job_list[i].state == FG)
                        {
                            // clear job
                            clear_job(i);
                        }
                    }
                }
            }
            else
            {
                // when there is backgrount process!
                // printf("%d %s", pid, cmdline);
                // int index = find_job_state(pid);
                // if (job_list[index].state == BG)
                //     printf("[%d] running\t %s\n", index, add_char);
                // else if (job_list[index].state == DONE)
                //     printf("[%d] stopped\t %s", index, add_char);
            }
        }
    }

    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit"))
    { /* quit command */
        exit(0);
    }
    if (!strcmp(argv[0], "&"))
    { /* Ignore singleton & */
        return 1;
    }
    if (!strcmp(argv[0], "cd"))
    {
        if (chdir(argv[1]) == -1)
        { // 디렉토리가 없는 경우
            printf("No such file or directory\n");
        }
        return 1;
    }
    if (!strcmp(argv[0], "history"))
    {
        history();
        return 1;
    }
    if (argv[0][0] == '!')
    {
        if (argv[0][1] == '!')
        {
            if (strstr(get_history[history_inst_num - 1], "|"))
            {
                eval_pipe(get_history[history_inst_num - 1]);
            }
            else
            {
                eval(get_history[history_inst_num - 1]);
            }
        }
        else if (isdigit(atoi(argv[0] + 1)) == 0)
        {
            int n = atoi(argv[0] + 1);
            insert_get_history(get_history[n - 1]);
            if (strstr(get_history[n - 1], "|"))
            {
                eval_pipe(get_history[n - 1]);
            }
            else
            {
                eval(get_history[n - 1]);
            }
        }
        return 1;
    }
    if (!strcmp(argv[0], "jobs"))
    {
        print_job();
        return 1;
    }

    if (!strcmp(argv[0], "kill"))
    {
        if (argv[1] == 0 || argv[1][0] != '%')
        {
            printf("Usage : kill %%job id\n");
            return 1;
        }
        if (argv[1][1])
        {
            int job_id = atoi(&argv[1][1]);
            if (job_list[job_id].state == NONE)
            {
                printf("No Such Job\n");
            }
            else
            {
                kill(job_list[job_id].pid, SIGKILL);
                job_list[job_id].state = KILL;
            }
        }
        return 1;
    }

    if (!strcmp(argv[0], "fg"))
    {
        if (argv[1] == 0 || argv[1][0] != '%')
        {
            printf("Usage : fg %%job id\n");
            return 1;
        }
        if (argv[1][1]) // fg로 바꿀 bg의 job ID
        {
            int job_id = atoi(&argv[1][1]);
            // job_id--;
            if (job_list[job_id].state == NONE || job_list[job_id].state == DONE || job_list[job_id].state == KILL)
            {
                printf("No Such Job\n");
                return 1;
            }
            else
            {
                job_list[job_id].state = FG;
                kill(job_list[job_id].pid, SIGCONT);
                printf("[%d]\trunning \t%s\n", job_list[job_id].id, job_list[job_id].cmdline);
                while (job_list[job_id].state == FG)
                {
                    if (job_list[job_id].state != FG)
                        break;
                }
            }
            return 1;
        }
    }

    if (!strcmp(argv[0], "bg"))
    {
        if (argv[1] == 0 || argv[1][0] != '%')
        {
            printf("Usage : bg %%job id\n");
            return 1;
        }
        if (argv[1][1]) // bg로 바꿀 fg의 job ID
        {
            int job_id = atoi(&argv[1][1]);
            // job_id--;
            if (job_list[job_id].state == NONE || job_list[job_id].state == DONE || job_list[job_id].state == KILL)
            {
                printf("No Such Job\n");
                return 1;
            }
            else
            {
                int bg_str_len = strlen(job_list[job_id].cmdline);
                // if (job_list[job_id].cmdline[bg_str_len - 1] != '&') // bg로 실행되므로 마지막 문자 뒤에 '&' 추가
                // {
                //     job_list[job_id].cmdline[bg_str_len - 1] = ' ';
                //     job_list[job_id].cmdline[bg_str_len] = '&';
                //     job_list[job_id].cmdline[bg_str_len + 1] = '\0';
                // }
                job_list[job_id].state = BG;
                printf("[%d]\trunning \t%s\n", job_list[job_id].id, job_list[job_id].cmdline);
                kill(job_list[job_id].pid, SIGCONT);
            }
        }
        return 1;
    }

    return 0; /* Not a builtin command */
}
/* $end eval */

int parse_pipe(char **argv, char *argv_pipe[30][128])
{
    int num_pipes = 0;
    int arg_idx = 0;
    int pipe_idx = 0;

    while (argv[arg_idx] != NULL)
    {
        char *arg = argv[arg_idx];
        int char_idx = 0;

        while (arg[char_idx] != '\0')
        {
            if (arg[char_idx] == '|')
            {
                argv_pipe[pipe_idx][num_pipes] = NULL;
                num_pipes = 0;
                pipe_idx++;
                break;
            }
            else if (!isspace(arg[char_idx]))
            {
                // 따옴표가 있는 경우 제거하여 저장
                if (arg[char_idx] == '\"' || arg[char_idx] == '\'')
                {
                    char quote_char = arg[char_idx];
                    char_idx++;
                    int quote_start = char_idx;

                    while (arg[char_idx] != '\0' && arg[char_idx] != quote_char)
                    {
                        char_idx++;
                    }

                    if (arg[char_idx] == quote_char)
                    {
                        arg[char_idx] = '\0'; // 따옴표를 문자열에서 제거
                        argv_pipe[pipe_idx][num_pipes] = &arg[quote_start];
                        num_pipes++;
                    }
                }
                else
                {
                    argv_pipe[pipe_idx][num_pipes] = &arg[char_idx];
                    num_pipes++;

                    while (arg[char_idx] != '\0' && !isspace(arg[char_idx]))
                    {
                        char_idx++;
                    }

                    if (arg[char_idx] == '\0')
                    {
                        break;
                    }
                    else
                    {
                        arg[char_idx] = '\0';
                    }
                }
            }
            else
            {
                char_idx++;
            }
        }

        arg_idx++;
    }

    argv_pipe[pipe_idx][num_pipes] = NULL;

    return pipe_idx;
}

/* $begin parseline */
/* parseline - parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim; /* points to first space delimiter */
    int argc;    /* number of args */
    int bg;      /* background job? */

    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' ')))
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;
    else if (argv[argc - 1][strlen(argv[argc - 1]) - 1] == '&') // 명령어 여러 단어일 때 마지막에 & 있는지.
    {
        bg = 1;
        argv[argc - 1][strlen(argv[argc - 1]) - 1] = '\0';
    }
    return bg;
}
/* $end parseline */

void history()
{
    char buf[MAXLINE];
    int line_cnt = 0;

    for (int i = 0; i < history_inst_num; i++)
    {
        printf("%d: %s", i + 1, get_history[i]);
    }
}

void insert_get_history(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); // 스페이스로 구문된 인자들 argv로 파싱
    if (strcmp(argv[0], "!!") == 0 || (argv[0][0] == '!' && isdigit(atoi(argv[0] + 1)) == 0))
    {
        return;
    }
    else
    {
        if (strcmp(get_history[history_inst_num - 1], cmdline) == 0)
        {
        }
        else
        {
            strcpy(get_history[history_inst_num], cmdline);
            history_inst_num++;
        }
    }
}

int count_history_inst()
{
    history_inst_num = 0;
    char line[MAXLINE];

    if (fp == NULL)
    {
        printf("File open error!\n");
        return -1;
    }
    rewind(fp);
    while (fgets(line, MAXLINE, fp) != NULL)
    {
        history_inst_num++;
    }
    return history_inst_num;
}

void read_history_file()
{
    char line[MAXLINE];
    int i = 0;

    // 파일 포인터를 파일의 끝으로 이동하여 파일의 크기를 계산
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);

    // 파일 포인터를 파일의 시작으로 이동
    rewind(fp);
    // 파일의 내용을 한 줄씩 읽어와서 getHistoryInst[][]에 저장
    while (fgets(line, MAXLINE, fp) != NULL && i < MAXLINE)
    {
        strcpy(get_history[i], line);
        i++;
    }
}

void sigint_handler()
{
    for (int i = 0; i < MAXARGS; i++)
    {
        if (job_list[i].state == FG)
        {
            // printf(" [Ctrl+C]\n");
            kill(job_list[i].pid, SIGKILL);
        }
    }
}

void sigtstp_handler()
{
    for (int i = 0; i < MAXARGS; i++)
    {
        if (job_list[i].state == FG)
        {
            job_list[i].state = SUSP;
            // printf(" [Ctrl+Z]\n");
            kill(job_list[i].pid, SIGTSTP);
        }
    }
}

void sigchld_handler() // child가 종료or중지 되었을 때 parent가 받게 되는 signal
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WCONTINUED)) > 0)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status)) // 종료됐다면
        {
            for (int i = 0; i < MAXARGS; i++)
            {
                if (job_list[i].pid == pid)
                    job_list[i].state = DONE;
            }
        }
    }
}

int find_job_state(pid_t pid)
{
    int i;
    for (i = 0; i < MAXARGS; i++)
    {
        if (job_list[i].pid == pid)
        {
            return i;
        }
    }
    return -1; // 작업이 없는 경우
}

void print_job()
{
    for (int i = 0; i < MAXARGS; i++)
    {
        if (job_list[i].state == SUSP)
        {
            printf("[%d]\tsuspended\t%s\n", job_list[i].id, job_list[i].cmdline);
        }
        else if (job_list[i].state == BG)
        {
            char *cmd = job_list[i].cmdline;
            int len = strlen(cmd);
            if (cmd[len - 1] == '&')
            {
                cmd[len - 1] = '\0'; // remove '&'
            }
            printf("[%d]\trunning \t%s\n", job_list[i].id, job_list[i].cmdline);
        }
    }
}

int add_job(job *job_list, pid_t pid, int state, char *cmdline)
{
    if (job_list[job_num].state == NONE)
    {
        job_list[job_num].pid = pid;
        job_list[job_num].state = state;
        strcpy(job_list[job_num].cmdline, cmdline);
        job_list[job_num].id = job_num;
        job_num++;
    }
}

void clear_job(int index)
{
    job_list[index].pid = 0;
    job_list[index].state = NONE;
    job_list[index].cmdline[0] = '\0';
    job_list[index].id = 0;
}

void done_job()
{
    for (int i = 0; i < MAXARGS; i++)
    {
        if (job_list[i].state == DONE)
        {
            printf("[%d]\tDone     \t%s\n", job_list[i].id, job_list[i].cmdline);
            clear_job(i);
        }
        else if (job_list[i].state == KILL)
        {
            clear_job(i);
        }
    }
    for (int i = MAXARGS - 1; i > 0; i--)
    {
        if (job_list[i].state == FG || job_list[i].state == BG || job_list[i].state == SUSP)
            break;
        job_num = i;
    }
}

void remove_ampersand(char *cmdline, char *add_cmdline)
{
    int len = strlen(cmdline);
    int i = len - 1;
    // 문자열 끝에서부터 첫 번째 공백 문자 이전의 문자 위치 찾기
    while (i >= 0 && cmdline[i] == ' ')
    {
        i--;
    }
    while (i >= 0 && cmdline[i] != ' ')
    {
        i--;
    }
    // '&' 문자가 있다면 제거하기
    if (i >= 0 && cmdline[i + 1] == '&')
    {
        i++;               // '&' 문자 위치로 돌아가기
        cmdline[i] = '\0'; // '&' 문자 제거
    }
    // 새로운 문자열 생성
    int j = 0;
    while (cmdline[j] == ' ')
    {
        j++;
    }
    strcpy(add_cmdline, cmdline + j);

    // add_cmdline 배열 마지막이 '\n' 문자일 때만 '\0' 으로 수정
    int k = strlen(add_cmdline);
    if (add_cmdline[k - 1] == '\n')
    {
        add_cmdline[k - 1] = '\0';
    }
}