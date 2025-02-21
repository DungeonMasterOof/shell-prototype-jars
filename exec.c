#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include "exec.h"


// Making global variables
list cmd_list =  NULL; // List of commands
list conveyor =  NULL; // List for conveyor commands
list command = NULL; // One command
int fd_cd[2]; // In case of cd
int fd_pid[2]; // To determine which pids we should wait for
char cur_dir[256]; // Current directory
char shell[256]; // $SHELL
char user[256]; // $USER
char euid[256]; // $EUID
char home[256]; // $HOME
char* cur_lex = NULL; // Current lexem (word in formed list)
char* prev_lex = NULL; // Previous
int ind_lex = 0; // Index among all lexems
int cnt_pid = 0; // Count of tracked pids
char* path_to_shell; // Realpath to shell

void hndlr_int() // Clearing everything if met SIGINT
{
	if (command != NULL)
		dellist(&command);
		
	if (conveyor != NULL)
		dellist(&conveyor);
		
	if (cmd_list != NULL)
		dellist(&cmd_list);
		
	exit(0);
}


void get_next_cmd() // Getting next lexem in the list
{
	list q = cmd_list;
	
	if (q != NULL)
	{
		for (int i = 0; i < ind_lex; ++i)
		{
			q = q->next;
			if (q == NULL)
				break;
		}
	} 
	
	if (q != NULL)
		cur_lex = q->elem;
		
	else
		cur_lex = NULL;
		
	++ind_lex;
}


