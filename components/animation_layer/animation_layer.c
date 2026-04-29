#include "animation_layer.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_random.h"


static const char *TAG = "anim_layer";
#define FRAME_MS 33
#define TASK_STACK 6144
#define TASK_PRIO 4
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static SemaphoreHandle_t s_cfg_mux=NULL, s_render_mux=NULL;
static TaskHandle_t s_task=NULL;
static animation_config_t s_cfg;

typedef struct {
    animation_type_t last_type; uint16_t idx; float hue; uint32_t frame, last_step;
    uint8_t *heat; uint16_t heat_sz;
} anim_state_t;
static anim_state_t S;

static void sr(animation_type_t t){S.last_type=t;S.idx=0;S.hue=0;S.last_step=0;S.frame=0;}
static uint8_t sc8(uint8_t v,uint8_t s){return(uint8_t)(((uint16_t)v*s)/255);}
static uint32_t now_ms(void){return(uint32_t)(esp_timer_get_time()/1000ULL);}
static uint32_t step_every(uint8_t spd,uint32_t slow){uint32_t s=1+((uint32_t)(255-spd)*slow)/255;return s?s:1;}

static void hsv2rgb(float h,float s,float v,uint8_t*r,uint8_t*g,uint8_t*b){
    while(h>=360) h-=360;
    while(h<0) h+=360;
    int i=(int)(h/60);float f=h/60-i,p=v*(1-s),q=v*(1-s*f),t2=v*(1-s*(1-f));
    float rf,gf,bf;
    switch(i){case 0:rf=v;gf=t2;bf=p;break;case 1:rf=q;gf=v;bf=p;break;case 2:rf=p;gf=v;bf=t2;break;
    case 3:rf=p;gf=q;bf=v;break;case 4:rf=t2;gf=p;bf=v;break;default:rf=v;gf=p;bf=q;break;}
    *r=(uint8_t)(rf*255);*g=(uint8_t)(gf*255);*b=(uint8_t)(bf*255);
}

static led_color_t blend(led_color_t a,led_color_t b,uint8_t t){
    return(led_color_t){sc8(a.r,255-t)+sc8(b.r,t),sc8(a.g,255-t)+sc8(b.g,t),sc8(a.b,255-t)+sc8(b.b,t),sc8(a.w,255-t)+sc8(b.w,t)};
}
static led_color_t dim(led_color_t c,uint8_t d){return(led_color_t){sc8(c.r,d),sc8(c.g,d),sc8(c.b,d),sc8(c.w,d)};}


/* ---- RENDERERS ---- */
static void r_solid(const animation_config_t*c,uint16_t n){led_driver_set_brightness(c->brightness);led_driver_set_all(c->primary_color);}

static void r_blink(const animation_config_t*c,uint16_t n){
    uint32_t p=2000-((uint32_t)(c->speed-1)*(2000-120)/254);if(p<50)p=50;
    bool on=((now_ms()/p)&1)==0;
    led_driver_set_brightness(c->brightness);if(on)led_driver_set_all(c->primary_color);else led_driver_clear();
}

static void r_breathing(const animation_config_t*c,uint16_t n){
    uint32_t p=4000-((uint32_t)(c->speed-1)*(4000-400)/254);if(p<100)p=100;
    float ph=(float)(now_ms()%p)/(float)p;
    float w=(1.0f-cosf(ph*2*M_PI))*0.5f;
    uint8_t eff=(uint8_t)(c->intensity/255.0f*200+55);
    uint8_t b=(uint8_t)((float)c->brightness*powf(w,255.0f/eff));
    led_driver_set_brightness(b);led_driver_set_all(c->primary_color);
}

