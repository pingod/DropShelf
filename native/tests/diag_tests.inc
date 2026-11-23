// 临时诊断：概览页的设置卡片静止时会不会自己上下动。

static void diag_hide_getting_started(AppSettings *settings, void *context)
{
    settings->show_getting_started = *(bool *)context;
}

/// 采样 count 帧，返回卡片的上下边和高度有没有变过；变过时打印出来。
static bool diag_sample(const char *label, float width, float height)
{
    float tops[40], bottoms[40], cards[40];
    bool moved = false, animating = false;
    for (int index = 0; index < 40; index++)
    {
        pump_for(16);
        tops[index] = M->tiles[PAGE_ACTIONS]->bounds.top;
        bottoms[index] = M->tiles[PAGE_ACTIONS]->bounds.bottom;
        cards[index] = pages_card_height();
        animating |= ui_window_is_animating(M->window);
        if (index > 0 && (tops[index] != tops[0] || bottoms[index] != bottoms[0] || cards[index] != cards[0])) moved = true;
    }
    if (moved || animating)
    {
        printf("  [抖动] %s %.0fx%.0f moved=%d animating=%d scroll=%.2f:", label, width, height, moved, animating, ui_scroll_extent(M->overview_scroll));
        for (int index = 0; index < 40; index += 2) printf(" %.2f/%.2f/%.0f", tops[index], bottoms[index], cards[index]);
        printf("\n");
    }
    return moved || animating;
}

/// 不处理窗口消息，按 render 的顺序（先 pages_animate，再排版）自己走 frames 帧，窗口大小是给定的小数 DIP。
/// 返回最后 30 帧里卡片有没有变过。
static bool diag_simulate(const char *label, float width, float height, int frames)
{
    double time = frames_time();
    float tops[200], bottoms[200];
    for (int frame = 0; frame < frames && frame < 200; frame++)
    {
        time += 1.0 / 60;
        pages_animate(time);
        ui_measure(M->root, width);
        ui_arrange(M->root, (GfxRect){ 0, 0, width, height });
        tops[frame] = M->tiles[PAGE_ACTIONS]->bounds.top;
        bottoms[frame] = M->tiles[PAGE_ACTIONS]->bounds.bottom;
    }
    bool moved = false;
    for (int frame = frames - 30; frame < frames; frame++)
        if (tops[frame] != tops[frames - 30] || bottoms[frame] != bottoms[frames - 30]) moved = true;
    if (moved)
    {
        printf("  [模拟抖动] %s %.4fx%.4f:", label, width, height);
        for (int frame = frames - 30; frame < frames; frame++) printf(" %.3f/%.3f", tops[frame], bottoms[frame]);
        printf("\n");
    }
    return moved;
}

static void test_card_jitter(void)
{
    language_set_mode(LANGUAGE_CHINESE);
    task_initialize();
    wchar_t *history_file = path_join(temporary_directory, L"history-diag.json");
    DeleteFileW(history_file);
    shelf_history_load(history_file);
    app_update_settings(reset_main_window_settings, NULL);
    main_window_show();
    if (!M->window)
    {
        free(history_file);
        return;
    }
    pump_until_main_idle(300);
    MONITORINFO info = { .cbSize = sizeof(info) };
    GetMonitorInfoW(MonitorFromWindow(ui_window_handle(M->window), MONITOR_DEFAULTTONEAREST), &info);
    float scale = ui_window_scale(M->window);
    float maximum_height = (info.rcWork.bottom - info.rcWork.top) / scale;
    printf("  [诊断] 缩放 %.2f，工作区高 %.0f\n", scale, maximum_height);
    wchar_t *history_folder = test_folder(L"diag-history");
    int jitters = 0, checked = 0;
    for (int state = 0; state < 3; state++)
    {
        const char *label = state == 0 ? "入门+无历史" : state == 1 ? "入门+有历史" : "无入门+有历史";
        if (state == 1)
        {
            for (int index = 0; index < 5; index++)
            {
                wchar_t name[16];
                swprintf(name, 16, L"%d.txt", index);
                wchar_t *path = test_file(history_folder, name, "x");
                ShelfItem *item = shelf_item_new(ITEM_FILE, path, NULL, false);
                shelf_history_record(&item, 1);
                shelf_item_release(item);
                free(path);
            }
            pump_for(200);
        }
        if (state == 2)
        {
            bool show = false;
            app_update_settings(diag_hide_getting_started, &show);
            pump_for(200);
        }
        const float widths[] = { 900, 1300 };
        for (int w = 0; w < 2; w++)
            for (float height = 520; height <= maximum_height; height += 15)
            {
                RECT rect = {0};
                GetWindowRect(ui_window_handle(M->window), &rect);
                ui_window_place(M->window, rect.left, rect.top, widths[w], height);
                pump_for(150);
                pump_until_done(main_window_idle, 1500);
                checked++;
                if (diag_sample(label, widths[w], height)) jitters++;
            }
        // 光标停在卡片上。
        UiElement *tile = M->tiles[PAGE_GENERAL];
        POINT point = { (LONG)((tile->bounds.left + tile->bounds.right) / 2 * scale), (LONG)((tile->bounds.top + tile->bounds.bottom) / 2 * scale) };
        ClientToScreen(ui_window_handle(M->window), &point);
        SetCursorPos(point.x, point.y);
        ui_window_update_hover(M->window);
        pump_for(300);
        checked++;
        if (diag_sample("悬停", 1300, 0)) jitters++;
        SetCursorPos(0, 0);
        ui_window_update_hover(M->window);
        pump_for(300);
    }
    printf("  [诊断] 共 %d 种情况，%d 种在动\n", checked, jitters);

    // 模拟不同缩放比例下的小数窗口大小：每个大小先走 60 帧稳定下来，再看最后 30 帧。
    const float scales[] = { 1.25f, 1.5f, 1.75f, 2.0f };
    int simulated = 0, simulated_moving = 0;
    for (int state = 0; state < 2; state++)
    {
        bool show = state == 1;
        app_update_settings(diag_hide_getting_started, &show);
        for (int s = 0; s < 4; s++)
            for (int pixels = 900; pixels <= 1900; pixels += 5)
            {
                float height = pixels / scales[s];
                float widths_px[] = { 1600, 2400 };
                for (int w = 0; w < (s == 3 ? 2 : 1); w++)
                {
                    char label[64];
                    snprintf(label, sizeof(label), "缩放%.2f %s", scales[s], show ? "入门" : "无入门");
                    simulated++;
                    if (diag_simulate(label, widths_px[w] / scales[s], height, 60)) simulated_moving++;
                }
            }
    }
    printf("  [诊断] 模拟 %d 种，%d 种在动\n", simulated, simulated_moving);
    ui_window_invalidate(M->window);
    pump_for(300);
    CHECK(false, "诊断结束（故意失败，好让 ctest 显示上面的输出）");
    bool show = true;
    app_update_settings(diag_hide_getting_started, &show);
    shelf_history_clear();
    main_window_close();
    pump_for(300);
    free(history_folder);
    free(history_file);
}
