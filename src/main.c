// Eye Sight: adiciona textura (grain) à tela, controlada por um ícone na bandeja do Windows.
//
// Como a textura é desenhada sem aparecer na barra de tarefas nem no Alt+Tab:
// cada monitor recebe uma janela "fantasma" com WS_EX_TOOLWINDOW (fora da barra de
// tarefas e do Alt+Tab), WS_EX_LAYERED + per-pixel alpha (o DWM compõe a textura por
// cima de tudo, sem custo por quadro, pois a imagem é estática), WS_EX_TRANSPARENT
// (cliques atravessam) e WS_EX_NOACTIVATE (nunca recebe foco). A intensidade do slider
// só muda o alpha global da camada, então arrastar o slider é instantâneo.
//
// A propriedade NonRudeHWND avisa o Explorer que a camada não é um app em tela cheia,
// senão a barra de tarefas com ocultação automática deixa de subir. Quando instalado
// com o instalador (assinado, em Arquivos de Programas, uiAccess), a camada é criada
// na faixa ZBID_UIACCESS, que fica acima do menu Iniciar e da barra de tarefas.

#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000006
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "grain.h"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x11
#endif
#ifndef WDA_NONE
#define WDA_NONE 0
#endif
#define DWMWA_USE_IMMERSIVE_DARK_MODE_ 20
#define DWMWA_WINDOW_CORNER_PREFERENCE_ 33
#define DWMWA_SYSTEMBACKDROP_TYPE_ 38

#define APP_NAME L"Eye Sight"
#define REG_KEY L"Software\\EyeSight"
#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_VALUE L"EyeSight"
#define CLS_MAIN L"EyeSightMain"
#define CLS_OVERLAY L"EyeSightOverlay"
#define CLS_FLYOUT L"EyeSightFlyout"

#define WM_TRAY (WM_APP + 1)
#define WM_SHOWFLYOUT (WM_APP + 2)
#define WM_QUITAPP (WM_APP + 3)
#define WM_RELOADCURSOR (WM_APP + 4)
#define TIMER_TOPMOST 1
#define TIMER_DISPLAY 2
#define TIMER_SAVE 3
#define TIMER_ANIM 4
#define TIMER_CURSOR 5
#define CURSOR_DIM_DEFAULT 4
#define CURSOR_WARM_DEFAULT 6
#define IDM_STARTUP 101
#define IDM_CAPTURE 102
#define IDM_EXIT 103
#define IDM_CURSOR 104
#define IDM_PRESET0 110
#define ZBID_UIACCESS 2
#define MAX_MON 16

// Layout do painel em pixels lógicos (96 DPI)
#define FW 360
#define FH 290
#define SL_X0 24
#define SL_X1 (FW - 74)
#define SL_CY 70
#define TILE_X0 22
#define TILE_GAP 8
#define TILE_W ((FW - 2 * TILE_X0 - (NUM_PRESETS - 1) * TILE_GAP) / NUM_PRESETS)
#define TILE_Y 98
#define TILE_H 40
#define LABEL_H 22
#define WARM_TITLE_Y 168
#define WARM_CY 214
#define WARM_COLOR 0xFF9A3C
#define FOOT_Y 246
#define BTN 32
#define GEAR_CX (FW - 36)
#define POWER_CX (FW - 74)

enum { HIT_NONE, HIT_SLIDER, HIT_WARM, HIT_POWER, HIT_GEAR, HIT_TILE0 = 16 };

typedef struct { uint32_t *px; int w, h; } Buf;
typedef struct { HWND hwnd; int x, y, w, h; UINT dpi; BOOL in_band; uint32_t *bits; HDC dc; HBITMAP bmp, old; } Overlay;

static HINSTANCE g_inst;
static HWND g_main, g_fly;
static Overlay g_ov[MAX_MON];
static int g_novl;
static UINT WM_TASKBARCREATED;
static NOTIFYICONDATAW g_nid;
static HICON g_icon;
static int g_icon_on = -1;
static DWORD g_build;
static ATOM g_overlay_atom;

static void update_tray(BOOL force_icon);
static void schedule_cursor_grain(void);
static void kelvin_rgb(float k, float rgb[3]);
static float warm_kelvin(int v);

// configuração (lembrada no registro)
static int g_value, g_last_on = 40, g_preset, g_warm;
static int g_cur_dim, g_cur_warm; // ajuste fino do cursor (%), só pelo registro
static BOOL g_hide_capture = TRUE, g_cursor_grain = TRUE;

// tema
static BOOL g_dark = TRUE, g_taskbar_dark = TRUE, g_transparency = TRUE, g_acrylic;
static uint32_t g_accent = 0x4CC2FF;

// painel
static float g_s = 1.0f;
static HFONT g_f_title, g_f_value, g_f_small, g_f_icon, g_f_label;
static wchar_t g_text_face[LF_FACESIZE] = L"Segoe UI", g_icon_face[LF_FACESIZE] = L"Segoe MDL2 Assets";
static HDC g_mdc, g_sdc;
static HBITMAP g_mbmp, g_sbmp;
static uint32_t *g_mpx, *g_spx;
static int g_bw, g_bh;
static uint32_t *g_tiles[NUM_PRESETS];
static int g_tile_w, g_tile_h;
static int g_dragging; // slider sendo arrastado (HIT_SLIDER/HIT_WARM) ou 0
static int g_hover;
static int g_wheel;
static DWORD g_hidden_at;
static int g_anim_step, g_anim_x, g_anim_y, g_anim_dx, g_anim_dy;

// ---------------------------------------------------------------- registro

static DWORD reg_read(HKEY root, const wchar_t *path, const wchar_t *name, DWORD def)
{
    HKEY k;
    DWORD v = def, sz = sizeof v, type;
    if (RegOpenKeyExW(root, path, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, name, NULL, &type, (BYTE *)&v, &sz) != ERROR_SUCCESS || type != REG_DWORD)
            v = def;
        RegCloseKey(k);
    }
    return v;
}

static DWORD cfg_get(const wchar_t *name, DWORD def) { return reg_read(HKEY_CURRENT_USER, REG_KEY, name, def); }

static void cfg_set(const wchar_t *name, DWORD v)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_DWORD, (BYTE *)&v, sizeof v);
        RegCloseKey(k);
    }
}

static BOOL startup_enabled(void) { return cfg_get(L"Startup", 1) != 0; }

// Grava (ou remove) a entrada em Run. Regravada a cada início para seguir o .exe se ele for movido.
static void apply_startup(BOOL on)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_WRITE, &k) != ERROR_SUCCESS)
        return;
    if (on) {
        wchar_t path[MAX_PATH], cmd[MAX_PATH + 16];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        wsprintfW(cmd, L"\"%s\" --tray", path);
        RegSetValueExW(k, RUN_VALUE, 0, REG_SZ, (BYTE *)cmd, (DWORD)((lstrlenW(cmd) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, RUN_VALUE);
    }
    RegCloseKey(k);
}

// Ao abrir, só assume a inicialização automática se for a versão instalada (Arquivos de
// Programas) ou se a entrada atual não existir ou apontar para um arquivo que sumiu. Assim
// uma cópia solta aberta para teste não toma o lugar da versão instalada.
static BOOL should_claim_startup(void)
{
    wchar_t self[MAX_PATH], pf[MAX_PATH], cur[MAX_PATH + 32], path[MAX_PATH];
    DWORD sz = sizeof cur, type;
    GetModuleFileNameW(NULL, self, MAX_PATH);
    if (GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH) && CompareStringOrdinal(self, lstrlenW(pf), pf, -1, TRUE) == CSTR_EQUAL)
        return TRUE;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return TRUE;
    LONG r = RegQueryValueExW(k, RUN_VALUE, NULL, &type, (BYTE *)cur, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || type != REG_SZ)
        return TRUE;
    // a entrada tem o formato "caminho" --tray
    const wchar_t *p = cur[0] == L'"' ? cur + 1 : cur;
    int n = 0;
    while (p[n] && p[n] != L'"' && n < MAX_PATH - 1) {
        path[n] = p[n];
        n++;
    }
    path[n] = 0;
    return GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES;
}

static void load_theme(void)
{
    const wchar_t *pers = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    g_dark = reg_read(HKEY_CURRENT_USER, pers, L"AppsUseLightTheme", 0) == 0;
    g_taskbar_dark = reg_read(HKEY_CURRENT_USER, pers, L"SystemUsesLightTheme", 0) == 0;
    g_transparency = reg_read(HKEY_CURRENT_USER, pers, L"EnableTransparency", 1) != 0;

    // AccentPalette: 8 cores RGBA, da mais clara para a mais escura. O Windows usa a
    // variação clara no tema escuro e a escura no tema claro.
    HKEY k;
    BYTE pal[32];
    DWORD sz = sizeof pal;
    g_accent = g_dark ? 0x4CC2FF : 0x005FB8;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, L"AccentPalette", NULL, NULL, pal, &sz) == ERROR_SUCCESS && sz >= 32) {
            int i = (g_dark ? 1 : 4) * 4;
            g_accent = ((uint32_t)pal[i] << 16) | ((uint32_t)pal[i + 1] << 8) | pal[i + 2];
        }
        RegCloseKey(k);
    }
}

// ---------------------------------------------------------------- desenho com anti-aliasing

static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

