#include <stdio.h>
#include <unistd.h>

#include "parse.h"

int main(int argc, char *argv[])
{
    struct parmeter_list *plist, *phead;
    char *id = "love";
    char *value;

    plist = parse_config_file("test.conf");
    phead = plist;
    
    while (plist != NULL) {
        printf("%s : %s\n", plist->id, plist->value);
        plist = plist->next;
    }

    value = search_parmeter_value(phead, id);
    if (value)
        printf("%s:%s\n", id, value);
    else
        printf("Can't find the id %s\n", id);
    
    destroy_parmeter_list(phead);

    return 0;
}

