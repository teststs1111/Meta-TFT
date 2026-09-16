#include "api_client.h"
#include "jsmn.h"
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/libssl.h>
#include <psp2/sysmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HTTP_RECV_CHUNK 4096
#define HTTP_BODY_MAX (512*1024)
#define JSON_TOKEN_MAX 4096
#define SSL_HEAP_SIZE (1024*1024)

static char g_net_mem[256*1024] __attribute__((aligned(8)));
static int g_net_inited=0, g_http_inited=0, g_ssl_inited=0;
static int g_net_module=0, g_http_module=0, g_ssl_module=0, g_https_module=0, g_http_tmpl_id=-1;

int api_client_init(void){
    int r=sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    if(r<0)return r;
    g_net_module=1;

    r=sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
    if(r<0)goto fail;
    g_https_module=1;

    r=sceSysmoduleLoadModule(SCE_SYSMODULE_SSL);
    if(r<0)goto fail;
    g_ssl_module=1;

    r=sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    if(r<0)goto fail;
    g_http_module=1;

    SceNetInitParam p={g_net_mem,sizeof(g_net_mem),0};
    r=sceNetInit(&p);
    if(r<0)goto fail;
    g_net_inited=1;

    r=sceNetCtlInit();
    if(r<0)goto fail;

    r=sceSslInit(SSL_HEAP_SIZE);
    if(r<0)goto fail;
    g_ssl_inited=1;

    r=sceHttpInit(4*1024*1024);
    if(r<0)goto fail;
    g_http_inited=1;

    /* iTLS-Enso provides the modern TLS stack. Vita's legacy CA/server
       verification can still reject otherwise usable modern certificates,
       so disable the legacy certificate checks for this public read-only API. */
    r=sceHttpsDisableOption(SCE_HTTPS_FLAG_SERVER_VERIFY |
                            SCE_HTTPS_FLAG_CN_CHECK |
                            SCE_HTTPS_FLAG_KNOWN_CA_CHECK |
                            SCE_HTTPS_FLAG_NOT_AFTER_CHECK |
                            SCE_HTTPS_FLAG_NOT_BEFORE_CHECK);
    if(r<0)goto fail;

    g_http_tmpl_id=sceHttpCreateTemplate("metatft-vita/0.1",SCE_HTTP_VERSION_1_1,SCE_TRUE);
    if(g_http_tmpl_id<0){r=g_http_tmpl_id;goto fail;}
    return 0;

fail:
    if(g_http_tmpl_id>=0){sceHttpDeleteTemplate(g_http_tmpl_id);g_http_tmpl_id=-1;}
    if(g_http_inited){sceHttpTerm();g_http_inited=0;}
    if(g_ssl_inited){sceSslTerm();g_ssl_inited=0;}
    if(g_net_inited){sceNetCtlTerm();sceNetTerm();g_net_inited=0;}
    if(g_http_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);g_http_module=0;}
    if(g_ssl_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_SSL);g_ssl_module=0;}
    if(g_https_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);g_https_module=0;}
    if(g_net_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);g_net_module=0;}
    return r;
}

void api_client_shutdown(void){
    if(g_http_tmpl_id>=0){sceHttpDeleteTemplate(g_http_tmpl_id);g_http_tmpl_id=-1;}
    if(g_http_inited){sceHttpTerm();g_http_inited=0;}
    if(g_ssl_inited){sceSslTerm();g_ssl_inited=0;}
    if(g_net_inited){sceNetCtlTerm();sceNetTerm();g_net_inited=0;}
    if(g_http_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);g_http_module=0;}
    if(g_ssl_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_SSL);g_ssl_module=0;}
    if(g_https_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);g_https_module=0;}
    if(g_net_module){sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);g_net_module=0;}
}

static int http_get(const char *url,char **out){
    int r,conn=-1,req=-1,status=0;
    char *body=NULL;
    size_t len=0,cap=HTTP_RECV_CHUNK;
    unsigned long long cl=0;

    conn=sceHttpCreateConnectionWithURL(g_http_tmpl_id,url,SCE_FALSE);
    if(conn<0){r=conn;goto fail;}
    req=sceHttpCreateRequestWithURL(conn,SCE_HTTP_METHOD_GET,url,0);
    if(req<0){r=req;goto fail;}
    sceHttpAddRequestHeader(req,"Accept","application/json",SCE_HTTP_HEADER_ADD);
    r=sceHttpSendRequest(req,NULL,0);
    if(r<0){
        int ssl_err=0;
        unsigned int ssl_detail=0;
        /* Keep the detailed SSL information available to a debugger/log. */
        (void)sceHttpsGetSslError(req,&ssl_err,&ssl_detail);
        goto fail;
    }
    r=sceHttpGetStatusCode(req,&status);
    if(r<0)goto fail;
    if(status<200||status>=300){r=-status;goto fail;}

    sceHttpGetResponseContentLength(req,&cl);
    if(cl>0&&cl<HTTP_BODY_MAX)cap=(size_t)cl+1;
    body=malloc(cap);
    if(!body){r=-1;goto fail;}

    for(;;){
        if(len+HTTP_RECV_CHUNK>cap){
            size_t nc=cap*2;
            if(nc>HTTP_BODY_MAX)nc=HTTP_BODY_MAX;
            if(nc<=cap){r=-1;goto fail;}
            char *nb=realloc(body,nc);
            if(!nb){r=-1;goto fail;}
            body=nb;
            cap=nc;
        }
        int n=sceHttpReadData(req,body+len,cap-len);
        if(n<0){r=n;goto fail;}
        if(n==0)break;
        len+=(size_t)n;
    }
    body[len]='\0';
    *out=body;
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return (int)len;

fail:
    if(body)free(body);
    if(req>=0)sceHttpDeleteRequest(req);
    if(conn>=0)sceHttpDeleteConnection(conn);
    return r;
}