// Mistura uma cor sobre o pixel (buffer em BGRA pré-multiplicado).
static void blend(Buf *b, int x, int y, uint32_t rgb, float a)
{
    if (x < 0 || y < 0 || x >= b->w || y >= b->h || a <= 0.0f)
        return;
    if (a > 1.0f)
        a = 1.0f;
    uint32_t *p = &b->px[y * b->w + x], d = *p;
    float ia = 1.0f - a;
    uint32_t oa = (uint32_t)(255.0f * a + (d >> 24) * ia + 0.5f);
    uint32_t orr = (uint32_t)(((rgb >> 16) & 255) * a + ((d >> 16) & 255) * ia + 0.5f);
    uint32_t og = (uint32_t)(((rgb >> 8) & 255) * a + ((d >> 8) & 255) * ia + 0.5f);
    uint32_t ob = (uint32_t)((rgb & 255) * a + (d & 255) * ia + 0.5f);
    *p = oa << 24 | orr << 16 | og << 8 | ob;
}

static void fill_rect(Buf *b, int x, int y, int w, int h, uint32_t rgb, float a)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            blend(b, i, j, rgb, a);
}

static void fill_circle(Buf *b, float cx, float cy, float r, uint32_t rgb, float a)
{
    for (int y = (int)floorf(cy - r - 1); y <= (int)ceilf(cy + r + 1); y++)
        for (int x = (int)floorf(cx - r - 1); x <= (int)ceilf(cx + r + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            blend(b, x, y, rgb, a * clampf(r - sqrtf(dx * dx + dy * dy) + 0.5f, 0, 1));
        }
}

static void stroke_circle(Buf *b, float cx, float cy, float r, float th, uint32_t rgb, float a)
{
    float R = r + th;
    for (int y = (int)floorf(cy - R - 1); y <= (int)ceilf(cy + R + 1); y++)
        for (int x = (int)floorf(cx - R - 1); x <= (int)ceilf(cx + R + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy);
            blend(b, x, y, rgb, a * clampf(th * 0.5f - fabsf(d - r) + 0.5f, 0, 1));
        }
}

// Barra horizontal com pontas arredondadas, de x0 a x1.
static void fill_capsule(Buf *b, float x0, float x1, float cy, float r, uint32_t rgb, float a)
{
    if (x1 < x0)
        return;
    for (int y = (int)floorf(cy - r - 1); y <= (int)ceilf(cy + r + 1); y++)
        for (int x = (int)floorf(x0 - r - 1); x <= (int)ceilf(x1 + r + 1); x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float qx = clampf(px, x0, x1), dx = px - qx, dy = py - cy;
            blend(b, x, y, rgb, a * clampf(r - sqrtf(dx * dx + dy * dy) + 0.5f, 0, 1));
        }
}

static float rrect_cov(float px, float py, float x, float y, float w, float h, float r)
{
    float qx = fabsf(px - (x + w / 2)) - (w / 2 - r), qy = fabsf(py - (y + h / 2)) - (h / 2 - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = qx > qy ? qx : qy;
    return clampf(0.5f - (sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r), 0, 1);
}

static void fill_rrect(Buf *b, float x, float y, float w, float h, float r, uint32_t rgb, float a)
{
    float cx = x + w / 2, cy = y + h / 2;
    for (int j = (int)floorf(y); j <= (int)ceilf(y + h); j++)
        for (int i = (int)floorf(x); i <= (int)ceilf(x + w); i++) {
            float qx = fabsf(i + 0.5f - cx) - (w / 2 - r), qy = fabsf(j + 0.5f - cy) - (h / 2 - r);
            float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
            float in = qx > qy ? qx : qy;
            float d = sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
            blend(b, i, j, rgb, a * clampf(0.5f - d, 0, 1));
        }
}

// Contorno de retângulo arredondado com espessura th, para dentro.
static void stroke_rrect(Buf *b, float x, float y, float w, float h, float r, float th, uint32_t rgb, float a)
{
    for (int j = (int)floorf(y); j <= (int)ceilf(y + h); j++)
        for (int i = (int)floorf(x); i <= (int)ceilf(x + w); i++) {
            float px = i + 0.5f, py = j + 0.5f;
            float c = rrect_cov(px, py, x, y, w, h, r) - rrect_cov(px, py, x + th, y + th, w - 2 * th, h - 2 * th, r - th);
            blend(b, i, j, rgb, a * c);
        }
}

// Ícone: um círculo com pontinhos irregulares, como uma lixa (menos pontos em tamanhos pequenos).
// {x, y, tamanho relativo}, com x e y em frações do raio interno.
static const float DOTS_SMALL[][3] = {{0.074f, -0.742f, 0.97f}, {-0.305f, -0.444f, 0.95f}, {0.234f, -0.452f, 0.94f}, {-0.401f, 0.057f, 1.17f}, {-0.029f, 0.036f, 1.16f}, {0.512f, -0.016f, 1.12f}, {-0.224f, 0.329f, 1.08f}, {0.219f, 0.403f, 1.17f}, {0.613f, 0.286f, 0.92f}, {0.087f, 0.719f, 0.87f}};
static const float DOTS_LARGE[][3] = {{-0.271f, -0.527f, 0.86f}, {0.047f, -0.502f, 1.06f}, {0.308f, -0.564f, 0.85f}, {0.589f, -0.503f, 1.06f}, {-0.504f, -0.332f, 0.8f}, {-0.122f, -0.333f, 0.99f}, {0.198f, -0.278f, 0.93f}, {0.498f, -0.278f, 1.11f}, {-0.715f, -0.008f, 0.83f}, {-0.368f, -0.062f, 1.07f}, {0.041f, 0.058f, 0.86f}, {0.361f, 0.023f, 0.86f}, {0.716f, 0.068f, 0.89f}, {-0.529f, 0.219f, 1.11f}, {-0.198f, 0.232f, 0.97f}, {0.225f, 0.332f, 0.9f}, {0.444f, 0.347f, 1.03f}, {-0.36f, 0.572f, 0.87f}, {-0.022f, 0.502f, 0.9f}, {0.26f, 0.605f, 1.02f}};

static void draw_grain_icon(Buf *b, float ox, float oy, float s, uint32_t rgb, float dots_alpha)
{
    float th = s * 0.085f < 1.1f ? 1.1f : s * 0.085f;
    float cx = ox + s / 2, cy = oy + s / 2, r = s / 2 - th / 2 - s * 0.03f;
    stroke_circle(b, cx, cy, r, th, rgb, 1.0f);
    float inner = r - th / 2;
    BOOL small = s < 22;
    int n = small ? (int)(sizeof DOTS_SMALL / sizeof DOTS_SMALL[0]) : (int)(sizeof DOTS_LARGE / sizeof DOTS_LARGE[0]);
    float dr = inner * (small ? 0.11f : 0.08f);
    for (int i = 0; i < n; i++) {
        const float *d = small ? DOTS_SMALL[i] : DOTS_LARGE[i];
        fill_circle(b, cx + d[0] * inner, cy + d[1] * inner, dr * d[2], rgb, dots_alpha);
    }
}

// ---------------------------------------------------------------- camada da textura

static UINT monitor_dpi(HMONITOR hm)
{
    static HRESULT(WINAPI * p)(HMONITOR, int, UINT *, UINT *);
    static BOOL tried;
    if (!tried) {
        tried = TRUE;
        HMODULE m = LoadLibraryW(L"shcore.dll");
        if (m)
            p = (HRESULT(WINAPI *)(HMONITOR, int, UINT *, UINT *))(void *)GetProcAddress(m, "GetDpiForMonitor");
    }
    UINT x = 96, y = 96;
    if (p && SUCCEEDED(p(hm, 0, &x, &y)))
        return x;
    return 96;
}

// Só funciona num processo com uiAccess (versão instalada); senão cai na janela comum.
typedef HWND(WINAPI *CreateWindowInBand_t)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE,
                                           LPVOID, DWORD);
static CreateWindowInBand_t g_create_in_band;

static BOOL CALLBACK add_overlay(HMONITOR hm, HDC hdc, LPRECT rc, LPARAM lp)
{
    (void)hdc; (void)rc; (void)lp;
    if (g_novl >= MAX_MON)
        return FALSE;
    MONITORINFO mi = {sizeof mi};
    GetMonitorInfoW(hm, &mi);
    Overlay *o = &g_ov[g_novl];
    o->x = mi.rcMonitor.left;
    o->y = mi.rcMonitor.top;
    o->w = mi.rcMonitor.right - mi.rcMonitor.left;
    // 1 px a menos na altura: a camada nunca cobre o monitor inteiro, o que reforça
    // para o Explorer que ela não é uma janela em tela cheia
    o->h = mi.rcMonitor.bottom - mi.rcMonitor.top - 1;
    o->dpi = monitor_dpi(hm);

    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), o->w, -o->h, 1, 32, BI_RGB}};
    void *bits;
    o->dc = CreateCompatibleDC(NULL);
    o->bmp = CreateDIBSection(o->dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!o->bmp) {
        DeleteDC(o->dc);
        return TRUE;
    }
    o->old = SelectObject(o->dc, o->bmp);
    o->bits = bits;
    render_grain(bits, o->w, o->h, o->dpi / 96.0f, 0x1234567u + g_novl * 7919u, &PRESETS[g_preset]);

    DWORD ex = WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    o->hwnd = NULL;
    if (g_create_in_band)
        o->hwnd = g_create_in_band(ex, MAKEINTATOM(g_overlay_atom), L"", WS_POPUP, o->x, o->y, o->w, o->h, NULL, NULL, g_inst, NULL,
                                   ZBID_UIACCESS);
    o->in_band = o->hwnd != NULL;
    if (!o->hwnd)
        o->hwnd = CreateWindowExW(ex, CLS_OVERLAY, L"", WS_POPUP, o->x, o->y, o->w, o->h, NULL, NULL, g_inst, NULL);
    SetPropW(o->hwnd, L"NonRudeHWND", (HANDLE)(INT_PTR)TRUE);
    if (g_hide_capture)
        SetWindowDisplayAffinity(o->hwnd, WDA_EXCLUDEFROMCAPTURE);
    g_novl++;
    return TRUE;
}

