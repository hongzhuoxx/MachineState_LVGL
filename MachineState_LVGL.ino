#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"


/* 屏幕分辨率 */
static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* 创建 TFT 实例 */

/* 显示刷新函数 */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp_drv);
}

/* 增加 LVGL 时间刻度 */
void increase_lvgl_tick(void *arg) {
  lv_tick_inc(2);  // 每 2 毫秒增加一次 LVGL 内部时间
}

void custom_arc_draw_cb_mem(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  // 只处理指示器头部部分
  if (dsc->part == LV_PART_INDICATOR && dsc->type == LV_ARC_DRAW_PART_KNOB) {
    dsc->arc_dsc->color = lv_palette_main(LV_PALETTE_GREEN);  // 设置头部颜色
    // dsc->draw_area->x1 -= 5;  // 增加宽度，使弧形整体变宽，类似增大直径效果
    // dsc->draw_area->y1 -= 5;  // 增加高度，保持绘制区域的合理形状，同样类似增大直径效果
    // dsc->draw_area->x2 -= 5;  // 增加宽度，使弧形整体变宽，类似增大直径效果
    // dsc->draw_area->y2 -= 5;  // 增加高度，保持绘制区域的合理形状，同样类似增大直径效果
  }
}

/* 定义全局变量 */
static lv_obj_t *label_cpu;   // 显示CPU占用
static lv_obj_t *label_mem;   // 显示内存占用
static lv_obj_t *label_tmp;   // 显示温度
static lv_obj_t *arc_cpu;     // 圆环进度条
static lv_obj_t *arc_mem;     // 圆环进度条
static int progress_cpu = 0;  // 当前进度值
static int progress_mem = 0;  // 当前进度值
static int progress_tmp = 0;  // 当前进度值


/* 更新进度条和文字 */
void update_ui(int v_cpu, int v_mem, int v_tmp) {
  char buf_cpu[16];
  sprintf(buf_cpu, "CPU: %d%%", v_cpu);
  lv_label_set_text(label_cpu, buf_cpu);            // 更新文字
  lv_obj_align(label_cpu, LV_ALIGN_CENTER, 0, 90);  // 保持文字位置

  char buf_mem[16];
  sprintf(buf_mem, "MEM: %d%%", v_mem);
  lv_label_set_text(label_mem, buf_mem);            // 更新文字
  lv_obj_align(label_mem, LV_ALIGN_CENTER, 0, 60);  // 保持文字位置

  char buf_tmp[16];
  sprintf(buf_tmp, "%d°", v_tmp);
  lv_label_set_text(label_tmp, buf_tmp);           // 更新文字
  lv_obj_align(label_tmp, LV_ALIGN_CENTER, 0, 0);  // 保持文字位置

  lv_arc_set_value(arc_cpu, v_cpu);  // 更新圆环进度条
  lv_arc_set_value(arc_mem, v_mem);  // 更新圆环进度条
}

/* 串口读取和更新 */
void read_serial_input() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');  // 读取一行串口输入
    input.trim();                                 // 去掉可能的前后空白字符
    Serial.println("Received: " + input);

    // cpu 内存 温度
    int values[3] = { 0, 0, 0 };  // 存储三个整数值
    int index = 0;

    // 使用逗号分隔输入并解析数字
    for (int i = 0; i < input.length(); i++) {
      if (input[i] == ',' || i == input.length() - 1) {
        // 最后一个数字
        if (i == input.length() - 1) values[index] = values[index] * 10 + (input[i] - '0');

        // 检查是否超出范围
        if (index > 2) {
          Serial.println("Invalid input! Too many values.");
          return;
        }
        index++;
      } else if (isdigit(input[i])) {
        values[index] = values[index] * 10 + (input[i] - '0');
      } else {
        Serial.println("Invalid input! Non-numeric character detected.");
        return;
      }
    }

    // 更新 UI
    progress_cpu = values[0];
    progress_mem = values[1];
    progress_tmp = values[2];
    update_ui(progress_cpu, progress_mem, progress_tmp);  // 更新 UI
    Serial.printf("Updated values: %d, %d, %d\n", progress_cpu, progress_mem, progress_tmp);
  }
}