int jars(list s, char* n) // The shell itself
{
	int flag_work = 1;
	pid_t pid; 
	int status;
	ind_lex = 0;
	cnt_pid = 0;
	int flag_logic = 1;
	cmd_list = s;
	path_to_shell = n;
	if (cmd_list == NULL) // If we got no commands, then do nothing
		flag_work = 0;
	get_next_cmd(); // Taking first lexem
	pipe(fd_pid);
	pipe(fd_cd); // If we get cd inside the conveyor, we will stop entire shell
	signal(SIGINT, hndlr_int);
	
	while (flag_work)
	{
		conveyor = NULL;
		if (strcmp(cur_lex, ";") == 0 || strcmp(cur_lex, "&") == 0 || strcmp(cur_lex, "&&") == 0 || strcmp(cur_lex, "||") == 0) 
		{
				get_next_cmd();	
				if (cur_lex == NULL)
				{
					flag_work = 0;
					break;
				}
		}
		while (cur_lex != NULL && strcmp(cur_lex, ";") != 0 && strcmp(cur_lex, "&") != 0 && strcmp(cur_lex, "&&") != 0 && strcmp(cur_lex, "||") != 0)
		{
			addtolist(&conveyor, cur_lex);
			get_next_cmd(); 
			if (cur_lex == NULL)
			{
				flag_work = 0;
				break;
			}	
		}
		
		if (conveyor == NULL)
		{
			while(wait(NULL)!=-1) {}
			dellist(&cmd_list);
			return 1;
		}
		
		// printlist(conveyor);
		if (conveyor != NULL)
		{
			if (cur_lex != NULL && strcmp(cur_lex, ";") == 0)
			{
				if ((pid = fork()) == 0)
				{
					close(fd_pid[0]);
					close(fd_pid[1]);
					if (flag_logic)
						proc_conveyor();
					close(fd_cd[0]); close(fd_cd[1]);
					_exit(0);
				}
				else if (pid > 0)
				{
					waitpid(pid, &status, 0);
					dellist(&conveyor);
					if (WIFEXITED(status) && WEXITSTATUS(status) == 1)
					{
						dellist(&cmd_list);
						cmd_list = NULL;
						close(fd_cd[0]); close(fd_cd[1]);
						return 1;
					}
					if (WIFEXITED(status) && WEXITSTATUS(status) == 2)
					{
						dellist(&cmd_list);
						cmd_list = NULL;
						close(fd_cd[0]); close(fd_cd[1]);
						return 2;
					}
					else if (WIFEXITED(status) && WEXITSTATUS(status) == 126)
					// Need to change directory
					{
						dellist(&cmd_list);
						cmd_list = NULL;
						read(fd_cd[0], cur_dir, 256);
						if (chdir(cur_dir) == -1)
						{
							close(fd_cd[0]); close(fd_cd[1]);
							return 1;
						}
						close(fd_cd[0]); close(fd_cd[1]);
						return 0;
					}
				}
			}
			
			else if (cur_lex != NULL && strcmp(cur_lex, "&") == 0)
			{
				if ((fork() == 0) && flag_logic) // Making subshell to run a conveyor in background
				{
					signal(SIGINT, SIG_IGN);
					close(fd_pid[0]);
					close(fd_pid[1]);
					int fd = open("/dev/null", O_RDONLY);
					dup2(fd, 0);
					close(fd);
					proc_conveyor();
					close(fd_cd[0]); close(fd_cd[1]);
					_exit(0);
				}
				dellist(&conveyor);
			}
			
			else if (cur_lex != NULL && ((strcmp(cur_lex, "||") == 0) || (strcmp(cur_lex, "&&") == 0)) && flag_logic)
			{
				if (strcmp(cur_lex, "||") == 0) 
					flag_logic = 0;
				else // "&&"
					flag_logic = 1;
					
				if ((pid = fork()) == 0)
				{
					close(fd_pid[0]);
					close(fd_pid[1]);
					proc_conveyor();
					close(fd_cd[0]); close(fd_cd[1]);
					_exit(1);
				}
				else if (pid > 0)
				{
					waitpid(pid, &status, 0);
					dellist(&conveyor);
					if (WIFEXITED(status) && WEXITSTATUS(status) == 126)
					// Need to change directory
					{
						dellist(&cmd_list);
						cmd_list = NULL;
						read(fd_cd[0], cur_dir, 256);
						if (chdir(cur_dir) == -1)
						{
							close(fd_cd[0]); close(fd_cd[1]);
							return 1;
						}
						close(fd_cd[0]); close(fd_cd[1]);
						return 0;
					}
					else if (!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0))
						flag_logic = !flag_logic;
						
					continue;
				}
			}
			
			else if ((cur_lex == NULL) && flag_logic)
			{
				if ((pid = fork()) == 0)
				{
					close(fd_pid[0]);
					close(fd_pid[1]);
					proc_conveyor();
					close(fd_cd[0]); close(fd_cd[1]);
					_exit(0);
				}
				
				write(fd_pid[1], &pid, sizeof(pid));
				++cnt_pid;
				dellist(&conveyor);
			}
			
			dellist(&conveyor);
		}

		
		if (!flag_logic)
			flag_logic = 1;
	}
	// End of while (flag_work)


	for (int k = 0; k < cnt_pid; ++k) // Waiting for all commands in shell to end
	{
		read(fd_pid[0], &pid, sizeof(pid));
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 1)
		{
			dellist(&cmd_list);
			cmd_list = NULL;
			close(fd_cd[0]); close(fd_cd[1]);
			close(fd_pid[0]);
			close(fd_pid[1]);
			return 1;
		}
		else if (WIFEXITED(status) && WEXITSTATUS(status) == 2)
		{
			dellist(&cmd_list);
			cmd_list = NULL;
			close(fd_cd[0]); close(fd_cd[1]);
			close(fd_pid[0]);
			close(fd_pid[1]);
			return 2;
		}
		else if (WIFEXITED(status) && WEXITSTATUS(status) == 126)
		// Need to change directory
		{
			dellist(&cmd_list);
			cmd_list = NULL;
			read(fd_cd[0], cur_dir, 256);
			if (chdir(cur_dir) == -1)
			{
				close(fd_cd[0]); close(fd_cd[1]);
				return 1;
			}
			close(fd_pid[0]);
			close(fd_pid[1]);
			close(fd_cd[0]); close(fd_cd[1]);
			return 0;
		}
	}
	// Everything has ended 
	
	dellist(&cmd_list);
	cmd_list = NULL;
	close(fd_cd[0]); close(fd_cd[1]);
	close(fd_pid[0]);
	close(fd_pid[1]);
	return 0;
	
}


