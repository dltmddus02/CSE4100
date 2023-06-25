/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#define MAXARGS 128

/* Function prototypes */
void eval(char *cmdline);
void eval_pipe(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
int parse_pipe(char **argv, char *argv_pipe[30][128]);
void insert_get_history(char *cmdline); // get_history 배열에 명령어 1개 추가
int count_history_inst();				// history에 줄 개수 세기
void read_history_file();				// history파일 읽어 get_history 배열에 명령어 전체 넣기
void history();
char path1[50];
char path2[50];
char get_history[MAXLINE][MAXLINE];
int history_inst_num = 0; // 현재 명령어 개수
FILE *fp;				  // 'a+'
char *pChar;
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
	char cmdline[MAXLINE];	 /* Command line */
	while (1)
	{
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
	}
}
/* $end shellmain */

// void insert_get_history(char *cmdline)
// {
// 	char *argv[MAXARGS]; /* Argument list execve() */
// 	char buf[MAXLINE];	 /* Holds modified command line */
// 	int bg;				 /* Should the job run in bg or fg? */

// 	strcpy(buf, cmdline);
// 	bg = parseline(buf, argv); // 스페이스로 구문된 인자들 argv로 파싱

// 	if (strcmp(argv[0], "history") == 0 || strcmp(argv[0], "!!") == 0 || (argv[0][0] == '!' && isdigit(atoi(argv[0] + 1)) == 0))
// 	{
// 		return;
// 	}
// 	else
// 	{
// 		strcpy(get_history[history_inst_num], cmdline);
// 		history_inst_num++;
// 	}
// }
void insert_get_history(char *cmdline)
{
	char *argv[MAXARGS]; /* Argument list execve() */
	char buf[MAXLINE];	 /* Holds modified command line */
	int bg;				 /* Should the job run in bg or fg? */

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

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
	char *argv[MAXARGS]; /* Argument list execve() */
	char buf[MAXLINE];	 /* Holds modified command line */
	int bg;				 /* Should the job run in bg or fg? */
	pid_t pid;			 /* Process id */

	strcpy(path1, "/bin/");
	strcpy(path2, "/usr/bin/");
	strcpy(buf, cmdline);
	bg = parseline(buf, argv); // 스페이스로 구문된 인자들 argv로 파싱
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
	{ // quit -> exit(0), & -> ignore, other -> run
		if ((pid = Fork()) == 0)
		{
			if (execvp(argv[0], argv) < 0)
			{ // ex) /bin/ls ls -al &
				printf("%s: Command not found.\n", argv[0]);
				exit(0);
			}
		}

		/* Parent waits for foreground job to terminate */
		if (!bg)
		{ // child가 foreground process인 경우
			int status;
			if (waitpid(pid, &status, 0) < 0)
				unix_error("waitfg: waitpid error");
		}
		else // when there is backgrount process!
			printf("%d %s", pid, cmdline);
	}
	return;
}

void eval_pipe(char *cmdline)
{
	int pfd[30];
	char *argv[MAXARGS] = {0};
	char buf[MAXLINE];
	char buf_copy[MAXLINE];
	int bg;
	pid_t pid;

	char *argv_pipe[30][128] = {0};

	strcpy(path1, "/bin/");
	strcpy(path2, "/usr/bin/");

	strcpy(buf, cmdline);
	strcpy(buf_copy, cmdline);

	bg = parseline(buf, argv); // 스페이스로 구문된 인자들 argv로 파싱
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
			if (i != 0)
				close(pfd[2 * (i - 1)]);
			if (i != argv_idx)
				close(pfd[2 * i + 1]);

			/* Parent waits for foreground job to terminate */
			if (!bg)
			{ // child가 foreground process인 경우
				int status;
				if (waitpid(pid, &status, 0) < 0)
					unix_error("waitfg: waitpid error");
			}
			else // when there is backgrount process!
				printf("%d %s", pid, cmdline);
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
			// eval(get_history[history_inst_num - 1]);
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
			// eval(get_history[n - 1]);
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
	int argc;	 /* number of args */
	int bg;		 /* background job? */

	buf[strlen(buf) - 1] = ' ';	  /* replace trailing '\n' with space */
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
