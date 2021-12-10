#include "logger.h"
#include <stdio.h>
#include <string>
#include <time.h>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>

static std::string cachePath;
static CacheMMap current_mapping;

void init_file(const std::string& cachedir) {
    //
    cachePath =  cachedir;
}

void show_cache(){
    LOGD(" the path is : %s",cachePath.c_str());
}

void create_mapping(std::string log){
    LOGD(" mapping log is  : %s",log.c_str());
    create_mapping_file();
    std::string path = current_mapping.getMappingPath();
    if(path.empty()){
        return;
    }
    int fd = open(path.c_str(), O_RDWR, 00777);
    lseek(fd,MMAP_SIZE-1,SEEK_END);
    write(fd, "", 1);
    char* addr = (char*)mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
    close(fd);
    memcpy(addr, log.c_str(), log.length());
    current_mapping.setAddr(addr);
    current_mapping.setCurrent(log.length());
}

std::string create_mapping_file(){
    //在mmap缓存文件夹中，映射的文件是yyyymmdd.mmap0、yyyymmdd.mmap1格式存在的
    //映射的文件已经存在，但是映射的内容暂时不超过10M，每一个映射文件超过10M新建
    std::string path = get_cache_file(0);
    //设置该文件为当前mapping文件
    current_mapping.setIsMapping(true);
    current_mapping.setMappingPath(path);
    return path;
}

std::string get_cache_file(int index){
    std::string cache_file_name;
    std::stringstream ss;
    std::string extra = "_";
    std::string file_extra = ".mmap";
    std::string sp = "/";
    ss << cachePath<< sp << getToday() << extra << index << file_extra;
    LOGD(" one file is :%s ",ss.str().c_str());
    std::string path = ss.str();
    FILE *fp;
    if(access(ss.str().c_str(),0) == -1){ //文件不存在
        //新建文件，将文件地址返回
        LOGD(" new file");
        fp = fopen(ss.str().c_str(),"at+");
        fclose(fp);
    } else { //存在该文件，获取文件大小
        //文件大小超过9M，新建文件
        //返回文件地址
         fp = fopen(ss.str().c_str(),"at+");
         fseek(fp,0L,SEEK_END);
         int size=ftell(fp);
         if(size > 2* 1024* 1024){
            path = get_cache_file(++index);
         }
         LOGD(" old file");
         current_mapping.setCurrent(size);
         fclose(fp);
    }
    return path;
}

std::string getToday(){
    time_t now;
    struct tm* p;
    int year,month,day;

    time(&now);
    p = localtime(&now);

    year = p->tm_year+1900;
    month = p->tm_mon + 1;
    day = p->tm_mday;
    std::stringstream ss;
    std::string extra = "_";
    ss << year << extra << month << extra <<day;

    return ss.str();
}

void logger_cache_write(const std::string& log){
    LOGD(" log is %s",log.c_str());
    if(&current_mapping == NULL || current_mapping.getMappingPath().empty()){//当前还没有映射
        //创建映射
        create_mapping(log);
//        create_mapping_file();
        LOGD(" new mapping");
    }else{
        LOGD(" old mapping");
        //正在映射
        //获取映射区域剩余的区域，如果写入的区域会超过映射的区域，重新创建新的映射
        if(current_mapping.getCurrent() < MMAP_SIZE){
            memcpy(current_mapping.getAddr() + current_mapping.getCurrent(), log.c_str(), log.length());
            current_mapping.addLen(log.length());
        }else {
            //关闭之前的映射
            create_mapping(log);
        }
    }
}

CacheMMap::CacheMMap(){
    is_mapping = false;
}

CacheMMap::CacheMMap(std::string path){
    is_mapping = false;
    mapping_path = path;
}

bool CacheMMap::isMapping(){
    return is_mapping;
}
std::string CacheMMap::getMappingPath(){
    return mapping_path;
}
void CacheMMap::setIsMapping(bool mapping){
    is_mapping = mapping;
}
void CacheMMap::setMappingPath(std::string path){
    mapping_path = path;
}

void CacheMMap::setTotal(int _total){
    total = _total;
}

void CacheMMap::setCurrent(int _current){
    current = _current;
}

int CacheMMap::getTotal(){
    return total;
}

int CacheMMap::getCurrent(){
    return current;
}

void CacheMMap::setAddr(char* _addr){
    addr = _addr;
}
char* CacheMMap::getAddr(){
    return addr;
}

void CacheMMap::addLen(int add){
    current = current + add;
}