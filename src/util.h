#ifndef _UTIL_H_
#define _UTIL_H_

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define True  1
#define False 0
#define source_base       "/usr/portage/distfile/"


#define oops(ch, args...)\
    {fprintf(stderr,ch,##args);perror("Reason from system call: ");return -1;}


#define INIT_LIST(instance, type) do {                                  \
        if (instance == NULL) {                                         \
            instance = (type *)malloc(sizeof(type));                    \
            if (instance == NULL) {                                     \
                fprintf(stderr, "ERROR: failed to alloc memory.\n");    \
                return -1;                                              \
            }                                                           \
            memset(instance, 0, sizeof(type));                          \
        }                                                               \
    } while (0);

// Seek to list tail and create new empty.
#define SEEK_LIST_TAIL(lst, ptr, type)                      \
    do                                                      \
    {                                                       \
        ptr = lst;                                          \
        while (ptr)                                         \
        {                                                   \
            if (ptr->next == NULL)                          \
            {                                               \
                ptr->next = (type*) malloc(sizeof(type));   \
                memset(ptr->next, 0, sizeof(type));         \
                break;                                      \
            }                                               \
            else                                            \
            {                                               \
                ptr = ptr->next;                            \
            }                                               \
        }                                                   \
    } while (0)                                             \

#define handle_error(msg)                               \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

// #define DEBUG       1

#ifdef DEBUG
#define PDEBUG(fmt, args...)                                \
    printf("%s(%d)-%s:\t",__FILE__,__LINE__,__FUNCTION__);  \
    printf("\033[31m"fmt"\033[0m", ##args);
#else
#define PDEBUG(fmt, args...)  ;
#endif


typedef unsigned int   uint32;

extern const char *dist_path;
/* Action types */
typedef enum _ActType{
    AT_UNKNOWN = 0,
    AT_ADD,
    AT_DELETE,
    AT_LIST,
    AT_CLEANUP,
} ActType;

/* Object types */
typedef enum _ActObject{
    AO_UNKNOWN = 0,
    AO_KEYWORD,
    AO_MASK,
    AO_USE,
    AO_UMASK,
} ActObject;

typedef struct _type2path {
    ActObject  obj;
    char   *path;
} type2path;

typedef struct _str_list {
    struct _str_list *next;
    char             *str;
    union{
        int counter; // counter for root node.
        int flag;    // Flag for leaves.
    } un;
} str_list;

typedef struct _NameList
{
    char* name;
    struct _NameList* next;
} NameList;

typedef struct _PkgInfo {
    char*     fullPath;                 /*!< Full Path of latest file. */
    long      version;                  /*!< Version of latest file  */
    long      size;                     /*!< Size of latest file  */
    NameList* del_list;                 /*!< List of files of older version. */
} PkgInfo;

typedef void (*DestroyFunction)(void* data);

typedef uint32  (*HashFunction)(const char* key);

// Hash Tables.
typedef struct _TableEntry
{
    char* key;
    void* val;
} TableEntry;

typedef struct _HashTable
{
    int         capacity;
    TableEntry* entries;

    HashFunction    hashFunctor;
    DestroyFunction deFunctor;
} HashTable;

typedef struct _KmuOpt
{
    bool      verbose;
    ActType   act;
    ActObject obj;
    char      args[1024];
} KmuOpt;


typedef struct _CharArray
{
    int    size;
    char** array;
} CharArray;


// Functions.
void HashTableDestroy(HashTable* table);
HashTable* HashTableCreate(uint32 size, HashFunction cFunctor, DestroyFunction dFunctor);
int InsertEntry(HashTable* table, char* key, void* val);
void* GetEntryFromHashTable(HashTable* table, char* key);

CharArray* ParseString(const char *str, bool flag);
int should_reserve(const char *key);
char *name_split(const char *fullname);

CharArray* CharArrayCreate(int size);
void CharArrayDestroy(CharArray* array);


void list_add(str_list *root, void *new);

int dir_exist(const char *path);
int file_exist(const char *path);
int cmpstringgp(const void *p1, const void *p2);


#endif /* _UTIL_H_ */
/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=4 tabstop=4 expandtab
 * :indentSize=4:tabSize=4:noTabs=true:
 */
