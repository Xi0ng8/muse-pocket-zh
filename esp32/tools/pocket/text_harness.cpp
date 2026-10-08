#include "pocket_text.h"
#include <cassert>
#include <cstring>
#include <string>

int main() {
    using namespace pocket_text;
    auto chinese=decode("中",3);
    assert(chinese.valid && chinese.codepoint==0x4e2d && chinese.bytes==3);
    const char truncated[]={char(0xe4),char(0xb8)};
    assert(!decode(truncated,2).valid && decode(truncated,2).bytes==2);
    const char invalid[]={char(0xc0),char(0xaf)};
    assert(!decode(invalid,2).valid);
    const char surrogate[]={char(0xed),char(0xa0),char(0x80)};
    assert(!decode(surrogate,3).valid);
    assert(decode("",0).bytes==0);
    assert(width("A中😀B",9)==34); // Unknown emoji is one six-pixel fallback.
    assert(width("你好，世界",15)==80);
    assert(width("“你好”—…",18)==96);
    assert(advance(0xff66)==16); // The bundled fullwidth region uses fixed 16px glyphs.
    char name[64];
    std::string long_name(60,'a');long_name+="中文";
    truncate(name,sizeof(name),long_name.data(),long_name.size());
    assert(std::strlen(name)==63 && std::string(name)==std::string(60,'a')+"中");
    char tiny[3];truncate(tiny,sizeof(tiny),"中文",6);assert(tiny[0]==0);
    char repaired[12];truncate(repaired,sizeof(repaired),truncated,2);assert(std::string(repaired)=="?");
    Line rows[8];
    const char* mixed="hello 世界 again";
    auto n=wrap(mixed,std::strlen(mixed),48,rows,8);
    assert(n==3);
    assert(std::string(mixed+rows[0].begin,rows[0].end-rows[0].begin)=="hello");
    assert(std::string(mixed+rows[1].begin,rows[1].end-rows[1].begin)=="世界");
    const char* punctuation="你好，世界。";
    n=wrap(punctuation,std::strlen(punctuation),48,rows,8);
    assert(n==2 && rows[0].end==9 && rows[1].begin==9);
    const char* closing="你好，世界";
    n=wrap(closing,std::strlen(closing),32,rows,8);
    assert(n==3 && std::string(closing+rows[1].begin,rows[1].end-rows[1].begin)=="好，");
    const char* opening="你好（世界）";
    n=wrap(opening,std::strlen(opening),48,rows,8);
    assert(n==3 && rows[0].end==6); // Opening/closing pairs need three bounded lines.
    const char* quotes="“你好”";
    n=wrap(quotes,std::strlen(quotes),48,rows,8);
    assert(n==2 && rows[0].end==6 && rows[1].begin==6);
    const char* explicit_lines="A\n\n中\nB";
    n=wrap(explicit_lines,std::strlen(explicit_lines),48,rows,8);
    assert(n==4 && rows[1].begin==rows[1].end);
    const char* long_word="abcdefghijk";
    n=wrap(long_word,std::strlen(long_word),24,rows,8);
    assert(n==3 && rows[0].end==4 && rows[1].end==8);
    std::string caption;for(int i=0;i<80;++i)caption+="中";
    n=wrap(caption.data(),caption.size(),210,rows,4);
    assert(n==4);
    for(size_t i=0;i<n;++i) {
        assert(width(caption.data()+rows[i].begin,rows[i].end-rows[i].begin)<=210);
        assert(rows[i].begin%3==0 && rows[i].end%3==0);
    }
    // Bounded decoding may inspect only the supplied bytes, even without NUL.
    char* bounded=new char[1];bounded[0]=char(0xf0);
    assert(!decode(bounded,1).valid);delete[] bounded;
    return 0;
}
