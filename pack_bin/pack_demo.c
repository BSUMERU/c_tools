#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define SYS_UPGRADE_FILE_MODE_MTD 0
#define SYS_UPGRADE_FILE_MODE_UBI 1
#define SYS_UPGRADE_FILE_MODE_HI3861L 2



#define IMAGE_DEBUG_LOG(fmt,...) printf("D[%s[%s %d]" fmt "\n", __FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define IMAGE_INFO_LOG(fmt,...) printf("I[%s[%s %d]" fmt "\n",__FILE__, __FUNCTION__,__LINE__,##__VA_ARGS__)
#define IMAGE_ERRO_LOG(fmt,...) printf("E[%s[%s %d]" fmt "\n",__FILE__, __FUNCTION__,__LINE__,##__VA_ARGS__)

#define MAX_HEAD_LEN 1024
/* 文件头信息 头部64字节：版本标识（16字节）|版本名（32字节）|版本文件数量（4字节）|版本文件总大小(4字节)|其余待用（8字节）*/
#define IMAGE_HEAD_START_ADDR 0
#define IMAGE_HEAD_FLAG_LEN 16
#define IMAGE_HEAD_FLAG "SUMERU"

#define VERSION_NAME_START_ADDR IMAGE_HEAD_START_ADDR+IMAGE_HEAD_FLAG_LEN
#define VERSION_NAME_LEN 32

#define FILE_NUM_START_ADDR VERSION_NAME_START_ADDR+VERSION_NAME_LEN
#define FILE_NUM_LEN 4

#define ALL_SIZE_START_ADDR FILE_NUM_START_ADDR+FILE_NUM_LEN
#define ALL_SIZE_LEN 4

/* 文件信息  文件名（56字节）|文件大小（4字节）|文件偏移(4字节)|...*/
#define MAX_FILE_NUM    15

#define PART_INFO_START_ADDR 64

#define MAX_FILE_NAME_LEN 56
#define MAX_FILE_SIZE_LEN 4
#define MAX_FILE_SEEK_LEN 4


struct partition_info{
    char name[MAX_FILE_NAME_LEN];
    int part_size;
    int file_seek_addr;
};

struct partition_info_group{
    int part_num;
    int total_size;
    char ver_name[VERSION_NAME_LEN];
    struct partition_info part_info[MAX_FILE_NUM];
};

int set_image_head(char *buff_addr,int *buff_len,struct partition_info_group* info_group)
{

    int part_index = 0;

    int all_load_size = 0;

    char *cp_addr = buff_addr;
    if(info_group->part_num > MAX_FILE_NUM){
        IMAGE_ERRO_LOG("invalid file num:%d",info_group->part_num);
        return -1;
    }

    cp_addr = buff_addr + IMAGE_HEAD_START_ADDR;
    memcpy(cp_addr,IMAGE_HEAD_FLAG,strlen(IMAGE_HEAD_FLAG));

    cp_addr = buff_addr + VERSION_NAME_START_ADDR;
    memcpy(cp_addr,info_group->ver_name,strlen(info_group->ver_name));

    cp_addr = buff_addr + FILE_NUM_START_ADDR;
    memcpy(cp_addr,&info_group->part_num,FILE_NUM_LEN);

    cp_addr = buff_addr + ALL_SIZE_START_ADDR;
    memcpy(cp_addr,&info_group->total_size,ALL_SIZE_LEN);

    cp_addr = buff_addr + PART_INFO_START_ADDR;
    for(part_index = 0; part_index < info_group->part_num; part_index++){
        memcpy(cp_addr,info_group->part_info[part_index].name,strlen(info_group->part_info[part_index].name));
        cp_addr+=MAX_FILE_NAME_LEN;
        memcpy(cp_addr,&info_group->part_info[part_index].part_size,MAX_FILE_SIZE_LEN);
        cp_addr+=MAX_FILE_SIZE_LEN;
        memcpy(cp_addr,&info_group->part_info[part_index].file_seek_addr,MAX_FILE_SEEK_LEN);
        cp_addr+=MAX_FILE_SEEK_LEN;
        IMAGE_INFO_LOG("part %d %d %d-> %s",part_index,info_group->part_info[part_index].part_size,info_group->part_info[part_index].file_seek_addr,info_group->part_info[part_index].name);
    }
    *buff_len = cp_addr-buff_addr;
    return 0;
}


