#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* POSIX */
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

/* p9p */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <9pclient.h>
#include <acme.h>

typedef struct Node Node;
typedef enum Flag Flag;

struct Node
{
	char*        name;
	struct stat* stat;
	int          nchildren;
	Node*        parent;
	Node**       children;
};

enum Flag
{
	CHILD = 0,
	PARENT = 1
};

Node*  getnode(char*, Node*, Flag);
void   freenode(Node*);
int    nchildren(Node*);
Node** getchildren(Node*);
void   writenode(Node*, Win*);
void   runeventloop(Node*);
char*  strtrim(char*);

void
threadmain(int argc, char *argv[])
{
	char cdir[PATH_MAX];
	Node* tree;
	
	if(getcwd(cdir, sizeof(cdir)) == NULL) 
	{
		perror("getcwd");
		return;
	}
	tree = getnode(cdir, NULL, PARENT);
	if(tree == NULL)
		return;
	runeventloop(tree);
	threadexitsall(NULL);
}

Node* 
getnode(char* name, Node* parent, Flag flag)
{
	Node *node;
	
	node = malloc(sizeof(Node));
	if(node == NULL)
	{
		perror("malloc");
		return NULL;
	}
	node->parent = parent;
	node->name = name;
	node->stat = malloc(sizeof(struct stat));
	if(node->stat == NULL)
	{
		perror("malloc");
		return NULL;
	}
	if(stat(node->name, node->stat) != 0)
	{
		perror("stat");
		free(node->stat);
		free(node);
		return NULL;
	}
	if(S_ISDIR(node->stat->st_mode))
	{
		if(flag)
		{
			node->nchildren = nchildren(node);
			node->children = getchildren(node);
		}
	}
	else
	{
		node->nchildren = 0;
		node->children = NULL;
	}
	return node;
}

void
freenode(Node* node)
{
	Node* top;
	
	top = node;
	while(top->parent != NULL)
		top = top->parent;
	/* Todo */
}

int
nchildren(Node* node)
{
	int nc;
	DIR* dir;
	
	dir = opendir(node->name);
	if(dir == NULL)
	{
		perror("opendir");
		return 0;
	}
	nc = 0;
	while(readdir(dir) != NULL)
    	nc++;
	closedir(dir);	
	return nc;
}

Node**
getchildren(Node* node)
{
	Node** children;
	struct dirent* entry;
	DIR* dir;
	int i;
	
	children = malloc(node->nchildren * sizeof(Node*));
	if(children == NULL)
	{
		perror("malloc");
		return NULL;
	}
 	dir = opendir(node->name);
	if(dir == NULL)
	{
		perror("opendir");
		free(children);
		return NULL;
	}
	for(i = 0; i < node->nchildren; i++)
	{
		entry = readdir(dir);
		if(entry == NULL)
			break; /* Todo */
		children[i] = getnode(entry->d_name, node, CHILD);
	}
	closedir(dir);
	return children;
}

void 
writenode(Node* node, Win* win)
{
	int i;
	
	winprint(win, "body", "%s/\n", basename(node->name));
	for(i = 0; i < node->nchildren; i++)
	{
		if(S_ISDIR(node->children[i]->stat->st_mode))
			winprint(win, "body", "\t%s/\n", basename(node->children[i]->name));
		else
			winprint(win, "body", "\t%s\n", basename(node->children[i]->name));		
	}
}

char*
strtrim(char* str)
{
	/* Todo */
	return str;
}

void
runeventloop(Node* node)
{
	Win* win;
	Event* ev;
	Node* current;
	
	current = node;
	win = newwin();
	winname(win, "+adir");
	winprint(win, "tag", "Get Win Hide");
	writenode(current, win);
	ev = malloc(sizeof(Event));
	
	while(winreadevent(win, ev))
	{
		switch(ev->c2)
		{
			case 'x': /* M2 in tag */
				if(strcmp(strtrim(ev->text), "Del") == 0)
				{
					windel(win, 1);
					break;
				} 
				else if(strcmp(strtrim(ev->text), "Get") == 0)
				{
					/* Todo */
				}
				else if(strcmp(strtrim(ev->text), "Win") == 0)
				{
					/* Todo */
				}
				else if(strcmp(strtrim(ev->text), "Hide") == 0)
				{
					/* Todo */
				}
				else
					winwriteevent(win, ev);
					
			//case 'X': /* M2 in body */
			
			//case 'L': /* M3 in body */
			
			default:
				winwriteevent(win, ev);
		}
	}
	
	winfree(win);
	free(ev);
	freenode(node);
}