void proc_conveyor() // Processing 1 conveyor
{
	list q = conveyor; // Can't be NULL because of previous function
	int status;
	int fd[2];
	int out, in, next_in;
	command = NULL;
	while (strcmp(q->elem, "|") != 0) 
	{
		addtolist(&command, q->elem);
		q = q->next;
		if (q == NULL)
			break;
	}
	
	if (command == NULL) // No commands formed or only |
	{
		dellist(&conveyor);
		dellist(&cmd_list);
		close(fd_cd[0]); close(fd_cd[1]);
		_exit(2);
	}
	
	if (q == NULL) // All q was written, so no | in conveyor
	{
		proc_command();
	}
	
	pipe(fd); 
	out = fd[1];
	next_in = fd[0];
	
	if (fork() == 0) // First in conveyor
	{
		close(next_in);
		dup2(out, 1);
		close(out);
		proc_command();
	}
	
	dellist(&command);
	command = NULL;
	in = next_in; 
	while (1)
	{	
		if (q != NULL)
			q = q->next;
		close(out);
		pipe(fd);
		out = fd[1];
		next_in = fd[0];
		while (strcmp(q->elem, "|") != 0)
		{
			addtolist(&command, q->elem);
			q = q->next;
			if (q == NULL)
				break;
		}
		
		if (command == NULL)
		{
			dellist(&conveyor);
			dellist(&cmd_list);
			close(fd_cd[0]); close(fd_cd[1]);
			_exit(2);
		}
		
		if (q == NULL) // End of conveyor
		{
			close(out);
			close(next_in);

			if (fork()==0)
			{
				dup2(in, 0);
				close(in);
				proc_command();
			}
			dellist(&command);
			command = NULL;
			close(in);
			while(wait(&status)!=-1)
			{
				if (WIFEXITED(status) && (WEXITSTATUS(status) > 0) && (WEXITSTATUS(status <= 3)))
				{
					dellist(&conveyor);
					dellist(&cmd_list);
					close(fd_cd[0]); close(fd_cd[1]);
					_exit(WEXITSTATUS(status));
				}
			}
			
			dellist(&conveyor);
			dellist(&cmd_list);
			close(fd_cd[0]); close(fd_cd[1]);
			_exit(0);		
		}
		else if (strcmp(q->elem, "|") == 0)
		{
			if (fork()==0)
			{
				close(next_in);
				dup2(out, 1);
				close(out);
				dup2(in, 0);
				close(in);
				proc_command();
			}
			dellist(&command);
			command = NULL;
			close(in);
			in = next_in;
		}
	}
	
	while(wait(NULL)!=-1) {}
	close(fd_cd[0]); close(fd_cd[1]);
	_exit(0);
}



void shell_var(char *args[], int ofs)
{
	int i = 0;
	char* s;
	int k;
	s = getenv("HOME");
	memcpy(home, s, 256);
	realpath(path_to_shell, shell);
	s = getlogin();
	memcpy(user, s, 256);
	k = geteuid();
	snprintf(euid, 256, "%d", k);
	for (i = 0; i < ofs; ++i)
	{
		if (strcmp(args[i], "$HOME")==0)
		{
			args[i] = home;
		}
		else if (strcmp(args[i], "$SHELL")==0)
		{
			args[i] = shell;
		}
		else if (strcmp(args[i], "$USER")==0)
		{
			args[i] = user;
		}
		else if (strcmp(args[i], "$EUID")==0)
		{
			args[i] = euid;
		}
	}
}