static void r_rainbow(const animation_config_t*c,uint16_t n){
    float dp=0.5f+(c->speed/255.0f)*8;S.hue+=dp;if(S.hue>=360)S.hue-=360;
    led_driver_set_brightness(c->brightness);
    for(uint16_t i=0;i<n;i++){float h=fmodf(S.hue+(360.0f*i/n),360);led_color_t cl={0};hsv2rgb(h,1,1,&cl.r,&cl.g,&cl.b);
        uint16_t ri=(c->direction==ANIM_DIR_REVERSE)?n-1-i:i;led_driver_set_pixel(ri,cl);}
}

static void r_color_wipe(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,30);if(S.frame-S.last_step>=se){S.idx++;S.last_step=S.frame;if(S.idx>n)S.idx=0;}
    led_driver_set_brightness(c->brightness);led_color_t off={0};
    for(uint16_t i=0;i<n;i++){uint16_t ri=(c->direction==ANIM_DIR_REVERSE)?n-1-i:i;led_driver_set_pixel(ri,i<S.idx?c->primary_color:off);}
}

static void r_running_dot(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,16);if(S.frame-S.last_step>=se){S.idx=(S.idx+1)%n;S.last_step=S.frame;}
    led_driver_set_brightness(c->brightness);led_color_t off={0};
    uint16_t sz=c->size>0?c->size:1;if(sz>n)sz=n;
    for(uint16_t i=0;i<n;i++){bool on=false;for(uint16_t s=0;s<sz;s++){if(((S.idx+s)%n)==i)on=true;}
        uint16_t ri=(c->direction==ANIM_DIR_REVERSE)?n-1-i:i;led_driver_set_pixel(ri,on?c->primary_color:off);}
}

static void r_comet(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,16);if(S.frame-S.last_step>=se){S.idx=(S.idx+1)%n;S.last_step=S.frame;}
    led_driver_set_brightness(c->brightness);led_color_t off={0};
    uint8_t tl=c->tail_length>0?c->tail_length:6;
    for(uint16_t i=0;i<n;i++){int32_t d=(int32_t)S.idx-(int32_t)i;if(d<0)d+=n;
        if(d<tl){uint8_t f=255-(uint8_t)((d*c->fade_amount)/tl);led_driver_set_pixel((c->direction==ANIM_DIR_REVERSE)?n-1-i:i,dim(c->primary_color,f));}
        else led_driver_set_pixel((c->direction==ANIM_DIR_REVERSE)?n-1-i:i,off);}
}

static void r_meteor(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,12);if(S.frame-S.last_step>=se){S.idx=(S.idx+1)%(n*2);S.last_step=S.frame;}
    led_driver_set_brightness(c->brightness);
    /* fade all */
    for(uint16_t i=0;i<n;i++){if((esp_random()%10)<3){led_driver_set_pixel(i,(led_color_t){0});}}
    uint8_t tl=c->tail_length>0?c->tail_length:12;
    uint16_t head=S.idx%n;
    for(uint8_t j=0;j<tl;j++){int16_t p=head-j;if(p<0)continue;
        uint8_t f=255-(j*c->fade_amount/tl);led_driver_set_pixel((c->direction==ANIM_DIR_REVERSE)?n-1-p:p,dim(c->primary_color,f));}
}

static void r_theater(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,20);if(S.frame-S.last_step>=se){S.idx=(S.idx+1)%6;S.last_step=S.frame;}
    led_driver_set_brightness(c->brightness);uint8_t sz=c->size>0?c->size:3;
    for(uint16_t i=0;i<n;i++){led_driver_set_pixel(i,((i+S.idx)%sz==0)?c->primary_color:c->secondary_color);}
}

static void r_scanner(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,10);if(S.frame-S.last_step>=se){S.idx++;S.last_step=S.frame;if(S.idx>=n*2)S.idx=0;}
    uint16_t pos=S.idx<n?S.idx:(n*2-1-S.idx);
    led_driver_set_brightness(c->brightness);uint8_t tl=c->tail_length>0?c->tail_length:8;
    for(uint16_t i=0;i<n;i++){int32_t d=abs((int32_t)i-(int32_t)pos);
        if(d==0)led_driver_set_pixel(i,c->primary_color);
        else if(d<tl)led_driver_set_pixel(i,dim(c->primary_color,255-d*c->fade_amount/tl));
        else led_driver_set_pixel(i,(led_color_t){0});}
}

