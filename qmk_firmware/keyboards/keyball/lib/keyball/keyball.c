/*
Copyright 2022 MURAOKA Taro (aka KoRoN)

このプログラムはフリーソフトウェアです。GNU一般公衆利用許諾契約書の第2版、
またはそれ以降のバージョンの条件の下で再配布や改変が可能です。

このプログラムは有用であることを願って提供されていますが、
商品性や特定目的への適合性についての明示的または黙示的な保証はありません。
詳細についてはGNU一般公衆利用許諾契約書を参照してください。

このプログラムのコピーは、GNUのウェブサイト<http://www.gnu.org/licenses/>から入手できます。
*/

#include "quantum.h"
#ifdef SPLIT_KEYBOARD
#include "transactions.h"
#endif

#include "keyball.h"
#include "drivers/pmw3360/pmw3360.h"

#include <string.h>

// デフォルトのCPI値と最大CPI値
const uint8_t CPI_DEFAULT = KEYBALL_CPI_DEFAULT / 100;
const uint8_t CPI_MAX = pmw3360_MAXCPI + 1;
const uint8_t SCROLL_DIV_MAX = 7;

// オートマウスレイヤーのタイムアウト設定
const uint16_t AML_TIMEOUT_MIN = 100;
const uint16_t AML_TIMEOUT_MAX = 1000;
const uint16_t AML_TIMEOUT_QU = 50; // 量子単位

static const char BL = '\xB0';                      // 空白表示文字
static const char LFSTR_ON[] PROGMEM = "\xB2\xB3";  // "ON"表示
static const char LFSTR_OFF[] PROGMEM = "\xB4\xB5"; // "OFF"表示

// Keyballの初期化
keyball_t keyball = {
    .this_have_ball = false,
    .that_enable = false,
    .that_have_ball = false,

    .this_motion = {0},
    .that_motion = {0},

    .cpi_value = 0,
    .cpi_changed = false,

    .scroll_mode = false,
    .scroll_div = 0,

#if KEYBALL_SCROLLSNAP_ENABLE == 2
    .scrollsnap_mode = KEYBALL_SCROLLSNAP_MODE_VERTICAL, // デフォルトを垂直に設定
#endif

    .last_kc = 0,
    .last_pos = {0, 0},
    .last_mouse = {0},

    .pressing_keys = {BL, BL, BL, BL, BL, BL, 0},
};

////////////////////////////////////////////////////////////////////////////////
// フックポイント

__attribute__((weak)) void keyball_on_adjust_layout(keyball_adjust_t v) {}

////////////////////////////////////////////////////////////////////////////////
// 静的ユーティリティ関数

// add16はint16_tをクリッピングして加算します。
static int16_t add16(int16_t a, int16_t b)
{
    int16_t r = a + b;
    if (a >= 0 && b >= 0 && r < 0)
    {
        r = 32767;
    }
    else if (a < 0 && b < 0 && r >= 0)
    {
        r = -32768;
    }
    return r;
}

// divmod16は*vをdivで割り、商を返し、余りを*vに代入します。
static int16_t divmod16(int16_t *v, int16_t div)
{
    int16_t r = *v / div;
    *v -= r * div;
    return r;
}

// clip2int8はint16_tをint8_tにクリップします。
static inline int8_t clip2int8(int16_t v)
{
    return (v) < -127 ? -127 : (v) > 127 ? 127
                                         : (int8_t)v;
}

#ifdef OLED_ENABLE
// 4桁の整数をフォーマットします。
static const char *format_4d(int8_t d)
{
    static char buf[5] = {0}; // 最大幅 (4) + NUL (1)
    char lead = ' ';
    if (d < 0)
    {
        d = -d;
        lead = '-';
    }
    buf[3] = (d % 10) + '0';
    d /= 10;
    if (d == 0)
    {
        buf[2] = lead;
        lead = ' ';
    }
    else
    {
        buf[2] = (d % 10) + '0';
        d /= 10;
    }
    if (d == 0)
    {
        buf[1] = lead;
        lead = ' ';
    }
    else
    {
        buf[1] = (d % 10) + '0';
        d /= 10;
    }
    buf[0] = lead;
    return buf;
}

// 1桁の16進数を文字に変換します。
static char to_1x(uint8_t x)
{
    x &= 0x0f;
    return x < 10 ? x + '0' : x + 'a' - 10;
}
#endif

