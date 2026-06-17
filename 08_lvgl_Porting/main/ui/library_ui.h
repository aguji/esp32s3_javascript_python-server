/*
 * 图书馆管理系统主界面入口（800x480）
 */
#ifndef LIBRARY_UI_H
#define LIBRARY_UI_H

/** 创建并显示图书馆管理界面（主屏、登录、主菜单、检索、地图等） */
void library_ui_create(void);

/** 清理 UI 资源（系统在关机时调用） */
void library_ui_destroy(void);

#endif
