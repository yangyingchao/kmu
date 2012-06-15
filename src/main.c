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
#include "util.h"


#ifdef __APPLE__
#define GET_TIME(X) (X)->st_mtimespec.tv_sec
#else
#define GET_TIME(X) (X)->st_mtime
#endif

#define PRINT_VERBOSE(format, args...)          \
    do                                          \
    {                                           \
        if (opts->verbose)                      \
        {                                       \
            printf(format, ##args);             \
        }                                       \
    } while (0)



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

static const type2path path_base[] =
{
    { AO_KEYWORD,       "/etc/portage/package.keywords/keywords"},
    { AO_MASK,          "/etc/portage/package.mask/mask"},
    { AO_USE,           "/etc/portage/package.use/use" },
    { AO_UMASK,         "/etc/portage/package.unmask/unmask"},
    { 0,                NULL},
};

static const char * obj_desc[] = {
    "Ubknown ActObject",
    "Keyword",
    "Mask",
    "USE",
    "Unmask"
};

PkgInfo           *source_list;

static HashTable*  SourceTable  = NULL;
static const int   HASH_SIZE    = 4096;
static int         deleted      = 0;
static int         freed_size   = 0;
static int         verbose      = 0;
static KmuOpt*     opts         = NULL;
str_list*          content_list = NULL;


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



/**
 * Get corresponding path based on type of action.
 *
 * @param obj
 *
 * @return
 */

char* GetPathFromType(ActObject obj)
{
    int   idx    = 0;
    int   N      = sizeof(path_base)/sizeof(type2path);
    char* prefix = getenv("EPREFIX"); // Used by gentoo prefix(MacOsX).
    char* path = NULL;
    for (idx = 0; idx < N; idx++) {
        if (path_base[idx].obj == obj) {
            if (prefix)
            {
                unsigned int size = strlen(prefix) + strlen(path_base[idx].path) + 1;
                path = (char*)malloc(size);
                memset(path, 0, size);
                sprintf(path, "%s/%s", prefix, path_base[idx].path);
            }
            else
            {
                path = strdup(path_base[idx].path);
            }
        }
    }
    return path;
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
    printf ("-a, --add: 	Add an ActObject.\n");
    printf ("-d, --delete: 	Delete an ActObject.\n");
    printf ("-l, --list: 	List an ActObject.\n");
    printf ("-c, --clean: 	Clean local resources.\n");
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
 * Change list into a sortted array.
 *
 * @return: char**
 */
CharArray* list_to_array()
{
    int size = content_list->un.counter;
    if (size <= 0) {
        printf("Empty list!\n");
        return NULL;
    }

    PDEBUG ("list, size: %d\n", size);
    CharArray* ca = CharArrayCreate(size);
    if (ca)
    {
        char** arrays = ca->array;
        if (arrays != NULL) {
            int i = 0;
            int len = 0;
            char tmp[256];
            str_list *ptr = NULL;

            ptr = content_list->next;
            while (ptr) {
                len = strlen(ptr->str);
                if (ptr->un.flag == True || len  <= 1)
                {
                    PRINT_VERBOSE ("%s will not saved.\n", ptr->str);
                    ptr = ptr->next;
                    continue;
                }
                else
                {
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
    }

    return ca;
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

    CharArray* ca = list_to_array();
    char **array = ca->array;
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
bool should_skip(char *item)
{
    bool ret = true;
    if (strlen(item) > 1) {	/* Nothing but a newline, skip it. */
        char *tmp = item;
        while (*tmp == ' ') { // Skip leading white spaces.
            ++tmp;
        }
        if (*tmp != '\0' && *tmp != '#') {
            ret = false;
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
    FILE *fd = NULL;
    ssize_t read;
    str_list *p = NULL;

    if (access(path, F_OK)) {
        printf ("Orignal file doest not exist!\n");
        return 0;
    }

    if ((fd = fopen(path, "r")) == NULL) {
        oops("Failed to open file");
    }

    size_t n;
    char * item = NULL;
    while ((read = getline(&item, &n, fd)) != -1)
    {
        // PDEBUG ("item: %s\n", item);
        if (!should_skip(item))
        {
            p = (str_list *) malloc(sizeof(str_list));
            memset(p, 0, sizeof(str_list));
            p->str  = item;
            p->next = NULL;
            list_add(content_list, p);
            content_list->un.counter ++;
        }
        else
        {
            PRINT_VERBOSE("Skip item: %s\n", item);
            free(item);
        }

        item = NULL;
    }
    PDEBUG ("End of loop, total items: %d\n", content_list->un.counter);

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
int merge_use(str_list *p, CharArray* ca2)
{
    if (!p || !p->str || !strlen(p->str) || !ca2)
    {
        return -1;
    }

    /* Caculate size to alloc. */
    CharArray* ca1 = ParseString(p->str, true);
    if (!ca1)
    {
        return -1;
    }

    int size = ca1->size + ca2->size;

    PDEBUG ("Total size: %d\n", size);

    CharArray* newCa = CharArrayCreate(size);

    int i = 0;
    while (ca1->array[i])
    {
        newCa->array[i] = strdup(ca1->array[i]);
        i++;
    }
    PDEBUG ("Finished to copy old USEs.\n");
    CharArrayDestroy(ca1);

    // Loop through ca2, and merge into newCa.
    i = 1;
    int j = 0;
    while (ca2->array[i])
    {
        bool found = false;
        char* item = ca2->array[i];
        if (!strlen(item))
        {
            i++;
            continue;
        }
        j = 1;
        while (newCa->array[j])
        {
            char* target = newCa->array[j++];
            PDEBUG ("Cmp: %s, %s\n",item, target);
            if (strcmp(target+1, item+1) == 0) // USE found, copy into this place.
            {
                found = true;
                strncpy(target, item, strlen(item));
                break;
            }
        }
        if (!found) // Append USE to the tail
        {
            newCa->array[j] = strdup(item);
        }
        i++;
    }
    PDEBUG ("Finished to merge into CharArray\n");

    j++;
    newCa->array[j] = NULL;
    i = 0;
    char tmp[1024];
    char new_item[1024];
    memset(new_item, 0, 1024);

    while (newCa->array[i])
    {
        char* ptr = newCa->array[i];
        PDEBUG ("newCa->array[%d] = %s\n", i, ptr);
        memset(tmp, 0, 1024);
        memcpy(tmp, new_item, strlen(new_item));
        if (*ptr == '+')
        {
            ++ ptr;
        }

        if (strlen(new_item))
        {
            sprintf (tmp, "%s %s", new_item, ptr);
            memcpy(new_item, tmp, strlen(tmp));
        }
        else {
            sprintf(new_item, "%s", ptr);
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
 * Add or merge a string into ActObject.
 *
 * @param obj
 * @param input_str
 *
 * @return
 */
int add_obj(ActObject obj, const char *input_str)
{
    if (input_str == NULL || strlen(input_str) == 0) {
        fprintf(stderr, "ERROR: You should provide a entry to add.\n");
        return -1;
    }

    int ret = 0;
    char *path;
    printf("Adding new Item to %s.\n", obj_desc[obj]);
    path = GetPathFromType(obj);
    if (path == NULL){
        printf ("Failed to get path according to ActObject!\n");
        return -1;
    }

    if (read_content(path) == -1) {
        fprintf(stderr, "ERROR: Failed to read content of file: %s!\n",
                path);
        return -1;
    }
    str_list *p = NULL;
    switch (obj) {
        case AO_USE: { /* USEs may need to be merged. */
            CharArray *margv = ParseString(input_str, true);
            p = key_exist(margv->array[0]);
            if (p != NULL) { /* Merge USE */
                char c;
                printf("Item %s is already in the destination!\n"
                       "Orignal content: %s\n", margv->array[0],  p->str);
                printf ("Would you like to merge it ?\n");
                c = fgetc(stdin);
                if ((c == 'Y') || (c == 'y')) {
                    if (merge_use(p, margv) != 0) {
                        fprintf(stderr, "ERROR: Failed to merge use "
                                "please update it manually!\n");
                        ret = -1;
                        CharArrayDestroy(margv);
                    }
                    PDEBUG ("After merged: %s\n", p->str);

                    CharArrayDestroy(margv);
                    goto dump_add;
                }
                else {
                    printf ("New item was not added to database.\n");
                    ret = 0;
                    CharArrayDestroy(margv);
                }
            }
        }

            /* Fall through */
        case AO_KEYWORD:
        case AO_MASK:
        case AO_UMASK: { /* These ActObjects does not need merge, just add new ones. */
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
    printf ("Item added: %s\n", p->str);
    ret = dump2file(path);
    free(path);
out:
    return ret;
}

/**
 * list_obj - List all items in this ActObject.
 * @obj -  ActObject
 * @key - Character key, a string of items interested in.
 *
 * Return: int
 * NOTE: Multiple items may be listed in one command.
 */
int list_obj(ActObject obj, const char *key)
{
    char *path;
    str_list *ptr;
    printf("List entry in %s.\n", obj_desc[obj]);

    path = GetPathFromType(obj);
    if (path == NULL){
        fprintf(stderr, "ERROR: Failed to get path according to ActObject!\n");
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

        CharArray* margv = ParseString(key, false);
        int i = 0;
        printf("Found entries including (%s):\n", key);
        while (margv->array[i]) {
            ptr = content_list->next;
            while (ptr) {
                CharArray* items = ParseString(ptr->str, false);
                if (strstr(items->array[0], margv->array[i]))
                    printf ("    %s", ptr->str);
                CharArrayDestroy(items);
                ptr = ptr->next;
            }
            i++;
        }
        CharArrayDestroy(margv);
    }
    return 0;
}

/**
 * Delete a record which contains input_str in this Object.
 *
 * @param obj: an ActObject used to identify which file to modify.
 * @param input_str: an input_str to identify which record to be removed.
 *
 * @Note: Multiple ActObjects (separeted by whitespace) can be deleted each time.
 */
int del_obj(ActObject obj, const char *input_str)
{
    int counter = 0, i = 0, ret = 0;
    char * path = GetPathFromType(obj);
    char c;

    if (input_str == NULL || strlen(input_str) == 0) {
        fprintf(stderr, "ERROR: You should provide a entry to delete.\n");
        return -1;
    }

    printf("Deleting Object: %s.\n", obj_desc[obj]);
    if (path == NULL){
        printf ("Failed to get path according to ActObject!\n");
        return -1;
    }
    printf("File: \t%s\nItem: \t%s\n", path, input_str);

    if (read_content(path) == -1) {
        fprintf(stderr, "ERROR: Failed to read content of file: %s!\n",
                path);
        return -1;
    }

    CharArray* ca = ParseString(input_str, false);
    str_list *ptr = NULL;
    while (ca->array[i]) {
        ptr  = content_list->next;
        PDEBUG ("Target: %s\n", ca->array[i]);
        while (ptr) {
            if (strstr(ptr->str, ca->array[i])) {
                printf ("Item: %s", ptr->str);
                ptr->un.flag = True;
                counter++;
            }
            ptr = ptr->next;
        }
        i++;
    }
    CharArrayDestroy(ca);

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
        PDEBUG ("before: %d\n", content_list->un.counter);
        content_list->un.counter -= 1;
        PDEBUG ("after: %d\n", content_list->un.counter);
        ret = dump2file(path);
    }
    else
        printf("No item has beed removed!\n");

    return ret;
}

/*! Creates an empty PkgInfo instance.

  @return PkgInfo*
*/
PkgInfo* PkgInfoCreate()
{
    PkgInfo* pkg = (PkgInfo*)malloc(sizeof(PkgInfo));
    if (pkg)
    {
        memset(pkg, 0, sizeof(PkgInfo));
        PDEBUG("pkg: %p, del_list: %p\n", pkg, pkg->del_list);
    }
    return pkg;
}

/*! Helper function to release resources for PkgInfo.

  @return void
*/
void PkgInfoDestroy(void *data)
{
    if (data)
    {
        PkgInfo* pkg = (PkgInfo*)data;
        if (pkg->fullPath)
        {
            free(pkg->fullPath);
        }
        if (pkg->del_list)
        {
            NameList* ptr = pkg->del_list;
            NameList* tmp;
            while (ptr != NULL)
            {
                tmp = ptr;
                ptr = ptr->next;

                free(tmp->name);
                free(tmp);
            }

            free(pkg->del_list);
        }
    }
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
    PDEBUG("\n\nenter, FilePath: %s\n", fpath);
    if (!fpath || (access(fpath, F_OK) == -1))
    {
        handle_error("File error\n");
    }
    /*
     * Skip reserved files, Reason:
     *		1. They are kept for special reason (samba)
     *		2. They don't take up much disk size.
     */
    if (should_reserve(fpath)) {
        PRINT_VERBOSE("Reserved file: %s\n", fpath);
        return 0;
    }

    struct stat sb;
    int ret = stat(fpath, &sb);
    if (ret == -1)
    {
        printf("Failed to get file status: %s\n", fpath);
        return -1;
    }


    char* cptr = name_split(fpath);
    if (cptr == NULL) {
        PRINT_VERBOSE("Unrecognized file: %s , will keep this package.\n",
                      fpath);
        return 0;
    }

    char* bname = strdup(basename(strdup(cptr))); //XXX: memory leak!

    void* val = GetEntryFromHashTable(SourceTable, bname);
    PDEBUG("val: %p\n", val);
    if (!val) // Pkg not found in DB.
    {
        PDEBUG("bname: %s\n", bname);
        PkgInfo* pkg = PkgInfoCreate();
        if (!pkg)
        {
            printf("No memory!\n");
            return -1;
        }
        pkg->fullPath = fpath;
        pkg->version = GET_TIME(&sb);
        pkg->size = sb.st_size;
        PDEBUG("pkg: %p, del_list: %p\n", pkg, pkg->del_list);
        return InsertEntry(SourceTable, bname, (void*)pkg);
    }

    // Old package found, compare version.
    PkgInfo*  pkg      = (PkgInfo*)val;

    // Create del_list for pkg, this is not initialized when creating PkgInfo, because not
    // all PkgInfo instance need it.

    if (pkg->del_list == NULL)
    {
        pkg->del_list = (NameList*)malloc(sizeof(NameList));
        memset(pkg->del_list, 0, sizeof(NameList));
    }

    NameList* ptr = NULL;
    // do {
    //     ptr = pkg->del_list;
    //     while (ptr)
    //     {
    //         if (ptr->next == NULL)
    //         {
    //             ptr->next = (NameList*)malloc(sizeof(NameList));
    //             memset(ptr->next, 0, sizeof(NameList));
    //             break;
    //         }
    //         else
    //         {
    //             ptr = ptr->next;
    //         }
    //     }
    // } while (0);
    SEEK_LIST_TAIL(pkg->del_list, ptr, NameList); // Seek to tail of list.

    if (pkg->version < GET_TIME(&sb)) // Target is newer.
    {
        ptr->name      = pkg->fullPath;
        freed_size    += pkg->size;
        pkg->fullPath  = fpath;
        pkg->size      = sb.st_size;
    }
    else
    {
        ptr->name = fpath;
        freed_size += sb.st_size;
    }

    deleted ++;

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
    if (doit) /* Real action to delete file!*/
    {
        for (; i < SourceTable->capacity; ++i)
        {
            TableEntry* entry = &SourceTable->entries[i];
            if (entry->key)
            {
                PkgInfo* pkg = (PkgInfo*)entry->val;
                if (pkg)
                {
                    NameList* ptr = pkg->del_list;
                    while (ptr != NULL)
                    {
                        if (ptr->name)
                        {
                            printf("Removing %s ...\n", ptr->name);
                            ret = unlink(ptr->name);
                            if (ret == -1)
                            {
                                printf ("Failed to unlink file: %s, reason: %s\n",
                                        ptr->name, strerror(errno));
                            }
                        }
                        ptr = ptr->next;
                    }
                }
            }
        }
    }
    else
    { /* Not really delete, but display files to be removed. */
        printf ("\nTo be deleted: \n");
        {
            int idx = 1;
            for (; i < SourceTable->capacity; ++i)
            {
                TableEntry* entry = &SourceTable->entries[i];
                if (entry->key)
                {
                    PkgInfo* pkg = (PkgInfo*)entry->val;
                    if (pkg)
                    {
                        if (!pkg->del_list) // no outdated files.
                        {
                            continue;
                        }

                        printf ("\n\t%03d KEEP:  %s\n", idx, pkg->fullPath);
                        NameList* ptr = pkg->del_list;
                        while (ptr != NULL)
                        {
                            if (ptr->name)
                            {
                                printf ("\t    DEL :  %s\n", ptr->name);
                            }
                            ptr = ptr->next;
                        }
                        idx++;
                    }
                }
            }
        }
    }
    return ret;
}

/**
 * Cleans up local cached files.
 *
 * @return: 0 if succeeded, or non-zero otherwise.
 */
int cleanup_localdist_resources(ActObject obj)
{
    int ret = 0;
    char c;
    if (obj == AO_UNKNOWN)
    {
        printf("Cleaning up distfiles.\n");

        if (SourceTable == NULL)
        {
            SourceTable = HashTableCreate(HASH_SIZE, StringHashFunction, PkgInfoDestroy);
            if (!SourceTable)
            {
                printf("Failed to create source table!\n");
                return -1;
            }
        }

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
            printf ("\nGoing to deleted %d files,  %dM diskspace will be freed.\n",
                    deleted,  freed_size/1024/1024);
            printf ("Keep going? [Y]\n");
            c = fgetc(stdin);
            if (c == 'N' || c == 'n')
            {
                printf ("Files are not deleted.\n");
            }
            else
            {
                ret = real_delete(1);
                if (ret)
                {
                    fprintf(stderr, "ERROR: Failed to execute delete command!\n");
                }
                else
                {
                    printf("Finished cleanup local resources.\n");
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
        char *path = GetPathFromType(obj);
        if (path == NULL)
        {
            fprintf(stderr, "ERROR: Failed to convert type to path\n");
            exit(1);
        }
        printf("Cleaning file: %s\n", path);
        if (read_content(path) == -1)
        {
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

int ParseOptions(int argc, char **argv)
{
    int ret = -1;
    if (!opts)
    {
        opts = (KmuOpt*)malloc(sizeof(KmuOpt));
    }
    if (opts)
    {
        memset(opts, 0, sizeof(KmuOpt));
        int  c;
        int err_flag = 0;
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
                    break;
                }
                case 'a': {
                    if (opts->act != AT_UNKNOWN) {
                        printf ("Confilict actions!\n");
                        err_flag = 1;
                    }
                    opts->act = AT_ADD;
                    break;
                }
                case 'c': {
                    if (opts->act != AT_UNKNOWN) {
                        printf ("Confilict actions!\n");
                        err_flag = 1;
                    }
                    opts->act = AT_CLEANUP;
                    break;
                }
                case 'u': {
                    if (opts->obj != AT_UNKNOWN) {
                        printf ("Confilict ActObjects!\n");
                        err_flag = 1;
                    }
                    opts->obj = AO_USE;
                    break;
                }

                case 'U': {
                    if (opts->obj != AT_UNKNOWN) {
                        printf ("Confilict ActObjects!\n");
                        err_flag = 1;
                    }
                    opts->obj = AO_UMASK;
                    break;
                }

                case 'm': {
                    if (opts->obj != AT_UNKNOWN) {
                        printf ("Confilict ActObjects!\n");
                        err_flag = 1;
                    }
                    opts->obj = AO_MASK;
                    break;
                }
                case 'k': {
                    if (opts->obj != AT_UNKNOWN) {
                        printf ("Confilict ActObjects!\n");
                        err_flag = 1;
                    }
                    opts->obj = AO_KEYWORD;
                    break;
                }
                case 'd': {
                    if (opts->act != AT_UNKNOWN) {
                        printf ("Confilict actions!\n");
                        err_flag = 1;
                    }
                    opts->act = AT_DELETE;
                    break;
                }
                case 'l': {
                    if (opts->act != AT_UNKNOWN) {
                        printf ("Confilict actions!\n");
                        err_flag = 1;
                    }
                    opts->act = AT_LIST;
                    break;
                }
                case 'v': {
                    opts->verbose = true;
                    verbose = 1;
                    break;
                }
                case '?':
                    break;

                default:
                    printf("??\n");
                    abort ();
            }
            if (err_flag)
            {
                free(opts);
                opts = NULL;
                break;
            }
        }

        if (!err_flag)
        {
            memset(opts->args, 0, 1024);
            if (optind < argc) {
                strncpy(opts->args, argv[optind], strlen(argv[optind]));
                optind ++;

                while (optind < argc) {
                    strncat(opts->args, " ", 1);
                    strncat(opts->args, argv[optind], strlen(argv[optind]));
		    optind ++;
                }
            }
            ret = 0;
        }
    }

#ifdef DEBUG
    if (opts)
    {
        printf ("opts: %p, act: %d, obj: %d, args: %s\n",
                opts, opts->act, opts->obj, opts->args);
    }
#endif
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
    PDEBUG ("enter\n");
    int  ret = 0;

    if (geteuid() != 0) {
        fprintf(stderr, "Should be executed as root, do not complain if you are not!\n");
    }

    INIT_LIST(content_list, str_list);

    ret = ParseOptions(argc, argv);
    if (ret < 0)
    {
        usage(argv);
        exit(1);
    }

    switch (opts->act) {
    case AT_ADD: {
        ret = add_obj(opts->obj, opts->args);
        break;
    }

    case AT_DELETE: {
        ret = del_obj(opts->obj, opts->args);
        break;
    }
    case AT_LIST: {
        ret = list_obj(opts->obj, opts->args);
        break;
    }
    case AT_CLEANUP:{
        ret = cleanup_localdist_resources(opts->obj);
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