static void destroy_overlays(void)
{
    for (int i = 0; i < g_novl; i++) {
        Overlay *o = &g_ov[i];
        DestroyWindow(o->hwnd);
        SelectObject(o->dc, o->old);
        DeleteObject(o->bmp);
        DeleteDC(o->dc);
    }
    g_novl = 0;
}

static void apply_overlays(void)
{
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, (BYTE)((g_value * 255 + 50) / 100), AC_SRC_ALPHA};
    for (int i = 0; i < g_novl; i++) {
        Overlay *o = &g_ov[i];
        if (g_value == 0) {
            ShowWindow(o->hwnd, SW_HIDE);
            continue;
        }
        POINT pt = {o->x, o->y}, src = {0, 0};
        SIZE sz = {o->w, o->h};
        UpdateLayeredWindow(o->hwnd, NULL, &pt, &sz, o->dc, &src, 0, &bf, ULW_ALPHA);
        if (!IsWindowVisible(o->hwnd)) {
            ShowWindow(o->hwnd, SW_SHOWNOACTIVATE);
            if (g_fly && IsWindowVisible(g_fly))
                SetWindowPos(g_fly, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

static void set_preset(int p)
{
    if (p < 0 || p >= NUM_PRESETS || p == g_preset)
        return;
    g_preset = p;
    for (int i = 0; i < g_novl; i++)
        render_grain(g_ov[i].bits, g_ov[i].w, g_ov[i].h, g_ov[i].dpi / 96.0f, 0x1234567u + i * 7919u, &PRESETS[p]);
    if (g_value == 0)
        g_value = g_last_on > 0 ? g_last_on : 40;
    apply_overlays();
    schedule_cursor_grain();
    update_tray(FALSE);
    if (g_fly)
        InvalidateRect(g_fly, NULL, FALSE);
    SetTimer(g_main, TIMER_SAVE, 500, NULL);
}

static BOOL is_own_window(HWND w)
{
    if (w == g_fly || w == g_main)
        return TRUE;
    for (int i = 0; i < g_novl; i++)
        if (g_ov[i].hwnd == w)
            return TRUE;
    return FALSE;
}

// Há alguma janela visível, de outro programa, acima da camada e sobre o monitor dela?
static BOOL overlay_covered(const Overlay *o)
{
    RECT mon = {o->x, o->y, o->x + o->w, o->y + o->h}, r, x;
    for (HWND w = GetWindow(o->hwnd, GW_HWNDPREV); w; w = GetWindow(w, GW_HWNDPREV)) {
        if (!IsWindowVisible(w) || is_own_window(w))
            continue;
        BOOL cloaked = FALSE;
        if (SUCCEEDED(DwmGetWindowAttribute(w, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked)
            continue;
        if (GetWindowRect(w, &r) && IntersectRect(&x, &r, &mon))
            return TRUE;
    }
    return FALSE;
}

typedef struct { int n; RECT rc[MAX_MON]; UINT dpi[MAX_MON]; } MonLayout;

static BOOL CALLBACK collect_monitor(HMONITOR hm, HDC hdc, LPRECT rc, LPARAM lp)
{
    (void)hdc; (void)rc;
    MonLayout *l = (MonLayout *)lp;
    if (l->n >= MAX_MON)
        return FALSE;
    MONITORINFO mi = {sizeof mi};
    GetMonitorInfoW(hm, &mi);
    l->rc[l->n] = mi.rcMonitor;
    l->dpi[l->n++] = monitor_dpi(hm);
    return TRUE;
}

// WM_DISPLAYCHANGE também chega por mudanças que não alteram os monitores (taxa de
// atualização, HDR, alguns drivers). Recriar as camadas nesses casos causaria piscadas.
static BOOL layout_changed(void)
{
    MonLayout l = {0};
    EnumDisplayMonitors(NULL, NULL, collect_monitor, (LPARAM)&l);
    if (l.n != g_novl)
        return TRUE;
    for (int i = 0; i < l.n; i++) {
        const Overlay *o = &g_ov[i];
        if (l.rc[i].left != o->x || l.rc[i].top != o->y || l.rc[i].right - l.rc[i].left != o->w ||
            l.rc[i].bottom - l.rc[i].top - 1 != o->h || l.dpi[i] != o->dpi)
            return TRUE;
    }
    return FALSE;
}

static void rebuild_overlays(void)
{
    destroy_overlays();
    EnumDisplayMonitors(NULL, NULL, add_overlay, 0);
    apply_overlays();
}

// ---------------------------------------------------------------- bandeja

static HICON make_icon(int s, uint32_t rgb, float dots_alpha)
{
    BITMAPV5HEADER bi = {0};
    bi.bV5Size = sizeof bi;
    bi.bV5Width = s;
    bi.bV5Height = -s;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    void *bits;
    HDC dc = GetDC(NULL);
    HBITMAP color = CreateDIBSection(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, dc);
    memset(bits, 0, (size_t)s * s * 4);
    Buf b = {bits, s, s};
    draw_grain_icon(&b, 0, 0, (float)s, rgb, dots_alpha);
    // ícones usam alpha não pré-multiplicado
    for (int i = 0; i < s * s; i++) {
        uint32_t p = b.px[i], a = p >> 24;
        b.px[i] = a ? (a << 24 | (rgb & 0xFFFFFF)) : 0;
    }
    BYTE *maskbits = calloc((size_t)((s + 15) / 16) * 2 * s, 1);
    HBITMAP mask = CreateBitmap(s, s, 1, 1, maskbits);
    free(maskbits);
    ICONINFO ii = {TRUE, 0, 0, mask, color};
    HICON h = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return h;
}

static void update_tray(BOOL force_icon)
{
    int on = g_value > 0;
    if (force_icon || on != g_icon_on) {
        HICON old = g_icon;
        g_icon = make_icon(GetSystemMetrics(SM_CXSMICON), g_taskbar_dark ? 0xFFFFFF : 0x000000, on ? 1.0f : 0.35f);
        g_icon_on = on;
        g_nid.hIcon = g_icon;
        if (old)
            DestroyIcon(old);
    }
    if (g_value > 0)
        wsprintfW(g_nid.szTip, L"Eye Sight: %s, %d%%", PRESETS[g_preset].name, g_value);
    else
        lstrcpyW(g_nid.szTip, L"Eye Sight: textura desligada");
    if (g_warm > 0)
        wsprintfW(g_nid.szTip + lstrlenW(g_nid.szTip), L"\nLuz quente: %d%%", g_warm);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void add_tray(void)
{
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_main;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_nid.uCallbackMessage = WM_TRAY;
    if (!g_icon)
        update_tray(TRUE);
    g_nid.hIcon = g_icon;
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    update_tray(FALSE);
}

// ---------------------------------------------------------------- textura no cursor

// O Windows desenha o cursor acima de qualquer janela, até da faixa uiAccess, e fora do filtro de
// cor da Luz quente, então nem a camada nem o filtro chegam nele. Em vez disso, os cursores do
// sistema são trocados por cópias com o mesmo grão e o mesmo tom quente aplicados (SetSystemCursor).
// SPI_SETCURSORS devolve os originais do esquema do usuário. Os animados viram um .ani temporário
// com todos os quadros, e os monocromáticos que invertem o fundo (o cursor de texto padrão) ganham
// cor: o traço fica preto com contorno branco, para continuar visível em qualquer fundo.
typedef struct { int w, h; DWORD hx, hy; uint32_t *px; } CursorFrame; // px: BGRA, alpha não premultiplicado
typedef struct { DWORD id; int n; CursorFrame *f; DWORD *rate; } CursorBase; // rate em 1/60 s, por quadro
static const DWORD CURSOR_IDS[] = {32512, 32513, 32514, 32515, 32516, 32642, 32643, 32644,
                                   32645, 32646, 32648, 32649, 32650, 32651, 32671, 32672};
// nome de cada um em HKCU\Control Panel\Cursors, na mesma ordem
static const wchar_t *const CURSOR_NAMES[] = {L"Arrow", L"IBeam", L"Wait", L"Crosshair", L"UpArrow", L"SizeNWSE",
                                              L"SizeNESW", L"SizeWE", L"SizeNS", L"SizeAll", L"No", L"Hand",
                                              L"AppStarting", L"Help", L"Pin", L"Person"};
#define MAX_CURSORS (sizeof CURSOR_IDS / sizeof CURSOR_IDS[0])
#define MAX_FRAMES 256
static CursorBase g_cur[MAX_CURSORS];
static int g_ncur = -1; // -1: originais ainda não lidos
static BOOL g_cur_applied;
// quadro N de um cursor animado (não documentada, mas existe no user32 desde o XP)
static HCURSOR(WINAPI *p_GetCursorFrameInfo)(HCURSOR, DWORD, DWORD, DWORD *, DWORD *);

static HKEY g_cur_key;
static HANDLE g_cur_event;

static void free_cursor_bases(void)
{
    for (int i = 0; i < g_ncur; i++) {
        for (int j = 0; j < g_cur[i].n; j++)
            free(g_cur[i].f[j].px);
        free(g_cur[i].f);
        free(g_cur[i].rate);
    }
    g_ncur = -1;
}

static void restore_cursors(void)
{
    if (g_cur_applied)
        SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, 0);
    g_cur_applied = FALSE;
    free_cursor_bases(); // ao religar, copia de novo o esquema que estiver em uso
}

// Avisa quando o esquema de cursores muda no registro. O WM_SETTINGCHANGE de quem troca o
// esquema nem sempre chega: o Windows bloqueia mensagens de programas comuns para a versão
// uiAccess, e um script pode trocar os cursores sem avisar ninguém.
static BOOL cursor_scheme_changed(void)
{
    if (!g_cur_key && RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_NOTIFY, &g_cur_key) != ERROR_SUCCESS)
        return FALSE;
    if (!g_cur_event && !(g_cur_event = CreateEventW(NULL, FALSE, TRUE, NULL)))
        return FALSE;
    if (WaitForSingleObject(g_cur_event, 0) != WAIT_OBJECT_0)
        return FALSE;
    RegNotifyChangeKeyValue(g_cur_key, TRUE, REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET, g_cur_event, TRUE);
    return TRUE;
}

// Lê um quadro como BGRA com alpha. Cursores sem alpha usam a máscara AND; os pixels que invertem
// o fundo (máscara 1 com cor) ficam pretos e ganham um contorno branco.
static BOOL read_cursor_frame(HDC dc, HCURSOR c, CursorFrame *f)
{
    ICONINFO ii;
    if (!GetIconInfo(c, &ii))
        return FALSE;
    BOOL ok = FALSE;
    BITMAP bm;
    if (GetObjectW(ii.hbmColor ? ii.hbmColor : ii.hbmMask, sizeof bm, &bm)) {
        int w = bm.bmWidth, h = ii.hbmColor ? bm.bmHeight : bm.bmHeight / 2; // monocromático: AND em cima, XOR embaixo
        int mh = ii.hbmColor ? h : h * 2;
        BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
        BITMAPINFO mi = {{sizeof(BITMAPINFOHEADER), w, -mh, 1, 32, BI_RGB}};
        uint32_t *px = calloc((size_t)w * h, 4), *mask = malloc((size_t)w * mh * 4);
        if (w > 0 && h > 0 && px && mask && GetDIBits(dc, ii.hbmMask, 0, mh, mask, &mi, DIB_RGB_COLORS) == mh &&
            (!ii.hbmColor || GetDIBits(dc, ii.hbmColor, 0, h, px, &bi, DIB_RGB_COLORS) == h)) {
            BOOL has_alpha = FALSE;
            for (int k = 0; ii.hbmColor && k < w * h && !has_alpha; k++)
                has_alpha = (px[k] >> 24) != 0;
            if (!has_alpha) {
                // inv[k]: o pixel invertia o fundo (reaproveita a área da máscara)
                BYTE *inv = (BYTE *)mask;
                BOOL inverts = FALSE;
                for (int k = 0; k < w * h; k++) {
                    uint32_t and = mask[k] & 0xFFFFFF, xor = (ii.hbmColor ? px[k] : mask[w * h + k]) & 0xFFFFFF;
                    px[k] = !and ? 0xFF000000u | xor : xor ? 0xFF000000u : 0; // quem invertia vira preto
                    inv[k] = and && xor;
                    inverts |= inv[k];
                }
                // contorno branco em volta do que antes invertia o fundo
                for (int y = 0; inverts && y < h; y++)
                    for (int x = 0; x < w; x++) {
                        BOOL edge = FALSE;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++) {
                                int xx = x + dx, yy = y + dy;
                                edge |= xx >= 0 && yy >= 0 && xx < w && yy < h && inv[yy * w + xx];
                            }
                        if (edge && !px[y * w + x])
                            px[y * w + x] = 0xFFFFFFFFu;
                    }
            }
            *f = (CursorFrame){w, h, ii.xHotspot, ii.yHotspot, px};
            px = NULL;
            ok = TRUE;
        }
        free(px);
        free(mask);
    }
    if (ii.hbmColor)
        DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
    return ok;
}

// Cursor do esquema do usuário a partir do arquivo indicado no registro (NULL se for o padrão)
static HCURSOR load_scheme_cursor(const wchar_t *name)
{
    wchar_t raw[MAX_PATH], path[MAX_PATH];
    DWORD sz = sizeof raw, type;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_READ, &k) != ERROR_SUCCESS)
        return NULL;
    LONG r = RegQueryValueExW(k, name, NULL, &type, (BYTE *)raw, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || sz < 4)
        return NULL;
    raw[sz / 2 < MAX_PATH ? sz / 2 : MAX_PATH - 1] = 0;
    if (!ExpandEnvironmentStringsW(raw, path, MAX_PATH))
        return NULL;
    return LoadCursorFromFileW(path);
}

static void load_cursor_bases(void)
{
    // recarrega o esquema do usuário para nunca copiar um cursor que já tem grão
    SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, 0);
    if (!p_GetCursorFrameInfo)
        p_GetCursorFrameInfo = (HCURSOR(WINAPI *)(HCURSOR, DWORD, DWORD, DWORD *, DWORD *))(void *)GetProcAddress(
            GetModuleHandleW(L"user32.dll"), "GetCursorFrameInfo");
    g_ncur = 0;
    HDC dc = GetDC(NULL);
    for (size_t i = 0; i < MAX_CURSORS; i++) {
        // lê direto do arquivo do esquema: o SPI_SETCURSORS feito pela versão uiAccess pode
        // carregar os cursores padrão do Windows em vez dos do usuário
        HCURSOR own = load_scheme_cursor(CURSOR_NAMES[i]);
        HCURSOR c = own ? own : LoadCursorW(NULL, MAKEINTRESOURCEW(CURSOR_IDS[i]));
        if (!c)
            continue;
        DWORD rate = 0, steps = 1;
        if (p_GetCursorFrameInfo && !p_GetCursorFrameInfo(c, 0, 0, &rate, &steps))
            steps = 1;
        if (steps < 1 || steps > MAX_FRAMES)
            steps = 1;
        CursorBase b = {CURSOR_IDS[i], 0, calloc(steps, sizeof(CursorFrame)), calloc(steps, sizeof(DWORD))};
        for (DWORD s = 0; b.f && b.rate && s < steps; s++) {
            HCURSOR fc = c;
            DWORD r = 0, n = 0;
            if (steps > 1 && !(fc = p_GetCursorFrameInfo(c, 0, s, &r, &n)))
                break;
            if (!read_cursor_frame(dc, fc, &b.f[b.n]))
                break;
            b.rate[b.n++] = r ? r : 10;
        }
        if (own)
            DestroyCursor(own);
        if (b.n == (int)steps) {
            g_cur[g_ncur++] = b;
        } else {
            for (int j = 0; j < b.n; j++)
                free(b.f[j].px);
            free(b.f);
            free(b.rate);
        }
    }
    ReleaseDC(NULL, dc);
}

