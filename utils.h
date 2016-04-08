#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

int fwriten(int fd, void *vptr, size_t n);
int freadn(int fd, void *vptr, size_t n);
int vpu_read(struct vc_config *config, char *buf, int n);
int vpu_write(struct vc_config *config, char *buf, int n);
void get_arg(char *buf, int *argc, char *argv[]);
int open_files(struct vc_config *config);
void close_files(struct vc_config *config);
int check_params(struct vc_config *config, int op);
char*skip_unwanted(char *ptr);
int parse_options(char *buf, struct vc_config *config, int *mode);
int check_and_make_workdir(char * workdir);
int check_and_make_subdir(char * workdir, const char *subdir_prefix, int channel);
char *get_the_filename(char *parent_dir);

#if __cplusplus
}
#endif

#endif
