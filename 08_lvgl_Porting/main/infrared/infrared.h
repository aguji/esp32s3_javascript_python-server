/*
 * 红外传感模块（E18 红外反射式传感器）
 * 用于还书口检测：刷完图书 RFID 后开启，检测到书本投入还书箱后回调并关闭。
 * E18 说明书：有遮挡=低电平，无遮挡=高电平。DOUT 接 GPIO6。
 * 若无反应：1) 看串口日志 GPIO 的 level；2) 若你的模块有遮挡=高电平，在 infrared.c 把 INFRARED_ACTIVE_HIGH 改为 1。
 */
#ifndef INFRARED_H
#define INFRARED_H

#include <stddef.h>

/**
 * 检测到遮挡时的回调（在检测任务中调用，如需更新 UI 请内部加 lvgl_port_lock）
 */
typedef void (*infrared_detected_fn)(void);

/** 初始化 E18 所用 GPIO（GPIO6），在界面创建或首次还书前调用一次 */
void infrared_init(void);

/**
 * 开始检测：轮询 GPIO，当 E18 输出低电平（有遮挡）并稳定后调用 on_detected，然后自动停止。
 * 若已在检测中则忽略。
 */
void infrared_start(infrared_detected_fn on_detected);

/** 停止检测并关闭轮询任务 */
void infrared_stop(void);

/** 当前是否正在检测中 */
int infrared_is_active(void);

/** 非阻塞读取：当前是否检测到遮挡（E18 低电平=1）。用于 LVGL 定时器轮询，无需回调。 */
int infrared_is_detected(void);

/**
 * 自检：确认 E18 GPIO 可读（初始化后读一次电平）。
 * @param err_msg 失败时写入错误信息，可为 NULL
 * @param err_len err_msg 缓冲区长度
 * @return 0 成功，-1 失败
 */
int infrared_selfcheck(char *err_msg, size_t err_len);

/**
 * 获取当前状态字符串，用于在屏幕显示（无串口时调试）。
 * 格式: "L=n T=n" 或 "L=n T=n TRIG"，L=GPIO 电平(0/1)，T=是否在轮询(0/1)，TRIG=最近触发过回调。
 * @param buf 输出缓冲区
 * @param len 缓冲区长度
 */
void infrared_get_status_str(char *buf, size_t len);

#endif
