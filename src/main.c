/***************************************************************************
 *  Copyright (C) 2010-2012 yangyingchao@gmail.com

 *  Author: yangyingchao <yangyingchao@gmail.com>

 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2, or (at your option) any later
 *  version.

 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.

 *  You should have received a copy of the GNU General Public License. If not,
 *  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 ***************************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <ftw.h>
#include "util.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#ifdef __APPLE__
#define TIME_TO_LONG(X) X.tv_sec
#else
#define TIME_TO_LONG(X) X
#endif

// Hash Tables.
typedef struct _TableEntry
{
    char* key;
    char* fullPath;
    long  version;
    long  size;
} TableEntry;

typedef struct _HashTable
{
    int         capacity;
    TableEntry* entries;
} HashTable;

/* Options will be parsed */
static struct option long_options[] = {
    /* Actions */
    {"cleanup", no_argument, 0, 'c'},
    {"list",    no_argument, 0, 'l'},
    {"help",    no_argument, 0, 'h'},
    {"add",     no_argument, 0, 'a'},
    {"delete",  no_argument, 0, 'd'},
    /* Objects */
    {"use",  	no_argument, 0, 'u'},
    {"Umask",  	no_argument, 0, 'U'},
    {"mask",  	no_argument, 0, 'm'},
    {"keyword", no_argument, 0, 'k'},
    /* Helper */
    {"verbose", no_argument, 0, 'v'},
    {0, 0, 0, 0}
};

static int  freed_size = 0;
static int  deleted    = 0;

name_version *source_list;
name_version *del_list;
str_list *content_list = NULL;

static const type2path path_base[] = {
    { KEYWORD, 	"/etc/portage/package.keywords/keywords"},
    { MASK, 	"/etc/portage/package.mask/mask"},
    { USE, 		"/etc/portage/package.use/use" },
    { UMASK,    "/etc/portage/package.unmask/unmask"},
    { 0, 		NULL},
};

static const char * obj_desc[] = {
    "Ubknown object",
    "Keyword",
    "Mask",
    "USE",
    "Unmask"
};

static int verbose = 0;

