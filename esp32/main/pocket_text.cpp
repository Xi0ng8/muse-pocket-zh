// Muse Pocket: bounded decoding and mixed ASCII/CJK layout.
#include "pocket_text.h"
#include <cstring>
#include <climits>

namespace pocket_text {
Decoded decode(const char* text, size_t length) {
    if(!text || !length) return {0,0,false};
    const auto* bytes=reinterpret_cast<const unsigned char*>(text);
    unsigned char first=bytes[0];
    if(first<0x80) return {first,1,true};
    size_t count=first>=0xc0 && first<=0xdf?2:first>=0xe0 && first<=0xef?3:
                 first>=0xf0 && first<=0xf7?4:1;
    size_t consumed=1;
    while(consumed<count && consumed<length && (bytes[consumed]&0xc0)==0x80) ++consumed;
    if(consumed!=count || first<0xc2 || first>0xf4) return {'?',consumed,false};
    uint32_t cp=first & (count==2?0x1f:count==3?0x0f:0x07);
    for(size_t i=1;i<count;++i) cp=(cp<<6)|(bytes[i]&0x3f);
    if((count==2 && cp<0x80) || (count==3 && cp<0x800) || (count==4 && cp<0x10000) ||
       cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return {'?',count,false};
    return {cp,count,true};
}
bool wide(uint32_t cp) {
    return (cp>=0x2010 && cp<=0x2027) || (cp>=0x2e80 && cp<=0xa4cf) || (cp>=0xac00 && cp<=0xd7af) ||
           (cp>=0xf900 && cp<=0xfaff) || (cp>=0xfe30 && cp<=0xfe6f) ||
           (cp>=0xff00 && cp<=0xffef) ||
           (cp>=0x20000 && cp<=0x323af);
}
int advance(uint32_t cp) { return cp=='\n' || cp=='\r'?0:wide(cp)?16:6; }
int width(const char* text, size_t length) {
    int result=0;
    for(size_t pos=0;text && pos<length && text[pos];) {
        auto d=decode(text+pos,length-pos);int next=advance(d.codepoint);
        if(result>INT_MAX-next) return INT_MAX;
        result+=next;pos+=d.bytes;
    }
    return result;
}
size_t truncate(char* destination, size_t capacity, const char* text, size_t length) {
    if(!destination || !capacity) return 0;
    size_t written=0;
    for(size_t pos=0;text && pos<length && text[pos];) {
        auto d=decode(text+pos,length-pos);size_t output=d.valid?d.bytes:1;
        if(output>=capacity-written) break;
        if(d.valid) std::memcpy(destination+written,text+pos,output);
        else destination[written]='?';
        written+=output;pos+=d.bytes;
    }
    destination[written]=0;return written;
}
namespace {
bool opening(uint32_t cp) {
    return cp=='(' || cp=='[' || cp=='{' || cp==0x2018 || cp==0x201c ||
           cp==0x3008 || cp==0x300a || cp==0x300c || cp==0x300e || cp==0x3010 || cp==0xff08;
}
bool closing(uint32_t cp) {
    return cp==',' || cp=='.' || cp=='!' || cp=='?' || cp==':' || cp==';' ||
           cp==')' || cp==']' || cp=='}' || cp==0x2019 || cp==0x201d ||
           cp==0x3001 || cp==0x3002 || cp==0x3009 || cp==0x300b || cp==0x300d ||
           cp==0x300f || cp==0x3011 || cp==0xff01 || cp==0xff08+1 ||
           cp==0xff0c || cp==0xff1a || cp==0xff1b || cp==0xff1f;
}
}
size_t wrap(const char* text,size_t length,int max_width,Line* lines,size_t max_lines) {
    if(!text || !lines || max_width<=0) return 0;
    size_t pos=0,count=0;
    while(pos<length && text[pos] && count<max_lines) {
        while(pos<length && (text[pos]==' ' || text[pos]=='\r')) ++pos;
        if(pos>=length || !text[pos]) break;
        size_t start=pos,end=pos,previous=pos,space=pos;
        uint32_t last=0;int pixels=0;
        while(end<length && text[end] && text[end]!='\n') {
            auto d=decode(text+end,length-end);
            if(advance(d.codepoint)>max_width-pixels) break;
            if(d.codepoint==' ') space=end;
            previous=end;last=d.codepoint;pixels+=advance(d.codepoint);end+=d.bytes;
        }
        bool overflow=end<length && text[end] && text[end]!='\n';
        if(overflow) {
            auto next=decode(text+end,length-end);
            if(space>start) end=space;
            else if(previous>start && (opening(last) || closing(next.codepoint))) end=previous;
            // Progress is required even when a single glyph exceeds the caller's width.
            if(end==start) end=start+decode(text+start,length-start).bytes;
        }
        size_t trimmed=end;
        while(trimmed>start && text[trimmed-1]==' ') --trimmed;
        lines[count++]={start,trimmed};pos=end;
        if(pos<length && text[pos]=='\n') ++pos;
    }
    return count;
}
}