// Aplica o grão e a Luz quente num quadro. out recebe BGRA com alpha não premultiplicado.
static void grain_cursor_frame(const CursorFrame *f, DWORD id, float t, const float mul[3], uint32_t *out)
{
    uint32_t *grain = malloc((size_t)f->w * f->h * 4);
    if (!grain) {
        memcpy(out, f->px, (size_t)f->w * f->h * 4);
        return;
    }
    // mesma mistura da tela: grão por cima do cursor (escurece pelo alpha e soma o grão
    // claro) e depois o filtro da Luz quente sobre tudo
    render_grain(grain, f->w, f->h, 1.0f, 0x51ed270bu + id, &PRESETS[g_preset]);
    for (int k = 0; k < f->w * f->h; k++) {
        uint32_t p = f->px[k], A = p >> 24;
        if (!A) {
            out[k] = 0;
            continue;
        }
        float ga = (grain[k] >> 24) / 255.0f * t, gv = (grain[k] & 255) * t;
        uint32_t o = A << 24;
        for (int sh = 0, ch = 0; sh <= 16; sh += 8, ch++) {
            float c = (((p >> sh) & 255) * (1 - ga) + gv) * mul[ch];
            o |= (uint32_t)(c > 255 ? 255 : c + 0.5f) << sh;
        }
        out[k] = o;
    }
    free(grain);
}

static HCURSOR make_static_cursor(const CursorFrame *f, const uint32_t *px)
{
    uint32_t *bits;
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), f->w, -f->h, 1, 32, BI_RGB}};
    HBITMAP color = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    // máscara AND: 1 onde o cursor é transparente (usada por quem ignora o alpha)
    int stride = (f->w + 15) / 16 * 2;
    BYTE *maskbits = calloc((size_t)stride * f->h, 1);
    HCURSOR c = NULL;
    if (color && maskbits) {
        memcpy(bits, px, (size_t)f->w * f->h * 4);
        for (int y = 0; y < f->h; y++)
            for (int x = 0; x < f->w; x++)
                if (!(px[y * f->w + x] >> 24))
                    maskbits[y * stride + x / 8] |= (BYTE)(0x80 >> (x % 8));
        GdiFlush();
        HBITMAP mask = CreateBitmap(f->w, f->h, 1, 1, maskbits);
        if (mask) {
            ICONINFO ii = {FALSE, f->hx, f->hy, mask, color};
            c = CreateIconIndirect(&ii);
            DeleteObject(mask);
        }
    }
    free(maskbits);
    if (color)
        DeleteObject(color);
    return c;
}

typedef struct { BYTE *p; size_t n, cap; BOOL bad; } Bytes;