static void r_twinkle(const animation_config_t*c,uint16_t n){
    led_driver_set_brightness(c->brightness);
    uint32_t se=step_every(c->speed,8);if(S.frame-S.last_step>=se){S.last_step=S.frame;
        uint8_t cnt=1+(c->density*n)/255;if(cnt>n)cnt=n;
        for(uint8_t j=0;j<cnt;j++){uint16_t p=esp_random()%n;led_driver_set_pixel(p,c->primary_color);}}
    /* fade all toward bg */
    for(uint16_t i=0;i<n;i++){/* natural fade handled by periodic overwrite */}
    if(S.frame%4==0){for(uint16_t i=0;i<n;i++){/* dim random pixels */
        if(esp_random()%4==0)led_driver_set_pixel(i,c->background_color);}}
}

static void r_sparkle(const animation_config_t*c,uint16_t n){
    led_driver_set_brightness(c->brightness);led_driver_set_all(c->background_color);
    uint8_t cnt=1+(c->density*n)/255;if(cnt>n)cnt=n;
    for(uint8_t j=0;j<cnt;j++){uint16_t p=esp_random()%n;
        if(c->random_color){led_color_t rc;hsv2rgb(esp_random()%360,1,1,&rc.r,&rc.g,&rc.b);rc.w=0;led_driver_set_pixel(p,rc);}
        else led_driver_set_pixel(p,c->primary_color);}
}

static void r_fire(const animation_config_t*c,uint16_t n){
    if(!S.heat||S.heat_sz!=n){free(S.heat);S.heat=calloc(n,1);S.heat_sz=n;if(!S.heat)return;}
    led_driver_set_brightness(c->brightness);
    uint32_t se=step_every(c->speed,6);if(S.frame-S.last_step<se)return;S.last_step=S.frame;
    for(uint16_t i=0;i<n;i++){uint8_t cool=esp_random()%(((c->cooling*10)/n)+2);S.heat[i]=S.heat[i]>cool?S.heat[i]-cool:0;}
    for(uint16_t k=n-1;k>=2;k--)S.heat[k]=(S.heat[k-1]+S.heat[k-2]+S.heat[k-2])/3;
    if((esp_random()%255)<c->sparking){uint8_t y=esp_random()%7;if(y<n)S.heat[y]+=160+(esp_random()%96);}
    for(uint16_t i=0;i<n;i++){uint8_t t=S.heat[i];led_color_t cl;
        if(t>200){cl=(led_color_t){255,255,(t-200)*5/55,0};}else if(t>100){cl=(led_color_t){255,(t-100)*255/100,0,0};}
        else{cl=(led_color_t){t*2,0,0,0};}led_driver_set_pixel(i,cl);}
}

static void r_police(const animation_config_t*c,uint16_t n){
    uint32_t p=400-((uint32_t)(c->speed-1)*350/254);if(p<30)p=30;
    uint8_t phase=(now_ms()/p)%4;uint16_t half=n/2;
    led_driver_set_brightness(c->brightness);
    led_color_t off={0};
    for(uint16_t i=0;i<n;i++){
        if(i<half)led_driver_set_pixel(i,(phase<2)?c->primary_color:off);
        else led_driver_set_pixel(i,(phase>=2)?c->secondary_color:off);}
}

static void r_neon(const animation_config_t*c,uint16_t n){
    led_driver_set_brightness(c->brightness);
    uint8_t base=180+(c->intensity*75/255);uint8_t flick=(esp_random()%60);
    uint8_t b2=base>flick?base-flick:0;
    if(esp_random()%100<5)b2=b2/3; /* random short blink */
    led_color_t cl=c->random_color?
        (led_color_t){(uint8_t)(esp_random()%256),(uint8_t)(esp_random()%256),(uint8_t)(esp_random()%256),0}:c->primary_color;
    led_driver_set_all(dim(cl,b2));
}

