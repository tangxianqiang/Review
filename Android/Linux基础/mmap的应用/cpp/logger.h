#include <android/log.h>
#include<string>
#define LOG_TAG "logger - test"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG, __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG, __VA_ARGS__)
#define LOGF(...)  __android_log_print(ANDROID_LOG_FATAL,LOG_TAG, __VA_ARGS__)

const int MMAP_SIZE = 3 *1024*1024;

void init_file( const std::string& cachedir);

void show_cache();

std::string create_mapping_file();

void create_mapping(std::string log);

void logger_cache_write(const std::string& log);

std::string get_cache_file(int index);

std::string getToday();

std::string datetimeToString(time_t time);

class CacheMMap{
private:
    bool is_mapping;
    std::string mapping_path;
    int total;//总共映射
    int current;//当前映射
    char* addr; //当前映射的地址
public:
    CacheMMap();
    CacheMMap(std::string);
    bool isMapping();
    std::string getMappingPath();
    void setIsMapping(bool);
    void setMappingPath(std::string path);
    void setTotal(int total);
    void setCurrent(int current);
    int getTotal();
    int getCurrent();
    void setAddr(char* addr);
    char* getAddr();
    void addLen(int add);
};