static void put(Bytes *b, const void *d, size_t n)
{
    if (b->n + n > b->cap) {
        size_t cap = (b->n + n) * 2;
        BYTE *p = realloc(b->p, cap);
        if (!p) {
            b->bad = TRUE;
            return;
        }
        b->p = p;
        b->cap = cap;
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}
static void put32(Bytes *b, uint32_t v) { put(b, &v, 4); }
static void put16(Bytes *b, uint16_t v) { put(b, &v, 2); }

// Grava um .cur de uma imagem 32 bpp (XOR com alpha + máscara AND), como os do Windows.
static void put_cur(Bytes *b, const CursorFrame *f, const uint32_t *px)
{
    uint32_t mstride = (f->w + 31) / 32 * 4, xor = (uint32_t)f->w * f->h * 4, and = mstride * f->h;
    put16(b, 0), put16(b, 2), put16(b, 1); // ICONDIR: cursor, 1 imagem
    BYTE wh[4] = {(BYTE)(f->w >= 256 ? 0 : f->w), (BYTE)(f->h >= 256 ? 0 : f->h), 0, 0};
    put(b, wh, 4);
    put16(b, (uint16_t)f->hx), put16(b, (uint16_t)f->hy);
    put32(b, 40 + xor + and), put32(b, 22);
    BITMAPINFOHEADER h = {sizeof h, f->w, f->h * 2, 1, 32, BI_RGB, xor + and};
    put(b, &h, sizeof h);
    for (int y = f->h - 1; y >= 0; y--) // de baixo para cima
        put(b, px + (size_t)y * f->w, (size_t)f->w * 4);
    BYTE *row = calloc(mstride, 1);
    for (int y = f->h - 1; row && y >= 0; y--) {
        memset(row, 0, mstride);
        for (int x = 0; x < f->w; x++)
            if (!(px[y * f->w + x] >> 24))
                row[x / 8] |= (BYTE)(0x80 >> (x % 8));
        put(b, row, mstride);
    }
    if (!row)
        b->bad = TRUE;
    free(row);
}

// Cursores animados só podem ser criados a partir de um .ani, então monta um temporário.
static HCURSOR make_animated_cursor(const CursorBase *cb, uint32_t *const *px)
{
    Bytes b = {0};
    put(&b, "RIFF", 4), put32(&b, 0), put(&b, "ACON", 4);
    put(&b, "anih", 4), put32(&b, 36);
    uint32_t anih[9] = {36, (uint32_t)cb->n, (uint32_t)cb->n, 0, 0, 0, 0, cb->rate[0], 1}; // 1: quadros em .cur
    put(&b, anih, sizeof anih);
    put(&b, "rate", 4), put32(&b, (uint32_t)cb->n * 4);
    put(&b, cb->rate, (size_t)cb->n * 4);
    put(&b, "LIST", 4);
    size_t list = b.n;
    put32(&b, 0), put(&b, "fram", 4);
    for (int i = 0; i < cb->n; i++) {
        put(&b, "icon", 4);
        size_t at = b.n;
        put32(&b, 0);
        put_cur(&b, &cb->f[i], px[i]);
        uint32_t sz = (uint32_t)(b.n - at - 4);
        if (!b.bad)
            memcpy(b.p + at, &sz, 4);
        if (sz & 1)
            put(&b, "", 1);
    }
    HCURSOR c = NULL;
    if (!b.bad) {
        uint32_t lsz = (uint32_t)(b.n - list - 4), rsz = (uint32_t)(b.n - 8);
        memcpy(b.p + list, &lsz, 4);
        memcpy(b.p + 4, &rsz, 4);
        wchar_t dir[MAX_PATH], path[MAX_PATH];
        if (GetTempPathW(MAX_PATH, dir) && GetTempFileNameW(dir, L"eys", 0, path)) {
            HANDLE fh = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
            DWORD wr = 0;
            if (fh != INVALID_HANDLE_VALUE) {
                BOOL ok = WriteFile(fh, b.p, (DWORD)b.n, &wr, NULL) && wr == b.n;
                CloseHandle(fh);
                if (ok)
                    c = LoadCursorFromFileW(path);
            }
            DeleteFileW(path);
        }
    }
    free(b.p);
    return c;
}

static void apply_cursor_grain(void)
{
    if (!g_cursor_grain || (g_value == 0 && g_warm == 0)) {
        restore_cursors();
        return;
    }
    if (g_ncur < 0)
        load_cursor_bases();
    float t = g_value / 100.0f, warm[3];
    // o cursor fica isolado por um contorno preto e o olho o vê mais claro que a tela em volta;
    // CursorDim escurece e CursorWarm esquenta só o cursor para compensar
    int w = g_warm ? g_warm + g_cur_warm : 0;
    kelvin_rgb(warm_kelvin(w > 100 ? 100 : w), warm); // multiplicadores R, G, B da Luz quente
    float k = 1 - g_cur_dim / 100.0f;
    float mul[3] = {warm[2] * k, warm[1] * k, warm[0] * k}; // ordem dos bytes: B, G, R
    for (int i = 0; i < g_ncur; i++) {
        CursorBase *b = &g_cur[i];
        uint32_t *px[MAX_FRAMES] = {0};
        BOOL ok = TRUE;
        for (int j = 0; j < b->n && ok; j++)
            if ((ok = (px[j] = malloc((size_t)b->f[j].w * b->f[j].h * 4)) != NULL))
                grain_cursor_frame(&b->f[j], b->id, t, mul, px[j]);
        HCURSOR c = !ok ? NULL : b->n == 1 ? make_static_cursor(&b->f[0], px[0]) : make_animated_cursor(b, px);
        if (c && SetSystemCursor(c, b->id)) // o sistema passa a ser dono do cursor
            g_cur_applied = TRUE;
        else if (c)
            DestroyCursor(c);
        for (int j = 0; j < b->n; j++)
            free(px[j]);
    }
}

static void load_cursor_tuning(void)
{
    g_cur_dim = (int)cfg_get(L"CursorDim", CURSOR_DIM_DEFAULT);
    g_cur_warm = (int)cfg_get(L"CursorWarm", CURSOR_WARM_DEFAULT);
    if (g_cur_dim > 50)
        g_cur_dim = CURSOR_DIM_DEFAULT;
    if (g_cur_warm > 50)
        g_cur_warm = CURSOR_WARM_DEFAULT;
}

// junta várias mudanças seguidas (arrastar o slider) numa troca só de cursores
static void schedule_cursor_grain(void) { SetTimer(g_main, TIMER_CURSOR, 120, NULL); }

// ---------------------------------------------------------------- luz quente

// O tom quente usa o filtro de cor de tela cheia da Magnification API (o mesmo dos "Filtros de
// cor" do Windows): a matriz é aplicada na composição do DWM. A Luz noturna do Windows mexe na
// tabela de gama da placa de vídeo, que passa a pontilhar os tons e faz o grão piscar.
typedef struct { float t[5][5]; } MagColorEffect;
static BOOL(WINAPI *p_MagInitialize)(void);
static BOOL(WINAPI *p_MagUninitialize)(void);
static BOOL(WINAPI *p_MagSetFullscreenColorEffect)(MagColorEffect *);
static BOOL g_mag_on;

// Cor de um corpo negro a k kelvins (aproximação de Tanner Helland), normalizada para 6500 K.
static void kelvin_rgb(float k, float rgb[3])
{
    float t = k / 100.0f;
    float g = 99.4708f * logf(t) - 161.1196f, g0 = 99.4708f * logf(65.0f) - 161.1196f;
    float b = t <= 19 ? 0 : 138.5177f * logf(t - 10) - 305.0448f, b0 = 138.5177f * logf(55.0f) - 305.0448f;
    rgb[0] = 1.0f;
    rgb[1] = clampf(g / g0, 0, 1);
    rgb[2] = clampf(b / b0, 0, 1);
}

// 0..100 -> 6500 K..2000 K, linear em mireds para os passos parecerem iguais (50 ~ 3000 K)
static float warm_kelvin(int v)
{
    float m0 = 1e6f / 6500, m1 = 1e6f / 2000;
    return 1e6f / (m0 + (m1 - m0) * v / 100.0f);
}

static void apply_warmth(void)
{
    if (!g_mag_on) {
        if (g_warm == 0)
            return;
        if (!p_MagSetFullscreenColorEffect) {
            HMODULE m = LoadLibraryExW(L"magnification.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!m)
                return;
            p_MagInitialize = (BOOL(WINAPI *)(void))(void *)GetProcAddress(m, "MagInitialize");
            p_MagUninitialize = (BOOL(WINAPI *)(void))(void *)GetProcAddress(m, "MagUninitialize");
            p_MagSetFullscreenColorEffect =
                (BOOL(WINAPI *)(MagColorEffect *))(void *)GetProcAddress(m, "MagSetFullscreenColorEffect");
            if (!p_MagInitialize || !p_MagUninitialize || !p_MagSetFullscreenColorEffect) {
                p_MagSetFullscreenColorEffect = NULL;
                return;
            }
        }
        if (!p_MagInitialize())
            return;
        g_mag_on = TRUE;
    }
    float rgb[3];
    kelvin_rgb(warm_kelvin(g_warm), rgb);
    MagColorEffect e = {{{0}}};
    e.t[0][0] = rgb[0];
    e.t[1][1] = rgb[1];
    e.t[2][2] = rgb[2];
    e.t[3][3] = e.t[4][4] = 1.0f;
    p_MagSetFullscreenColorEffect(&e);
    if (g_warm == 0) {
        p_MagUninitialize();
        g_mag_on = FALSE;
    }
}

static void set_warm(int v)
{
    v = v < 0 ? 0 : v > 100 ? 100 : v;
    if (v == g_warm)
        return;
    g_warm = v;
    apply_warmth();
    schedule_cursor_grain();
    update_tray(FALSE);
    if (g_fly)
        InvalidateRect(g_fly, NULL, FALSE);
    SetTimer(g_main, TIMER_SAVE, 500, NULL);
}

// ---------------------------------------------------------------- valor

static void set_value(int v)
{
    v = v < 0 ? 0 : v > 100 ? 100 : v;
    if (v == g_value)
        return;
    g_value = v;
    if (v > 0)
        g_last_on = v;
    apply_overlays();
    schedule_cursor_grain();
    update_tray(FALSE);
    if (g_fly)
        InvalidateRect(g_fly, NULL, FALSE);
    SetTimer(g_main, TIMER_SAVE, 500, NULL);
}

static void toggle_power(void)
{
    if (g_value > 0)
        set_value(0);
    else
        set_value(g_last_on > 0 ? g_last_on : 40);
}

static void save_config(void)
{
    cfg_set(L"Intensity", (DWORD)g_value);
    cfg_set(L"LastOn", (DWORD)g_last_on);
    cfg_set(L"HideFromCapture", (DWORD)g_hide_capture);
    cfg_set(L"Preset", (DWORD)g_preset);
    cfg_set(L"Warmth", (DWORD)g_warm);
    cfg_set(L"CursorGrain", (DWORD)g_cursor_grain);
}

// ---------------------------------------------------------------- menu

static void show_menu(HWND owner, int x, int y)
{
    HMENU m = CreatePopupMenu(), sub = CreatePopupMenu();
    for (int i = 0; i < NUM_PRESETS; i++)
        AppendMenuW(sub, MF_STRING | (i == g_preset ? MF_CHECKED : 0), IDM_PRESET0 + i, PRESETS[i].name);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sub, L"Estilo da textura");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (startup_enabled() ? MF_CHECKED : 0), IDM_STARTUP, L"Iniciar com o Windows");
    AppendMenuW(m, MF_STRING | (g_cursor_grain ? MF_CHECKED : 0), IDM_CURSOR, L"Textura e Luz quente no cursor");
    AppendMenuW(m, MF_STRING | (g_hide_capture ? MF_CHECKED : 0), IDM_CAPTURE, L"Esconder a textura em prints");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, L"Sair");
    if (owner == g_main)
        SetForegroundWindow(g_main);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, x, y, 0, owner, NULL);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(m);

    switch (cmd) {
    case IDM_STARTUP: {
        BOOL on = !startup_enabled();
        cfg_set(L"Startup", on);
        apply_startup(on);
        break;
    }
    case IDM_CAPTURE:
        g_hide_capture = !g_hide_capture;
        for (int i = 0; i < g_novl; i++)
            SetWindowDisplayAffinity(g_ov[i].hwnd, g_hide_capture ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
        save_config();
        break;
    case IDM_CURSOR:
        g_cursor_grain = !g_cursor_grain;
        apply_cursor_grain();
        save_config();
        break;
    case IDM_EXIT:
        DestroyWindow(g_main);
        break;
    default:
        if (cmd >= IDM_PRESET0 && cmd < IDM_PRESET0 + NUM_PRESETS)
            set_preset(cmd - IDM_PRESET0);
    }
}