void setup() {
  Serial.begin(115200);
  lv_init();  // 初始化 LVGL

  /* 初始化屏幕 */
  tft.begin();
  tft.setRotation(2);

  /* 初始化显示缓冲区 */
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

  /* 注册显示驱动 */
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // 设置屏幕背景色为黑色，获取当前屏幕对象指针
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_make(0,0,0), LV_PART_MAIN);
  // =======================================================================================
  // ============================ CPU TEXT

  /* 创建并初始化样式 */
  static lv_style_t style_label_cpu;
  lv_style_init(&style_label_cpu);

  /* 设置文字颜色为蓝色 */
  lv_style_set_text_color(&style_label_cpu, lv_color_make(0, 0, 200));

  /* 创建一个标签用于显示文字 */
  label_cpu = lv_label_create(lv_scr_act());
  lv_label_set_text(label_cpu, "CPU: 0%");
  lv_obj_align(label_cpu, LV_ALIGN_CENTER, 0, 90);
  /* 应用样式到标签 */
  lv_obj_add_style(label_cpu, &style_label_cpu, 0);

  // =======================================================================================
  // ============================ MEM TEXT

  /* 创建并初始化样式 */
  static lv_style_t style_label_mem;
  lv_style_init(&style_label_mem);

  /* 设置文字颜色为蓝色 */
  lv_style_set_text_color(&style_label_mem, lv_color_make(0, 200, 0));

  /* 创建一个标签用于显示文字 */
  label_mem = lv_label_create(lv_scr_act());
  lv_label_set_text(label_mem, "CPU: 0%");
  lv_obj_align(label_mem, LV_ALIGN_CENTER, 0, 60);
  /* 应用样式到标签 */
  lv_obj_add_style(label_mem, &style_label_mem, 0);

  // =======================================================================================
  // ============================ TMP TEXT

  /* 创建并初始化样式 */
  static lv_style_t style_label_tmp;
  lv_style_init(&style_label_tmp);

  /* 设置文字颜色为蓝色 */
  lv_style_set_text_color(&style_label_tmp, lv_color_make(255, 165, 0));

  /* 创建一个标签用于显示文字 */
  label_tmp = lv_label_create(lv_scr_act());
  lv_label_set_text(label_tmp, "n°");
  lv_obj_align(label_tmp, LV_ALIGN_CENTER, 0, 0);


  lv_style_set_text_font(&style_label_tmp, &lv_font_montserrat_46);
  /* 应用样式到标签 */
  lv_obj_add_style(label_tmp, &style_label_tmp, 0);

  // =======================================================================================
  // ============================ CPU

  // 初始化样式
  static lv_style_t style_arc_bg_cpu;   // 背景样式
  static lv_style_t style_arc_ind_cpu;  // 指示器样式

  lv_style_init(&style_arc_bg_cpu);
  lv_style_set_arc_width(&style_arc_bg_cpu, 20);                                // 背景宽度
  lv_style_set_arc_color(&style_arc_bg_cpu, lv_palette_main(LV_PALETTE_GREY));  // 背景颜色

  lv_style_init(&style_arc_ind_cpu);
  lv_style_set_arc_width(&style_arc_ind_cpu, 20);                                // 指示器宽度
  lv_style_set_arc_color(&style_arc_ind_cpu, lv_palette_main(LV_PALETTE_BLUE));  // 指示器颜色

  /* 创建圆环进度条 */
  arc_cpu = lv_arc_create(lv_scr_act());
  lv_obj_set_size(arc_cpu, 230, 230);            // 设置圆环大小
  lv_obj_align(arc_cpu, LV_ALIGN_CENTER, 0, 0);  // 圆环放在屏幕中心
  lv_arc_set_range(arc_cpu, 0, 100);             // 进度条范围 0-100
  lv_arc_set_value(arc_cpu, 0);                  // 初始值为 0

  // 应用样式
  lv_obj_add_style(arc_cpu, &style_arc_bg_cpu, LV_PART_MAIN);        // 背景样式
  lv_obj_add_style(arc_cpu, &style_arc_ind_cpu, LV_PART_INDICATOR);  // 指示器样式

  // =======================================================================================
  // =======================================================================================
  // ============================ MEM
  /* 创建圆环进度条 */
  arc_mem = lv_arc_create(lv_scr_act());
  lv_obj_set_size(arc_mem, 180, 180);            // 设置圆环大小
  lv_obj_align(arc_mem, LV_ALIGN_CENTER, 0, 0);  // 圆环放在屏幕中心
  lv_arc_set_range(arc_mem, 0, 100);             // 进度条范围 0-100
  lv_arc_set_value(arc_mem, 0);                  // 初始值为 0

  // 初始化样式
  static lv_style_t style_arc_bg_mem;   // 背景样式
  static lv_style_t style_arc_ind_mem;  // 指示器样式

  lv_style_init(&style_arc_bg_mem);
  lv_style_set_arc_width(&style_arc_bg_mem, 20);                                // 背景宽度
  lv_style_set_arc_color(&style_arc_bg_mem, lv_palette_main(LV_PALETTE_GREY));  // 背景颜色

  lv_style_init(&style_arc_ind_mem);
  lv_style_set_arc_width(&style_arc_ind_mem, 20);                                 // 指示器宽度
  lv_style_set_arc_color(&style_arc_ind_mem, lv_palette_main(LV_PALETTE_GREEN));  // 指示器颜色
  lv_style_set_bg_grad_color(&style_arc_ind_mem, lv_palette_main(LV_PALETTE_GREEN));

  static lv_style_t style_knob;
  lv_style_init(&style_knob);
  lv_style_set_bg_color(&style_knob, lv_palette_main(LV_PALETTE_GREEN));  // 设置 Knob 的颜色
  lv_style_set_bg_opa(&style_knob, LV_OPA_COVER);                         // 设置不透明度

  // 将样式应用到 Knob 部分
  lv_obj_add_style(arc_mem, &style_knob, LV_PART_KNOB);


  // 应用样式
  lv_obj_add_style(arc_mem, &style_arc_bg_mem, LV_PART_MAIN);        // 背景样式
  lv_obj_add_style(arc_mem, &style_arc_ind_mem, LV_PART_INDICATOR);  // 指示器样式
  // 添加绘制回调
  lv_obj_add_event_cb(arc_mem, custom_arc_draw_cb_mem, LV_EVENT_DRAW_PART_BEGIN, NULL);

  // =======================================================================================

  /* 创建 ESP 定时器，周期性调用 increase_lvgl_tick */
  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &increase_lvgl_tick,
    .name = "lvgl_tick"
  };

  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, 1000);  // 每 2 毫秒调用一次
}

void loop() {
  lv_timer_handler();   // 处理 LVGL 的任务
  read_serial_input();  // 检查串口输入
  delay(5);             // 短延时
}