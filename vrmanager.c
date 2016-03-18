#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>

#include <sys/vfs.h> 
#include <sys/types.h>
#include <sys/wait.h>

#include "vrecord.h"
#include "parse.h"

#define CMD "/usr/bin/vrecord"
int vrecord_dbg_level = 0;
int threadRun;

struct vrecord_config
{
    int channel;
    int duration;
    int num;
    char *path;
};

int customFilter(const struct dirent *pDir)
{
#if 0
    if (strncmp("test", pDir->d_name, 4) == 0 
            && pDir->d_type & 0x04 
            && strcmp(pDir->d_name, ".") 
            && strcmp(pDir->d_name, ".."))
    {
        return 1;
    }
    return 0;
#else
    if (strcmp(pDir->d_name, ".") && strcmp(pDir->d_name, ".."))
        return 1;
    else
        return 0;
#endif
}

void remove_oldvideo(char *path)
{
    struct dirent **namelist;
    char * pwd;
    int n;
    int dnum;
    int i;

    pwd = get_current_dir_name();
    n = scandir(path, &namelist, customFilter, alphasort);
    if (n < 0)
        perror("scandir");
    else
    {
        chdir(path);
        info_msg("%s saved %d video files.\n",path,n);
        dnum = n>=5?4:0;
        info_msg("==== delete below files ====\n");

        /* delete the oldest video files */
        for (i = 0; i < dnum; i++) {
            printf("%s\n", namelist[i]->d_name);
            unlink(namelist[i]->d_name);
        }

        for (i = 0; i < n; i++)
            free(namelist[i]);

        free(namelist);
    }

    chdir(pwd);
}


void monitor_thread(void *ptr)
{
    char video_dir[256];
    float freedisk_rate;
    int i;
    int ret;
    int sleep_duration;
    unsigned long long blocksize;
    unsigned long long totalsize;
    unsigned long long freedisk;
    unsigned long long availabledisk;
    struct statfs diskinfo;
    struct vrecord_config *config = (struct vrecord_config *)ptr;

    if (config->num > 1 || config->num < 0)
        sleep_duration = config->duration;
    else
        sleep_duration = config->duration/2;

    chdir(config->path);
    while(threadRun) {
        sleep(sleep_duration);
        ret = statfs(config->path, &diskinfo);
        if (!ret) {
            blocksize = diskinfo.f_bsize;
            totalsize = blocksize * diskinfo.f_blocks;

            info_msg("Total_size = %llu B = %llu KB = %llu MB = %llu GB\n",   
                totalsize, totalsize>>10, totalsize>>20, totalsize>>30);

            totalsize >>=20;
            freedisk = diskinfo.f_bfree * blocksize;
            availabledisk = diskinfo.f_bavail * blocksize;
            info_msg("Disk_free = %llu MB = %llu GB\nDisk_available = %llu MB = %llu GB\n"
               , freedisk>>20, freedisk>>30, availabledisk>>20, availabledisk>>30);  

            availabledisk >>= 20;
            freedisk_rate = (float)availabledisk / (float)totalsize;
            info_msg("The free disk rate is %f\n", freedisk_rate);
            
            //If the free disk is lower than 15%, remove the older files
            if (freedisk_rate < 0.15) {
                for(i=0; i<config->channel; i++) {
                    sprintf(video_dir,"%s/%s%d", config->path, SUBDIR, i);
                    remove_oldvideo(video_dir);
                }
            }
        }

        sync();
    }
}

int main(int argc, char* argv[])
{
    struct parmeter_list *list;
    struct vrecord_config config;
    int channel;
    int vduration;
    int vnum;
    int i;
    int ret;
    pid_t *ppid;
    pid_t cpid;
    pthread_t tpid;
    //pthread_attr_t pattr;
    char args[20];
    char* path;

    list = parse_config_file(CONFIGFILE_PATH);
    if (list == NULL){
        err_msg("Please check the config file %s\n", CONFIGFILE_PATH);
        return -1;
    }

    channel = atoi(search_parmeter_value(list, DEVICE_CHANNEL));
    printf("channel number : %d\n", channel);
    path = search_parmeter_value(list, SAVE_PATH);
    printf("video saved at %s\n", path);
    vduration = atoi(search_parmeter_value(list, VIDEO_DURATION));
    vnum = atoi(search_parmeter_value(list, VIDEO_NUM));

    config.channel = channel;
    config.path = path;
    config.duration = vduration;
    config.num = vnum;

    /* check the work directory */
    ret = check_and_make_workdir(path);
    if (ret)
        return ret;

    ppid = (pid_t*)calloc(channel, sizeof(pid_t));
    for(i=0;i<channel;i++) {
        cpid = fork();

        if (cpid == 0) {
            sprintf(args, "%d", i);
            execl(CMD, CMD, args, NULL);
        }
        else
            ppid[i] = cpid;
    }

    threadRun = 1;
    //pthread_attr_init(&pattr);  
    //pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE); 
    pthread_create(&tpid, NULL, monitor_thread, &config);

    for(i=0;i<channel;i++) {
        cpid = waitpid(-1, NULL, 0);
        info_msg("child process %d exit!\n", cpid);
    }

    sync();
    threadRun = 0;
    return 0;
}

