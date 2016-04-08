#ifndef __PARSE_H__
#define __PARSE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STRING_LEN 64
struct parmeter_list
{
    char id[MAX_STRING_LEN];
    char value[MAX_STRING_LEN];
    struct parmeter_list *next;
};

char * search_parmeter_value(struct parmeter_list *list, const char* id);
struct parmeter_list *parse_config_file(const char * path);
void destroy_parmeter_list(struct parmeter_list *list);

#ifdef __cplusplus
}
#endif

#endif

