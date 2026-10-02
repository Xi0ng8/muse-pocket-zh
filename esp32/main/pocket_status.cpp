// Muse Pocket: portrait e-paper UI and physical controls for the X4 Pro.
#include "sdkconfig.h"
#include "pocket.h"
#include "BoardConfig.h"
#include "XteinkDetect.h"
#include "driver/Ssd1677Driver.h"
#include "driver/Uc8179Driver.h"
#include "driver/Uc8279X4Driver.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs.h"
#include "freertos/semphr.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
extern "C" {
#include "led_status.h"
#include "pixel_font.h"
#include "happy_anim.h"
}

namespace {
constexpr int W=480, H=800, AVATAR=480, AVATAR_Y=104;
constexpr size_t FRAME=800*480/8;
constexpr uint8_t bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
const char* TAG="link.pocket.ui";
freeink::EpdBus bus;
freeink::PanelDriver* panel;
SemaphoreHandle_t lock_, image_done;
TaskHandle_t renderer;
uint8_t *canvas, *avatar, *staging, *frame, *shown;
bool custom_avatar=false, custom_status=false, loading=false, menu=false, flipped=false, sleeping=false;
bool recovery=false, force_full=true, initialized=false;
int selected=0, brightness=25, warmth=50, cadence=5, refreshes=0, battery=-1;
int64_t last_status_us=0, last_draw_us=0;
uint32_t requested_frame=0, finished_frame=0;
led_state_t state=LED_STATE_BOOT;
char title[64]="Muse Pocket", status[241]="Pair with Muse to get started";
i2c_master_bus_handle_t i2c_bus;
i2c_master_dev_handle_t touch, gauge;
const int cadences[]={2,5,15,30};

const char* connection(led_state_t s) {
    switch(s) {
    case LED_STATE_SETUP_IDLE: case LED_STATE_BLE_ADVERTISING: return "Open Muse > Add gadget";
    case LED_STATE_BLE_CONNECTED: return "Phone connected";
    case LED_STATE_PAIRING_CONFIRM_REQUIRED: return "Press LEFT to confirm";
    case LED_STATE_WS_CONNECTED: return "Connected to Muse";
    case LED_STATE_WS_DISCONNECTED: return "Reconnecting";
    case LED_STATE_UNPAIRED: return "Pairing required";
    case LED_STATE_ERROR: return "Connection error";
    case LED_STATE_WIFI_CONNECTING: return "Joining Wi-Fi";
    default: return "Connecting";
    }
}
void notify() { if(renderer) xTaskNotifyGive(renderer); }
uint32_t request_frame() {
    xSemaphoreTake(lock_,portMAX_DELAY);uint32_t ticket=++requested_frame;xSemaphoreGive(lock_);
    notify();return ticket;
}
bool await_frame(uint32_t ticket) {
    int64_t deadline=esp_timer_get_time()+20000000;
    while(esp_timer_get_time()<deadline) {
        xSemaphoreTake(lock_,portMAX_DELAY);bool done=finished_frame>=ticket;xSemaphoreGive(lock_);
        if(done) return bus.healthy();
        xSemaphoreTake(image_done,pdMS_TO_TICKS(100));
    }
    return false;
}
void save_settings() {
    nvs_handle_t h;
    if(nvs_open("muse_pocket",NVS_READWRITE,&h)!=ESP_OK) return;
    nvs_set_u8(h,"light",brightness); nvs_set_u8(h,"warmth",warmth);
    nvs_set_u8(h,"cadence",cadence); nvs_set_u8(h,"flip",flipped);
    nvs_commit(h); nvs_close(h);
}
void light(int b, int warm) {
    uint32_t total=std::clamp(b,0,100)*1023/100;
    uint32_t amber=total*std::clamp(warm,0,100)/100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_4,total-amber);
    ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_4);
    ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_5,amber);
    ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_5);
}
void light_init() {
    ledc_timer_config_t t={}; t.speed_mode=LEDC_LOW_SPEED_MODE;
    t.duty_resolution=LEDC_TIMER_10_BIT; t.timer_num=LEDC_TIMER_2;
    t.freq_hz=25000; t.clk_cfg=LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    for(int channel=4;channel<=5;++channel) {
        ledc_channel_config_t c={}; c.gpio_num=channel==4?8:9;
        c.speed_mode=LEDC_LOW_SPEED_MODE; c.channel=static_cast<ledc_channel_t>(channel);
        c.timer_sel=LEDC_TIMER_2; ESP_ERROR_CHECK(ledc_channel_config(&c));
    }
    nvs_handle_t h; uint8_t value;
    if(nvs_open("muse_pocket",NVS_READONLY,&h)==ESP_OK) {
        if(nvs_get_u8(h,"light",&value)==ESP_OK) brightness=std::min<int>(100,value);
        if(nvs_get_u8(h,"warmth",&value)==ESP_OK) warmth=std::min<int>(100,value);
        if(nvs_get_u8(h,"cadence",&value)==ESP_OK && (value==2||value==5||value==15||value==30)) cadence=value;
        if(nvs_get_u8(h,"flip",&value)==ESP_OK) flipped=value!=0;
        nvs_close(h);
    }
    light(brightness,warmth);
}
bool i2c_read(i2c_master_dev_handle_t dev,uint16_t reg,uint8_t* data,size_t n,bool wide) {
    if(!dev) return false;
    uint8_t r[]={uint8_t(reg>>8),uint8_t(reg)};
    return i2c_master_transmit_receive(dev,wide?r:r+1,wide?2:1,data,n,50)==ESP_OK;
}
void peripherals_init() {
    pinMode(2,OUTPUT); digitalWrite(2,LOW); // active-low touch rail
    pinMode(4,OUTPUT); digitalWrite(4,LOW);
    pinMode(10,OUTPUT); digitalWrite(10,LOW); delay(10);
    digitalWrite(4,HIGH); delay(60); pinMode(10,INPUT); delay(80);
    i2c_master_bus_config_t cfg={}; cfg.i2c_port=I2C_NUM_0;
    cfg.sda_io_num=GPIO_NUM_39; cfg.scl_io_num=GPIO_NUM_38;
    cfg.clk_source=I2C_CLK_SRC_DEFAULT; cfg.glitch_ignore_cnt=7;
    cfg.flags.enable_internal_pullup=true;
    if(i2c_new_master_bus(&cfg,&i2c_bus)!=ESP_OK) return;
    i2c_device_config_t dev={}; dev.dev_addr_length=I2C_ADDR_BIT_LEN_7; dev.scl_speed_hz=100000;
    for(uint8_t address:{0x5d,0x14}) {
        if(i2c_master_probe(i2c_bus,address,50)==ESP_OK) {
            dev.device_address=address;
            i2c_master_bus_add_device(i2c_bus,&dev,&touch); break;
        }
    }
    dev.device_address=0x63;
    if(i2c_master_probe(i2c_bus,0x63,50)==ESP_OK) i2c_master_bus_add_device(i2c_bus,&dev,&gauge);
}
void rect(int x,int y,int w,int h,uint8_t color) {
    int x0=std::max(0,x),y0=std::max(0,y),x1=std::min(W,x+w),y1=std::min(H,y+h);
    for(int row=y0;row<y1;++row) if(x1>x0) memset(canvas+row*W+x0,color,x1-x0);
}
void text(const char* str,int x,int y,int scale,int max_chars=80) {
    for(int i=0;str[i]&&i<max_chars;++i) {
        unsigned char ch=str[i]; if(ch<32||ch>126) ch='?';
        const uint8_t* glyph=pixel_font[ch-32];
        for(int gx=0;gx<5;++gx) for(int gy=0;gy<8;++gy)
            if(glyph[gx]&(1<<gy)) rect(x+(i*6+gx)*scale,y+gy*scale,scale,scale,0);
    }
}
void centred(const char* str,int y,int max_scale) {
    int len=strlen(str),scale=max_scale;
    while(scale>2&&len*6*scale>W-32) --scale;
    len=std::min(len,(W-32)/(6*scale));
    text(str,(W-len*6*scale)/2,y,scale,len);
}
void wrapped(const char* str,int y) {
    // Four readable lines; preserve newlines and prefer word boundaries.
    size_t pos=0,len=strlen(str);
    for(int line=0;line<4&&pos<len;++line) {
        size_t end=std::min(pos+35,len),newline=pos;
        while(newline<end&&str[newline]!='\n') ++newline;
        if(newline<end) end=newline;
        else if(end<len) { size_t space=end; while(space>pos&&str[space]!=' ') --space; if(space>pos) end=space; }
        char row[36]={}; memcpy(row,str+pos,end-pos); text(row,30,y+line*32,2);
        pos=end; while(pos<len&&(str[pos]==' '||str[pos]=='\n')) ++pos;
    }
}
uint8_t luma(uint16_t rgb) {
    int r=((rgb>>11)&31)*255/31,g=((rgb>>5)&63)*255/63,b=(rgb&31)*255/31;
    return (r*77+g*150+b*29)>>8;
}
void default_character() {
    constexpr int scale=5, x0=(W-HAPPY_ANIM_WIDTH*scale)/2,y0=AVATAR_Y+70;
    for(int y=0;y<HAPPY_ANIM_HEIGHT;++y) for(int x=0;x<HAPPY_ANIM_WIDTH;++x) {
        uint8_t c=happy_anim_frames[0][y*HAPPY_ANIM_WIDTH+x];
        uint16_t be=happy_anim_palette[c];
        rect(x0+x*scale,y0+y*scale,scale,scale,c?luma((be>>8)|(be<<8)):255);
    }
}
void compose() {
    memset(canvas,255,W*H);
    char bat[20]; snprintf(bat,sizeof(bat),battery>=0?"%d%%":"--%%",battery);
    text("MUSE POCKET",24,20,2); text(bat,390,20,2);
    if(sleeping) {
        centred("Sleeping",62,4); default_character();
        centred("Press POWER to wake",650,2); return;
    }
    if(menu) {
        centred("Settings",65,4);
        char rows[7][64];
        snprintf(rows[0],64,"Brightness: %d%%",brightness);
        snprintf(rows[1],64,"Warmth: %d%%",warmth);
        snprintf(rows[2],64,"Refresh: every %ds",cadence);
        snprintf(rows[3],64,"Orientation: %s",flipped?"flipped":"normal");
        snprintf(rows[4],64,"Sleep");
        snprintf(rows[5],64,"%s",recovery?"Return to CrossPoint":"CrossPoint not verified");
        snprintf(rows[6],64,"Back to Muse");
        for(int i=0;i<7;++i) {
            if(i==selected) {rect(14,156+i*72,W-28,3,0);rect(14,210+i*72,W-28,3,0);text(">",22,172+i*72,3);}
            text(rows[i],48,174+i*72,2);
        }
        text("RIGHT: next  POWER: change",30,720,2);
        text("Hold POWER on Return to restore",24,753,2);
        return;
    }
    centred(title,60,4);
    if(custom_avatar) memcpy(canvas+AVATAR_Y*W,avatar,AVATAR*W);
    else default_character();
    rect(24,603,W-48,2,0);
    wrapped(status,625);
    centred(connection(state),755,2);
    text("RIGHT: settings",140,782,1);
}
void encode() {
    memset(frame,255,FRAME);
    for(int y=0;y<H;++y) for(int x=0;x<W;++x) {
        int px=flipped?H-1-y:y,py=flipped?x:W-1-x;
        uint8_t g=canvas[y*W+x];
        if(g<int(bayer[y%4][x%4])*16+8) frame[py*100+px/8]&=~(0x80>>(px%8));
    }
}
void render_task(void*) {
    TickType_t wait=portMAX_DELAY;
    for(;;) {
        ulTaskNotifyTake(pdTRUE,wait);
        wait=portMAX_DELAY;
        xSemaphoreTake(lock_,portMAX_DELAY);
        int64_t now=esp_timer_get_time();
        bool immediate=menu||sleeping||force_full||requested_frame>finished_frame;
        if(!immediate && last_status_us>last_draw_us && now-last_draw_us<cadence*1000000LL) {
            wait=pdMS_TO_TICKS(std::max<int64_t>(1,(cadence*1000000LL-(now-last_draw_us))/1000));
            xSemaphoreGive(lock_); continue;
        }
        compose(); encode();
        bool changed=memcmp(frame,shown,FRAME)!=0;
        bool full=force_full||refreshes>=10;
        bool go_to_sleep=sleeping;
        uint32_t ticket=requested_frame;
        force_full=false;
        xSemaphoreGive(lock_);
        if((changed || full) && bus.healthy()) {
            panel->display(bus,frame,shown,full?freeink::RefreshMode::Full:freeink::RefreshMode::Fast,true);
            if(bus.healthy()) {
                memcpy(shown,frame,FRAME);
                xSemaphoreTake(lock_,portMAX_DELAY);
                refreshes=full?0:refreshes+1; last_draw_us=now;
                xSemaphoreGive(lock_);
            }
        }
        if(go_to_sleep) {
            light(0,warmth);
            if(bus.healthy()) panel->deepSleep(bus);
            digitalWrite(2,HIGH);
            pinMode(5,OUTPUT);digitalWrite(5,HIGH);
        }
        xSemaphoreTake(lock_,portMAX_DELAY);finished_frame=ticket;xSemaphoreGive(lock_);
        xSemaphoreGive(image_done);
        if(go_to_sleep) vTaskSuspend(nullptr);
    }
}
void sleep_now() {
    xSemaphoreTake(lock_,portMAX_DELAY); sleeping=true;force_full=true;xSemaphoreGive(lock_);
    if(!await_frame(request_frame())) {
        ESP_LOGE(TAG,"sleep screen or panel power-down failed; leaving recovery buttons active");
        return;
    }
    gpio_hold_en(GPIO_NUM_14);
    gpio_hold_en(GPIO_NUM_1);
    gpio_deep_sleep_hold_en();
    esp_sleep_enable_ext1_wakeup_io(1ULL<<3,ESP_EXT1_WAKEUP_ANY_LOW);
    while(digitalRead(3)==LOW) delay(30);
    esp_deep_sleep_start();
}
void activate() {
    xSemaphoreTake(lock_,portMAX_DELAY);
    switch(selected) {
    case 0: brightness=(brightness+25)%125; light(brightness,warmth);save_settings();break;
    case 1: warmth=(warmth+25)%125; light(brightness,warmth);save_settings();break;
    case 2: for(int i=0;i<4;++i) if(cadence==cadences[i]) {cadence=cadences[(i+1)%4];break;} save_settings();break;
    case 3: flipped=!flipped;force_full=true;save_settings();break;
    case 4: xSemaphoreGive(lock_);sleep_now();return;
    case 5: break; // Recovery requires a deliberate hold, never a tap.
    case 6: menu=false;force_full=true;break;
    }
    xSemaphoreGive(lock_);notify();
}
void input_task(void*) {
    bool was_right=false,was_power=false,fired=false,touched=false;
    int64_t right_down=0,power_down=0,last_battery=0;
    for(;;) {
        int64_t now=esp_timer_get_time();
        bool right=digitalRead(7)==LOW,power=digitalRead(3)==LOW;
        if(right&&!was_right) right_down=now;
        if(!right&&was_right&&now-right_down>=50000) {
            xSemaphoreTake(lock_,portMAX_DELAY);
            if(!menu) {menu=true;selected=0;} else selected=(selected+1)%7;
            xSemaphoreGive(lock_);notify();
        }
        if(power&&!was_power) {power_down=now;fired=false;}
        if(power&&!fired&&now-power_down>=3000000) {
            fired=true;
            xSemaphoreTake(lock_,portMAX_DELAY);bool restore=menu&&selected==5&&recovery;xSemaphoreGive(lock_);
            if(restore) pocket_return_to_crosspoint(); else sleep_now();
        }
        if(!power&&was_power&&!fired&&now-power_down>=50000) {
            xSemaphoreTake(lock_,portMAX_DELAY);bool in_menu=menu;xSemaphoreGive(lock_);
            if(in_menu) activate();
            else {xSemaphoreTake(lock_,portMAX_DELAY);force_full=true;xSemaphoreGive(lock_);notify();}
        }
        uint8_t touch_status=0;
        if(i2c_read(touch,0x814e,&touch_status,1,true)&&(touch_status&0x80)) {
            uint8_t point[8]={};
            bool contact=(touch_status&0x0f)&&i2c_read(touch,0x8150,point,8,true);
            if(contact&&!touched) {
                int x=point[0]|point[1]<<8,y=point[2]|point[3]<<8;
                xSemaphoreTake(lock_,portMAX_DELAY);
                if(flipped) {x=W-1-x;y=H-1-y;}
                bool change=menu&&x>=0&&x<W&&y>=156&&y<660;
                if(change) selected=std::clamp((y-156)/72,0,6);
                else if(y>=740) menu=!menu;
                xSemaphoreGive(lock_);
                if(change) activate();else notify();
            }
            touched=contact;
            uint8_t clear[]={0x81,0x4e,0};i2c_master_transmit(touch,clear,sizeof(clear),50);
        }
        if(now-last_battery>=60000000 || !last_battery) {
            uint8_t soc=0;
            if(i2c_read(gauge,0x04,&soc,1,false)&&soc<=100) {
                xSemaphoreTake(lock_,portMAX_DELAY);battery=soc;xSemaphoreGive(lock_);notify();
            }
            last_battery=now;
        }
        was_right=right;was_power=power;delay(30);
    }
}
} // namespace