// ---------------------------------------------------------------- painel (flyout)

static int CALLBACK font_found(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp)
{
    (void)lf; (void)tm; (void)type;
    *(BOOL *)lp = TRUE;
    return 0;
}

static BOOL font_exists(const wchar_t *face)
{
    LOGFONTW lf = {0};
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    BOOL found = FALSE;
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)font_found, (LPARAM)&found, 0);
    ReleaseDC(NULL, dc);
    return found;
}

static HFONT make_font(float px, int weight, const wchar_t *face)
{
    return CreateFontW(-(int)(px + 0.5f), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
}

static void make_fonts(void)
{
    HFONT *fs[] = {&g_f_title, &g_f_value, &g_f_small, &g_f_icon, &g_f_label};
    for (int i = 0; i < 5; i++)
        if (*fs[i])
            DeleteObject(*fs[i]);
    g_f_title = make_font(14.5f * g_s, FW_SEMIBOLD, g_text_face);
    g_f_value = make_font(21.0f * g_s, FW_NORMAL, g_text_face);
    g_f_small = make_font(13.0f * g_s, FW_NORMAL, g_text_face);
    g_f_icon = make_font(16.0f * g_s, FW_NORMAL, g_icon_face);
    g_f_label = make_font(11.5f * g_s, FW_NORMAL, g_text_face);
}

static void ensure_buffers(int w, int h)
{
    if (w == g_bw && h == g_bh && g_mdc)
        return;
    if (g_mdc) {
        DeleteDC(g_mdc);
        DeleteObject(g_mbmp);
        DeleteDC(g_sdc);
        DeleteObject(g_sbmp);
    }
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
    g_mdc = CreateCompatibleDC(NULL);
    g_mbmp = CreateDIBSection(g_mdc, &bi, DIB_RGB_COLORS, (void **)&g_mpx, NULL, 0);
    SelectObject(g_mdc, g_mbmp);
    g_sdc = CreateCompatibleDC(NULL);
    g_sbmp = CreateDIBSection(g_sdc, &bi, DIB_RGB_COLORS, (void **)&g_spx, NULL, 0);
    SelectObject(g_sdc, g_sbmp);
    SetBkMode(g_sdc, TRANSPARENT);
    SetTextColor(g_sdc, RGB(255, 255, 255));
    g_bw = w;
    g_bh = h;
}

// O GDI não escreve alpha, então o texto é desenhado branco sobre preto num buffer
// auxiliar e usado como máscara de cobertura (funciona por cima do acrílico).
static void draw_text(Buf *b, float x0, float y0, float x1, float y1, const wchar_t *str, HFONT f, UINT flags,
                      uint32_t rgb, float a)
{
    RECT r = {(int)x0, (int)y0, (int)x1, (int)y1};
    if (r.left < 0) r.left = 0;
    if (r.top < 0) r.top = 0;
    if (r.right > b->w) r.right = b->w;
    if (r.bottom > b->h) r.bottom = b->h;
    for (int y = r.top; y < r.bottom; y++)
        memset(g_spx + (size_t)y * b->w + r.left, 0, (size_t)(r.right - r.left) * 4);
    SelectObject(g_sdc, f);
    DrawTextW(g_sdc, str, -1, &r, flags | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
    GdiFlush();
    for (int y = r.top; y < r.bottom; y++)
        for (int x = r.left; x < r.right; x++) {
            uint32_t p = g_spx[(size_t)y * b->w + x];
            uint32_t cov = (((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255)) / 3;
            if (cov)
                blend(b, x, y, rgb, a * cov / 255.0f);
        }
}

static void draw_slider(Buf *b, int value, int hit, uint32_t color, uint32_t fg)
{
    float s = g_s, cy = (hit == HIT_WARM ? WARM_CY : SL_CY) * s;
    float x0 = SL_X0 * s, x1 = SL_X1 * s, tx = (SL_X0 + (SL_X1 - SL_X0) * value / 100.0f) * s;
    fill_capsule(b, x0, x1, cy, 2.0f * s, fg, g_dark ? 0.54f : 0.45f);
    fill_capsule(b, x0, tx, cy, 2.0f * s, color, 1.0f);
    if (g_dark)
        fill_circle(b, tx, cy, 10.0f * s, 0x454545, 1.0f);
    else {
        fill_circle(b, tx, cy, 10.5f * s, 0xD0D0D0, 1.0f);
        fill_circle(b, tx, cy, 10.0f * s, 0xFFFFFF, 1.0f);
    }
    float inner = g_dragging == hit ? 5.0f : (g_hover == hit ? 7.0f : 6.0f);
    fill_circle(b, tx, cy, inner * s, color, 1.0f);

    wchar_t num[8];
    wsprintfW(num, L"%d", value);
    draw_text(b, (FW - 66) * s, cy - 18 * s, (FW - 20) * s, cy + 18 * s, num, g_f_value, DT_RIGHT, fg, 1.0f);
}

static void paint_flyout(HDC hdc)
{
    RECT rc;
    GetClientRect(g_fly, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0)
        return;
    ensure_buffers(w, h);
    Buf b = {g_mpx, w, h};
    float s = g_s;
    uint32_t fg = g_dark ? 0xFFFFFF : 0x000000;
    uint32_t bg = g_dark ? 0x202020 : 0xF3F3F3;

    // fundo: semitransparente sobre o acrílico do Windows 11, sólido nos demais casos
    float bga = (g_acrylic && g_transparency) ? 0.45f : 1.0f;
    uint32_t A = (uint32_t)(bga * 255 + 0.5f);
    uint32_t pbg = A << 24 | (((bg >> 16) & 255) * A / 255) << 16 | (((bg >> 8) & 255) * A / 255) << 8 | ((bg & 255) * A / 255);
    for (int i = 0; i < w * h; i++)
        g_mpx[i] = pbg;

    int fy = (int)(FOOT_Y * s);
    fill_rect(&b, 0, fy, w, h - fy, 0x000000, g_dark ? 0.16f : 0.03f);
    fill_rect(&b, 0, fy, w, s < 1.5f ? 1 : (int)s, fg, g_dark ? 0.08f : 0.07f);

    // título
    draw_grain_icon(&b, 22 * s, 22 * s, 20 * s, fg, 1.0f);
    draw_text(&b, 52 * s, 18 * s, (FW - 20) * s, 46 * s, L"Textura da tela", g_f_title, DT_LEFT, fg, 1.0f);

    // slider da textura
    draw_slider(&b, g_value, HIT_SLIDER, g_accent, fg);

    // estilos: amostras de papel com a textura de cada predefinição
    int tw = (int)(TILE_W * s + 0.5f), th = (int)(TILE_H * s + 0.5f);
    if (tw != g_tile_w || th != g_tile_h) {
        for (int i = 0; i < NUM_PRESETS; i++) {
            free(g_tiles[i]);
            g_tiles[i] = malloc((size_t)tw * th * 4);
            render_grain(g_tiles[i], tw, th, s, 0xabcdefu, &PRESETS[i]);
        }
        g_tile_w = tw;
        g_tile_h = th;
    }
    for (int i = 0; i < NUM_PRESETS; i++) {
        float tx0 = (TILE_X0 + i * (TILE_W + TILE_GAP)) * s, ty0 = TILE_Y * s;
        BOOL sel = i == g_preset, hov = g_hover == HIT_TILE0 + i;
        if (sel)
            stroke_rrect(&b, tx0 - 3 * s, ty0 - 3 * s, tw + 6 * s, th + 6 * s, 8 * s, 2 * s, g_accent, 1.0f);
        else if (hov)
            stroke_rrect(&b, tx0 - 3 * s, ty0 - 3 * s, tw + 6 * s, th + 6 * s, 8 * s, 1.5f * s, fg, 0.4f);
        int ix = (int)tx0, iy = (int)ty0;
        for (int y = 0; y < th; y++)
            for (int x = 0; x < tw; x++) {
                float cov = rrect_cov(x + 0.5f, y + 0.5f, 0, 0, (float)tw, (float)th, 5 * s);
                if (cov <= 0)
                    continue;
                uint32_t p = g_tiles[i][y * tw + x];
                float a = (p >> 24) / 255.0f * 1.4f, v = (p & 255) / 255.0f * 1.4f;
                if (a > 1) a = 1;
                float o = 236.0f * (1 - a) + v * 255.0f;
                uint32_t gch = (uint32_t)(o > 255 ? 255 : o);
                blend(&b, ix + x, iy + y, gch << 16 | gch << 8 | gch, cov);
            }
        draw_text(&b, tx0 - 6 * s, ty0 + th + 2 * s, tx0 + tw + 6 * s, ty0 + th + LABEL_H * s, PRESETS[i].name, g_f_label,
                  DT_CENTER, sel ? g_accent : fg, sel ? 1.0f : 0.85f);
    }

    // luz quente
    fill_rect(&b, (int)(22 * s), (int)((WARM_TITLE_Y - 2) * s), (int)((FW - 44) * s), s < 1.5f ? 1 : (int)s, fg,
              g_dark ? 0.08f : 0.07f);
    draw_text(&b, 18 * s, WARM_TITLE_Y * s, 46 * s, (WARM_TITLE_Y + 28) * s, L"\xE708", g_f_icon, DT_CENTER,
              g_warm > 0 ? WARM_COLOR : fg, 1.0f);
    draw_text(&b, 52 * s, WARM_TITLE_Y * s, (FW - 20) * s, (WARM_TITLE_Y + 28) * s, L"Luz quente", g_f_title, DT_LEFT, fg,
              1.0f);
    draw_slider(&b, g_warm, HIT_WARM, WARM_COLOR, fg);

    // rodapé
    float fcy = (FOOT_Y + (FH - FOOT_Y) / 2.0f) * s;
    draw_text(&b, 22 * s, FOOT_Y * s, (POWER_CX - 24) * s, FH * s, PRESETS[g_preset].desc, g_f_small,
              DT_LEFT | DT_END_ELLIPSIS, fg, 0.85f);
    struct { int cx, hit; const wchar_t *glyph; uint32_t color; } btns[] = {
        {POWER_CX, HIT_POWER, L"\xE7E8", g_value > 0 ? g_accent : fg},
        {GEAR_CX, HIT_GEAR, L"\xE713", fg},
    };
    for (int i = 0; i < 2; i++) {
        float bx = btns[i].cx * s - BTN * s / 2, by = fcy - BTN * s / 2;
        if (g_hover == btns[i].hit)
            fill_rrect(&b, bx, by, BTN * s, BTN * s, 4 * s, fg, g_dark ? 0.08f : 0.06f);
        draw_text(&b, bx, by, bx + BTN * s, by + BTN * s, btns[i].glyph, g_f_icon, DT_CENTER, btns[i].color, 1.0f);
    }

    BitBlt(hdc, 0, 0, w, h, g_mdc, 0, 0, SRCCOPY);
}

static int hit_test(float mx, float my)
{
    if (mx >= SL_X0 - 14 && mx <= SL_X1 + 14 && my >= SL_CY - 18 && my <= SL_CY + 18)
        return HIT_SLIDER;
    if (mx >= SL_X0 - 14 && mx <= SL_X1 + 14 && my >= WARM_CY - 18 && my <= WARM_CY + 18)
        return HIT_WARM;
    if (my >= TILE_Y - 3 && my <= TILE_Y + TILE_H + LABEL_H)
        for (int i = 0; i < NUM_PRESETS; i++) {
            float tx0 = TILE_X0 + i * (TILE_W + TILE_GAP);
            if (mx >= tx0 - TILE_GAP / 2 && mx <= tx0 + TILE_W + TILE_GAP / 2)
                return HIT_TILE0 + i;
        }
    float fcy = FOOT_Y + (FH - FOOT_Y) / 2.0f;
    if (fabsf(my - fcy) <= BTN / 2) {
        if (fabsf(mx - POWER_CX) <= BTN / 2)
            return HIT_POWER;
        if (fabsf(mx - GEAR_CX) <= BTN / 2)
            return HIT_GEAR;
    }
    return HIT_NONE;
}

static void drag_to(float mx)
{
    int v = (int)floorf((mx - SL_X0) / (SL_X1 - SL_X0) * 100.0f + 0.5f);
    if (g_dragging == HIT_WARM)
        set_warm(v);
    else
        set_value(v);
}

static void apply_flyout_theme(void)
{
    BOOL dark = g_dark;
    DwmSetWindowAttribute(g_fly, DWMWA_USE_IMMERSIVE_DARK_MODE_, &dark, sizeof dark);
}

static void hide_flyout(void)
{
    if (!IsWindowVisible(g_fly))
        return;
    KillTimer(g_fly, TIMER_ANIM);
    if (g_dragging) {
        g_dragging = 0;
        ReleaseCapture();
    }
    ShowWindow(g_fly, SW_HIDE);
    g_hidden_at = GetTickCount();
}

// Abre o painel no canto da área de trabalho junto à barra de tarefas, como o Twinkle Tray.
static void show_flyout(void)
{
    POINT pt;
    GetCursorPos(&pt);
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    RECT tr;
    HMONITOR hm = (tray && GetWindowRect(tray, &tr) && PtInRect(&tr, pt)) ? MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY)
                                                                        : MonitorFromWindow(tray, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {sizeof mi};
    GetMonitorInfoW(hm, &mi);
    g_s = monitor_dpi(hm) / 96.0f;
    load_theme();
    apply_flyout_theme();
    make_fonts();

    int w = (int)(FW * g_s + 0.5f), h = (int)(FH * g_s + 0.5f), m = (int)(12 * g_s + 0.5f);
    RECT wa = mi.rcWork, mr = mi.rcMonitor;
    int x = wa.right - w - m, y = wa.bottom - h - m, dx = 0, dy = (int)(14 * g_s);
    if (wa.left > mr.left) { x = wa.left + m; dx = -(int)(14 * g_s); dy = 0; }   // barra à esquerda
    else if (wa.right < mr.right) { dx = (int)(14 * g_s); dy = 0; }               // barra à direita
    if (wa.top > mr.top) { y = wa.top + m; dy = -(int)(14 * g_s); dx = 0; }        // barra no topo

    g_anim_x = x; g_anim_y = y; g_anim_dx = dx; g_anim_dy = dy; g_anim_step = 0;
    g_hover = HIT_NONE;
    g_wheel = 0;
    SetWindowPos(g_fly, HWND_TOPMOST, x + dx, y + dy, w, h, SWP_SHOWWINDOW);
    SetForegroundWindow(g_fly);
    SetFocus(g_fly);
    InvalidateRect(g_fly, NULL, FALSE);
    SetTimer(g_fly, TIMER_ANIM, 10, NULL);
}

static LRESULT CALLBACK flyout_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp)
            return 0;
        break;
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_flyout(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DPICHANGED:
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE)
            hide_flyout();
        return 0;
    case WM_TIMER:
        if (wp == TIMER_ANIM) {
            float t = ++g_anim_step / 9.0f;
            if (t >= 1.0f) {
                t = 1.0f;
                KillTimer(hwnd, TIMER_ANIM);
            }
            float e = (1 - t) * (1 - t) * (1 - t);
            SetWindowPos(hwnd, NULL, g_anim_x + (int)(g_anim_dx * e), g_anim_y + (int)(g_anim_dy * e), 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        float mx = GET_X_LPARAM(lp) / g_s, my = GET_Y_LPARAM(lp) / g_s;
        int hit = hit_test(mx, my);
        if (hit == HIT_SLIDER || hit == HIT_WARM) {
            g_dragging = hit;
            SetCapture(hwnd);
            drag_to(mx);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (hit == HIT_POWER) {
            toggle_power();
        } else if (hit >= HIT_TILE0 && hit < HIT_TILE0 + NUM_PRESETS) {
            set_preset(hit - HIT_TILE0);
        } else if (hit == HIT_GEAR) {
            POINT p = {(int)((GEAR_CX - BTN / 2) * g_s), (int)((FOOT_Y + 4) * g_s)};
            ClientToScreen(hwnd, &p);
            show_menu(hwnd, p.x, p.y);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        float mx = GET_X_LPARAM(lp) / g_s, my = GET_Y_LPARAM(lp) / g_s;
        if (g_dragging) {
            drag_to(mx);
            return 0;
        }
        int hit = hit_test(mx, my);
        if (hit != g_hover) {
            g_hover = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_hover = HIT_NONE;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP:
        if (g_dragging) {
            g_dragging = 0;
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (g_dragging) {
            g_dragging = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEWHEEL:
        g_wheel += GET_WHEEL_DELTA_WPARAM(wp);
        if (g_wheel >= WHEEL_DELTA || g_wheel <= -WHEEL_DELTA) {
            int steps = g_wheel / WHEEL_DELTA;
            g_wheel -= steps * WHEEL_DELTA;
            if (g_hover == HIT_WARM)
                set_warm(g_warm + steps * 2);
            else
                set_value(g_value + steps * 2);
        }
        return 0;
    case WM_KEYDOWN:
        switch (wp) {
        case VK_ESCAPE: hide_flyout(); break;
        // Shift + setas: luz quente
        case VK_LEFT: case VK_DOWN:
            if (GetKeyState(VK_SHIFT) < 0) set_warm(g_warm - 1); else set_value(g_value - 1);
            break;
        case VK_RIGHT: case VK_UP:
            if (GetKeyState(VK_SHIFT) < 0) set_warm(g_warm + 1); else set_value(g_value + 1);
            break;
        case VK_NEXT: set_value(g_value - 10); break;
        case VK_PRIOR: set_value(g_value + 10); break;
        case VK_HOME: set_value(0); break;
        case VK_END: set_value(100); break;
        case VK_SPACE: toggle_power(); break;
        default:
            if (wp >= '1' && wp < '1' + NUM_PRESETS)
                set_preset((int)(wp - '1'));
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void create_flyout(void)
{
    g_fly = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, CLS_FLYOUT, APP_NAME, WS_POPUP | WS_THICKFRAME, 0, 0,
                            FW, FH, NULL, NULL, g_inst, NULL);
    int corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(g_fly, DWMWA_WINDOW_CORNER_PREFERENCE_, &corner, sizeof corner);
    apply_flyout_theme();
    int backdrop = 3; // DWMSBT_TRANSIENTWINDOW (acrílico)
    g_acrylic = g_build >= 22621 && SUCCEEDED(DwmSetWindowAttribute(g_fly, DWMWA_SYSTEMBACKDROP_TYPE_, &backdrop, sizeof backdrop));
    MARGINS mg = {0, 0, 0, 1};
    if (g_acrylic)
        mg = (MARGINS){-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(g_fly, &mg);
}

// ---------------------------------------------------------------- janela principal (oculta)

static LRESULT CALLBACK main_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TASKBARCREATED && msg != 0) {
        add_tray();
        return 0;
    }
    switch (msg) {
    case WM_TRAY:
        switch (LOWORD(lp)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
            if (IsWindowVisible(g_fly))
                hide_flyout();
            else if (GetTickCount() - g_hidden_at > 300)
                show_flyout();
            break;
        case WM_CONTEXTMENU:
            show_menu(hwnd, GET_X_LPARAM(wp), GET_Y_LPARAM(wp));
            break;
        }
        return 0;
    case WM_SHOWFLYOUT:
        show_flyout();
        return 0;
    case WM_DISPLAYCHANGE:
        SetTimer(hwnd, TIMER_DISPLAY, 800, NULL);
        return 0;
    case WM_SETTINGCHANGE:
        if (wp == SPI_SETCURSORS) {
            // o usuário trocou o esquema ou o tamanho do ponteiro: copia os novos cursores
            g_cur_applied = FALSE;
            free_cursor_bases();
            schedule_cursor_grain();
            return 0;
        }
        if (lp && lstrcmpW((const wchar_t *)lp, L"ImmersiveColorSet") == 0) {
            load_theme();
            update_tray(TRUE);
            apply_flyout_theme();
            InvalidateRect(g_fly, NULL, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_DISPLAY) {
            KillTimer(hwnd, TIMER_DISPLAY);
            if (layout_changed())
                rebuild_overlays();
        } else if (wp == TIMER_CURSOR) {
            KillTimer(hwnd, TIMER_CURSOR);
            apply_cursor_grain();
        } else if (wp == TIMER_SAVE) {
            KillTimer(hwnd, TIMER_SAVE);
            save_config();
        } else if (wp == TIMER_TOPMOST) {
            if (cursor_scheme_changed() && g_cur_applied) {
                free_cursor_bases();
                schedule_cursor_grain();
            }
            // Outras janelas "sempre no topo" podem passar por cima. Só reordena quando
            // isso realmente aconteceu: mexer no z-order à toa faz o DWM recompor a tela
            // e pode causar piscadas.
            if (g_value > 0 && !IsWindowVisible(g_fly))
                for (int i = 0; i < g_novl; i++)
                    if (!g_ov[i].in_band && overlay_covered(&g_ov[i]))
                        SetWindowPos(g_ov[i].hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
        return 0;
    case WM_QUERYENDSESSION:
        save_config();
        return TRUE;
    case WM_QUITAPP:
        DestroyWindow(hwnd);
        return 0;
    case WM_RELOADCURSOR:
        load_cursor_tuning();
        schedule_cursor_grain();
        return 0;
    case WM_DESTROY:
        save_config();
        restore_cursors();
        if (g_warm > 0) {
            g_warm = 0;
            apply_warmth(); // devolve as cores normais
        }
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        destroy_overlays();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK overlay_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCHITTEST)
        return HTTRANSPARENT;
    if (msg == WM_MOUSEACTIVATE)
        return MA_NOACTIVATE;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static DWORD windows_build(void)
{
    typedef LONG(WINAPI * RtlGetVersion_t)(OSVERSIONINFOW *);
    OSVERSIONINFOW vi = {sizeof vi};
    RtlGetVersion_t f = (RtlGetVersion_t)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (f && f(&vi) == 0)
        return vi.dwBuildNumber;
    return 0;
}

// Menus de contexto escuros quando o Windows está no tema escuro (uxtheme, ordinais 135/136).
static void allow_dark_menus(void)
{
    if (g_build < 18362)
        return;
    HMODULE ux = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ux)
        return;
    int(WINAPI * set_mode)(int) = (int(WINAPI *)(int))(void *)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    void(WINAPI * flush)(void) = (void(WINAPI *)(void))(void *)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (set_mode)
        set_mode(1); // AllowDark: segue o tema do sistema
    if (flush)
        flush();
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show)
{
    (void)prev; (void)show;
    g_inst = inst;

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\EyeSight.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // já está rodando: abre o painel da instância existente, ou a fecha com --exit
        // (usado pelo instalador para o Eye Sight devolver cursores e cores antes de sair)
        HWND w = FindWindowW(CLS_MAIN, NULL);
        if (w && wcsstr(cmdline, L"--exit")) {
            PostMessageW(w, WM_QUITAPP, 0, 0);
        } else if (w && wcsstr(cmdline, L"--reload-cursor")) {
            PostMessageW(w, WM_RELOADCURSOR, 0, 0);
        } else if (w) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessageW(w, WM_SHOWFLYOUT, 0, 0);
        }
        return 0;
    }

    g_build = windows_build();
    allow_dark_menus();
    if (font_exists(L"Segoe UI Variable Text"))
        lstrcpyW(g_text_face, L"Segoe UI Variable Text");
    if (font_exists(L"Segoe Fluent Icons"))
        lstrcpyW(g_icon_face, L"Segoe Fluent Icons");
    load_theme();

    BOOL first_run = cfg_get(L"Intensity", 0xFFFFFFFF) == 0xFFFFFFFF;
    g_value = (int)cfg_get(L"Intensity", 0);
    if (g_value > 100)
        g_value = 0;
    g_last_on = (int)cfg_get(L"LastOn", 40);
    if (g_last_on < 1 || g_last_on > 100)
        g_last_on = 40;
    g_hide_capture = cfg_get(L"HideFromCapture", 1) != 0;
    g_cursor_grain = cfg_get(L"CursorGrain", 1) != 0;
    load_cursor_tuning();
    g_preset = (int)cfg_get(L"Preset", DEFAULT_PRESET);
    if (g_preset < 0 || g_preset >= NUM_PRESETS)
        g_preset = DEFAULT_PRESET;
    g_warm = (int)cfg_get(L"Warmth", 0);
    if (g_warm > 100)
        g_warm = 0;
    if (startup_enabled() && should_claim_startup())
        apply_startup(TRUE);
    if (first_run)
        save_config();

    WNDCLASSEXW wc = {sizeof wc};
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpfnWndProc = main_proc;
    wc.lpszClassName = CLS_MAIN;
    RegisterClassExW(&wc);
    wc.lpfnWndProc = overlay_proc;
    wc.lpszClassName = CLS_OVERLAY;
    g_overlay_atom = RegisterClassExW(&wc);
    g_create_in_band = (CreateWindowInBand_t)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "CreateWindowInBand");
    wc.lpfnWndProc = flyout_proc;
    wc.lpszClassName = CLS_FLYOUT;
    RegisterClassExW(&wc);

    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    g_main = CreateWindowExW(WS_EX_TOOLWINDOW, CLS_MAIN, APP_NAME, WS_POPUP, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    // na versão instalada (uiAccess) o Windows bloqueia mensagens vindas de processos comuns,
    // como a segunda instância pedindo para abrir o painel
    ChangeWindowMessageFilterEx(g_main, WM_SHOWFLYOUT, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(g_main, WM_QUITAPP, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(g_main, WM_RELOADCURSOR, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(g_main, WM_SETTINGCHANGE, MSGFLT_ALLOW, NULL); // troca de esquema de cursores e de tema
    create_flyout();
    rebuild_overlays();
    SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, 0); // tira o grão que tenha ficado de uma execução interrompida
    cursor_scheme_changed(); // começa a observar o esquema de cursores
    apply_cursor_grain();
    apply_warmth();
    add_tray();
    SetTimer(g_main, TIMER_TOPMOST, 2000, NULL);

    // aberto manualmente pela primeira vez: mostra o painel para o usuário achar o app
    if (first_run && !wcsstr(cmdline, L"--tray"))
        show_flyout();

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    ReleaseMutex(mutex);
    return 0;
}