static int skip_token(const jsmntok_t*t,int i,int count){
    if(i<0||i>=count)return count;
    int e=i+1,c=t[i].size;
    if(t[i].type==JSMN_OBJECT){
        for(int k=0;k<c&&e<count;k++){
            e=skip_token(t,e,count);
            e=skip_token(t,e,count);
        }
    }else if(t[i].type==JSMN_ARRAY){
        for(int k=0;k<c&&e<count;k++)e=skip_token(t,e,count);
    }
    return e;
}

static int eq(const char*j,const jsmntok_t*t,const char*s){
    size_t n=(size_t)(t->end-t->start);
    return t->type==JSMN_STRING&&strlen(s)==n&&strncmp(j+t->start,s,n)==0;
}

static void cp(const char*j,const jsmntok_t*t,char*d,size_t c){
    size_t n=(size_t)(t->end-t->start);
    if(n>=c)n=c-1;
    memcpy(d,j+t->start,n);
    d[n]='\0';
}

static int parse(const char*j,int len,const char*name,RankedEntry*out,int max,int*oc){
    jsmn_parser p;
    jsmntok_t *t=malloc(sizeof(*t)*JSON_TOKEN_MAX);
    if(!t)return -3;
    jsmn_init(&p);
    int n=jsmn_parse(&p,j,(size_t)len,t,JSON_TOKEN_MAX);
    if(n<1||t[0].type!=JSMN_OBJECT){free(t);return -1;}

    int i=1,data=-1;
    for(int k=0;k<t[0].size&&i<n;k++){
        if(eq(j,&t[i],"data")){if(i+1<n)data=i+1;break;}
        i++;
        i=skip_token(t,i,n);
    }
    if(data<0||data>=n||t[data].type!=JSMN_ARRAY){free(t);return -2;}

    int cur=data+1,cnt=0;
    for(int e=0;e<t[data].size&&cnt<max&&cur<n;e++){
        if(t[cur].type!=JSMN_OBJECT){cur=skip_token(t,cur,n);continue;}
        RankedEntry x;
        memset(&x,0,sizeof(x));
        int hn=0,hc=0;
        long cts[8]={0};
        int z=cur+1;
        for(int f=0;f<t[cur].size&&z<n;f++){
            int v=z+1;
            if(v>=n)break;
            if(eq(j,&t[z],name)&&t[v].type==JSMN_STRING){
                cp(j,&t[v],x.name,sizeof(x.name));
                hn=1;
            }else if(eq(j,&t[z],"placement_count")&&t[v].type==JSMN_ARRAY){
                int q=v+1;
                for(int a=0;a<t[v].size&&a<8&&q<n;a++){
                    char b[32];
                    cp(j,&t[q],b,sizeof(b));
                    cts[a]=strtol(b,NULL,10);
                    q=skip_token(t,q,n);
                }
                hc=1;
            }
            z=skip_token(t,v,n);
        }
        if(hn&&hc){
            long total=0;
            for(int a=0;a<8;a++)total+=cts[a];
            if(total>0){
                x.n=(int)total;
                x.win_rate=(float)cts[0]/total;
                x.top4_rate=(float)(cts[0]+cts[1]+cts[2]+cts[3])/total;
                double sum=0;
                for(int a=0;a<8;a++)sum+=(double)(a+1)*cts[a];
                x.avg_place=(float)(sum/total);
                out[cnt++]=x;
            }
        }
        cur=skip_token(t,cur,n);
    }
    *oc=cnt;
    free(t);
    return 0;
}

int api_fetch_ranked_list(const char*path,const char*query,const char*name,RankedEntry*e,int max,int*oc){
    char url[API_MAX_URL_LEN];
    char*b=NULL;
    int r;
    snprintf(url,sizeof(url),"%s%s%s%s",API_BASE_URL,path,(query&&*query)?"?":"",(query&&*query)?query:"");
    r=http_get(url,&b);
    if(r<0)return r;
    int n=parse(b,r,name,e,max,oc);
    free(b);
    return n;
}

static int cmp(const void*a,const void*b){
    const RankedEntry*x=a,*y=b;
    return x->avg_place>y->avg_place?1:x->avg_place<y->avg_place?-1:0;
}

void ranked_entries_sort_by_place(RankedEntry*e,int n){qsort(e,n,sizeof(*e),cmp);}
