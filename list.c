#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "list.h"


void dellist(list* lst) // Removing the list from memory
{
	list q;
	while( *lst != NULL)
	{
		q = *lst;
		*lst = (*lst)->next;
		free(q);
	}
	*lst = NULL;
}


void addtolist(list* lst, char str[]) // Adding a word to a list
{  
	list p = *lst;
	if (*lst == NULL) 
	{
		*lst = calloc(1, sizeof(node));
		memcpy((*lst)->elem, str, 256);
		(*lst)->next = NULL;
	}

	else
	{
		while (p->next != NULL) // Going to the end
			p = p->next;
		
		p->next = calloc(1, sizeof(node));
		memcpy(p->next->elem, str, 256);
		p->next->next = NULL;	
	}
}


void printlist(list lst) // Printing the entire list
{
	list q = lst;
	while(q != NULL)
	{
		printf("\'%s\'  ", q->elem);
		q = q->next;
	}
	printf("\n");
}


void addtoword(char* word, int* index, char c) // Adding a char to word
{
	word[*index - 1] = c;
	++(*index); 
	word[*index - 1] = '\0';
}


int isvalidchr(char c) // Checking if a char is valid
{
	char chr[] = {'*', '~', '\"',' ', '\t', '\n', '$', '|', '&', ';', ':', '>', '<', '_', '/', '-', '.', '\0'};
	
	if (isalnum(c))
		return 1;
	
	
	for (int i = 0; chr[i] != '\0'; ++i)
		if (c == chr[i])
			return 1;
			
	fprintf(stderr, "Met unsupported character %c\n", c);
	return 0;
}


int formlist(list *dst, char str[]) // Forming a list
{	
	char c; // Current char
	list lst = NULL; // Word list that is being built
	char w[256] = "\0"; // Current word
	int i = 1, j = 0; // i - length of the current word, j - current position in string str[]
	int slash_flag = 0; // Flag for detecting if we are in slash-expression
	int quot_flag = 0; // Flag for expressions in " "
	
	while (1!=2)
	{
		c = str[j];

		if (c == '\\')
		{
			if (slash_flag == 0)
				slash_flag = 1;

			else
			{
				// In commands you can't write escape-sequences without taking them in " "
				if (!quot_flag) // That's why \\ is not supported outside of " " 
				{
					slash_flag = 0;
					dellist(&lst);
					lst = NULL;
				
					return -1;
				}
				else
				{
					slash_flag = 0;
					addtoword(w, &i, c);
				}
			}
		}
		
		else if (c == '\"') // Met quotation sign
		{
			if (!slash_flag)
				quot_flag = !quot_flag;

			else
			{
				// In commands you can't write escape-sequences without taking them in " "
				if (!quot_flag)
				{
					slash_flag = 0;
					dellist(&lst);
					lst = NULL;
				
					return -1;
				}
				
				else
				{
					slash_flag = 0;
					addtoword(w, &i, c);
				}
			}
		}

		else if (c == ';' || c == '<' ) // Met special char => going to the next char
		{
			slash_flag = 0;
			
			if (i > 1)
			{
				addtolist(&lst, w);
				w[0] = '\0';
				i = 1;
				addtoword(w, &i, c);
			}
			else
				addtoword(w, &i, c);
		}
		
		else if (c == '>' || c == '|' || c == '&' ) // Special chars that could be doubled
		{
			slash_flag = 0;
			if (i > 1) // If it is not the first char
			{
				if (strcmp(w, ">") == 0 && c == '>') // If it is ">>"
				{
					addtoword(w, &i, c);
				}
				else if (strcmp(w, "|") == 0 && c == '|') // If it is "||"
				{
					addtoword(w, &i, c);
				}				
				else if (strcmp(w, "&") == 0 && c == '&') // If it is "&&"
				{
					addtoword(w, &i, c);
				}								
				else
				{
					addtolist(&lst, w);
					w[0] = '\0';
					i = 1;
					addtoword(w, &i, c);
				}
			}
			else
				addtoword(w, &i, c);
		}
		
		else if (c == ' ' || c  == '\t') // Met space or tab
		{
			slash_flag = 0;
			
			if (!quot_flag)
			{
				if (i > 1)
				{
					addtolist(&lst, w);
					w[0] = '\0';
					i = 1;
				}
			}	
			
			else
				addtoword(w, &i, c);
		}
		
		else if (c == '\0' || (c == '#' && slash_flag == 0)) // Ending the list
		{
			slash_flag = 0;
			if (i > 1)
			{
				addtolist(&lst, w);
				w[0] = '\0';
				i = 1;
			}
			if (quot_flag == 0)
			{
				*dst = lst;
				lst = NULL;
				return 0;
				//printlist(lst);
			}
			else // Quotation was made, but didn't end
			{
				fprintf(stderr, "Error: quotation mark wasn't closed\n");
				dellist(&lst);
				lst = NULL;
			
				return -1;
			}
		}
		else // Anything else (regular chars)
		{
			slash_flag = 0;
			
			if (quot_flag || isvalidchr(c))
				addtoword(w, &i, c);
				
			else // Bad char met
			{
				dellist(&lst);
				lst = NULL;
				
				return -1;
			}	
		}
		
		// printlist(lst); // Debugging
		j++;
	}
	
	return -1; // List was made, but no returns
}

