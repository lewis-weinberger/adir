#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

/* p9p */
//#include "acme.h"
//#include "utf.h"

typedef struct Node Node;
typedef struct Tree Tree;

struct Node
{
	char *name;
	struct stat *stat;
};

struct Tree 
{
	Tree *parent;
	Node *root;
	int  nchildren;
	Node **children;
};

Node*  getnode(char*);
Tree*  gettree(Node*, Tree*);
int    nchildren(Node*);
Node** getchildren(Node*, int);
int    runeventloop(Tree*);
int    writetree(FILE*, Tree*);

int
main(int argc, char *argv[])
{
	char cdir[PATH_MAX];
	if(getcwd(cdir, sizeof(cdir)) == NULL) 
	{
		perror("getcwd");
		return 1;
	}
	Node *rnode = getnode(cdir);
	if(rnode == NULL)
		return 1;	
	Tree *rtree = gettree(rnode, NULL);
	if(rtree == NULL)
		return 1;
	return runeventloop(rtree);
}

Node*
getnode(char* name)
{
    Node *node = malloc(sizeof(Node));
    node->name = name;
    node->stat = malloc(sizeof(struct stat));
	if(stat(name, node->stat) != 0)
	{
		perror("stat");
		free(node->stat);
		free(node);
		return NULL;
	}
	return node;
}

Tree* 
gettree(Node* root, Tree* parent)
{
	Tree *tree = malloc(sizeof(Tree));
	tree->parent = parent;
    tree->root = root;
	if(S_ISDIR(root->stat->st_mode))
	{
	    tree->nchildren = nchildren(root);
		tree->children = getchildren(root, tree->nchildren);
	}
	else
	{
	    tree->nchildren = 0;
		tree->children = NULL;
	}
	return tree;
}

int
nchildren(Node* node)
{
	int nc = 0;
	DIR *dir = opendir(node->name);
	if(dir == NULL)
	{
		perror("opendir");
		return 0;
	}
	while(readdir(dir) != NULL)
    	nc++;
    closedir(dir);	
	return nc;
}

Node**
getchildren(Node *node, int nc)
{
	Node **children = malloc(nc * sizeof(Node*));
	struct dirent *entry;
	DIR *dir = opendir(node->name);
	if(dir == NULL)
	{
		perror("opendir");
		free(children);
		return 0;
	}
	for(int i = 0; i < nc; i++)
	{
		entry = readdir(dir);
		if(entry == NULL)
			break; /* Todo: handle properly */
    	children[i] = getnode(entry->d_name);
    }
    closedir(dir);
	return children;
}

int
runeventloop(Tree *tree)
{
	return writetree(stdout, tree);
}

int
writetree(FILE *file, Tree *tree)
{
	printf("%s/\n", basename(tree->root->name));
	for(int i = 0; i < tree->nchildren; i++)
	{
		if(S_ISDIR(tree->children[i]->stat->st_mode))
			printf("\t%s/\n", basename(tree->children[i]->name));
		else
			printf("\t%s\n", basename(tree->children[i]->name));		
	}
	return 0;
}