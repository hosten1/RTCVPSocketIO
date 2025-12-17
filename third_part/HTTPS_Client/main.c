//
//  main.m
//  testHttpsClient
//
//  Created by luoyongmeng on 2022/8/11.
//

#import <stdio.h>
#include <string.h>
#include "https.h"

int main(int argc, const char * argv[]) {
        // insert code here...
        printf("Hello, World!\n");
        
        char *url;
        char *postURL;
        char data[1024], response[4096];
        char postData[1024], postResponse[4096];
        int  i, ret, size,code;

        HTTP_INFO hi1, hi2;


        // Init http session. verify: check the server CA cert.
        http_init(&hi1, FALSE);
        http_init(&hi2, FALSE);
        
        url = "https://172.16.8.52:10670/api/base/getSupportList?pkey=rRw1284Kdfl3pd";
        url = "https://211.159.153.77:10008/getData?name=zhangSan";
        postURL = "https://211.159.153.77:10008/postData";
        if(http_open(&hi1, url) < 0)
        {
            http_strerror(data, 1024);
            printf("socket error: %s \n", data);

    //        goto error;
        }

        ret = http_get(&hi1, url, response, sizeof(response));

        printf("return code: %d \n", ret);
        printf("return body: %s \n", response);
        mbedtls_net_usleep(1000);
        if (http_open(&hi2, postURL) < 0) {
            http_strerror(postData, 1024);
            printf("socket error: %s \n", data);
        }
        char * datas = "{\"data\":\"and so on\"}";
        code = http_post(&hi2, postURL, datas, postResponse, sizeof(postResponse));
        printf("return code: %d \n", code);
        printf("return body: %s \n", postResponse);
        http_close(&hi1);
        http_close(&hi2);
        return 0;
}
