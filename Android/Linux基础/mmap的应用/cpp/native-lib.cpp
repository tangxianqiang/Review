#include <jni.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include "logger.h"
/**
** 1.mmap缓存文件以10M为单位，超过10M新建
** 2.什么时候将缓存拷贝到外存？手动？因为缓存文件会越来越多，被动情况是不是满了10M就拷贝一次
** 3.利用Android的worker上传服务器，不断检测mmap外存，所以需要手动调用缓存写入外存，否则不能及时拥有log信息
**/
#define PACKAGE(name) Java_com_blcodes_logger_Logger_##name

extern "C" {
JNIEXPORT void JNICALL PACKAGE(init) (JNIEnv* env,jobject obj,jstring path,jstring name){

    const char* str;
    const char* nameStr;
    str = env->GetStringUTFChars(path,0);
    nameStr = env->GetStringUTFChars(name,0);
    if(str == NULL)
    {
        return ;
    }



    char* spector = "/";
    char* c = (char *)malloc(strlen(str) + strlen(nameStr) + strlen(spector) + 1);
    	strcpy(c, str);
        strcat(c,spector);
    	strcat(c, nameStr);
    LOGD(" the cache path is %s",c);
    init_file(c);
//   jstring ss  = jstrCat(env,str,name)
    if(access(c ,0) == -1) //文件不存在
    {

        int flag = mkdir(c,0777);
        if (flag == 0)
        {
            LOGD(" make successfully");
        }
        else
        {
            LOGD("make errorly");
        }

    }

    //释放资源
    env->ReleaseStringUTFChars(path, str);
}

JNIEXPORT void JNICALL PACKAGE(logI) (JNIEnv* env,jobject obj,jstring log){
    const char* log_str;
    log_str = env->GetStringUTFChars(log,0);
    logger_cache_write(log_str);
    //释放资源
        env->ReleaseStringUTFChars(log, log_str);
}
}

