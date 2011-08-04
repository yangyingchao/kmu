#ifndef _UTIL_H_
#define _UTIL_H_

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define True  1
#define False 0
#define source_base       "/usr/portage/distfile/"

#define oops(ch, args...)\
    {fprintf(stderr,ch,##args);perror("Reason from system call: ");return -1;}


#ifdef DEBUG
#define PDEBUG(fmt, args...)                                \
    printf("%s(%d)-%s:\t",__FILE__,__LINE__,__FUNCTION__);  \
    printf("\033[31m"fmt"\033[0m", ##args);
#else
#define PDEBUG(fmt, args...)  ;
#endif

#ifdef DEBUG
#define PRINT_DEBUG(format, args...)                        \
    printf("%s(%d)-%s:\t",__FILE__,__LINE__,__FUNCTION__);  \
    printf("\033[31m"format"\033[0m", ##args);
#else
#define PRINT_DEBUG(format, args...)
#endif


extern const char *dist_path;
/* Action types */
typedef enum _act_type{
    UNKNOWN = 0,
    ADD,
    DELETE,
    LIST,
    CLEANUP,
} act_type;

/* Object types */
typedef enum _object{
    UNKNOWN_OBJ = 0,
    KEYWORD,
    MASK,
    USE,
    UMASK,
} object;

typedef struct _type2path {
    object  obj;
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

typedef struct _name_version {
    struct _name_version *next;
    char                 *name;
    char                 *to_keep;
    time_t                version;
    size_t                size;
} name_version;


char **strsplit(const char *str);
int should_reserve(const char *key);
char *name_split(const char *fullname);
void free_array(char **array);

void list_add(str_list *root, void *new);

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
