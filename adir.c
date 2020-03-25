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
	int          noff;       /* Character offset when drawn on window */
	Node*        parent;
	Node**       children;
};

enum
{
	CHILD,
	PARENT
};

char* plan9;
char* acmeshell;

Node*  getnode(char*, Node*, int);
int    nchildren(Node*);
Node** getchildren(Node*);
int    winclear(Win*);
int    writenode(Node*, Win*, int, int);
void   runeventloop(Node*);
char*  strtrim(char*);
Node*  refreshnode(Node*);
void   freenode(Node*);
int    findnode(Node*, Node**, int);
void   runcommand(char*, char*, ...);

void
threadmain(int argc, char *argv[])
{
	char cdir[PATH_MAX];
	Node* tree;
	
	if((plan9 = getenv("PLAN9")) == NULL)
		sysfatal("PLAN9 not defined");
	if((acmeshell = getenv("acmeshell")) == NULL)
		acmeshell = "rc";
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
	int fd;
	
	node = emalloc(sizeof(Node));
	if(parent)
	{
		node->name = emalloc(strlen(name)+strlen(parent->name) + 1);
		strcpy(node->name, parent->name);
		strcat(node->name, "/");
		strcat(node->name, name);
	}
	else
	{
		node->name = emalloc(strlen(name));
		strcpy(node->name, name);
	}
	node->parent = parent;
	node->ishidden = 0;
	if(flag)
		node->isfolded = 0;
	else
		node->isfolded = 1;
	node->stat = emalloc(sizeof(struct stat));
	if((fd = open(node->name, O_RDONLY)) < 0)
		sysfatal("Unable to open()");
	if(fstat(fd, node->stat) < 0)
		sysfatal("Unable to fstat()");
	close(fd);		
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
		sysfatal("Unable to opendir()");
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

int 
writenode(Node* node, Win* win, int depth, int noff)
{
	int i, j, n;
	
	n = noff;
	node->noff = n;
	if(S_ISDIR(node->stat->st_mode))
	{
		n += winprint(win, "body", "%s/ %d\n", basename(node->name), node->noff);
		if(!node->isfolded)
		{
			for(i = 0; i < node->nchildren; i++)
			{
				if(!(node->ishidden && node->children[i]->name[0] == '.'))
				{
					if(depth <= MAX_DEPTH) /* avoid recursive stack overflow */
					{
						j = depth;
						while(j)
						{
							n += winprint(win, "body", "\t");
							j--;
						}
						n += writenode(node->children[i], win, depth + 1, n);
					}
				}
			}
		}
	}
	else
		n += winprint(win, "body", "%s %d\n", basename(node->name), node->noff);
	return n;
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
freenode(Node* node)
{
	/* Todo */
}

Node*
refreshnode(Node* old)
{
	Node* new;
	
	new = getnode(old->name, NULL, PARENT);
	freenode(old);
	return new;
}

int
findnode(Node* node, Node** found, int noff)
{
	int i;
	
	/* TODO FIX THIS */
	if(node->noff == noff)
	{
		*found = node;
		return 1;
	}
	else
	{
		if(S_ISDIR(node->stat->st_mode))
		{
			for(i = 0; i < node->nchildren; i++)
			{
				if(findnode(node->children[i], found, noff))
					return 1;
			}
		}
	}
	return 0; /* Failed to find matching node at offset */
}

/* Like sysrun but runs commands in the desired directory and */
/* respects the acmeshell environment variable,               */
/* Note: doesn't return first KB of output from command.      */
void runcommand(char* dir, char* fmt, ...)
{
	char*   args[4];
	int     fd[3], p[2];
	char*   cmd;
	va_list arg;

#undef pipe
	if(pipe(p) < 0)
		sysfatal("pipe: %r");
	fd[0] = open("/dev/null", OREAD);
	fd[1] = p[1];
	fd[2] = dup(p[1], -1);

	va_start(arg, fmt);
	cmd = evsmprint(fmt, arg);
	va_end(arg);
	
	args[0] = acmeshell;
	args[1] = "-c";
	args[2] = cmd;
	args[3] = NULL;
	threadspawnd(fd, args[0], args, dir);
	free(cmd);
}

void
loctoq(Event* ev, int* q)
{
	char* s1;
	char* s2;
	
	s1 = ev->loc;
	while(*s1 != ':')
		s1++;
	s2 = s1;
	while(*s2 != ',')
		s2++;
	q[0] = atoi(&s1[2]);
	q[1] = atoi(&s2[2]);
}

void
runeventloop(Node* node)
{
	Win* win;
	Event* ev;
	Node* loc;
	int q[2];
	
	win = newwin();
	winname(win, "%s/+adir", node->name);
	winprint(win, "tag", "Get Win Hide");
	writenode(node, win, 1, 0);
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
					goto Exit;
				} 
				else if(strcmp(strtrim(ev->text), "Get") == 0)
				{
					node = refreshnode(node);
				}
				else if(strcmp(strtrim(ev->text), "Win") == 0)
				{
					loctoq(ev, q);
					if(findnode(node, &loc, q[0])) /* M2+M1 chording */
						runcommand(loc->name, "%s/bin/win", plan9);
					else
						runcommand(node->name, "%s/bin/win", plan9);				
				}
				else if(strcmp(strtrim(ev->text), "Hide") == 0)
				{
					node->ishidden = 1 - node->ishidden;
					winclear(win);
					writenode(node, win, 1, 0);
				}
				else
					winwriteevent(win, ev);
				break;
						
			case 'X': /* M2 in body */
				if(findnode(node, &loc, ev->q0))
					winprint(win, "body", "%s\n", loc->name); /* TODO */
				winprint(win, "body", "%d\n", ev->q0);
				break;
				
			//case 'L': /* M3 in body */
				
			default:
				winwriteevent(win, ev);
		}
	}
	
	Exit:
		windel(win, 1);
		winfree(win);
		free(ev);
		freenode(node);
}