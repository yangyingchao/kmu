/***************************************************************************
 *  Copyright (C) 2010-2011 yangyingchao@gmail.com

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
#include <error.h>
#include <errno.h>
#include <sys/stat.h>
#include <ftw.h>
#include "util.h"

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

static struct list_head source_list;
static struct list_head del_list;
static struct list_head content_list;

static type2path path_base[] = {
    { KEYWORD, 	"/etc/portage/package.keywords/keywords"},
    { MASK, 	"/etc/portage/package.mask/mask"},
    { USE, 		"/etc/portage/package.use/use" },
    { UMASK,    "/etc/portage/package.unmask/unmask"},
    { 0, 		NULL},
};

static char * obj_desc[] = {
    "Ubknown object",
    "Keyword",
    "Mask",
    "USE",
    "Unmask"
};

static verbose = 0;

#define PRINT_VERBOSE(format, args...)                            \
    if (verbose)\
    printf(format, ##args);

/**
 * Get corresponding path based on type of action.
 *
 * @param obj
 *
 * @return
 */

char * get_path(object obj)
{
    int idx = 0;
    int N   =  sizeof(path_base)/sizeof(type2path);
    for (idx = 0; idx < N; idx++) {
        if (path_base[idx].obj == obj) {
            return path_base[idx].path;
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
    printf ("Usage: %s -a|d|l|h -k|m|u|U [package_string]\n", argv[0]);
    printf ("****** Objects: ********\n");
    printf ("-k, --keyword: Accept a new keyword specified by package_string\n");
    printf ("-m, --mask: 	mask a new keyword specified by package_string\n");
    printf ("-u, --use: 	Modify or add new use to package_string\n");
    printf ("-U, --Umask: 	Unmask a package\n");
    printf ("****** Operations: ********\n");
    printf ("-a, --add: 	Add a object.\n");
    printf ("-d, --delete: 	Delete a object.\n");
    printf ("-l, --list: 	List a object.\n");
    printf ("-h, --help: 	Print this message.\n");
}

static int
cmpstringgp(const void *p1, const void *p2)
{
    char *pp1 = (char *)p1;
    char *pp2 = (char *)p2;
    while ( *pp1 == '<' || *pp1 == '=' || *pp1 == '>') {
        pp1 ++;
    }
    while ( *pp2 == '<' || *pp2 == '=' || *pp2 == '>') {
        pp2 ++;
    }
    return (0 - strcmp(* (char * const *) pp1, * (char * const *) pp2));
}


/**
 * list_to_array() - Change list into a sortted array.
 *
 * Return: char**
 */
char **list_to_array()
{
    PDEBUG ("called.\n");

    struct list_head *ptr = NULL;
    str_list *p = NULL;

    int size = content_list.counter;
    if (size <= 0) {
        return NULL;
    }
    char **array = calloc(size, sizeof(char *));
    if (array != NULL) {
        int i = 0;
        char tmp[256];
        list_for_each(ptr, &content_list){
            memset(tmp, 0, 256);
            p = list_entry(ptr, str_list, head);
            if (p->flag == True || strlen(p->str) == 1)
                continue;
            else {
                strncpy(tmp, p->str, strlen(p->str));
                strcat(tmp, "\n");
                PDEBUG ("Item: %s\n", tmp);
                array[i] = strndup(tmp, strlen(tmp));
            }
            i++;
        }
        qsort((void *)array, size, sizeof(char *), cmpstringgp);
    }
    return array;
}

/**
 * dump2file - Write content stored in content_list into a file.
 * @path - Character path, in which the contents will be writen into.
 *
 * Return: int
 */
int dump2file(const char *path)
{
    str_list *p = NULL;
    int ret = 0, writen, fd = 0;
    struct list_head *ptr = NULL;

    if (path == NULL) {
        fprintf(stderr, "ERROR: Empty path! \n");
        return -1;
    }

    char tmpl[256] = {'0'};
    sprintf(tmpl, "%s-XXXXXX", path);
    fd = mkstemp(tmpl);
    if (fd == -1)
        oops("Failed to mktemp");

    char **array = list_to_array();
    if (array == NULL) {
        fprintf(stderr, "ERROR: failed to convert list into string"
                "Items will be recorded without order.");
        list_for_each(ptr, &content_list){
            p = list_entry(ptr, str_list, head);
            if (p->flag == True || strlen(p->str) == 1)
                continue;
            else {
                writen = write(fd, p->str, strlen(p->str));
                if (writen == -1)
                    oops("Failed to write:");
                writen = write(fd, "\n", 1);
            }
        }
    }
    else {
        int i;
        char *str;
        for (i = 0; i < content_list.counter; i++) {
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
    }
    return 0;
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
        if (strlen(item) == 1) 			/* Nothing but a newline, skip it. */
            continue;

        p = (str_list *) malloc(sizeof(str_list));
        memset(p, 0, sizeof(str_list));

        tmp = (char *)calloc(strlen(item), 1);
        strncpy(tmp, item, strlen(item)-1);
        p->str = tmp;
        list_add(&p->head, &content_list);
        content_list.counter ++;
    }
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
    str_list *p = NULL;
    struct list_head *ptr = NULL;

    list_for_each(ptr, &content_list){
        p = list_entry(ptr, str_list, head);
        if (strstr(p->str, key)) {
            printf ("Found entry: %s\n", p->str);
            return p;
        }
    }
    return NULL;
}

/**
 * merge_use() - Merge new use into old ptr.
 * @p: ptr where old USE was stored
 * @new: New USE to be merged.
 *
 * Return: int
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

    new_array = (char **)calloc(size, sizeof(char *));

    i = 0;
    while (item_old[i]) {
        new_array[i] = item_old[i];
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
        PDEBUG ("J = %d\n", j);
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
    p->str = strdup(new_item);
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
        list_add(&p->head, &content_list);
        content_list.counter ++;
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
    str_list *p = NULL;
    struct list_head *ptr = NULL;

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

    printf("Total entries: %d\n", content_list.counter);

    if (strlen(key) == 0) {
        /* All items will be displayed. */
        printf("Display all items:\n");
        list_for_each(ptr, &content_list){
            p = list_entry(ptr, str_list, head);
            printf ("    %s\n", p->str);
        }
    }
    else {
        char **margv = strsplit(key);
        char *item = NULL;
        int i = 0;
        printf("Found entries including (%s):\n", key);
        list_for_each(ptr, &content_list) {
            i = 0;
            p = list_entry(ptr, str_list, head);
            while (margv[i]) {
                if (strstr(strsplit(p->str)[0], margv[i]))
                    printf ("    %s\n", p->str);
                i++;
            }
        }
        free_array(margv);
    }
    return 0;
}

