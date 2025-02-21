typedef struct Node *list; 

typedef struct Node
{
	char elem[256]; 
	struct Node* next; 
} node; 

void addtoword(char* word, int* index, char c); // Adding a char to word

int formlist(list *dst, char str[]); // Forming a list

void printlist(list lst); // Printing the entire list

void addtolist(list* lst, char str[]); // Adding a word to a list

void dellist(list* lst); // Removing the list from memory

int isvalidchr(char c); // Checking if a char is valid