static void r_gradient(const animation_config_t*c,uint16_t n){
    S.hue+=0.5f+(c->speed/255.0f)*4;if(S.hue>=n*2)S.hue-=n*2;
    led_driver_set_brightness(c->brightness);
    for(uint16_t i=0;i<n;i++){float pos=fmodf(i+S.hue,n)/(float)n*255;
        uint16_t ri=(c->direction==ANIM_DIR_REVERSE)?n-1-i:i;
        led_driver_set_pixel(ri,blend(c->primary_color,c->secondary_color,(uint8_t)pos));}
}

static void r_wave(const animation_config_t*c,uint16_t n){
    float sp=0.02f+(c->speed/255.0f)*0.2f;S.hue+=sp;
    led_driver_set_brightness(c->brightness);
    float sz=c->size>0?(float)c->size:10.0f;
    for(uint16_t i=0;i<n;i++){float w=(sinf(S.hue+i*2*M_PI/sz)+1)/2;
        uint8_t b=(uint8_t)(w*c->intensity);
        uint16_t ri=(c->direction==ANIM_DIR_REVERSE)?n-1-i:i;
        led_driver_set_pixel(ri,blend(c->secondary_color,c->primary_color,b));}
}

static void r_confetti(const animation_config_t*c,uint16_t n){
    led_driver_set_brightness(c->brightness);
    /* fade existing */
    for(uint16_t i=0;i<n;i++){/* naiive fade: just dim occasionally */}
    uint32_t se=step_every(c->speed,4);if(S.frame-S.last_step>=se){S.last_step=S.frame;
        uint8_t cnt=1+(c->density*n)/400;
        for(uint8_t j=0;j<cnt;j++){uint16_t p=esp_random()%n;led_color_t cl;
            if(c->random_color){hsv2rgb(esp_random()%360,1,1,&cl.r,&cl.g,&cl.b);cl.w=0;}else cl=c->primary_color;
            led_driver_set_pixel(p,cl);}}
    /* fade step */
    if(S.frame%2==0){for(uint16_t i=0;i<n;i++){if(esp_random()%3==0)led_driver_set_pixel(i,(led_color_t){0});}}
}

static void r_pulse(const animation_config_t*c,uint16_t n){
    uint32_t p=2000-((uint32_t)(c->speed-1)*(2000-200)/254);if(p<100)p=100;
    float ph=(float)(now_ms()%p)/(float)p;
    float sharp=(float)c->intensity/100.0f;if(sharp<0.5f)sharp=0.5f;
    float w=powf((sinf(ph*2*M_PI-M_PI/2)+1)/2,sharp);
    uint8_t b=(uint8_t)((float)c->brightness*w);
    led_driver_set_brightness(b);led_driver_set_all(c->primary_color);
}

/* ---- CUSTOM PATTERNS ---- */
static void rc_dot(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,16);if(S.frame-S.last_step>=se){S.idx=(S.idx+1)%n;S.last_step=S.frame;}
    led_driver_set_brightness(c->brightness);
    uint8_t sz=c->size>0?c->size:1;
    for(uint16_t i=0;i<n;i++){bool on=false;for(uint8_t s=0;s<sz;s++){if(((S.idx+s)%n)==i)on=true;}
        uint16_t ri=(c->direction==ANIM_DIR_REVERSE)?n-1-i:i;
        led_driver_set_pixel(ri,on?c->primary_color:c->background_color);}
}

