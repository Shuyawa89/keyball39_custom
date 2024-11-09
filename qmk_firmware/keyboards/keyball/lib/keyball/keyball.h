/*
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

このプログラムはフリーソフトウェアです。GNU一般公衆利用許諾契約書の第2版、
またはそれ以降のバージョンの条件の下で再配布や改変が可能です。

このプログラムは有用であることを願って提供されていますが、
商品性や特定目的への適合性についての明示的または黙示的な保証はありません。
詳細についてはGNU一般公衆利用許諾契約書を参照してください。

このプログラムのコピーは、GNUのウェブサイト<http://www.gnu.org/licenses/>から入手できます。
*/

#pragma once

//////////////////////////////////////////////////////////////////////////////
// 設定

#ifndef KEYBALL_CPI_DEFAULT
#    define KEYBALL_CPI_DEFAULT 500 // デフォルトのCPI値
#endif

#ifndef KEYBALL_SCROLL_DIV_DEFAULT
#    define KEYBALL_SCROLL_DIV_DEFAULT 4 // スクロール除数のデフォルト値 (4: 1/8)
#endif

#ifndef KEYBALL_REPORTMOUSE_INTERVAL
#    define KEYBALL_REPORTMOUSE_INTERVAL 8 // マウスレポート間隔: 125Hz
#endif

#ifndef KEYBALL_SCROLLBALL_INHIVITOR
#    define KEYBALL_SCROLLBALL_INHIVITOR 50 // スクロールモード切替後の無視時間(ms)
#endif

/// スクロールスナップ機能を無効化する場合、config.hに0を定義
#ifndef KEYBALL_SCROLLSNAP_ENABLE
#    define KEYBALL_SCROLLSNAP_ENABLE 2 // スクロールスナップの有効化 (2: 新バージョン)
#endif

#ifndef KEYBALL_SCROLLSNAP_RESET_TIMER
#    define KEYBALL_SCROLLSNAP_RESET_TIMER 100 // スクロールスナップリセットタイマー(ms)
#endif

#ifndef KEYBALL_SCROLLSNAP_TENSION_THRESHOLD
#    define KEYBALL_SCROLLSNAP_TENSION_THRESHOLD 12 // スクロールスナップのテンション閾値
#endif

/// 特定のレイヤーでズーム機能を有効にするためのレイヤー番号
#ifndef KEYBALL_ZOOM_LAYER
    #define KEYBALL_ZOOM_LAYER 2 // デフォルト値。config.hで上書き可能
#endif
//////////////////////////////////////////////////////////////////////////////
// 定数

#define KEYBALL_TX_GETINFO_INTERVAL 500
#define KEYBALL_TX_GETINFO_MAXTRY 10
#define KEYBALL_TX_GETMOTION_INTERVAL 4

#if (PRODUCT_ID & 0xff00) == 0x0000
#    define KEYBALL_MODEL 46
#elif (PRODUCT_ID & 0xff00) == 0x0100
#    define KEYBALL_MODEL 61
#elif (PRODUCT_ID & 0xff00) == 0x0200
#    define KEYBALL_MODEL 39
#elif (PRODUCT_ID & 0xff00) == 0x0300
#    define KEYBALL_MODEL 147
#elif (PRODUCT_ID & 0xff00) == 0x0400
#    define KEYBALL_MODEL 44
#endif

#define KEYBALL_OLED_MAX_PRESSING_KEYCODES 6

//////////////////////////////////////////////////////////////////////////////
// 型定義

enum keyball_keycodes {
    KBC_RST  = QK_KB_0, // Keyball設定: デフォルトにリセット
    KBC_SAVE = QK_KB_1, // Keyball設定: EEPROMに保存

    CPI_I100 = QK_KB_2, // CPIを+100増加
    CPI_D100 = QK_KB_3, // CPIを-100減少
    CPI_I1K  = QK_KB_4, // CPIを+1000増加
    CPI_D1K  = QK_KB_5, // CPIを-1000減少

    // スクロールモードでは、プライマリトラックボールの動きをスクロールホイールとして扱う
    SCRL_TO  = QK_KB_6, // スクロールモードのトグル
    SCRL_MO  = QK_KB_7, // 一時的なスクロールモード
    SCRL_DVI = QK_KB_8, // スクロール除数の増加
    SCRL_DVD = QK_KB_9, // スクロール除数の減少

    SSNP_VRT = QK_KB_13, // スクロールスナップモードを垂直に設定
    SSNP_HOR = QK_KB_14, // スクロールスナップモードを水平に設定
    SSNP_FRE = QK_KB_15, // スクロールスナップモードを無効化 (フリースクロール)

    // オートマウスレイヤー制御用キーコード
    // POINTING_DEVICE_AUTO_MOUSE_ENABLEが定義されている場合のみ有効
    AML_TO   = QK_KB_10, // オートマウスレイヤーのトグル
    AML_I50  = QK_KB_11, // オートマウスレイヤーのタイムアウトを50ms増加
    AML_D50  = QK_KB_12, // オートマウスレイヤーのタイムアウトを50ms減少

    // ユーザーカスタマイズ可能な32キーコード
    KEYBALL_SAFE_RANGE = QK_USER_0,
};