int list_source_file(const char *source_dir,struct partition_info_group* info_group)
{
    DIR *s_dir;
    struct dirent *entry;
    int file_count = 0;
    int file_name_len = 0;

    s_dir = opendir(source_dir);
    if(!s_dir){
        IMAGE_ERRO_LOG("can not open:%s",source_dir);
        return -1;
    }

    while(entry = readdir(s_dir)){
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            IMAGE_INFO_LOG("find file:%s",entry->d_name);
            file_name_len = strlen(entry->d_name);
            if(file_name_len >= MAX_FILE_NAME_LEN){
                closedir(s_dir);
                IMAGE_ERRO_LOG("file name to long:%d %s",file_name_len,entry->d_name);
                return -2;
            }
            memcpy(&info_group->part_info[file_count++].name,entry->d_name,file_name_len);
        }
    }

    
    info_group->part_num = file_count;
    IMAGE_INFO_LOG("totol file count:%d",info_group->part_num);
    closedir(s_dir);
    return 0;
}


int main(int argc, char *argv[])
{
    int ret;
    int file_index = 0;
    
    size_t source_size;
    

    FILE *firmware_bin = NULL;
    FILE *file_current;

    int image_head_len;
    char image_head_buff[MAX_HEAD_LEN] = {0};

    char version_name[VERSION_NAME_LEN] = {0};
    char image_bin_name[64] = {0};

    char source_dir_path[256] = {0};
    char current_file_path[256] = {0};

    char copy_buff[1024] = {0};
    size_t source_size_current;

    struct partition_info_group pinfo_group;
    memset (&pinfo_group,0,sizeof(pinfo_group));

    if((!argv[1]) || (strlen(argv[1]) > VERSION_NAME_LEN)){
        IMAGE_ERRO_LOG("invaild input!! not input or version_name too long");
        return -1;
    }

    /* image包名和版本号 */
    sprintf(pinfo_group.ver_name,"%s",argv[1]);
    sprintf(image_bin_name,"%s.bin",pinfo_group.ver_name);
    IMAGE_INFO_LOG("input version:%s  will got:%s",pinfo_group.ver_name,image_bin_name);

    if((!argv[2]) || (strlen(argv[2]) > 256)){
        IMAGE_ERRO_LOG("invaild input!! not input or source_dir_path too long[%s]",argv[2]);
        return -2;
    }

    memcpy(source_dir_path,argv[2],strlen(argv[2]));
    IMAGE_INFO_LOG("input source path:%s ",source_dir_path);

    ret = list_source_file(source_dir_path,&pinfo_group);
    if(ret){
        IMAGE_ERRO_LOG("list_source_file fail:%d",ret);
        return -3;
    }

    IMAGE_DEBUG_LOG("start open file:%s",image_bin_name);
    firmware_bin = fopen(image_bin_name, "wb");
    if(!firmware_bin){
        IMAGE_ERRO_LOG("open firmware_bin fail");
        return -4;
    }

    fseek(firmware_bin,MAX_HEAD_LEN,SEEK_SET);
    pinfo_group.total_size = 0;
    /* 拷贝文件 */
    for(file_index; file_index < pinfo_group.part_num; file_index++){
        source_size = 0;
        file_current = NULL;
        
        sprintf(current_file_path,"%s/%s",source_dir_path,pinfo_group.part_info[file_index].name);
        IMAGE_INFO_LOG("start copy %s",current_file_path);
        
        file_current = fopen(current_file_path, "rb");
        if(!file_current){
            IMAGE_ERRO_LOG("can not open %s",current_file_path);
            return -5;
        }

        pinfo_group.part_info[file_index].file_seek_addr = pinfo_group.total_size;

        while ((source_size_current = fread(copy_buff, 1, sizeof(copy_buff), file_current)) > 0) {
            fwrite(copy_buff, 1, source_size_current, firmware_bin);
            source_size += source_size_current;
        }

        fclose(file_current);

        pinfo_group.part_info[file_index].part_size = source_size;
        pinfo_group.total_size+=source_size;
        IMAGE_INFO_LOG("end copy %s size:%d",pinfo_group.part_info[file_index].name,pinfo_group.part_info[file_index].part_size);
    }

    
    ret = set_image_head(image_head_buff,&image_head_len,&pinfo_group);
    if(ret){
        IMAGE_ERRO_LOG("set_image_head fail:%d",ret);
        return -5;
    }

    fseek(firmware_bin,0,SEEK_SET);
    fwrite(image_head_buff, 1,image_head_len, firmware_bin);
    fclose(firmware_bin);
    return 0;
}