// CPI値を増減させる関数
static void add_cpi(int8_t delta)
{
    int16_t v = keyball_get_cpi() + delta;
    keyball_set_cpi(v < 1 ? 1 : v);
}

// スクロール除数を増減させる関数
static void add_scroll_div(int8_t delta)
{
    int8_t v = keyball_get_scroll_div() + delta;
    keyball_set_scroll_div(v < 1 ? 1 : v);
}

////////////////////////////////////////////////////////////////////////////////
// ポインティングデバイスドライバー

#if KEYBALL_MODEL == 46
void keyboard_pre_init_kb(void)
{
    keyball.this_have_ball = pmw3360_init();
    keyboard_pre_init_user();
}
#endif

void pointing_device_driver_init(void)
{
#if KEYBALL_MODEL != 46
    keyball.this_have_ball = pmw3360_init();
#endif
    if (keyball.this_have_ball)
    {
#if defined(KEYBALL_PMW3360_UPLOAD_SROM_ID)
#if KEYBALL_PMW3360_UPLOAD_SROM_ID == 0x04
        pmw3360_srom_upload(pmw3360_srom_0x04);
#elif KEYBALL_PMW3360_UPLOAD_SROM_ID == 0x81
        pmw3360_srom_upload(pmw3360_srom_0x81);
#else
#error Invalid value for KEYBALL_PMW3360_UPLOAD_SROM_ID. Please choose 0x04 or 0x81 or disable it.
#endif
#endif
        pmw3360_cpi_set(CPI_DEFAULT - 1);
    }
}

uint16_t pointing_device_driver_get_cpi(void)
{
    return keyball_get_cpi();
}

void pointing_device_driver_set_cpi(uint16_t cpi)
{
    keyball_set_cpi(cpi);
}

__attribute__((weak)) void keyball_on_apply_motion_to_mouse_move(keyball_motion_t *m, report_mouse_t *r, bool is_left)
{
#if KEYBALL_MODEL == 61 || KEYBALL_MODEL == 39 || KEYBALL_MODEL == 147 || KEYBALL_MODEL == 44
    r->x = clip2int8(m->y);
    r->y = clip2int8(m->x);
    if (is_left)
    {
        r->x = -r->x;
        r->y = -r->y;
    }
#elif KEYBALL_MODEL == 46
    r->x = clip2int8(m->x);
    r->y = -clip2int8(m->y);
#else
#error "unknown Keyball model"
#endif
    // 動きをクリア
    m->x = 0;
    m->y = 0;
}

// 関数の定義を外に移動
static void motion_to_mouse(keyball_motion_t *m, report_mouse_t *r, bool is_left, bool as_scroll)
{
    if (as_scroll)
    {
        keyball_on_apply_motion_to_mouse_scroll(m, r, is_left);
    }
    else
    {
        keyball_on_apply_motion_to_mouse_move(m, r, is_left);
    }
}

static inline bool should_report(void)
{
    uint32_t now = timer_read32();
#if defined(KEYBALL_REPORTMOUSE_INTERVAL) && KEYBALL_REPORTMOUSE_INTERVAL > 0
    // マウスレポートレートをスロットリング
    static uint32_t last = 0;
    if (TIMER_DIFF_32(now, last) < KEYBALL_REPORTMOUSE_INTERVAL)
    {
        return false;
    }
    last = now;
#endif
#if defined(KEYBALL_SCROLLBALL_INHIVITOR) && KEYBALL_SCROLLBALL_INHIVITOR > 0
    if (TIMER_DIFF_32(now, keyball.scroll_mode_changed) < KEYBALL_SCROLLBALL_INHIVITOR)
    {
        keyball.this_motion.x = 0;
        keyball.this_motion.y = 0;
        keyball.that_motion.x = 0;
        keyball.that_motion.y = 0;
    }
#endif
    return true;
}