static void rc_bar(const animation_config_t*c,uint16_t n){
    uint32_t se=step_every(c->speed,12);if(S.frame-S.last_step>=se){S.idx=(S.idx+1)%n;S.last_step=S.frame;}
    led_driver_set_brightness(c->brightness);uint8_t sz=c->size>0?c->size:5;
    for(uint16_t i=0;i<n;i++){int32_t d=(int32_t)i-(int32_t)S.idx;if(d<0)d+=n;
        bool inbar=d<sz;uint16_t ri=(c->direction==ANIM_DIR_REVERSE)?n-1-i:i;
        led_driver_set_pixel(ri,inbar?c->primary_color:c->secondary_color);
        if(c->mirror&&ri<n){uint16_t mi=n-1-ri;led_driver_set_pixel(mi,inbar?c->primary_color:c->secondary_color);}}
}

static void rc_gradient(const animation_config_t*c,uint16_t n){
    r_gradient(c,n); /* reuse gradient flow */
}

static void r_custom(const animation_config_t*c,uint16_t n){
    switch(c->custom_pattern){
        case CUSTOM_PATTERN_DOT:rc_dot(c,n);break;
        case CUSTOM_PATTERN_BAR:rc_bar(c,n);break;
        case CUSTOM_PATTERN_GRADIENT:rc_gradient(c,n);break;
        case CUSTOM_PATTERN_WAVE:r_wave(c,n);break;
        case CUSTOM_PATTERN_RANDOM_SPARK:r_sparkle(c,n);break;
        case CUSTOM_PATTERN_DUAL_COLOR_CHASE:r_theater(c,n);break;
        default:rc_dot(c,n);break;
    }
}

/* ---- REGISTRY ---- */
static const animation_renderer_t k_registry[]={
    {ANIM_SOLID,"solid",r_solid},{ANIM_BLINK,"blink",r_blink},{ANIM_BREATHING,"breathing",r_breathing},
    {ANIM_RAINBOW,"rainbow",r_rainbow},{ANIM_COLOR_WIPE,"color_wipe",r_color_wipe},
    {ANIM_RUNNING_DOT,"running_dot",r_running_dot},{ANIM_COMET,"comet",r_comet},
    {ANIM_METEOR_RAIN,"meteor_rain",r_meteor},{ANIM_THEATER_CHASE,"theater_chase",r_theater},
    {ANIM_SCANNER,"scanner",r_scanner},{ANIM_TWINKLE,"twinkle",r_twinkle},{ANIM_SPARKLE,"sparkle",r_sparkle},
    {ANIM_FIRE,"fire",r_fire},{ANIM_POLICE_LIGHT,"police_light",r_police},
    {ANIM_NEON_FLICKER,"neon_flicker",r_neon},{ANIM_GRADIENT_FLOW,"gradient_flow",r_gradient},
    {ANIM_WAVE,"wave",r_wave},{ANIM_CONFETTI,"confetti",r_confetti},{ANIM_PULSE,"pulse",r_pulse},
    {ANIM_CUSTOM,"custom",r_custom},
};
#define REG_COUNT (sizeof(k_registry)/sizeof(k_registry[0]))

static animation_render_fn_t find_renderer(animation_type_t t){
    for(size_t i=0;i<REG_COUNT;i++){if(k_registry[i].type==t)return k_registry[i].render;}
    return r_solid;
}

static void render_frame(const animation_config_t*c,uint16_t n){
    if(!c->power){led_driver_set_brightness(0);led_driver_clear();return;}
    if(S.last_type!=c->type)sr(c->type);
    find_renderer(c->type)(c,n);
}

static void animation_task(void*arg){
    TickType_t lw=xTaskGetTickCount();
    while(1){
        animation_config_t cfg;
        xSemaphoreTake(s_cfg_mux,portMAX_DELAY);cfg=s_cfg;xSemaphoreGive(s_cfg_mux);

        xSemaphoreTake(s_render_mux,portMAX_DELAY);
        uint16_t cnt=led_driver_get_count();if(cnt>0){render_frame(&cfg,cnt);led_driver_show();}
        xSemaphoreGive(s_render_mux);
        S.frame++;vTaskDelayUntil(&lw,pdMS_TO_TICKS(FRAME_MS));
    }
}