#define PRINT_VERBOSE(format, args...)                            \
    if (verbose)\
    printf(format, ##args);

#define HASH_SIZE       4096

typedef unsigned int   uint32;


uint32 StringHashFunction(const char* str)
{
    uint32 hash = 0;
    uint32 i    = 0;
    const char*  key  = str;

    for (; i < strlen(str); ++i)
    {
        hash += *(key + i);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash % HASH_SIZE;
}

void HashTableDestroy(HashTable* table)
{
    if (table)
    {
        if (table->entries)
        {
            int i = 0;
            for (; i < table->capacity; ++i)
            {
                TableEntry* entry = &table->entries[i];
                if (entry->key)
                {
                    free(entry->key);
                }
                if (entry->fullPath)
                {
                    free(entry->fullPath);
                }
            }
            free(table->entries);
        }
        free(table);
    }
}

HashTable* HashTableCreate()
{
    HashTable* table = malloc(sizeof(HashTable));
    if (table)
    {
        memset(table, 0, sizeof(HashTable));
        table->capacity = HASH_SIZE;
        table->entries = malloc(sizeof(TableEntry) * HASH_SIZE);
        if (table->entries)
        {
            memset(table->entries, 0, sizeof(TableEntry) * HASH_SIZE);
        }
        else
        {
            HashTableDestroy(table);
            table = NULL;
        }
    }
    return table;
}

int InsertEntry(HashTable* table, char* key, char* fullPath, long version, long size)
{
    int ret = 0;
    if (!table || !key || !fullPath )
    {
        return ret;
    }

    uint32 index = StringHashFunction(key);
    // Insert entry into the first open slot starting from index.
    uint32 i;
    for (i = index; i < HASH_SIZE; ++i)
    {
        TableEntry* entry = &table->entries[i];
        if (entry->key == NULL)
        {
            ret = 1;
            entry->key      = key;
            entry->fullPath = fullPath;
            entry->version  = version;
            entry->size     = size;
            break;
        }
    }
    return ret;
}

TableEntry* GetEntryFromHashTable(HashTable* table, char* key)
{
    TableEntry* entry = NULL;
    uint32 index = StringHashFunction(key);
    int i;
    for (i = index; i < HASH_SIZE; ++i)
    {
        entry = &table->entries[i];
        if (entry->key == NULL)
        {
            return NULL;
        }
        if (strcmp(entry->key, key) == 0)
        {
            break;
        }
    }
    return entry;
}


static HashTable* SourceTable = NULL;


/**
 * Get corresponding path based on type of action.
 *
 * @param obj
 *
 * @return
 */

char* get_path(object obj)
{
    int idx = 0;
    int N   =  sizeof(path_base)/sizeof(type2path);
    char* prefix = NULL;
    prefix = getenv("EPREFIX");
    for (idx = 0; idx < N; idx++) {
        if (path_base[idx].obj == obj) {
            if (prefix)
            {
                return strcat(prefix, path_base[idx].path);
            }
            else
            {
                return path_base[idx].path;
            }
        }
    }
    return NULL;
}

/**
 * Demonstrate how to use this app.
 *
 * @param argv
 */
void usage(char **argv)
{
    printf ("Usage: kmu -a|d|l|h -k|m|u|U [package_string]\n");
    printf ("****** Objects: ********\n");
    printf ("-k, --keyword: Accept a new keyword specified by package_string\n");
    printf ("-m, --mask: 	mask a new keyword specified by package_string\n");
    printf ("-u, --use: 	Modify or add new use to package_string\n");
    printf ("-U, --Umask: 	Unmask a package\n\n");
    printf ("****** Operations: ********\n");
    printf ("-a, --add: 	Add an object.\n");
    printf ("-d, --delete: 	Delete an object.\n");
    printf ("-l, --list: 	List an object.\n");
    printf ("-c, --clean: 	Clean local resources.\n\n");
    printf ("-h, --help: 	Print this message.\n\n");
    printf("****** Examples: ******\n");
    printf("List all keywords stored in /etc/portage/package.keyword:\n"
           "\tkmu -lk\n");
    printf("Add a keyword into /etc/portage/package.keyword:\n"
           "\t kmu -ak\n");
    printf("Delete keyword entry which includes xxx\n"
           "\t kmu -du xxx\n");
}

/**
 * Compare two input strings to sort.
 *
 * @param p1,p2 - Strings to be compared.
 *
 * @return: int
 */
int cmpstringgp(const void *p1, const void *p2)
{
    char *pp1 = *(char * const *)p1;
    char *pp2 = *(char * const *)p2;

    while ( *pp1 == '<' || *pp1 == '=' || *pp1 == '>') {
        pp1 ++;
    }
    while ( *pp2 == '<' || *pp2 == '=' || *pp2 == '>') {
        pp2 ++;
    }

    return strcmp(pp1, pp2);
}


/**
 * Change list into a sortted array.
 *
 * @return: char**
 */
char **list_to_array()
{
    PDEBUG ("called.\n");

    int size = content_list->un.counter;
    if (size <= 0) {
        printf("Empty list!\n");
        return NULL;
    }

    size = size * sizeof(char *);

    PDEBUG ("A, size: %d\n", size);
    char **arrays = malloc(size);
    memset(arrays, 0, size);
    PDEBUG ("B\n");

    if (arrays != NULL) {
        int i = 0;
        int len = 0;
        char tmp[256];
        str_list *ptr = NULL;

        ptr = content_list->next;
        while (ptr) {
            len = strlen(ptr->str);
            if (ptr->un.flag == True || len  <= 1) {
                PDEBUG ("%s will not saved.\n", ptr->str);
                ptr = ptr->next;
                continue;
            }
            else {
                PDEBUG ("str: %s\n", ptr->str);

                memset(tmp, 0, 256);
                strncpy(tmp, ptr->str, len);
                if (*(ptr->str+len-1) != '\n') {
                    strcat(tmp, "\n");
                }
                arrays[i] = strdup(tmp);
            }
            i++;
            ptr = ptr->next;
        }

        PDEBUG ("Send to qsort, size: %d!\n", content_list->un.counter);
        qsort((void *)&arrays[0], content_list->un.counter,
              sizeof(char *), cmpstringgp);
    }
    PDEBUG ("return\n");

    return arrays;
}

/**
 * dump2file - Write content stored in content_list into a file.
 * @path - Character path, in which the contents will be writen into.
 *
 * Return: int
 */
int dump2file(const char *path)
{
    int ret = 0, writen, fd = 0;
    struct list_head *ptr = NULL;

    if (path == NULL) {
        fprintf(stderr, "ERROR: Empty path! \n");
        return -1;
    }

    char *dirn = dirname(strdup(path));
    if (dirn == NULL)
        oops ("Failed to get dirname");
    printf("Dirname: %s\n", dirn);
    if (dir_exist(dirn) != 0) {
        umask(000);
        ret = mkdir(dirn, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        if(ret)
            oops("Failed to create directory.");
    }

    char tmpl[256] = {'0'};
    sprintf(tmpl, "%s-XXXXXX", path);
    fd = mkstemp(tmpl);
    if (fd == -1)
        oops("Failed to mktemp");

    char **array = list_to_array();
    if (array == NULL) {
        fprintf(stderr, "ERROR: failed to convert list into string"
                "Items will be recorded without order.\n");
        int len;
        str_list *ptr = content_list->next;
        while (ptr != NULL) {
            len = strlen(ptr->str);
            PDEBUG ("%p, next: %p\n", ptr, ptr->next);
            if (ptr->un.flag == True || len <= 1) {
                PDEBUG ("%s will not saved.\n", ptr->str);
            }
            else {
                writen = write(fd, ptr->str, len);
                if (writen == -1)
                    oops("Failed to write:");
                writen = write(fd, "\n", 1);
            }
            ptr = ptr->next;
        }
    }
    else {
        int i;
        char *str;
        for (i = 0; i < content_list->un.counter; i++) {
            str = array[i];
            writen = write(fd, str, strlen(str));
            if (writen < 0) {
                fprintf(stderr, "ERROR: failed to write: %s, reason: %s\n",
                        str, strerror(errno));
                continue;
            }
        }
    }

    close(fd);

    ret = unlink(path);
    if ((ret == -1) && (errno == ENOTDIR)){
        oops("Failed to unlink old file!");
    }
    else if ((ret = link(tmpl, path)) == -1) {
        fprintf(stderr, "ERROR: Failed to create new file: %s\n", path);
        printf("%s\n",strerror(errno));
        fprintf(stderr, "ERROR: You can copy %s to %s by hand\n", tmpl, path);
    }
    else {
        unlink(tmpl);
        chmod(path,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        printf("Finished to write file: %s, total items: %d\n",
               path, content_list->un.counter);
    }
    return 0;
}

/**
 * Judge whether this item should be skipped.
 *
 * It may be skiped because:
 *    1. It's empty or nothing but new line.
 *    2. It starts with a # (a comment).
 *
 * @param item - Character item
 *
 * @return: bool
 */
int should_skip(char *item)
{
    int ret = 0;
    if (strlen(item) <= 1)	/* Nothing but a newline, skip it. */
        ret = 1;
    else {
        char *tmp = item;
        while (*tmp == ' ') {
            tmp ++;
        }
        if (*tmp == '\0' || *tmp == '#') {
            ret = 1;
        }
    }
    return ret;
}

/**
 * read_content - Read content of file into the global list.
 * @path - Character path, path of file to be read.
 *
 * Return: int
 */
int read_content(const char *path)
{
    size_t n;
    FILE *fd = NULL;
    ssize_t read;
    str_list *p = NULL;
    char * item = NULL;
    char *tmp = NULL;

    if (access(path, F_OK)) {
        printf ("Orignal file doest not exist!\n");
        return 0;
    }

    if ((fd = fopen(path, "r")) == NULL) {
        oops("Failed to open file");
    }
    while ((read = getline(&item, &n, fd)) != -1) {
        PDEBUG ("item: %s\n", item);

        if (should_skip(item)) {
            printf("Skip item: %s\n", item);
            goto next;
        }

        p = (str_list *) malloc(sizeof(str_list));
        memset(p, 0, sizeof(str_list));
        p->str = strdup(item);
        p->next = NULL;
        list_add(content_list, p);
        content_list->un.counter ++;
    next:
        if (item) {
            free(item);
        }
        item = NULL;
        continue;
    }
    PDEBUG ("End of loop\n");

    if (item)
        free(item);

    fclose(fd);
    return 0;
}

/**
 * key_exist - To query whether this key exists in memory or not.
 * @key - Character key
 *
 * Return: int
 */
str_list *key_exist(const char *key)
{
    str_list *ptr = content_list->next;

    while (ptr) {
        if (strstr(ptr->str, key)) {
            printf ("Found entry: %s\n", ptr->str);
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

/**
 * Merge new use into old ptr.
 * @param p -  ptr where old USE was stored
 *
 * @param  - New USE to be merged.
 *
 * @return: int
*/
int merge_use(str_list *p, const char *new)
{
    char **item_old = strsplit(p->str);
    char **item_new = strsplit(new);
    char new_item[1024];
    int i = 0, j = 0, size = 0, existed = 0;
    char **new_array = NULL;
    char *item;

    char tmp[1024];

    memset(new_item, 0, 1024);

    PDEBUG ("Old str: %s, new: %s\n", p->str, new);


    /* Caculate size to alloc. */
    while (item_old[i]) {
        size++;
        i++;
    }
    i = 1;
    while (item_new[i]) {
        size++;
        i++;
    }

    PDEBUG ("size = %d\n", size);

    new_array = (char **)malloc(size * sizeof(char *));

    i = 0;
    while (item_old[i]) {
        new_array[i] = strdup(item_old[i]);
        i++;
    }
    PDEBUG ("Finished to copy old USEs.\n");
    i = 1;
    while (item_new[i]) {
        item = item_new[i];
        if (!strlen(item)) {
            i++;
            continue;
        }
        existed = 0;
        j = 1;
        if (*item == '-') { /* -USE */
            PDEBUG ("Delete item: %s\n", item);
            while (new_array[j]) {
                if (strstr(new_array[j], item+1) != NULL ) {
                    if (strcmp(item+1, new_array[j]) == 0) {
                        /* USE vs -USE */
                        new_array[j] = strdup(item);
                        existed = 1;
                    }
                    else if (strcmp(item, new_array[j]) == 0){
                        /* -USE vs USE*/
                        existed = 1;
                    }
                    else {
                        /* Others */
                        existed = 0;
                    }
                }
                j++;
            }
        }
        else { /* +USE */
            PDEBUG ("Add item: %s\n", item);
            while (new_array[j]) {
                if (strstr(new_array[j], item)) {
                    PDEBUG ("Found: %s\n",new_array[j]);
                    if (*new_array[j] == '-' &&
                        strcmp(new_array[j]+1, item) == 0) {
                        PDEBUG ("Remove '-' from use!\n");
                        new_array[j] = strdup(item);
                        existed = 1;
                    }
                    else if (strcmp(new_array[j], item) == 0){
                        existed = 1;
                        PDEBUG ("%s: Excactly the same, skip.\n", item);
                    }
                    else {
                        PDEBUG ("Not the same: %s VS %s\n",
                                new_array[j], item);
                    }
                }
                j++;
            }
        }
        PDEBUG ("Existed ?%s\n", existed?"Y":"N");
        if (!existed) {
            PDEBUG ("Add: %d-%s\n", j, item);
            new_array[j] = strdup(item);
        }
        i++;
    }
    PDEBUG ("Finished to merge into char **\n");

    j++;
    new_array[j] = NULL;
    i = 0;
    while (new_array[i]) {
        PDEBUG ("new_array[%d] = %s\n", i, new_array[i]);

        memset(tmp, 0, 1024);
        memcpy(tmp, new_item, strlen(new_item));
        if (strlen(new_item)){
            sprintf (tmp, "%s %s", new_item, new_array[i]);
            memcpy(new_item, tmp, strlen(tmp));
        }
        else {
            sprintf(new_item, "%s", new_array[i]);
        }
        i++;
    }
    PDEBUG ("new_item: %s\n", new_item);

    free(p->str);
    p->str = strdup(new_item);
    PDEBUG ("return\n");
    return 0;
}

/**
 * Add or merge a string into object.
 *
 * @param obj
 * @param input_str
 *
 * @return
 */
int add_obj(object obj, const char *input_str)
{
    char *path;
    str_list *p = NULL;
    int  i = 0;

    if (input_str == NULL || strlen(input_str) == 0) {
        fprintf(stderr, "ERROR: You should provide a entry to add.\n");
        return -1;
    }

    printf("Adding new Item to %s.\n", obj_desc[obj]);
    path = get_path(obj);
    if (path == NULL){
        oops("Failed to get path according to object!\n");
    }

    if (read_content(path) == -1) {
        fprintf(stderr, "ERROR: Failed to read content of file: %s!\n",
                path);
        return -1;
    }


    switch (obj) {
    case USE: { /* USEs may need to be merged. */
        char **margv = strsplit(input_str);
        p = key_exist(margv[0]);
        free_array(margv);
        if (p != NULL) { /* Merge USE */
            char c;
            printf("Item %s is already in the destination!\n"
                   "Orignal content: %s\n", margv[0],  p->str);
            printf ("Would you like to merge it ?\n");
            c = fgetc(stdin);
            if ((c == 'Y') || (c == 'y')) {
                if (merge_use(p, input_str) != 0) {
                    fprintf(stderr, "ERROR: Failed to merge use "
                            "please update it manually!\n");
                    return -1;
                }
                PDEBUG ("After merged: %s\n", p->str);

                goto dump_add;
            }
            else {
                printf ("New item was not added to database.\n");
                return 0;
            }
        }
    }

    /* Fall through */
    case KEYWORD:
    case MASK:
    case UMASK: { /* These objects does not need merge, just add new ones. */
        p = (str_list *) malloc(sizeof(str_list));
        memset(p, 0, sizeof(str_list));
        p->str = strdup(input_str);
        list_add(content_list, p);
        content_list->un.counter ++;
        break;
    }
    default:
        break;
    }
dump_add:
    printf ("Added item: %s\n", p->str);
    return  dump2file(path);
}

/**
 * list_obj - List all items in this object.
 * @obj -  object
 * @key - Character key, a string of items interested in.
 *
 * Return: int
 * NOTE: Multiple items may be listed in one command.
 */
int list_obj(object obj, const char *key)
{
    char *path;
    str_list *ptr;
    printf("List entry in %s.\n", obj_desc[obj]);

    path = get_path(obj);
    if (path == NULL){
        fprintf(stderr, "ERROR: Failed to get path according to object!\n");
        return -1;
    }

    if (read_content(path) == -1) {
        fprintf(stderr, "ERROR: Failed to read content of file: %s!\n",
                path);
        return -1;
    }

    printf("Total entries: %d\n", content_list->un.counter);
    ptr = content_list->next;

    if (strlen(key) == 0) {
        /* All items will be displayed. */
        printf("Display all items:\n");
        ptr = content_list->next;
        while (ptr) {
            printf("    %s", ptr->str);
            ptr = ptr->next;
        }
    }
    else {
        PDEBUG ("key: %s\n", key);

        char **margv = strsplit(key);
        char **items = NULL;
        int i = 0;
        printf("Found entries including (%s):\n", key);
        while (margv[i]) {
            ptr = content_list->next;
            while (ptr) {
                items = strsplit(ptr->str);
                if (strstr(items[0], margv[i]))
                    printf ("    %s", ptr->str);
                free_array(items);
                ptr = ptr->next;
            }
            i++;
        }
        free_array(margv);
    }
    return 0;
}

/**
 * Delete a record which contains input_str in this Object.
 *
 * @param obj: an object used to identify which file to modify.
 * @param input_str: an input_str to identify which record to be removed.
 *
 * @Note: Multiple objects (separeted by whitespace) can be deleted each time.
 */
int del_obj(object obj, const char *input_str)
{
    int counter = 0, i = 0, ret = 0;
    char * path = get_path(obj);
    char c;

    if (input_str == NULL || strlen(input_str) == 0) {
        fprintf(stderr, "ERROR: You should provide a entry to delete.\n");
        return -1;
    }

    printf("Deleting entry for: %s.\n", obj_desc[obj]);
    if (path == NULL){
        printf ("Failed to get path according to object!\n");
        return -1;
    }
    printf("File: \t%s\nItem: \t%s\n", path, input_str);

    if (read_content(path) == -1) {
        fprintf(stderr, "ERROR: Failed to read content of file: %s!\n",
                path);
        return -1;
    }

    char **margv = strsplit(input_str);
    str_list *ptr = NULL;
    while (margv[i]) {
        ptr  = content_list->next;
        PDEBUG ("Target: %s\n", margv[i]);
        while (ptr) {
            if (strstr(ptr->str, margv[i])) {
                printf ("Item: %s", ptr->str);
                ptr->un.flag = True;
                content_list->un.counter --;
                counter++;
            }
            ptr = ptr->next;
        }
        i++;
    }
    free_array(margv);

    printf ("\nI found %d item(s).\n", counter);

    if (counter > 1) {
        printf ("Muliple items will be removed, as bellow:\n");
        ptr = content_list->next;
        while (ptr) {
            if (ptr->un.flag == True) {
                printf ("\t%s", ptr->str);
            }
            ptr = ptr->next;
        }
        printf ("Are you sure to do this?(Y or N)[Y]\n");
        c = fgetc(stdin);
        if ((c == 'Y') || (c == 'y')) {
            content_list->un.counter -= counter;
            ret= dump2file(path);
        }
        else {
            printf("Skipped.\n");
        }
    }
    else if (counter == 1) {
        content_list->un.counter -= 1;
        ret = dump2file(path);
    }
    else
        printf("No item has beed removed!\n");

    return ret;
}

/**
 * process_file - Called by ftw(), process single file.
 * @fpath - Character fpath
 * @sb -  sb
 * @typeflag - Type of
 * It manage two lists: source_list and del_list, the former one is used to
 * record files that are not going to be deleted, while the latter one is
 * used to record files that are going to be deleted.
 * Return: int
 */
int process_file(char *fpath)
{
    char *cptr, *bname;
    name_version     *p   = NULL;
    struct list_head *pptr = NULL;
    int found = 0, ret = 0;
    char *to_delete = NULL;
    char *to_keep = NULL;

    if (!fpath || (access(fpath, F_OK) == -1))
    {
        handle_error("File error\n");
    }
    struct stat sb;
    ret = stat(fpath, &sb);
    if (ret == -1)
    {
        printf("Failed to get file status: %s\n", fpath);
        return -1;
    }

    /*
     * Skip reserved files, Reason:
     *		1. They are kept for special reason (samba)
     *		2. They don't take up much disk size.
     */
    if (should_reserve(fpath)) {
        printf("Reserved file: %s\n", fpath);
        return 0;
    }

    cptr = name_split(fpath);
    if (cptr == NULL) {
        printf ("Unrecognized file: %s , will keep this package.\n",
                fpath);
        return 0;
    }

    bname = strdup(basename(strdup(cptr)));
    TableEntry* entry = GetEntryFromHashTable(SourceTable, bname);
    if (!entry)
    {
        return InsertEntry(SourceTable, bname, fpath, TIME_TO_LONG(sb.st_mtimespec), sb.st_size);
    }
    else
    {
        char* toDelete = NULL;

        if (entry->version < TIME_TO_LONG(sb.st_mtimespec)) // Target is newer.
        {
            printf ("A");
            toDelete         = entry->fullPath;
            freed_size      += entry->size; 
            entry->fullPath  = fpath;
            entry->size      = sb.st_size;
        }
        else
        {
            printf ("B");
            toDelete    = fpath;
            freed_size += sb.st_size;
        }

        name_version *ptr = calloc(sizeof(name_version), 1);
        ptr->name    = strdup(toDelete);
        ptr->to_keep = strdup(entry->fullPath);
        list_add((str_list *)del_list, ptr);
        deleted ++;
    }

    return 0;
}

/**
 * Delete or display files to be removed.
 *
 * @param doit - flag, whether to delete or not.
 *
 * @return: int
 */
int real_delete(int doit)
{
    int ret = 0;
    int i = 0;
    name_version     *ptr   = del_list->next;
    if (doit) { /* Real action to delete file!*/
        while (ptr) {
            ret = unlink(ptr->name);
            if (ret == -1)
                oops ("Failed to unlink file: %s\n", ptr->name);
            ptr = ptr->next;
        }
    }
    else { /* Not really delete, but display files to be removed. */
        printf ("To be deleted: \n");
        while (ptr) {
            printf ("\t%03d DEL:  %s\n", i, ptr->name);
            printf ("\t    KEEP: %s\n", ptr->to_keep);
            i++;
            ptr = ptr->next;
        }
    }
    return ret;
}

/**
 * Cleans up local cached files.
 *
 * @return: 0 if succeeded, or non-zero otherwise.
 */
int cleanup_localdist_resources(object obj)
{
    int ret;
    char c;
    PDEBUG ("enter\n");

    if (obj == UNKNOWN)
    {
        printf("Cleaning up distfiles.\n");

        if (SourceTable == NULL)
        {
            SourceTable = HashTableCreate();
            if (!SourceTable)
            {
                printf("Failed to create source table!\n");
                return -1;
            }
        }

        if (source_list == NULL)
        {
            source_list = malloc(sizeof(name_version));
            memset(source_list, 0, sizeof(name_version));
        }

        INIT_LIST(source_list, name_version);
        INIT_LIST(del_list, name_version);

        printf ("Scanning local resources...\n");
        DIR* dir = opendir(dist_path);
        if (dir)
        {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL)
            {
                if (entry->d_type & DT_DIR)
                {
                    continue;
                }
                char* name = entry->d_name;
                if (name)
                {
                    int length = strlen(dist_path) + strlen(name) + 2;
                    char* fullname = malloc(length);
                    if (!fullname)
                    {
                        handle_error("Failed to malloc");
                    }
                    sprintf(fullname, "%s/%s", dist_path, name);
                    process_file(fullname);
                }
            }
            if (deleted == 0) {
                printf ("No outdated file found!\n");
                goto out;
            }
            ret = real_delete(0);
            printf ("Going to deleted %d files,  %dM diskspace will be freed.\n",
                    deleted,  freed_size/1024/1024);
            printf ("Keep going? [Y]\n");
            c = fgetc(stdin);
            if (c == 'N' || c == 'n')
                printf ("Files are not deleted.\n");
            else{
                ret = real_delete(1);
                if (ret) {
                    fprintf(stderr, "ERROR: Failed to execute delete command!\n");
                }
            }
        }
        else
        {
            printf("Failed to open directory: %s\n", dist_path);
            ret = -1;
        }
    }
    else
    {
        char *path = get_path(obj);
        if (path == NULL) {
            fprintf(stderr, "ERROR: Failed to convert type to path\n");
            exit(1);
        }
        printf("Cleaning file: %s\n", path);
        if (read_content(path) == -1) {
            fprintf(stderr, "ERROR: Failed to read content of file: %s!\n",
                    path);
            exit(1);
        }
        ret = dump2file(path);
    }
 out:
    PDEBUG ("leave\n");

    return ret;
}

/**
 * Entry of KMU.
 *
 * @param argc - Number of arguments
 *
 * @param argv - Argument vector
 *
 * @return: 0 if succeeded, or non-zero otherwise.
 */
int main(int argc, char **argv)
{
    int c;
    act_type type = UNKNOWN;
    object obj = UNKNOWN;
    int ret=0;
    char items[1024];
    int err_flag = 0;

    PDEBUG ("enter\n");

    if (geteuid() != 0) {
            fprintf(stderr, "Should be executed as root, do not complain if you are not!\n");
    }

    INIT_LIST(content_list, str_list);

    /* Parse options from command line, store them into TYPE and OBJ */
    while (1) {

        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "adumklhcvU",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 0: {
            /* If this option set a flag, do nothing else now. */
            if (long_options[option_index].flag != 0)
                break;
            printf ("option %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;
        }
        case 'h': {
            usage(argv);
            return 1;
            break;
        }
        case 'a': {
            if (type != UNKNOWN) {
                printf ("Confilict actions!\n");
                err_flag = 1;
            }
            type = ADD;
            break;
        }
        case 'c': {
            if (type != UNKNOWN) {
                printf ("Confilict actions!\n");
                err_flag = 1;
            }
            type = CLEANUP;
            break;
        }
        case 'u': {
            if (obj != UNKNOWN) {
                printf ("Confilict objects!\n");
                err_flag = 1;
            }
            obj = USE;
            break;
        }

        case 'U': {
            if (obj != UNKNOWN) {
                printf ("Confilict objects!\n");
                err_flag = 1;
            }
            obj = UMASK;
            break;
        }

        case 'm': {
            if (obj != UNKNOWN) {
                printf ("Confilict objects!\n");
                err_flag = 1;
            }
            obj = MASK;
            break;
        }
        case 'k': {
            if (obj != UNKNOWN) {
                printf ("Confilict objects!\n");
                err_flag = 1;
            }
            obj = KEYWORD;
            break;
        }
        case 'd': {
            if (type != UNKNOWN) {
                printf ("Confilict actions!\n");
                err_flag = 1;
            }
            type = DELETE;
            break;
        }
        case 'l': {
            if (type != UNKNOWN) {
                printf ("Confilict actions!\n");
                err_flag = 1;
            }
            type = LIST;
            break;
        }
        case 'v': {
            verbose = 1;
            break;
        }
        case '?':
            break;

        default:
            printf("??\n");
            abort ();
        }
        if (err_flag) {
            usage(argv);
            return -1;
        }
    }

    memset(items, 0, 1024);
    if (optind < argc) {
        while (optind < argc) {
            strncat(items, argv[optind++], strlen(argv[optind]));
            strncat(items, " ", 1);
        }
    }

    switch (type) {
    case ADD: {
        ret = add_obj(obj, items);
        break;
    }

    case DELETE: {
        ret = del_obj(obj, items);
        break;
    }
    case LIST: {
        ret = list_obj(obj, items);
        break;
    }
    case CLEANUP:{
        ret = cleanup_localdist_resources(obj);
        break;
    }
    default:
        printf ("Unknown usage. \n");
        usage(argv);
        ret = -1;
        break;
    }

    /* It is not a daemon, and allocated memory will be released after this
     * app quit. */

    PDEBUG ("leave\n");

    return ret;
}