__attribute__((weak)) void keyball_on_apply_motion_to_mouse_scroll(keyball_motion_t *m, report_mouse_t *r, bool is_left)
{
    uint32_t now = timer_read32(); // 'now' を定義

    // トラックボールの動きを処理する
    int16_t div = 1 << (keyball_get_scroll_div() - 1);
    int16_t x = divmod16(&m->x, div);
    int16_t y = divmod16(&m->y, div);

    // 特定のレイヤーでズーム機能を有効化
    bool zoom_mode = layer_state_is(KEYBALL_ZOOM_LAYER);

    if (zoom_mode)
    {
        // ズームイン・ズームアウトの処理
        if (y > 0)
        {
            // 一律でCtrl + '+' を送信
            register_code(KC_LCTL);
            register_code(KC_EQUAL);
            unregister_code(KC_EQUAL);
            unregister_code(KC_LCTL);
        }
        else if (y < 0)
        {
            // 一律でCtrl + '-' を送信
            register_code(KC_LCTL);
            register_code(KC_MINUS);
            unregister_code(KC_MINUS);
            unregister_code(KC_LCTL);
        }
    }
    else
    {
        // 通常のスクロール処理
#if KEYBALL_MODEL == 61 || KEYBALL_MODEL == 39 || KEYBALL_MODEL == 147 || KEYBALL_MODEL == 44
        r->h = clip2int8(y);  // Y方向の動きを水平方向に適用
        r->v = -clip2int8(x); // X方向の動きを垂直方向に適用（方向を反転）
        if (is_left)
        { // 左手用デバイスの場合、方向をさらに反転
            r->h = -r->h;
            r->v = -r->v;
        }
#elif KEYBALL_MODEL == 46
        r->h = clip2int8(x); // X方向の動きを水平方向に適用
        r->v = clip2int8(y); // Y方向の動きを垂直方向に適用
#else
#error "unknown Keyball model" // 未知のKeyballモデルの場合、コンパイルエラーを出力
#endif

        // スクロールスナップ機能を適用する（スクロールの引っ掛かり効果を追加）
#if KEYBALL_SCROLLSNAP_ENABLE == 1
        // 旧バージョンのスナップ機能（バージョン1.3.2まで）
        if (r->h != 0 || r->v != 0)
        {                                   // マウスレポートに動きがある場合
            keyball.scroll_snap_last = now; // 最後のスナップタイムを更新
        }
        else if (TIMER_DIFF_32(now, keyball.scroll_snap_last) >= KEYBALL_SCROLLSNAP_RESET_TIMER)
        {
            keyball.scroll_snap_tension_h = 0; // 一定時間動きがない場合、張力をリセット
        }
        if (abs(keyball.scroll_snap_tension_h) < KEYBALL_SCROLLSNAP_TENSION_THRESHOLD)
        {
            keyball.scroll_snap_tension_h += y; // 張力を増加させて引っ掛かり効果を再現
            r->h = 0;                           // スクロール方向は固定
        }
#elif KEYBALL_SCROLLSNAP_ENABLE == 2
        // 新バージョンのスナップ機能
        switch (keyball_get_scrollsnap_mode())
        {
        case KEYBALL_SCROLLSNAP_MODE_VERTICAL:
            r->h = 0; // 水平方向の動きを無効化（縦方向のみにスクロール）
            break;
        case KEYBALL_SCROLLSNAP_MODE_HORIZONTAL:
            r->v = 0; // 垂直方向の動きを無効化（横方向のみにスクロール）
            break;
        default:
            // 何もしない
            break;
        }
#endif

        // スクロール方向を反転
        r->h = -r->h;
        r->v = -r->v;
    }

#if defined(KEYBALL_SCROLLBALL_INHIVITOR) && KEYBALL_SCROLLBALL_INHIVITOR > 0
    if (TIMER_DIFF_32(now, keyball.scroll_mode_changed) < KEYBALL_SCROLLBALL_INHIVITOR)
    {
        keyball.this_motion.x = 0;
        keyball.this_motion.y = 0;
        keyball.that_motion.x = 0;
        keyball.that_motion.y = 0;
    }
#endif
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t rep)
{
    // 光学センサーからデータを取得
    if (keyball.this_have_ball)
    {
        pmw3360_motion_t d = {0};
        if (pmw3360_motion_burst(&d))
        {
            ATOMIC_BLOCK_FORCEON
            {
                keyball.this_motion.x = add16(keyball.this_motion.x, d.x);
                keyball.this_motion.y = add16(keyball.this_motion.y, d.y);
            }
        }
    }
    // キーボードがマスターの場合、マウスイベントを報告
    if (is_keyboard_master() && should_report())
    {
        // PMW3360の動きに基づいてマウスレポートを修正
        motion_to_mouse(&keyball.this_motion, &rep, is_keyboard_left(), keyball.scroll_mode);
        motion_to_mouse(&keyball.that_motion, &rep, !is_keyboard_left(), keyball.scroll_mode ^ keyball.this_have_ball);
        // OLED用にマウスレポートを保存
        keyball.last_mouse = rep;
    }
    return rep;
}

