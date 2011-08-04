#include "util.h"
#include <sys/stat.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

const char *dist_path = "/usr/portage/distfiles/";

static char name_tmp[256];

/* All samba packages will be left. */
const char *reserved[] = {
    "samba",
    "patch",
    "linux",
    "firefox",
    0
};

/**
 * strsplit - Split a string into an array, splitted by whitspace.
 * @str - Character string
 *
 * Return: char**
 *
 * NOTE: The leading character will be stripped if its is one of the
 * following:
 * ['=', '<', '>']
 */
char **strsplit(const char *str)
{
    int i=0, num=0;
    char **array = NULL;
    char *ptr = NULL, *p = NULL;
    char tmp_str[strlen(str)+1];

    memset(tmp_str, 0, strlen(str)+1);

    strcpy(tmp_str, str);
    p = tmp_str;
    /* Compute total number of white spaces */
    while ((ptr = strchr(p, ' ')) != NULL) {
        num++;
        p = ptr + 1;
    }

    /* Alloc memory for new array */
    array = (char **)calloc(num+2, sizeof(char *));

    /* Strip some leading characters */
    p = tmp_str;
    if (*p == '=' || *p == '<' || *p == '>') {
        p++;
    }
    /* Copy components of string into array. */
    while ((ptr = strchr(p, ' ')) != NULL) {
        *ptr = '\0';
        if (strlen(p)) {
            array[i] = strdup(p);
            i++;
        }
        p = ptr + 1;
    }

    /* XXX: Append the last component if it is not obscurer character.*/
    if ((strlen(p) > 1) || (*p >= '0')) {
        while ((ptr = strchr(p, '\n')) != NULL) {
            *ptr = '\0';
        }
        array[i++]=strdup(p);
    }
    array[i] = 0;
    return array;
}

/**
 * should_reserve - To judge whether this entry should be reserved or not.
 * @key - Character key
 *
 * Return: int
 */
int should_reserve(const char *key)
{
    int i = 0;
    while (reserved[i]) {
        if (strstr(key, reserved[i])) {
            return True;
        }
        i++;
    }
    return False;
}
/**
 * name_split - Abstract filename from file-version format.
 * @fullname - Full name to be splitted.
 *
 * Return: char*
 */
char * name_split(const char *fullname)
{
    char *ptr = name_tmp;

    memset(name_tmp, 0, 256);
    if (strncpy(name_tmp, fullname, strlen(fullname)) == NULL){
        fprintf(stderr, "ERROR: Failed to copy string\n");
        return NULL;
    }

    for (ptr = name_tmp; *ptr; ptr++) {
        if (*ptr == '-' && *(ptr+1) >= '0' && *(ptr+1) <= '9') {
            *ptr = '\0';
            return strdup(name_tmp);
        }
    }
    return NULL;
}

/**
 * free_array - Free allocated memory.
 * @array - Character array
 *
 * Return: void
 */
void free_array(char **array)
{
    if (array != NULL) {
        int i = 0;
        while (array[i]) {
            free(array[i]);
            i++;
        }
        free(array[i]);
    }
}


void list_add(str_list *root, void *new)
{
    str_list *ptr = root;
    while (ptr->next != NULL) {
        ptr = ptr->next;
    }
    ptr->next = new;
}

