#include "util.h"
#include <sys/stat.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

const char *dist_path = "/usr/portage/distfiles/";

static char name_tmp[256];

/* All samba packages will be left. */
const char *reserved[] = {
    // "samba",
    "/linux-",
    "patches",
    // "/firefox",
    NULL
};

static const char* FMT1 = "%s";
static const char* FMT2 = "+%s";

/**
 * ParseString - Split a string into an array, splitted by whitspace.
 * @str - Character string
 * @flag - flag to indicate whether a prefix is needed.
 * Return: char**
 *
 * NOTE: The leading character will be stripped if its is one of the
 * following:
 * ['=', '<', '>']
 */
CharArray* ParseString(const char *str, bool flag)
{
    int i=0, num=0;

    char* tmp_str = (char*)malloc(strlen(str)+1);
    memset(tmp_str, 0, strlen(str)+1);
    strcpy(tmp_str, str);
    char* p   = tmp_str;
    char* ptr = NULL;
    /* Compute total number of white spaces */
    while ((ptr = strchr(p, ' ')) != NULL) {
        num++;
        p = ptr + 1;
    }

    CharArray* ca = CharArrayCreate(num+2);
    if (!ca)
    {
        free(tmp_str);
        return NULL;
    }

    /* Strip some leading characters */
    p = tmp_str;
    if (*p == '=' || *p == '<' || *p == '>' || *p == ' ')
    {
        p++;
    }

    char** array = ca->array;
    const char* fmt;
    size_t length = 0;
    /* Copy components of string into array. */
    while ((ptr = strchr(p, ' ')) != NULL) {
        *ptr = '\0';
        if (strlen(p))
        {
            length = strlen(p) + 1;
            array[i] = (char*)malloc(length);
            memset(array[i], 0, length);
            if (i == 0 || !isalpha(*p) || !flag)
            {
                fmt = FMT1;
            }
            else
            {
                fmt = FMT2;
            }
            sprintf(array[i], fmt, p);
            i++;
        }
        p = ptr + 1;
    }

    /* XXX: Append the last component if it is not strange character.*/
    length = strlen(p) + 1;
    if (length > 2 && (*p >= '0' || *p == '-' || *p == '+'))
    {
        while ((ptr = strchr(p, '\n')) != NULL)
        {
            *ptr = '\0';
        }

        array[i] = (char*)malloc(length);
        memset(array[i], 0, length);
        if (!isalpha(*p) || !flag)
        {
            fmt = FMT1;
        }
        else
        {
            fmt = FMT2;
        }
        sprintf(array[i++], fmt, p);
        PDEBUG ("Appended: %s\n",array[i]);
    }
    array[i] = 0;
    return ca;
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

    // Name-Version
    for (ptr = name_tmp; *ptr; ptr++) {
        if (*ptr == '-' && *(ptr+1) >= '0' && *(ptr+1) <= '9') {
            *ptr = '\0';
            return strdup(name_tmp);
        }
    }

    // Name_Version
    for (ptr = name_tmp; *ptr; ptr++) {
        if ((*ptr == '-' || *ptr == '_') && *(ptr+1) >= '0' && *(ptr+1) <= '9') {
            *ptr = '\0';
            return strdup(name_tmp);
        }
    }

    return NULL;
}

CharArray* CharArrayCreate(int size)
{
    CharArray* ca = malloc(sizeof(CharArray));
    if (ca)
    {
        memset(ca, 0, sizeof(*ca));
        ca->size = size;
        /* Alloc memory for new array */
        ca->array = (char **)calloc(size, sizeof(char *));
        if (!ca->array)
        {
            free(ca);
            ca = NULL;
        }
    }
    return ca;
}

/**
 * CharArrayCreate - Free allocated memory.
 * @array - Character array
 *
 * Return: void
 */
void CharArrayDestroy(CharArray* ca)
{
    if (ca)
    {
        if (ca->array)
        {
            int i = 0;
            while (ca->array[i]) {
                free(ca->array[i]);
                i++;
            }
            free(ca->array[i]);
            free(ca->array);
        }
        free(ca);
        ca = NULL;
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


int dir_exist(const char *path)
{
        if (!path)
                return -1;

        int ret;
        ret = access(path, F_OK);
        if (ret < 0) 
                return -1;
        struct stat sb;
        if ((ret = stat(path, &sb)) == 0) {
                if (S_ISDIR(sb.st_mode)) 
                        return 0;
        }
        return -1;
}


int file_exist(const char *path)
{
        if (!path)
                return -1;

        int ret;
        ret = access(path, F_OK);
        if (ret < 0) 
                return -1;
        struct stat sb;
        if ((ret = stat(path, &sb)) == 0) {
                if (S_ISREG(sb.st_mode)) 
                        return 0;
        }
        return -1;
}


// Hash table operations.

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
                if (entry->val && table->deFunctor)
                {
                    table->deFunctor(entry->val);
                }
            }
            free(table->entries);
        }
        free(table);
    }
}

HashTable* HashTableCreate(uint32 hashSize, HashFunction cFunctor, DestroyFunction dFunctor)
{
    HashTable* table = malloc(sizeof(HashTable));
    if (table)
    {
        memset(table, 0, sizeof(HashTable));

        table->capacity    = hashSize;
        table->entries     = malloc(sizeof(TableEntry) * hashSize);
        table->hashFunctor = cFunctor;
        table->deFunctor   = dFunctor;

        if (table->entries)
        {
            memset(table->entries, 0, sizeof(TableEntry) * hashSize);
        }
        else
        {
            HashTableDestroy(table);
            table = NULL;
        }
    }
    return table;
}

int InsertEntry(HashTable* table, char* key, void* val)
{
    int ret = 0;
    if (!table || !key || !val )
    {
        return ret;
    }

    PDEBUG ("KEY: %s, val: %p\n", key, val);

    uint32 index = table->hashFunctor(key);
    // Insert entry into the first open slot starting from index.
    uint32 i;
    for (i = index; i < table->capacity; ++i)
    {
        TableEntry* entry = &table->entries[i];
        if (entry->key == NULL)
        {
            ret        = 1;
            entry->key = key;
            entry->val = val;
            break;
        }
    }
    return ret;
}

/*! Looks for the given data based on key.

  @return void*
*/
void* GetEntryFromHashTable(HashTable* table, char* key)
{
    TableEntry* entry = NULL;
    uint32 index = table->hashFunctor(key);
    int i;
    for (i = index; i < table->capacity; ++i)
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
    if (entry)
    {
        PDEBUG("Key: %s - %s, val: %p\n",
               key, entry->key, entry->val);
    }
    return entry->val;
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