////////////////////////////////////////////////////////////////////////////////
// スプリットRPC

#ifdef SPLIT_KEYBOARD

static void rpc_get_info_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data)
{
    keyball_info_t info = {
        .ballcnt = keyball.this_have_ball ? 1 : 0,
    };
    *(keyball_info_t *)out_data = info;
    keyball_on_adjust_layout(KEYBALL_ADJUST_SECONDARY);
}

static void rpc_get_info_invoke(void)
{
    static bool negotiated = false;
    static uint32_t last_sync = 0;
    static int round = 0;
    uint32_t now = timer_read32();
    if (negotiated || TIMER_DIFF_32(now, last_sync) < KEYBALL_TX_GETINFO_INTERVAL)
    {
        return;
    }
    last_sync = now;
    round++;
    keyball_info_t recv = {0};
    if (!transaction_rpc_exec(KEYBALL_GET_INFO, 0, NULL, sizeof(recv), &recv))
    {
        if (round < KEYBALL_TX_GETINFO_MAXTRY)
        {
            dprintf("keyball:rpc_get_info_invoke: missed #%d\n", round);
            return;
        }
    }
    negotiated = true;
    keyball.that_enable = true;
    keyball.that_have_ball = recv.ballcnt > 0;
    dprintf("keyball:rpc_get_info_invoke: negotiated #%d %d\n", round, keyball.that_have_ball);

    // スプリットキーボードの交渉が完了

#ifdef VIA_ENABLE
    // 現在の組み合わせに応じてVIAレイアウトオプションを調整
    uint8_t layouts = (keyball.this_have_ball ? (is_keyboard_left() ? 0x02 : 0x01) : 0x00) | (keyball.that_have_ball ? (is_keyboard_left() ? 0x01 : 0x02) : 0x00);
    uint32_t curr = via_get_layout_options();
    uint32_t next = (curr & ~0x3) | layouts;
    if (next != curr)
    {
        via_set_layout_options(next);
    }
#endif

    keyball_on_adjust_layout(KEYBALL_ADJUST_PRIMARY);
}

static void rpc_get_motion_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data)
{
    *(keyball_motion_t *)out_data = keyball.this_motion;
    // 動きをクリア
    keyball.this_motion.x = 0;
    keyball.this_motion.y = 0;
}

static void rpc_get_motion_invoke(void)
{
    static uint32_t last_sync = 0;
    uint32_t now = timer_read32();
    if (TIMER_DIFF_32(now, last_sync) < KEYBALL_TX_GETMOTION_INTERVAL)
    {
        return;
    }
    keyball_motion_t recv = {0};
    if (transaction_rpc_exec(KEYBALL_GET_MOTION, 0, NULL, sizeof(recv), &recv))
    {
        keyball.that_motion.x = add16(keyball.that_motion.x, recv.x);
        keyball.that_motion.y = add16(keyball.that_motion.y, recv.y);
    }
    last_sync = now;
    return;
}

static void rpc_set_cpi_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data)
{
    keyball_set_cpi(*(keyball_cpi_t *)in_data);
}

