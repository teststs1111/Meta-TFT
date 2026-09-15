#ifndef JSMN_H
#define JSMN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { JSMN_UNDEFINED=0,JSMN_OBJECT=1,JSMN_ARRAY=2,JSMN_STRING=3,JSMN_PRIMITIVE=4 } jsmntype_t;
typedef enum { JSMN_ERROR_NOMEM=-1,JSMN_ERROR_INVAL=-2,JSMN_ERROR_PART=-3 } jsmnerr_t;
typedef struct {jsmntype_t type;int start;int end;int size;} jsmntok_t;
typedef struct {unsigned int pos;unsigned int toknext;int toksuper;} jsmn_parser;
static jsmntok_t*jsmn_alloc_token(jsmn_parser*p,jsmntok_t*t,unsigned int n){if(p->toknext>=n)return NULL;jsmntok_t*x=&t[p->toknext++];x->start=x->end=-1;x->size=0;return x;}
static void jsmn_fill_token(jsmntok_t*t,jsmntype_t type,int s,int e){t->type=type;t->start=s;t->end=e;t->size=0;}
static int jsmn_parse_primitive(jsmn_parser*p,const char*j,size_t l,jsmntok_t*t,unsigned int n){int s=p->pos;for(;p->pos<l;p->pos++){char c=j[p->pos];if(c=='\t'||c=='\r'||c=='\n'||c==' '||c==','||c==']'||c=='}')break;if((unsigned char)c<32)return JSMN_ERROR_INVAL;}if(!t){p->pos--;return 0;}jsmntok_t*x=jsmn_alloc_token(p,t,n);if(!x){p->pos=s;return JSMN_ERROR_NOMEM;}jsmn_fill_token(x,JSMN_PRIMITIVE,s,p->pos);p->pos--;return 0;}
static int jsmn_parse_string(jsmn_parser*p,const char*j,size_t l,jsmntok_t*t,unsigned int n){int s=p->pos++;for(;p->pos<l;p->pos++){char c=j[p->pos];if(c=='"'){if(!t)return 0;jsmntok_t*x=jsmn_alloc_token(p,t,n);if(!x){p->pos=s;return JSMN_ERROR_NOMEM;}jsmn_fill_token(x,JSMN_STRING,s+1,p->pos);return 0;}if(c=='\\'&&p->pos+1<l)p->pos++;}p->pos=s;return JSMN_ERROR_PART;}
static int jsmn_parse(jsmn_parser*p,const char*j,size_t l,jsmntok_t*t,unsigned int n){int r,count=p->toknext;for(;p->pos<l;p->pos++){char c=j[p->pos];jsmntok_t*x;int i;switch(c){case '{':case '[':count++;if(!t)break;x=jsmn_alloc_token(p,t,n);if(!x)return JSMN_ERROR_NOMEM;x->type=(c=='{'?JSMN_OBJECT:JSMN_ARRAY);x->start=p->pos;if(p->toksuper!=-1)t[p->toksuper].size++;p->toksuper=p->toknext-1;break;case '}':case ']':if(!t)break;for(i=p->toknext-1;i>=0;i--)if(t[i].start!=-1&&t[i].end==-1){if((c=='}'?JSMN_OBJECT:JSMN_ARRAY)!=t[i].type)return JSMN_ERROR_INVAL;t[i].end=p->pos+1;p->toksuper=-1;break;}if(i<0)return JSMN_ERROR_INVAL;for(;i>=0;i--)if(t[i].start!=-1&&t[i].end==-1){p->toksuper=i;break;}break;case '"':r=jsmn_parse_string(p,j,l,t,n);if(r<0)return r;count++;if(p->toksuper!=-1&&t)t[p->toksuper].size++;break;case '\t':case '\r':case '\n':case ' ':break;case ':':p->toksuper=p->toknext-1;break;case ',':if(t&&p->toksuper!=-1&&t[p->toksuper].type!=JSMN_ARRAY&&t[p->toksuper].type!=JSMN_OBJECT){for(i=p->toknext-1;i>=0;i--)if(t[i].type==JSMN_ARRAY||t[i].type==JSMN_OBJECT){if(t[i].start!=-1&&t[i].end==-1){p->toksuper=i;break;}}}break;default:r=jsmn_parse_primitive(p,j,l,t,n);if(r<0)return r;count++;if(p->toksuper!=-1&&t)t[p->toksuper].size++;break;}}if(t)for(int i=p->toknext-1;i>=0;i--)if(t[i].start!=-1&&t[i].end==-1)return JSMN_ERROR_PART;return count;}
static void jsmn_init(jsmn_parser*p){p->pos=0;p->toknext=0;p->toksuper=-1;}
#ifdef __cplusplus
}
#endif
#endif
