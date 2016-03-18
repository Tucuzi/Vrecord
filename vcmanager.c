#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>

#include <sys/vfs.h> 
#include <sys/types.h>
#include <sys/wait.h>


#include "vrecord.h"
#include "parse.h"

#define CMD "/media/mmcblk2p1/vrecord"
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

int remove_oldvideo(char *path)
{
    struct dirent **namelist;
    char *path;
    int n;
    int i;

    n = scandir(path, &namelist, customFilter, alphasort);
    if (n < 0) {
        perror("scandir");
    }
    else {
        for (i = 0; i < n; i++) {
            printf("%s\n", namelist[i]->d_name);
            free(namelist[i]);
        }

        free(namelist);
    }
}

void * monitor_thread(void *ptr)
{
    char video_dir[256];
    int ret;
    int sleep_duration;
    float freedisk_rate;
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
        //info_msg("Thread wake, check the disk\n");
        ret = statfs(config->path, &diskinfo);
        if (!ret) {
            blocksize = diskinfo.f_bsize;
            totalsize = blocksize * diskinfo.f_blocks;
#if 1
            info_msg("Total_size = %llu B = %llu KB = %llu MB = %llu GB\n",   
                totalsize, totalsize>>10, totalsize>>20, totalsize>>30);
#endif
            freedisk = diskinfo.f_bfree * blocksize;
            availabledisk = diskinfo.f_bavail * blocksize;
#if 1
            info_msg("Disk_free = %llu MB = %llu GB\nDisk_available = %llu MB = %llu GB\n"
               , freedisk>>20, freedisk>>30, availabledisk>>20, availabledisk>>30);  
#endif

            freedisk_rate = availabledisk / totalsize;
            info_msg("The free disk rate is %f\n", freedisk_rate);
            for(i=0; i<config->channel; i++) {
                sprintf(video_dir,"%s/%s%d", config->path, SUBDIR, i);
                remove_oldvideo(video_dir);
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
    pthread_attr_t pattr;
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
    pthread_create(&tpid, NULL, monitor_thread, (void*)&config);

    for(i=0;i<channel;i++) {
        cpid = waitpid(-1, NULL, 0);
        info_msg("Video record process %d exit!\n", cpid);
    }

    sync();
    threadRun = 0;
    return 0;
}