esp_err_t animation_layer_init(void){
    if(s_cfg_mux)return ESP_OK;
    s_cfg_mux=xSemaphoreCreateMutex();s_render_mux=xSemaphoreCreateMutex();
    if(!s_cfg_mux||!s_render_mux){ESP_LOGE(TAG,"mutex fail");return ESP_ERR_NO_MEM;}
    memset(&S,0,sizeof(S));sr(s_cfg.type);ESP_LOGI(TAG,"init OK");return ESP_OK;
}

esp_err_t animation_layer_start(void){
    if(s_task)return ESP_OK;
    if(xTaskCreate(animation_task,"anim",TASK_STACK,NULL,TASK_PRIO,&s_task)!=pdPASS){ESP_LOGE(TAG,"task fail");return ESP_ERR_NO_MEM;}
    ESP_LOGI(TAG,"started @ %d FPS",1000/FRAME_MS);return ESP_OK;
}

esp_err_t animation_layer_stop(void){if(!s_task)return ESP_OK;vTaskDelete(s_task);s_task=NULL;return ESP_OK;}

esp_err_t animation_layer_set_config(const animation_config_t*c){
    if(!c||!s_cfg_mux)return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_cfg_mux,portMAX_DELAY);s_cfg=*c;xSemaphoreGive(s_cfg_mux);return ESP_OK;
}

esp_err_t animation_layer_get_config(animation_config_t*c){
    if(!c||!s_cfg_mux)return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_cfg_mux,portMAX_DELAY);*c=s_cfg;xSemaphoreGive(s_cfg_mux);return ESP_OK;
}

esp_err_t animation_layer_set_animation(animation_type_t t){
    if(!config_manager_animation_is_valid(t)||!s_cfg_mux)return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_cfg_mux,portMAX_DELAY);s_cfg.type=t;xSemaphoreGive(s_cfg_mux);return ESP_OK;
}

esp_err_t animation_layer_set_speed(uint8_t spd){
    if(!s_cfg_mux)return ESP_ERR_INVALID_STATE;
    if(spd<1)spd=1;
    xSemaphoreTake(s_cfg_mux,portMAX_DELAY);s_cfg.speed=spd;xSemaphoreGive(s_cfg_mux);return ESP_OK;
}

esp_err_t animation_layer_set_brightness(uint8_t b){
    if(!s_cfg_mux)return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_cfg_mux,portMAX_DELAY);s_cfg.brightness=b;xSemaphoreGive(s_cfg_mux);return ESP_OK;
}

esp_err_t animation_layer_set_power(bool p){
    if(!s_cfg_mux)return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_cfg_mux,portMAX_DELAY);s_cfg.power=p;xSemaphoreGive(s_cfg_mux);return ESP_OK;
}

esp_err_t animation_layer_pause(void){
    if(!s_render_mux)return ESP_ERR_INVALID_STATE;
    return xSemaphoreTake(s_render_mux,portMAX_DELAY)==pdTRUE?ESP_OK:ESP_FAIL;
}

esp_err_t animation_layer_resume(void){
    if(!s_render_mux)return ESP_ERR_INVALID_STATE;
    return xSemaphoreGive(s_render_mux)==pdTRUE?ESP_OK:ESP_FAIL;
}

esp_err_t animation_layer_update(uint32_t delta_ms){
    if(!s_cfg_mux||!s_render_mux)return ESP_ERR_INVALID_STATE;
    animation_config_t cfg;
    xSemaphoreTake(s_cfg_mux,portMAX_DELAY);cfg=s_cfg;xSemaphoreGive(s_cfg_mux);
    xSemaphoreTake(s_render_mux,portMAX_DELAY);
    uint16_t cnt=led_driver_get_count();if(cnt>0){render_frame(&cfg,cnt);led_driver_show();}
    xSemaphoreGive(s_render_mux);
    S.frame++;
    return ESP_OK;
}
