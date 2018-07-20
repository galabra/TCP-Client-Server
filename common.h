#define DIR_MAX_SIZE 2048
#define LS_RESP_SIZE 2048
#define FILE_BUFF_SIZE 1024
#define MAX_ARGUMENTS 256

typedef enum {
	IDLE,
	CONNECTING,
	CONNECTED,
	DOWNLOADING,
	SIZE
} c_state;
	
typedef struct client_state {
	char* server_addr;	// Address of the server as given in the [connect] command. "nil" if not connected to any server
	c_state conn_state;	// Current state of the client. Initially set to IDLE
	char* client_id;	// Client identification given by the server. NULL if not connected to a server.
	int sock_fd;
} client_state;

int file_size(char * filename);
char* list_dir();


// from <LineParser.h>

typedef struct cmdLine
{
    char * const arguments[MAX_ARGUMENTS]; /* command line arguments (arg 0 is the command)*/
    int argCount;		/* number of arguments */
    char const *inputRedirect;	/* input redirection path. NULL if no input redirection */
    char const *outputRedirect;	/* output redirection path. NULL if no output redirection */
    char blocking;	/* boolean indicating blocking/non-blocking */
    int idx;				/* index of current command in the chain of cmdLines (0 for the first) */
    struct cmdLine *next;	/* next cmdLine in chain */
} cmdLine;

/* Parses a given string to arguments and other indicators */
/* Returns NULL when there's nothing to parse */ 
/* When successful, returns a pointer to cmdLine (in case of a pipe, this will be the head of a linked list) */
cmdLine *parseCmdLines(const char *strLine);	/* Parse string line */

/* Releases all allocated memory for the chain (linked list) */
void freeCmdLines(cmdLine *pCmdLine);		/* Free parsed line */

/* Replaces arguments[num] with newString */
/* Returns 0 if num is out-of-range, otherwise - returns 1 */
int replaceCmdArg(cmdLine *pCmdLine, int num, const char *newString);