/**
 * Delete a record which contains keyword in this Object.
 *
 * @param obj: an object used to identify which file to modify.
 * @param keyword: an keyword to identify which record to be removed.
 *
 * @Note: Multiple objects (separeted by whitespace) can be deleted each time.
 */
int del_obj(object obj, const char *keyword)
{
    str_list *p = NULL;
    struct list_head *ptr = NULL;
    int counter = 0, i = 0, ret = 0;
    char * path = get_path(obj);
    char c;

    printf("Deleting entry for: %s.\n", obj_desc[obj]);
    if (path == NULL){
        printf ("Failed to get path according to object!\n");
        return -1;
    }
    printf("File: \t%s\nItem: \t%s\n", path, keyword);

    if (read_content(path) == -1) {
        fprintf(stderr, "ERROR: Failed to read content of file: %s!\n",
                path);
        return -1;
    }

    char **margv = strsplit(keyword);

    while (margv[i]) {
        list_for_each(ptr, &content_list){
            p = list_entry(ptr, str_list, head);
            if (strstr(p->str, margv[i])) {
                printf ("Item: %s", p->str);
                p->flag = True;
                content_list.counter --;
                counter++;
            }
        }
        i++;
    }
    free_array(margv);

    printf ("\nI found %d item(s).\n", counter);

    if (counter > 1) {
        printf ("Muliple items will be removed, as bellow:\n");
        list_for_each(ptr, &content_list){
            p = list_entry(ptr, str_list, head);
            if (p->flag == True)
                printf ("\t%s\n", p->str);
        }
        printf ("Are you sure to do this?(Y or N)[Y]\n");
        c = fgetc(stdin);
        if ((c == 'Y') || (c == 'y')) {
            ret= dump2file(path);
        }
    }
    else if (counter == 1) {
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
int process_file(const char *fpath, const struct stat *sb, int typeflag)
{
    char *ptr, *bname;
    name_version     *p   = NULL;
    struct list_head *pptr = NULL;
    int found = 0, ret = 0;
    char *to_delete = NULL;

    if (typeflag == FTW_F) {
        /*
         * Skip reserved files, Reason:
         *		1. They are kept for special reason (samba)
         *		2. They don't take up much disk size.
         */
        if (should_reserve(fpath)) {
            PRINT_VERBOSE ("Reserved file: %s\n", fpath);
            return 0;
        }

        ptr = name_split(fpath);
        if (ptr == NULL){
            PRINT_DEBUG ("Unrecognized file: %s , will keep this package.",
                         fpath);
            return 0;
        }

        bname = (char *)basename(strdup(ptr));
        list_for_each(pptr, &source_list){
            p = list_entry(pptr, name_version, head);
            if (strcmp(name_split((char *)basename(strdup(p->name))),
                       bname) == 0) {
                found = 1;
                deleted ++;
                if (p->version < sb->st_mtime) {
                    to_delete = strdup(p->name);
                    /* Update statistics */
                    freed_size += p->size;
                    /* Update source info. */
                    p->name = strdup(fpath);
                    p->version = sb->st_mtime;
                }
                else
                    to_delete = strdup(fpath);
                    freed_size += sb->st_size;
                break;
            }
        }

    do_delte:
        if (found == 0) { /* Add file into source_list. */
            p          = calloc(sizeof(name_version), 1);
            p->name    = strdup(fpath);
            p->version = sb->st_mtime;
            p->size    = sb->st_size;
            list_add(&p->head, &source_list);
        }
        else {  /* Add file into del_list if found was set. */
            p          = calloc(sizeof(name_version), 1);
            p->name    = strdup(to_delete);
            list_add(&p->head, &del_list);
        }
    }
    return 0;
}

/**
 * real_delete - Delete or display files to be removed.
 * @doit - delete or just display.
 *
 * Return: int
 */
int real_delete(int doit)
{
    int ret = 0;
    struct list_head *pptr = NULL;
    name_version     *p   = NULL;
    if (doit) { /* Real action to delete file!*/
        list_for_each(pptr, &del_list){
            p = list_entry(pptr, name_version, head);
            ret = unlink(p->name);
            if (ret == -1)
                oops ("Failed to unlink file: %s\n", p->name);
        }
    }
    else { /* Not really delete, but display files to be removed. */
        printf ("To be deleted: \n");
        list_for_each(pptr, &del_list){
            p = list_entry(pptr, name_version, head);
            printf ("\t %s\n", p->name);
        }
    }
    return ret;
}

int cleanup_localdist_resources()
{
    int ret;
    char c;
    INIT_LIST_HEAD(&source_list);
    INIT_LIST_HEAD(&del_list);

    printf ("Scanning local resources...\n");
    ret = ftw(dist_path, process_file, 0);
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
 out:
    return ret;
}

int main(int argc, char **argv)
{
    int c;
    act_type type = UNKNOWN;
    object obj = UNKNOWN;
    int ret=0;
    char items[1024];
    int err_flag = 0;

    INIT_LIST_HEAD(&content_list);

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
        ret = cleanup_localdist_resources();
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
    return ret;
}
