#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include "exec.h"

jmp_buf SIGINT_buf;

list shell_c = NULL;
char host[100];

void hndlr_int0(int s)
{
	if (shell_c != NULL)
		dellist(&shell_c);
	printf("\n");
	longjmp(SIGINT_buf, 2);
}


void inv()
{
	printf("%s%s", "\x1b[32m", "\033[1m"); // Changing color to green bold
	getcwd(host, 100);
	printf("%s%s%s:%s", getenv("USER"), "\033[22;35m", "\033[1m", host);
	printf("%s%s", "\x1B[37m", "\033[1m"); // Changing color to default bold
	printf(":#$ ");
}


int main(int argc, char* argv[])
{
	gethostname(host, 100);
	printf("%sWelcome to jars - Just Another Ruslan Shell!%s\n", "\033[01;36m", "\033[0m");
	char c;  int res;
	char string[256] = "\0";
	int j = 0;
	char* shell_n = argv[0];
	setjmp(SIGINT_buf);
	while(1!=2)
	{
		signal(SIGINT, hndlr_int0);
		inv();
		j = 0;
		while (1 != 2)
		{
			c = getchar();
			if (c == '\n')
			{
				string[j++] = '\0';
				break;
			}
			
			else if (c == EOF)
			{
				_exit(0);
			}
			
			string[j++] = c;
		}
		
		res = formlist(&shell_c, string);
		if (res)
		{
			printf("Met an error while making a list\n");
			dellist(&shell_c);
			continue;
		}
		res = jars(shell_c, shell_n);
		if (res== -1)
		{
			printf("Met an error while executing the list of commands\n");
		}
		
		else if (res == 2) // Met an exit
			_exit(0);
			
		shell_c = NULL;
	}
}
