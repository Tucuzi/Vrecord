#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

#define COMMENT_SIGN '#'
#define EQ_SIGN '='
#define BLANK_SIGN ' '
#define CONFIGLINE_SZ 128

char *find_first_validchar(char *buff)
{
	if (buff == NULL)
		return NULL;

	while (*buff==' ')
		buff++;

	if (*buff == '\n' || *buff == '\0') 
		return NULL;
	else
		return buff;
}

struct parmeter_list *parse_parmeter_from_line(char *buff)
{
	char *dstart, *dend, *dblank;
    struct parmeter_list *context;

    context = (struct parmeter_list *)calloc(1, sizeof(struct parmeter_list));
    /* skip the comment line */
	if ((dstart = find_first_validchar(buff)) != NULL) {
		if (*dstart == COMMENT_SIGN) 
            goto nofound;
	}
    else
        goto nofound;

    /* find the id */
    if ((dend = strchr(dstart, EQ_SIGN)) != NULL) {
        dblank = strchr(dstart, BLANK_SIGN);
        if (dblank == NULL) {
            strncpy(context->id, dstart, dend - dstart);
            context->id[dend - dstart] = '\0';
        }
        else if (dblank < dend) {
            strncpy(context->id, dstart, dblank - dstart);
            context->id[dblank - dstart] = '\0';
		}
        else {
            strncpy(context->id, dstart, dend - dstart);
            context->id[dend - dstart] = '\0';
        }
	}
    else 
        goto nofound;

    /* find the value */
    if ((dstart = find_first_validchar(dend+1)) != NULL) {
		dblank = strchr(dstart, BLANK_SIGN);
        if (dblank!=NULL) {
            strncpy(context->value, dstart, dblank - dstart);
            context->value[dblank - dstart] = '\0';
		}
        else {
            if ((dend = strchr(dstart, '\0')) == NULL) 
                dend = strchr(dstart, '\n');

            strncpy(context->value, dstart, dend - dstart);
            context->value[dend - dstart -1] = '\0';
        }
	}
    else
        context->value[0] = '\0';
    
	return context;

nofound:
    free(context);
    return NULL;
}

struct parmeter_list *parse_config_file(const char * path)
{
    int n = 0;
    FILE *fconfig;
    struct parmeter_list *phead = NULL;
    struct parmeter_list *plist;
    struct parmeter_list *ptmp;
	char line[CONFIGLINE_SZ];

    fconfig = fopen(path, "r");
    if(fconfig == NULL)
        return NULL;

	while(fgets(line, CONFIGLINE_SZ, fconfig) != NULL) {
        ptmp = parse_parmeter_from_line(line);

        if (ptmp != NULL) {
            if (n++ == 0) {
                plist = ptmp;
                phead = plist;
            }
            
            plist->next = ptmp;
            plist = ptmp;
        }
	}

    if (plist != NULL)
	    plist->next = NULL;

	return phead;
}

void destroy_parmeter_list(struct parmeter_list *list)
{
    struct parmeter_list *ptmp;

    while (list != NULL) {
        ptmp = list;
        list = list->next;
        free(ptmp);
    }
}

char * search_parmeter_value(struct parmeter_list *list, const char* id)
{
    struct parmeter_list *plist = list;

    while (plist != NULL) {
        if (!strcmp(plist->id, id)) {
            return plist->value;
        }

        plist = plist->next;
    }

    return NULL;
}


