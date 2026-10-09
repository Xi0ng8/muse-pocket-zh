"""Compile production pagination/overlay methods with lock and renderer doubles."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(source, name):
    match = re.search(r"(?:size_t|int|bool|void) " + re.escape(name) + r"\(", source)
    if not match:
        raise AssertionError("production result method is missing: " + name)
    begin = source.index("{", match.start())
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


class ResultUiTest(unittest.TestCase):
    def test_pagination_overlay_and_dismissal(self):
        source = (ROOT / "main/pocket_status.cpp").read_text()
        production = "\n".join(method(source, name) for name in
                               ["result_page", "result_page_count", "pocket_set_call_state",
                                "pocket_show_call_result", "pocket_result_next", "pocket_set_status"])
        with tempfile.TemporaryDirectory() as directory:
            harness = Path(directory) / "result.cpp"
            binary = Path(directory) / "result"
            harness.write_text(PREFIX + production + TESTS)
            subprocess.run([os.getenv("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", "-I", str(ROOT / "main"),
                            str(ROOT / "main/pocket_text.cpp"), str(harness), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


PREFIX = r'''
#include "pocket_text.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <cassert>
constexpr int RESULT_LINES=12, RESULT_WIDTH=216;
char call_caption[241]={},result_text[3073]={};
char status[241]={};bool custom_status=false;
size_t result_length=0,result_offset=0;
int result_index=0,result_total=0;
bool renderer=true,menu=true,force_full=false,result_visible=false,chinese=true;
int lock_=1,portMAX_DELAY=1,lock_depth=0,notifications=0;
int64_t last_status_us=0;
void xSemaphoreTake(int,int){assert(lock_depth==0);++lock_depth;}
void xSemaphoreGive(int){assert(lock_depth==1);--lock_depth;}
int64_t esp_timer_get_time(){return 42;}
void notify(){assert(lock_depth==0);++notifications;}
'''

TESTS = r'''
int main() {
  std::string long_text;for(int i=0;i<1400;++i)long_text+="中";
  pocket_set_call_state("正在呼叫");
  assert(!menu && force_full && std::string(call_caption)=="正在呼叫");
  pocket_set_status("普通状态更新");
  assert(custom_status && std::string(status)=="普通状态更新" && std::string(call_caption)=="正在呼叫");
  pocket_show_call_result(long_text.c_str());
  assert(std::string(call_caption)=="Muse 已返回结果");
  assert(result_visible && result_length==3072 && result_length%3==0 && result_total>1);
  int seen=1;
  while(result_visible) {
    pocket_text::Line rows[RESULT_LINES];size_t next=0;
    size_t n=result_page(result_text,result_length,result_offset,rows,&next);
    assert(n>0 && next>result_offset && next<=result_length);
    for(size_t i=0;i<n;++i) {
      assert(rows[i].begin%3==0 && rows[i].end%3==0);
      assert(pocket_text::width(result_text+result_offset+rows[i].begin,rows[i].end-rows[i].begin)<=RESULT_WIDTH);
    }
    bool last=seen==result_total;
    assert(pocket_result_next());
    if(last)assert(!result_visible && call_caption[0]==0);
    else {++seen;assert(result_visible && result_index==seen-1);}
  }
  assert(!pocket_result_next());
  pocket_set_call_state("连接失败");
  assert(pocket_result_next() && call_caption[0]==0);
  pocket_show_call_result("A\n\nB\nC");
  assert(result_visible && result_total==1);
  pocket_set_call_state("重新呼叫");assert(!result_visible);
  pocket_show_call_result("");assert(!result_visible);
  pocket_set_call_state(nullptr);assert(call_caption[0]==0);
  std::string blanks(100,'\n');
  pocket_show_call_result(blanks.c_str());
  assert(result_total==9);
  for(int i=0;i<9;++i)assert(pocket_result_next());
  assert(!result_visible && lock_depth==0 && notifications>0);
}
'''


if __name__ == "__main__":
    unittest.main()
