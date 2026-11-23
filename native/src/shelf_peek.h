// 观察的文件夹里出现新文件时，从屏幕边缘斜着探出的一张牌：贴着光标那一侧（左或右）的边缘，和光标一样高。
// 牌角印着新文件的张数和花色，牌面是最新那个文件的缩略图。点一下，置物架像一张牌从那条边甩出来，新文件都在里面；
// 光标停在牌上时再往外探一点、暂停计时；没点开时过一会儿缩回。连着出现的新文件合在同一张牌里。
// 全屏游戏、演示或锁屏时先等着，之后再探出。
#pragma once

#include "base.h"
#include "gesture.h"

/// 新文件就绪（folder_watch 交来的）。在界面线程调用。
void shelf_peek_add(const StrList *paths);

/// 点开：伸出放着这些文件的置物架，角收起。没有探出时返回 false。
bool shelf_peek_open(void);

/// 不点开，直接缩回（右键点角时）。
void shelf_peek_dismiss(void);

/// 退出时：关掉窗口，忘掉还没提示的文件。
void shelf_peek_close(void);

/// 角现在是否探出着，里面有几个文件（测试和诊断用）。
bool shelf_peek_is_shown(void);
int shelf_peek_count(void);
ScreenEdge shelf_peek_edge(void);

/// 测试用：不看用户是不是在全屏或锁屏，有新文件就探出（构建机上可能一直算作没人在用）。
void shelf_peek_ignore_busy(bool ignore);

/// 角的窗口放在哪里（屏幕像素）：光标靠左半边就贴左边，否则贴右边；和光标一样高，不超出工作区。
POINT shelf_peek_place(POINT cursor, RECT work, int width, int height, ScreenEdge *edge);