static void rpc_set_cpi_invoke(void)
{
    if (!keyball.cpi_changed)
    {
        return;
    }
    keyball_cpi_t req = keyball.cpi_value;
    if (!transaction_rpc_send(KEYBALL_SET_CPI, sizeof(req), &req))
    {
        return;
    }
    keyball.cpi_changed = false;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// OLEDユーティリティ

#ifdef OLED_ENABLE
// キーコードから名前への変換テーブル
const char PROGMEM code_to_name[] = {
    'a', 'b', 'c', 'd', 'e', 'f',  'g', 'h', 'i',  'j',
    'k', 'l', 'm', 'n', 'o', 'p',  'q', 'r', 's',  't',
    'u', 'v', 'w', 'x', 'y', 'z',  '1', '2', '3',  '4',
    '5', '6', '7', '8', '9', '0',  'R', 'E', 'B',  'T',
    '_', '-', '=', '[', ']', '\\', '#', ';', '\'', '`',
    ',', '.', '/',
};
#endif

void keyball_oled_render_ballinfo(void)
{
#ifdef OLED_ENABLE
    // フォーマット: `Ball:{mouse x}{mouse y}{mouse h}{mouse v}`
    //
    // 出力例:
    //
    //     Ball: -12  34   0   0

    // 1行目: "Ball"ラベル、マウスx, y, h, v
    oled_write_P(PSTR("Ball\xB1"), false);
    oled_write(format_4d(keyball.last_mouse.x), false);
    oled_write(format_4d(keyball.last_mouse.y), false);
    oled_write(format_4d(keyball.last_mouse.h), false);
    oled_write(format_4d(keyball.last_mouse.v), false);

    // 2行目: 空白ラベルとCPI
    oled_write_P(PSTR("    \xB1\xBC\xBD"), false);
    oled_write(format_4d(keyball_get_cpi()) + 1, false);
    oled_write_P(PSTR("00 "), false);

    // スクロールスナップモードを表示: "VT" (垂直), "HO" (水平), "SCR" (自由)
#if KEYBALL_SCROLLSNAP_ENABLE == 2
    switch (keyball_get_scrollsnap_mode())
    {
    case KEYBALL_SCROLLSNAP_MODE_VERTICAL:
        oled_write_P(PSTR("VT"), false);
        break;
    case KEYBALL_SCROLLSNAP_MODE_HORIZONTAL:
        oled_write_P(PSTR("HO"), false);
        break;
    default:
        oled_write_P(PSTR("\xBE\xBF"), false);
        break;
    }
#else
    oled_write_P(PSTR("\xBE\xBF"), false);
#endif
    // スクロールモードの表示: ON/OFF
    if (keyball.scroll_mode)
    {
        oled_write_P(LFSTR_ON, false);
    }
    else
    {
        oled_write_P(LFSTR_OFF, false);
    }

    // スクロール除数の表示:
    oled_write_P(PSTR(" \xC0\xC1"), false);
    oled_write_char('0' + keyball_get_scroll_div(), false);
#endif
}

void keyball_oled_render_ballsubinfo(void)
{
#ifdef OLED_ENABLE
    // 追加のボール情報が必要な場合に実装
#endif
}

void keyball_oled_render_keyinfo(void)
{
#ifdef OLED_ENABLE
    // フォーマット: `Key :  R{row}  C{col} K{kc} {name}{name}{name}`
    //
    // `kc` はキーコードの下位8ビット。
    // `name` は押下中のキーのラベル。
    //
    // 出力例:
    //
    //     Key :  R2  C3 K06 abc
    //     Ball:   0   0   0   0

    // "Key"ラベル
    oled_write_P(PSTR("Key \xB1"), false);

    // 行と列
    oled_write_char('\xB8', false);
    oled_write_char(to_1x(keyball.last_pos.row), false);
    oled_write_char('\xB9', false);
    oled_write_char(to_1x(keyball.last_pos.col), false);

    // キーコード
    oled_write_P(PSTR("\xBA\xBB"), false);
    oled_write_char(to_1x(keyball.last_kc >> 4), false);
    oled_write_char(to_1x(keyball.last_kc), false);

    // 押下中のキー
    oled_write_P(PSTR("  "), false);
    oled_write(keyball.pressing_keys, false);
#endif
}

void keyball_oled_render_layerinfo(void)
{
#ifdef OLED_ENABLE
    // フォーマット: `Layer:{layer state}`
    //
    // 出力例:
    //
    //     Layer:-23------------

    oled_write_P(PSTR("L\xB6\xB7r\xB1"), false);
    for (uint8_t i = 1; i < 8; i++)
    {
        oled_write_char((layer_state_is(i) ? to_1x(i) : BL), false);
    }
    oled_write_char(' ', false);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    oled_write_P(PSTR("\xC2\xC3"), false);
    if (get_auto_mouse_enable())
    {
        oled_write_P(LFSTR_ON, false);
    }
    else
    {
        oled_write_P(LFSTR_OFF, false);
    }

    oled_write(format_4d(get_auto_mouse_timeout() / 10) + 1, false);
    oled_write_char('0', false);
#else
    oled_write_P(PSTR("\xC2\xC3\xB4\xB5 ---"), false);
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
// 公開API関数

bool keyball_get_scroll_mode(void)
{
    return keyball.scroll_mode;
}

void keyball_set_scroll_mode(bool mode)
{
    if (mode != keyball.scroll_mode)
    {
        keyball.scroll_mode_changed = timer_read32();
    }
    keyball.scroll_mode = mode;
}

keyball_scrollsnap_mode_t keyball_get_scrollsnap_mode(void)
{
#if KEYBALL_SCROLLSNAP_ENABLE == 2
    return keyball.scrollsnap_mode;
#else
    return KEYBALL_SCROLLSNAP_MODE_VERTICAL; // デフォルト値
#endif
}

void keyball_set_scrollsnap_mode(keyball_scrollsnap_mode_t mode)
{
#if KEYBALL_SCROLLSNAP_ENABLE == 2
    keyball.scrollsnap_mode = mode;
#endif
}

uint8_t keyball_get_scroll_div(void)
{
    return keyball.scroll_div == 0 ? KEYBALL_SCROLL_DIV_DEFAULT : keyball.scroll_div;
}

void keyball_set_scroll_div(uint8_t div)
{
    keyball.scroll_div = div > SCROLL_DIV_MAX ? SCROLL_DIV_MAX : div;
}

uint8_t keyball_get_cpi(void)
{
    return keyball.cpi_value == 0 ? CPI_DEFAULT : keyball.cpi_value;
}

void keyball_set_cpi(uint8_t cpi)
{
    if (cpi > CPI_MAX)
    {
        cpi = CPI_MAX;
    }
    keyball.cpi_value = cpi;
    keyball.cpi_changed = true;
    if (keyball.this_have_ball)
    {
        pmw3360_cpi_set(cpi == 0 ? CPI_DEFAULT - 1 : cpi - 1);
    }
}

////////////////////////////////////////////////////////////////////////////////
// キーボードフック

void keyboard_post_init_kb(void)
{
#ifdef SPLIT_KEYBOARD
    // セカンダリでトランザクションハンドラーを登録
    if (!is_keyboard_master())
    {
        transaction_register_rpc(KEYBALL_GET_INFO, rpc_get_info_handler);
        transaction_register_rpc(KEYBALL_GET_MOTION, rpc_get_motion_handler);
        transaction_register_rpc(KEYBALL_SET_CPI, rpc_set_cpi_handler);
    }
#endif

    // EEPROMからKeyball設定を読み込む
    if (eeconfig_is_enabled())
    {
        keyball_config_t c = {.raw = eeconfig_read_kb()};
        keyball_set_cpi(c.cpi);
        keyball_set_scroll_div(c.sdiv);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        set_auto_mouse_enable(c.amle);
        set_auto_mouse_timeout(c.amlto == 0 ? AUTO_MOUSE_TIME : (c.amlto + 1) * AML_TIMEOUT_QU);
#endif
#if KEYBALL_SCROLLSNAP_ENABLE == 2
        keyball_set_scrollsnap_mode(c.ssnap);
#endif
    }

    keyball_on_adjust_layout(KEYBALL_ADJUST_PENDING);
    keyboard_post_init_user();
}

#if SPLIT_KEYBOARD
void housekeeping_task_kb(void)
{
    if (is_keyboard_master())
    {
        rpc_get_info_invoke();
        if (keyball.that_have_ball)
        {
            rpc_get_motion_invoke();
            rpc_set_cpi_invoke();
        }
    }
}
#endif

// 押下中のキーを更新する関数
static void pressing_keys_update(uint16_t keycode, keyrecord_t *record)
{
    // 有効なキーコードのみ処理
    if (keycode >= 4 && keycode < 57)
    {
        char value = pgm_read_byte(code_to_name + keycode - 4);
        char where = BL;
        if (!record->event.pressed)
        {
            // キーを離した場合、表示をクリア
            where = value;
            value = BL;
        }
        // pressing_keysの最後のwhereをvalueに書き換える
        for (int i = 0; i < KEYBALL_OLED_MAX_PRESSING_KEYCODES; i++)
        {
            if (keyball.pressing_keys[i] == where)
            {
                keyball.pressing_keys[i] = value;
                break;
            }
        }
    }
}

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
bool is_mouse_record_kb(uint16_t keycode, keyrecord_t *record)
{
    switch (keycode)
    {
    case SCRL_MO:
        return true;
    }
    return is_mouse_record_user(keycode, record);
}
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record)
{
    // OLED用に最後のキーコード、行、列を保存
    keyball.last_kc = keycode;
    keyball.last_pos = record->event.key;

    pressing_keys_update(keycode, record);

    if (!process_record_user(keycode, record))
    {
        return false;
    }

    // QK_MODS部分を削除
    if (keycode >= QK_MODS && keycode <= QK_MODS_MAX)
    {
        keycode &= 0xff;
    }

    switch (keycode)
    {
#ifndef MOUSEKEY_ENABLE
    // 自前でKC_MS_BTN1~8を処理
    case KC_MS_BTN1 ... KC_MS_BTN8:
    {
        extern void register_mouse(uint8_t mouse_keycode, bool pressed);
        register_mouse(keycode, record->event.pressed);
        // QK_MODSアクションを適用するため、他を処理可能にする
        return true;
    }
#endif

    case SCRL_MO:
        keyball_set_scroll_mode(record->event.pressed);
        // process_auto_mouseは将来的にこれを使用する場合があるため、処理順序を変更
        return true;
    }

    // 押下のみ処理
    if (record->event.pressed)
    {
        switch (keycode)
        {
        case KBC_RST:
            keyball_set_cpi(0);
            keyball_set_scroll_div(0);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
            set_auto_mouse_enable(false);
            set_auto_mouse_timeout(AUTO_MOUSE_TIME);
#endif
            break;
        case KBC_SAVE:
        {
            keyball_config_t c = {
                .cpi = keyball.cpi_value,
                .sdiv = keyball.scroll_div,
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
                .amle = get_auto_mouse_enable(),
                .amlto = (get_auto_mouse_timeout() / AML_TIMEOUT_QU) - 1,
#endif
#if KEYBALL_SCROLLSNAP_ENABLE == 2
                .ssnap = keyball_get_scrollsnap_mode(),
#endif
            };
            eeconfig_update_kb(c.raw);
        }
        break;

        case CPI_I100:
            add_cpi(1);
            break;
        case CPI_D100:
            add_cpi(-1);
            break;
        case CPI_I1K:
            add_cpi(10);
            break;
        case CPI_D1K:
            add_cpi(-10);
            break;

        case SCRL_TO:
            keyball_set_scroll_mode(!keyball.scroll_mode);
            break;
        case SCRL_DVI:
            add_scroll_div(1);
            break;
        case SCRL_DVD:
            add_scroll_div(-1);
            break;

#if KEYBALL_SCROLLSNAP_ENABLE == 2
        case SSNP_HOR:
            keyball_set_scrollsnap_mode(KEYBALL_SCROLLSNAP_MODE_HORIZONTAL);
            break;
        case SSNP_VRT:
            keyball_set_scrollsnap_mode(KEYBALL_SCROLLSNAP_MODE_VERTICAL);
            break;
        case SSNP_FRE:
            keyball_set_scrollsnap_mode(KEYBALL_SCROLLSNAP_MODE_FREE);
            break;
#endif

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        case AML_TO:
            set_auto_mouse_enable(!get_auto_mouse_enable());
            break;
        case AML_I50:
        {
            uint16_t v = get_auto_mouse_timeout() + 50;
            set_auto_mouse_timeout(MIN(v, AML_TIMEOUT_MAX));
        }
        break;
        case AML_D50:
        {
            uint16_t v = get_auto_mouse_timeout() - 50;
            set_auto_mouse_timeout(MAX(v, AML_TIMEOUT_MIN));
        }
        break;
#endif

        default:
            return true;
        }
        return false;
    }

    return true;
}

// 魔法キーコード機能を無効化し、サイズを削減
#if !defined(MAGIC_KEYCODE_ENABLE) && !defined(KEYBALL_KEEP_MAGIC_FUNCTIONS)

uint16_t keycode_config(uint16_t keycode)
{
    return keycode;
}

uint8_t mod_config(uint8_t mod)
{
    return mod;
}

#endif