void proc_command() // Processing a command with arguments
{
	errno = 0;
	pid_t pid0;
	int status;
	
	if ((pid0 = fork())==0)
	{
		char* args[256];
		int ofs = 0;
		int fd1;
		int writing_flag = 1, reading_flag = 1; 
		// Checking that everything is okay according to structure
		// reading_flag is check for "<"
		// writing_flag is check for ">" and ">>"
		list p = command;
		while (p!=NULL)
		{
			if (strcmp(p->elem, "<") == 0) // Checking from file
			{
				if (!reading_flag)
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					fprintf(stderr, "Error: too many input files\n");
					_exit(2);
				}
				
				p = p->next;
				
				if (p == NULL)
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					fprintf(stderr, "Error: nowhere to read from\n");
					_exit(2);
				}
				
				fd1 = open(p->elem, O_RDONLY);
				
				if (fd1 < 0) // Error in file
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					perror("Error in opening file");
					_exit(1);
				}
				dup2(fd1, 0);
				close(fd1);
				p = p->next;
				reading_flag = 0;
			}
			else if (strcmp(p->elem, ">")==0)
			{
				if (!writing_flag)
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					fprintf(stderr, "Error: too many output files\n");
					_exit(2);
				}
				p = p->next;

				if (p==NULL)
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					fprintf(stderr, "Error: nowhere to write\n");
					_exit(2);
				}

				fd1 = open(p->elem, O_WRONLY | O_CREAT | O_TRUNC, 0664);

				if (fd1 < 0)
				{
					
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					perror("Error in opening file");
					_exit(1);
				}
				
				dup2(fd1, 1);
				
				close(fd1);
				
				p = p->next;
				writing_flag = 0;
			}
			
			else if (strcmp(p->elem, ">>")==0)
			{
				if (!writing_flag)
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					fprintf(stderr, "Error: too many output files\n");
					_exit(2);
				}
				p = p->next;
				if (p==NULL)
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					fprintf(stderr, "Error: nowhere to write\n");
					_exit(2);
				}
				fd1 = open(p->elem, O_WRONLY | O_CREAT | O_APPEND, 0664);
				if (fd1 < 0)
				{
					dellist(&command);
					dellist(&cmd_list);
					dellist(&conveyor);
					close(fd_cd[0]); close(fd_cd[1]);
					perror("Error in opening file");
					_exit(1);
				}
				dup2(fd1, 1);
				close(fd1);
				p = p->next;
				writing_flag = 0;
			}
			else // Writing everything into args - array of char*
			{
				args[ofs++] = p->elem;
				p = p->next;
			}
		}
		
		args[ofs] = NULL; // Last arg
		shell_var(args, ofs);
		
		if (strcmp(args[0], "exit")==0)
		{
			dellist(&command);
			dellist(&cmd_list);
			dellist(&conveyor);
			close(fd_cd[0]); close(fd_cd[1]);
			_exit(2);
		}
		
		else if (strcmp(args[0], "cd") == 0)
		{
			char* h;
			h = getenv("HOME");
			if (args[1] == NULL)
			{
				memcpy(cur_dir, h, 256);
				dellist(&command);
			}
			else
			{
				memcpy(cur_dir, args[1], 256);
			}
			write(fd_cd[1], cur_dir, 256);
			dellist(&cmd_list);
			dellist(&command);
			dellist(&conveyor);
			close(fd_cd[0]); close(fd_cd[1]);
			_exit(126);
		}
		else
		{
			if (fork()==0)
			{
				execvp(args[0], args);
				perror("Error in executing command");
				dellist(&command);
				dellist(&cmd_list);
				dellist(&conveyor);
				close(fd_cd[0]); close(fd_cd[1]);
				_exit(1);
			}
			else
			{
				wait(&status);
				{
					if (WIFEXITED(status) && WEXITSTATUS(status) == 1)
					{
						dellist(&command);
						dellist(&cmd_list);
						dellist(&conveyor);
						close(fd_cd[0]); close(fd_cd[1]);
						_exit(1);
					}
					else if (WIFSIGNALED(status))
					{

						dellist(&command);
						dellist(&cmd_list);
						dellist(&conveyor);
						close(fd_cd[0]); close(fd_cd[1]);
						if (WTERMSIG(status) == SIGPIPE)
							_exit(0);
						
						_exit(1);
					}
				}
				
				dellist(&command);
				dellist(&cmd_list);
				dellist(&conveyor);
			}
		}
		close(fd_cd[0]); close(fd_cd[1]);
		_exit(0);
	}
	else if (pid0 > 0)
	{
		wait(&status);
		{
			if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			{
				dellist(&command);
				dellist(&cmd_list);
				dellist(&conveyor);
				close(fd_cd[0]); close(fd_cd[1]);
				_exit(WEXITSTATUS(status));
			}
		}
	}
	dellist(&command);
	dellist(&cmd_list);
	dellist(&conveyor);
	close(fd_cd[0]); close(fd_cd[1]);
	_exit(0);
	
}
