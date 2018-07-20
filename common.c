#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include "common.h"

static char* _list_dir(DIR * dir, int len) {
	struct dirent * ent = readdir(dir);
	struct stat info;

	if (ent == NULL) {
		char * listing = malloc((len + 1) * sizeof(char));
		listing[len] = '\0';
		return listing;
	}
	if (stat(ent->d_name, &info) < 0) {
		perror("stat");
		return NULL;
	}
	
	int ent_len = 0;
	if(S_ISREG(info.st_mode)){
		ent_len = strlen(ent->d_name);
		ent_len++; /* Account for the \n */
	}
	
	char * listing = _list_dir(dir, len + ent_len);	
	
	if(S_ISREG(info.st_mode)){	
		strcpy(&listing[len], ent->d_name);
		listing[len + ent_len - 1] = '\n';
	}
	return listing;
}


int file_size(char * filename){
	long filesize = -1;
	FILE * file;
	file = fopen (filename, "r");
	if (file == NULL){
		perror("fopen");
		goto err;
	}
	
	if(fseek (file, 0, SEEK_END) != 0){
		perror("fseek");
		goto err;
	}
	if((filesize = ftell(file)) < 0){
		perror("ftell");
		goto err;
	}		
	
	fclose(file);
	
	return filesize;
	
	err:
	if (file) {
		fclose(file);
	}
	return -1;
}

//@return must be freed by the caller
char* list_dir(){
	DIR *dir = opendir("."); 
	if (dir == NULL){
		perror("opendir");
		return NULL;
	}
	
	char * listing = _list_dir(dir, 0);
	closedir(dir);
	return listing;
}


// from <LineParser.c>

#ifndef NULL
    #define NULL 0
#endif

#define FREE(X) if(X) free((void*)X)

static char *cloneFirstWord(char *str)
{
    char *start = NULL;
    char *end = NULL;
    char *word;

    while (!end) {
        switch (*str) {
            case '>':
            case '<':
            case 0:
                end = str - 1;
                break;
            case ' ':
                if (start)
                    end = str - 1;
                break;
            default:
                if (!start)
                    start = str;
                break;
        }
        str++;
    }

    if (start == NULL)
        return NULL;

    word = (char*) malloc(end-start+2);
    strncpy(word, start, ((int)(end-start)+1)) ;
    word[ (int)((end-start)+1)] = 0;

    return word;
}

static void extractRedirections(char *strLine, cmdLine *pCmdLine)
{
    char *s = strLine;

    while ( (s = strpbrk(s,"<>")) ) {
        if (*s == '<') {
            FREE(pCmdLine->inputRedirect);
            pCmdLine->inputRedirect = cloneFirstWord(s+1);
        }
        else {
            FREE(pCmdLine->outputRedirect);
            pCmdLine->outputRedirect = cloneFirstWord(s+1);
        }

        *s++ = 0;
    }
}

static char *strClone(const char *source)
{
    char* clone = (char*)malloc(strlen(source) + 1);
    strcpy(clone, source);
    return clone;
}

static int isEmpty(const char *str)
{
  if (!str)
    return 1;
  
  while (*str)
    if (!isspace(*(str++)))
      return 0;
    
  return 1;
}

static cmdLine *parseSingleCmdLine(const char *strLine)
{
    char *delimiter = " ";
    char *line, *result;
    
    if (isEmpty(strLine))
      return NULL;
    
    cmdLine* pCmdLine = (cmdLine*)malloc( sizeof(cmdLine) ) ;
    memset(pCmdLine, 0, sizeof(cmdLine));
    
    line = strClone(strLine);
         
    extractRedirections(line, pCmdLine);
    
    result = strtok( line, delimiter);    
    while( result && pCmdLine->argCount < MAX_ARGUMENTS-1) {
        ((char**)pCmdLine->arguments)[pCmdLine->argCount++] = strClone(result);
        result = strtok ( NULL, delimiter);
    }

    FREE(line);
    return pCmdLine;
}

static cmdLine* _parseCmdLines(char *line)
{
	char *nextStrCmd;
	cmdLine *pCmdLine;
	char pipeDelimiter = '|';
	
	if (isEmpty(line))
	  return NULL;
	
	nextStrCmd = strchr(line , pipeDelimiter);
	if (nextStrCmd)
	  *nextStrCmd = 0;
	
	pCmdLine = parseSingleCmdLine(line);
	if (!pCmdLine)
	  return NULL;
	
	if (nextStrCmd)
	  pCmdLine->next = _parseCmdLines(nextStrCmd+1);

	return pCmdLine;
}

cmdLine *parseCmdLines(const char *strLine)
{
	char* line, *ampersand;
	cmdLine *head, *last;
	int idx = 0;
	
	if (isEmpty(strLine))
	  return NULL;
	
	line = strClone(strLine);
	if (line[strlen(line)-1] == '\n')
	  line[strlen(line)-1] = 0;
	
	ampersand = strchr( line,  '&');
	if (ampersand)
	  *(ampersand) = 0;
		
	if ( (last = head = _parseCmdLines(line)) )
	{	
	  while (last->next)
	    last = last->next;
	  last->blocking = ampersand? 0:1;
	}
	
	for (last = head; last; last = last->next)
		last->idx = idx++;
			
	FREE(line);
	return head;
}


void freeCmdLines(cmdLine *pCmdLine)
{
  int i;
  if (!pCmdLine)
    return;

  FREE(pCmdLine->inputRedirect);
  FREE(pCmdLine->outputRedirect);
  for (i=0; i<pCmdLine->argCount; ++i)
      FREE(pCmdLine->arguments[i]);

  if (pCmdLine->next)
	  freeCmdLines(pCmdLine->next);

  FREE(pCmdLine);
}

int replaceCmdArg(cmdLine *pCmdLine, int num, const char *newString)
{
  if (num >= pCmdLine->argCount)
    return 0;
  
  FREE(pCmdLine->arguments[num]);
  ((char**)pCmdLine->arguments)[num] = strClone(newString);
  return 1;
}