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
#include <plumb.h>

#define MAX_DEPTH 8 /* Maximum number of unfolded nested directories */

typedef struct Node Node;

struct Node
{
	char*        name;
	struct stat* stat;
	int          nchildren;
	int          ishidden;   /* Toggle hidden files */
	int          isfolded;   /* Toggle directory folding */
	Node*        parent;
	Node**       children;
};

enum
{
	CHILD,
	PARENT
};

Node*  getnode(char*, Node*, int);
int    nchildren(Node*);
Node** getchildren(Node*);
int    winclear(Win*);
void   writenode(Node*, Win*, unsigned int);
void   runeventloop(Node*);
char*  strtrim(char*);
void   refreshnode(Node*);
void   freenode(Node*);
Node*  findnode(Node*, Event*);

void
threadmain(int argc, char *argv[])
{
	char cdir[PATH_MAX];
	Node* tree;
	
	if(getcwd(cdir, sizeof(cdir)) == NULL)
		sysfatal("Unable to getcwd()");
	tree = getnode(cdir, NULL, PARENT);
	runeventloop(tree);
	threadexitsall(NULL);
}

Node* 
getnode(char* name, Node* parent, int flag)
{
	Node *node;
	
	node = emalloc(sizeof(Node));
	node->parent = parent;
	node->name = name;
	node->ishidden = 0;
	if(flag)
		node->isfolded = 0;
	node->stat = emalloc(sizeof(struct stat));
	if(stat(node->name, node->stat) != 0)
		sysfatal("Unable to stat()");		
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
	
	children = emalloc(node->nchildren * sizeof(Node*));
 	dir = opendir(node->name);
	if(dir == NULL)
		sysfatal("Unable to opendir()");
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

int
winclear(Win* win)
{
	int n;
	
	n = winaddr(win, ",");
	if(n <= 0)
		return -1;
	return winwrite(win, "data", NULL, 0);
}

void 
writenode(Node* node, Win* win, unsigned int offset)
{
	int i, j;
	
	if(S_ISDIR(node->stat->st_mode))
	{
		winprint(win, "body", "%s/\n", basename(node->name));
		if(!node->isfolded)
		{
			for(i = 0; i < node->nchildren; i++)
			{
				if(!(node->ishidden && node->children[i]->name[0] == '.'))
				{
					if(offset <= MAX_DEPTH) /* avoid stack overflow */
					{
						j = offset;
						while(j)
						{
							winprint(win, "body", "\t");
							j--;
						}
						writenode(node->children[i], win, offset + 1);
					}
				}
			}
		}
	}
	else
		winprint(win, "body", "%s\n", basename(node->name));
}

/* Trim whitespace in place           */
/* https://stackoverflow.com/a/122721 */
char*
strtrim(char* str)
{
	char* end;
	
	while(isspace((unsigned char)*str))
		str++;
	if(*str == 0)
		return str;
	end = str + strlen(str) - 1;
	while(end > str && isspace((unsigned char)*end))
		end--;
	end[1] = '\0';
	return str;
}

void
freenode(Node *node)
{
	/* Todo */
}

void
refreshnode(Node* node)
{
	/* Todo */
}

Node*
findnode(Node* root, Event* ev)
{
	Node* node;
	
	/* Todo */
	node = root;
	return node;
}

void
runeventloop(Node* node)
{
	Win* win;
	Event* ev;
	Node* loc;
	
	win = newwin();
	winname(win, "+adir");
	winprint(win, "tag", "Get Win Hide");
	writenode(node, win, 1);
	ev = emalloc(sizeof(Event));
	
	for(;;)
	{
		if(winreadevent(win, ev) <= 0)
			break;
			
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
					loc = findnode(node, ev);
					refreshnode(loc);
				}
				else if(strcmp(strtrim(ev->text), "Win") == 0)
				{
					loc = findnode(node, ev);
					/* Todo */
				}
				else if(strcmp(strtrim(ev->text), "Hide") == 0)
				{
					node->ishidden = 1 - node->ishidden;
					winclear(win);
					writenode(node, win, 1);
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
}