typedef union {
    uint32_t raw;
    struct {
        uint8_t cpi : 7;      // CPI値
        uint8_t sdiv : 3;     // スクロール除数
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        uint8_t amle : 1;     // オートマウスレイヤーの有効化
        uint16_t amlto : 5;   // オートマウスレイヤーのタイムアウト
#endif
#if KEYBALL_SCROLLSNAP_ENABLE == 2
        uint8_t ssnap : 2;    // スクロールスナップモード
#endif
    };
} keyball_config_t;

typedef struct {
    uint8_t ballcnt; // ボールの数: 現在は0または1のみ対応
} keyball_info_t;

typedef struct {
    int16_t x;
    int16_t y;
} keyball_motion_t;

typedef uint8_t keyball_cpi_t;

typedef enum {
    KEYBALL_SCROLLSNAP_MODE_VERTICAL   = 0, // 垂直スクロールスナップ
    KEYBALL_SCROLLSNAP_MODE_HORIZONTAL = 1, // 水平スクロールスナップ
    KEYBALL_SCROLLSNAP_MODE_FREE       = 2, // フリースクロール
} keyball_scrollsnap_mode_t;

typedef struct {
    bool this_have_ball;                  // プライマリボールの有無
    bool that_enable;                     // セカンダリの有効化
    bool that_have_ball;                  // セカンダリボールの有無

    keyball_motion_t this_motion;         // プライマリの動き
    keyball_motion_t that_motion;         // セカンダリの動き

    uint8_t cpi_value;                    // CPI値
    bool    cpi_changed;                  // CPI変更フラグ

    bool     scroll_mode;                 // スクロールモードの有効化
    uint32_t scroll_mode_changed;         // スクロールモード変更時刻
    uint8_t  scroll_div;                  // スクロール除数

#if KEYBALL_SCROLLSNAP_ENABLE == 1
    uint32_t scroll_snap_last;            // 最後のスナップタイム
    int8_t   scroll_snap_tension_h;       // スクロールスナップのテンション (水平)
#elif KEYBALL_SCROLLSNAP_ENABLE == 2
    keyball_scrollsnap_mode_t scrollsnap_mode; // 現在のスクロールスナップモード
#endif

    uint16_t       last_kc;                // 最後のキーコード
    keypos_t       last_pos;               // 最後のキー位置
    report_mouse_t last_mouse;             // 最後のマウスレポート

    // 押下中のキーを示すバッファ
    char pressing_keys[KEYBALL_OLED_MAX_PRESSING_KEYCODES + 1];
} keyball_t;

typedef enum {
    KEYBALL_ADJUST_PENDING    = 0, // レイアウト調整保留中
    KEYBALL_ADJUST_PRIMARY    = 1, // プライマリ調整中
    KEYBALL_ADJUST_SECONDARY  = 2, // セカンダリ調整中
} keyball_adjust_t;

//////////////////////////////////////////////////////////////////////////////
// エクスポートされる値 (慎重に操作してください)

extern keyball_t keyball;

//////////////////////////////////////////////////////////////////////////////
// フックポイント

/// keyball_on_adjust_layoutはキーボードレイアウトが調整されたときに呼び出されます
void keyball_on_adjust_layout(keyball_adjust_t v);

/// keyball_on_apply_motion_to_mouse_moveはトラックボールの動きをマウス移動として適用します。
/// デフォルトのアルゴリズムを変更する場合、この関数をオーバーライドしてください。
void keyball_on_apply_motion_to_mouse_move(keyball_motion_t *m, report_mouse_t *r, bool is_left);

/// keyball_on_apply_motion_to_mouse_scrollはトラックボールの動きをマウススクロールとして適用します。
/// デフォルトのアルゴリズムを変更する場合、この関数をオーバーライドしてください。
void keyball_on_apply_motion_to_mouse_scroll(keyball_motion_t *m, report_mouse_t *r, bool is_left);

////////////////////////////////////////////////////////////////////////////////
// 公開API関数

/// keyball_oled_render_ballinfoはボール情報をOLEDに表示します。
/// 21列のみを使用して情報を表示します。
void keyball_oled_render_ballinfo(void);

/// keyball_oled_render_keyinfoは最後に処理されたキー情報をOLEDに表示します。
/// 列、行、キーコード、キー名（利用可能な場合）を表示します。
void keyball_oled_render_keyinfo(void);

/// keyball_oled_render_layerinfoは現在のレイヤーステータス情報をOLEDに表示します。
/// アクティブなレイヤーには番号（1~f）が表示され、非アクティブなレイヤーには'_'が表示されます。
void keyball_oled_render_layerinfo(void);

/// keyball_get_scroll_modeは現在のスクロールモードを取得します。
bool keyball_get_scroll_mode(void);

/// keyball_set_scroll_modeはスクロールモードを変更します。
void keyball_set_scroll_mode(bool mode);

/// keyball_get_scrollsnap_modeは現在のスクロールスナップモードを取得します。
keyball_scrollsnap_mode_t keyball_get_scrollsnap_mode(void);

/// keyball_set_scrollsnap_modeはスクロールスナップモードを変更します。
void keyball_set_scrollsnap_mode(keyball_scrollsnap_mode_t mode);

/// keyball_get_scroll_divは現在のスクロール除数を取得します。
uint8_t keyball_get_scroll_div(void);

/// keyball_set_scroll_divはスクロール除数を変更します。
void keyball_set_scroll_div(uint8_t div);

/// keyball_get_cpiは現在のトラックボールのCPIを取得します。
uint8_t keyball_get_cpi(void);

/// keyball_set_cpiはトラックボールのCPIを変更します。
void keyball_set_cpi(uint8_t cpi);