extern "C" bool led_status_init(void) {
    lock_=xSemaphoreCreateMutex();image_done=xSemaphoreCreateBinary();
    canvas=static_cast<uint8_t*>(heap_caps_malloc(W*H,MALLOC_CAP_SPIRAM));
    avatar=static_cast<uint8_t*>(heap_caps_malloc(W*AVATAR,MALLOC_CAP_SPIRAM));
    staging=static_cast<uint8_t*>(heap_caps_malloc(W*AVATAR,MALLOC_CAP_SPIRAM));
    frame=static_cast<uint8_t*>(heap_caps_malloc(FRAME,MALLOC_CAP_SPIRAM));
    shown=static_cast<uint8_t*>(heap_caps_malloc(FRAME,MALLOC_CAP_SPIRAM));
    if(!lock_||!image_done||!canvas||!avatar||!staging||!frame||!shown) return false;
    memset(shown,0,FRAME);memset(avatar,255,W*AVATAR);
    gpio_deep_sleep_hold_dis();
    uint8_t version[5],flags;
    auto verdict=freeink::detectXteinkDisplayController(version,&flags);
    bool uc=verdict==freeink::DisplayControllerVerdict::Uc81xxConfirmed;
    if(verdict==freeink::DisplayControllerVerdict::Inconclusive) {
        ESP_LOGE(TAG,"panel identification inconclusive; recovery remains available");return false;
    }
    if(uc) {
        auto v=version[2];BoardConfig::ACTIVE.displayControllerVariant=v;
        bool uc8279=v==2||v==3||v==0x67||v==0x68||v==0x69;
        panel=uc8279?&freeink::uc8279X4Driver():&freeink::uc8179Driver();
        ESP_LOGI(TAG,"controller %s variant %02x",uc8279?"UC8279":"UC8179",v);
    } else {panel=&freeink::ssd1677Driver();ESP_LOGI(TAG,"controller SSD1677");}
    bus.begin({12,11,13,18,14,6},panel->spiHz(),panel->busyPolarity());panel->begin(bus);
    if(!bus.healthy()) return false;
    light_init();peripherals_init();recovery=pocket_recovery_available();
    pinMode(7,INPUT_PULLUP);pinMode(3,INPUT_PULLUP);
    if(xTaskCreate(render_task,"pocket_display",6144,nullptr,3,&renderer)!=pdPASS)return false;
    if(xTaskCreate(input_task,"pocket_input",6144,nullptr,2,nullptr)!=pdPASS)return false;
    xSemaphoreTake(lock_,portMAX_DELAY);initialized=true;xSemaphoreGive(lock_);
    notify();return true;
}
extern "C" bool pocket_local_boot_ready(void) {
    if(!renderer||!lock_)return false;
    xSemaphoreTake(lock_,portMAX_DELAY);
    bool ready=initialized&&recovery&&last_draw_us>0&&bus.healthy();
    xSemaphoreGive(lock_);return ready;
}
extern "C" void led_status_set_state(led_state_t value) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);state=value;
    if(value==LED_STATE_WS_CONNECTED&&!custom_status)
        snprintf(status,sizeof(status),"Ready for your Muse");
    last_status_us=esp_timer_get_time();xSemaphoreGive(lock_);notify();
}
extern "C" void led_status_set_title(const char* value) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);snprintf(title,sizeof(title),"%s",value&&*value?value:"Muse Pocket");last_status_us=esp_timer_get_time();xSemaphoreGive(lock_);notify();
}
extern "C" void pocket_set_status(const char* value) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);custom_status=true;snprintf(status,sizeof(status),"%s",value?value:"");last_status_us=esp_timer_get_time();xSemaphoreGive(lock_);notify();
}
extern "C" void pocket_set_frontlight(int value,int temperature) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);brightness=std::clamp(value,0,100);warmth=std::clamp(temperature,0,100);
    light(brightness,warmth);save_settings();xSemaphoreGive(lock_);notify();
}
// Display commands target the square character canvas, leaving status visible.
extern "C" bool led_status_display_info(int* width,int* height) {
    if(!renderer) return false;
    if(width) *width=W;
    if(height) *height=AVATAR;
    return true;
}
extern "C" int led_status_display_bits(void) {return 1;}
extern "C" bool led_status_draw_rect(int x,int y,int w,int h,const uint16_t* pixels) {
    if(!renderer||!bus.healthy()||!pixels||w<=0||h<=0||x<0||y<0||w>W-x||h>AVATAR-y)return false;
    xSemaphoreTake(lock_,portMAX_DELAY);
    if(!loading){memset(staging,255,W*AVATAR);loading=true;}
    const uint8_t* bytes=reinterpret_cast<const uint8_t*>(pixels);
    for(int row=0;row<h;++row)for(int col=0;col<w;++col){size_t i=(row*w+col)*2;staging[(y+row)*W+x+col]=luma(bytes[i]<<8|bytes[i+1]);}
    xSemaphoreGive(lock_);return true;
}
extern "C" void led_status_draw_done(void) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);std::swap(avatar,staging);custom_avatar=true;loading=false;force_full=true;xSemaphoreGive(lock_);
    await_frame(request_frame());
}
extern "C" void led_status_show_animation(void) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);custom_avatar=false;loading=false;force_full=true;xSemaphoreGive(lock_);notify();
}
extern "C" void led_status_set_voice(led_voice_t) {}
extern "C" void led_status_set_level(float) {}
extern "C" void led_status_show_volume(int) {}

extern "C" void pocket_image_begin(void) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);memset(staging,255,W*AVATAR);loading=true;xSemaphoreGive(lock_);
}
extern "C" void pocket_image_abort(void) {
    if(!renderer)return;
    xSemaphoreTake(lock_,portMAX_DELAY);loading=false;xSemaphoreGive(lock_);
}
extern "C" bool pocket_image_complete(void) {
    if(!renderer)return false;
    xSemaphoreTake(lock_,portMAX_DELAY);std::swap(avatar,staging);custom_avatar=true;loading=false;force_full=true;xSemaphoreGive(lock_);
    return await_frame(request_frame());
}
