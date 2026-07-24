/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * shell.c — Luma Shell: window manager, taskbar, Start menu, and apps
 *
 * A double-buffered, immediate-mode desktop. Every frame the whole scene is
 * composed into the backbuffer (wallpaper -> windows -> Start menu -> taskbar
 * -> cursor) and flushed with fb_present(). Windows are singletons keyed by
 * app id; the Start menu opens them and the taskbar switches between them.
 * ==========================================================================*/

#include "shell.h"
#include "fb.h"
#include "input.h"
#include "timer.h"
#include "rtc.h"
#include "serial.h"
#include "string.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Apps                                                               */
/* ------------------------------------------------------------------ */
enum { APP_WELCOME, APP_NOTEPAD, APP_CALC, APP_CLOCK,
       APP_SETTINGS, APP_PAINT, APP_TERM, APP_COUNT };

static const char *app_name[APP_COUNT] = {
    "Welcome", "Notepad", "Calculator", "Clock", "Settings", "Paint", "Terminal"
};
static const uint32_t app_color[APP_COUNT] = {
    0x006366F1u, 0x0022C55Eu, 0x00F59E0Bu, 0x003B82F6u,
    0x008B8B9Au, 0x00EF4444u, 0x00111827u
};

struct win { int x, y, w, h; uint8_t open; };
static struct win wins[APP_COUNT];
static int  zlist[APP_COUNT];   /* open apps, back(0)..front(zn-1) */
static int  zn;

/* ------------------------------------------------------------------ */
/* Theme                                                              */
/* ------------------------------------------------------------------ */
static uint32_t TH_accent = 0x006366F1u;
static int      TH_dark   = 0;

static uint32_t co_win(void)  { return TH_dark ? 0x001C1C28u : 0x00FAFAFDu; }
static uint32_t co_fg(void)   { return TH_dark ? 0x00E8E8F2u : 0x00242432u; }
static uint32_t co_sub(void)  { return TH_dark ? 0x009494A8u : 0x006E6E82u; }
static uint32_t co_bar(void)  { return TH_dark ? 0x00141420u : 0x00ECEEF6u; }
static uint32_t co_barfg(void){ return TH_dark ? 0x00D0D0E0u : 0x00303040u; }
static uint32_t co_panel(void){ return TH_dark ? 0x00202030u : 0x00FFFFFFu; }
static const uint32_t C_WHITE  = 0x00FFFFFFu;
static const uint32_t C_SHADOW = 0x000C0A1Au;

/* ------------------------------------------------------------------ */
/* Geometry                                                           */
/* ------------------------------------------------------------------ */
#define TB     34          /* title bar height           */
#define PAD    12
#define TASKH  46          /* taskbar height             */
static int SW, SH;         /* screen dimensions          */

/* wallpapers */
#define MAX_WP 12
static const uint8_t *wp_px[MAX_WP];
static uint32_t wp_w[MAX_WP], wp_h[MAX_WP];
static int wp_count, wp_active;

/* interaction state */
static int cx, cy;                 /* cursor                     */
static uint8_t prev_btn;
static int dragging = -1;          /* app being dragged, or -1   */
static int drag_ox, drag_oy;
static int start_open;

/* ---- app state ---- */
static char np_buf[600]; static int np_len;
static long calc_acc, calc_cur; static char calc_op; static int calc_fresh = 1;
#define CANVAS_W 380
#define CANVAS_H 240
static uint32_t paint_canvas[CANVAS_W * CANVAS_H];
static uint32_t paint_color = 0x00EF4444u;
static int paint_inited;
#define TERM_ROWS 10
#define TERM_COLS 44
static char term_lines[TERM_ROWS][TERM_COLS + 1];
static int  term_n;
static char term_in[TERM_COLS + 1]; static int term_ilen;

/* ------------------------------------------------------------------ */
/* Small helpers                                                      */
/* ------------------------------------------------------------------ */
static int in_r(int px, int py, int x, int y, int w, int h)
{ return px >= x && px < x + w && py >= y && py < y + h; }

static int clampi(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }

static int str_eq(const char *a, const char *b)
{ while (*a && *a == *b) { a++; b++; } return *a == *b; }

/* ------------------------------------------------------------------ */
/* Window stack management                                            */
/* ------------------------------------------------------------------ */
static int z_index(int app)
{ for (int i = 0; i < zn; i++) if (zlist[i] == app) return i; return -1; }

static void focus_app(int app)
{
    int i = z_index(app);
    if (i < 0) return;
    for (; i < zn - 1; i++) zlist[i] = zlist[i + 1];
    zlist[zn - 1] = app;
}

static void open_app(int app)
{
    if (!wins[app].open) {
        /* cascade default position */
        int base = 60 + app * 26;
        wins[app].x = base;
        wins[app].y = 60 + app * 20;
        wins[app].open = 1;
        zlist[zn++] = app;
    }
    focus_app(app);
}

static void close_app(int app)
{
    wins[app].open = 0;
    int i = z_index(app);
    if (i < 0) return;
    for (; i < zn - 1; i++) zlist[i] = zlist[i + 1];
    zn--;
}

static int content_x(int a) { return wins[a].x + PAD; }
static int content_y(int a) { return wins[a].y + TB + PAD; }
static int content_w(int a) { return wins[a].w - 2 * PAD; }
static int content_h(int a) { return wins[a].h - TB - 2 * PAD; }

/* ------------------------------------------------------------------ */
/* App content rendering                                              */
/* ------------------------------------------------------------------ */
static const char *calc_str(char *buf)
{
    long v = calc_cur; int i = 0; int neg = v < 0; unsigned long u = neg ? -v : v;
    char t[24]; int n = 0;
    if (u == 0) t[n++] = '0';
    while (u && n < 20) { t[n++] = (char)('0' + u % 10); u /= 10; }
    if (neg) buf[i++] = '-';
    while (n) buf[i++] = t[--n];
    buf[i] = 0;
    return buf;
}

static const char *calc_keys[16] = {
    "7","8","9","/", "4","5","6","*", "1","2","3","-", "C","0","=","+"
};

static void draw_content(int a)
{
    int x = content_x(a), y = content_y(a), w = content_w(a), h = content_h(a);
    uint32_t fg = co_fg(), sub = co_sub();

    switch (a) {
    case APP_WELCOME:
        fb_print(x, y, "Aurelian OS", TH_accent, 3);
        fb_print(x, y + 34, "codename \"Luma\"", sub, 2);
        fb_print(x, y + 56, "Aurelion kernel v1.0.0-dev", sub, 2);
        fb_print(x, y + 90, "The Luma Shell is running.", fg, 2);
        fb_print(x, y + 114, "Open Start (bottom-left) to", fg, 2);
        fb_print(x, y + 136, "launch apps. Drag a window", fg, 2);
        fb_print(x, y + 158, "by its title bar; x closes it.", fg, 2);
        break;
    case APP_NOTEPAD: {
        fb_fill_rect(x, y, w, h, TH_dark ? 0x00121218u : C_WHITE);
        fb_rect_border(x, y, w, h, 1, sub);
        int tx = x + 6, ty = y + 6, col = 0;
        int maxcol = (w - 12) / 16;
        for (int i = 0; i < np_len; i++) {
            char ch = np_buf[i];
            if (ch == '\n' || col >= maxcol) { ty += 20; col = 0; if (ch == '\n') continue; }
            fb_draw_char(tx + col * 16, ty, ch, fg, 2);
            col++;
        }
        /* caret */
        fb_fill_rect(tx + col * 16, ty, 2, 16, TH_accent);
        break;
    }
    case APP_CALC: {
        char buf[24];
        fb_fill_rect(x, y, w, 44, TH_dark ? 0x00101018u : 0x00F0F0F5u);
        fb_rect_border(x, y, w, 44, 1, sub);
        const char *s = calc_str(buf);
        fb_print(x + w - fb_text_width(s, 3) - 8, y + 10, s, fg, 3);
        int gy = y + 54, bw = (w - 18) / 4, bh = 34;
        for (int i = 0; i < 16; i++) {
            int bx = x + (i % 4) * (bw + 6);
            int by = gy + (i / 4) * (bh + 6);
            int isop = (i % 4 == 3) || i == 12 || i == 14;
            fb_rounded_rect(bx, by, bw, bh, 8, isop ? TH_accent : (TH_dark ? 0x002A2A3Au : 0x00E4E4EE));
            uint32_t tc = isop ? C_WHITE : fg;
            fb_print(bx + (bw - fb_text_width(calc_keys[i], 2)) / 2, by + 9, calc_keys[i], tc, 2);
        }
        break;
    }
    case APP_CLOCK: {
        struct rtc_time t; rtc_read(&t);
        char b[16]; int i = 0;
        b[i++] = (char)('0' + t.hour / 10); b[i++] = (char)('0' + t.hour % 10); b[i++] = ':';
        b[i++] = (char)('0' + t.min / 10);  b[i++] = (char)('0' + t.min % 10);  b[i++] = ':';
        b[i++] = (char)('0' + t.sec / 10);  b[i++] = (char)('0' + t.sec % 10);  b[i] = 0;
        fb_print(x + (w - fb_text_width(b, 5)) / 2, y + 30, b, fg, 5);
        int dx = x + (w - 0) / 2;
        int lx = x + w / 2 - 90;
        int yy = y + 100;
        lx = fb_print_uint(lx, yy, t.year, sub, 2);
        lx = fb_print(lx, yy, "-", sub, 2);
        if (t.month < 10) lx = fb_print(lx, yy, "0", sub, 2);
        lx = fb_print_uint(lx, yy, t.month, sub, 2);
        lx = fb_print(lx, yy, "-", sub, 2);
        if (t.day < 10) lx = fb_print(lx, yy, "0", sub, 2);
        fb_print_uint(lx, yy, t.day, sub, 2);
        (void)dx;
        break;
    }
    case APP_SETTINGS: {
        fb_print(x, y, "Accent color", sub, 2);
        static const uint32_t sw[6] = {
            0x006366F1u, 0x00A855F7u, 0x003B82F6u, 0x0022C55Eu, 0x00EF4444u, 0x00F59E0Bu
        };
        for (int i = 0; i < 6; i++) {
            int bx = x + i * 44, by = y + 26;
            fb_rounded_rect(bx, by, 34, 34, 8, sw[i]);
            if (sw[i] == TH_accent) fb_rect_border(bx - 2, by - 2, 38, 38, 2, co_fg());
        }
        fb_print(x, y + 84, "Appearance", sub, 2);
        fb_rounded_rect(x, y + 110, 150, 34, 8, TH_dark ? TH_accent : 0x00E4E4EE);
        fb_print(x + 14, y + 119, TH_dark ? "Dark  (on)" : "Dark  (off)",
                 TH_dark ? C_WHITE : co_fg(), 2);
        if (wp_count > 0) {
            fb_print(x, y + 160, "Wallpaper", sub, 2);
            for (int k = 0; k < wp_count; k++) {
                int col = k % 4, row = k / 4;
                int tx = x + col * 80, tyy = y + 186 + row * 66;
                for (int py = 0; py < 54; py++)
                    for (int px = 0; px < 72; px++) {
                        uint32_t sxp = (uint32_t)px * wp_w[k] / 72;
                        uint32_t syp = (uint32_t)py * wp_h[k] / 54;
                        const uint8_t *pp = wp_px[k] + ((uint64_t)syp * wp_w[k] + sxp) * 4;
                        fb_put_pixel((uint32_t)(tx + px), (uint32_t)(tyy + py),
                                     (uint32_t)pp[0] | ((uint32_t)pp[1] << 8) | ((uint32_t)pp[2] << 16));
                    }
                if (k == wp_active) fb_rect_border(tx - 2, tyy - 2, 76, 58, 2, TH_accent);
                else                fb_rect_border(tx, tyy, 72, 54, 1, co_sub());
            }
        }
        break;
    }
    case APP_PAINT: {
        int cvx = x, cvy = y;
        fb_rect_border(cvx - 1, cvy - 1, CANVAS_W + 2, CANVAS_H + 2, 1, sub);
        for (int j = 0; j < CANVAS_H; j++)
            for (int i = 0; i < CANVAS_W; i++)
                fb_put_pixel((uint32_t)(cvx + i), (uint32_t)(cvy + j),
                             paint_canvas[j * CANVAS_W + i]);
        static const uint32_t pal[6] = {
            0x00EF4444u, 0x00F59E0Bu, 0x0022C55Eu, 0x003B82F6u, 0x00A855F7u, 0x00111111u
        };
        for (int i = 0; i < 6; i++) {
            int bx = x + i * 34, by = cvy + CANVAS_H + 8;
            fb_rounded_rect(bx, by, 26, 26, 6, pal[i]);
            if (pal[i] == paint_color) fb_rect_border(bx - 2, by - 2, 30, 30, 2, co_fg());
        }
        break;
    }
    case APP_TERM: {
        fb_fill_rect(x, y, w, h, 0x000C0C14u);
        fb_rect_border(x, y, w, h, 1, sub);
        int ty = y + 6;
        for (int i = 0; i < term_n; i++, ty += 18)
            fb_print(x + 6, ty, term_lines[i], 0x0055F0A0u, 2);
        int px = fb_print(x + 6, ty, "> ", 0x0080B0FFu, 2);
        px = fb_print(px, ty, term_in, 0x00E8E8F2u, 2);
        fb_fill_rect(px, ty, 2, 16, 0x00E8E8F2u);
        break;
    }
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* App input                                                          */
/* ------------------------------------------------------------------ */
static void calc_digit(int d)
{ if (calc_fresh) { calc_cur = 0; calc_fresh = 0; } calc_cur = calc_cur * 10 + d; }

static void calc_apply(void)
{
    switch (calc_op) {
    case '+': calc_acc += calc_cur; break;
    case '-': calc_acc -= calc_cur; break;
    case '*': calc_acc *= calc_cur; break;
    case '/': calc_acc = calc_cur ? calc_acc / calc_cur : 0; break;
    default:  calc_acc = calc_cur; break;
    }
    calc_cur = calc_acc;
}

static void calc_key(const char *k)
{
    char c = k[0];
    if (c >= '0' && c <= '9') { calc_digit(c - '0'); }
    else if (c == 'C') { calc_acc = calc_cur = 0; calc_op = 0; calc_fresh = 1; }
    else if (c == '=') { calc_apply(); calc_op = 0; calc_fresh = 1; }
    else { /* + - * / */ calc_apply(); calc_op = c; calc_fresh = 1; }
}

static void term_run(void)
{
    if (term_n >= TERM_ROWS) {
        for (int i = 1; i < TERM_ROWS; i++)
            for (int j = 0; j <= TERM_COLS; j++) term_lines[i - 1][j] = term_lines[i][j];
        term_n--;
    }
    char *out = term_lines[term_n];
    const char *in = term_in;
    if (str_eq(in, "help"))       { const char *m = "help ver clear echo uptime about"; int j=0; while(m[j]&&j<TERM_COLS){out[j]=m[j];j++;} out[j]=0; }
    else if (str_eq(in, "ver"))   { const char *m = "Aurelion v1.0.0-dev / Luma Shell"; int j=0; while(m[j]&&j<TERM_COLS){out[j]=m[j];j++;} out[j]=0; }
    else if (str_eq(in, "about")) { const char *m = "Aurelian OS - Prism UI"; int j=0; while(m[j]&&j<TERM_COLS){out[j]=m[j];j++;} out[j]=0; }
    else if (str_eq(in, "clear")) { term_n = 0; term_ilen = 0; term_in[0] = 0; return; }
    else if (str_eq(in, "uptime")){ char b[32]; unsigned long s=(unsigned long)(timer_ticks()/100); int j=0; unsigned long m=s/60; s%=60; unsigned long tmp=m; char r[12]; int rn=0; if(tmp==0)r[rn++]='0'; while(tmp){r[rn++]=(char)('0'+tmp%10);tmp/=10;} while(rn)b[j++]=r[--rn]; b[j++]='m'; b[j++]=' '; tmp=s; rn=0; if(tmp==0)r[rn++]='0'; while(tmp){r[rn++]=(char)('0'+tmp%10);tmp/=10;} while(rn)b[j++]=r[--rn]; b[j++]='s'; b[j]=0; int k=0; while(b[k]&&k<TERM_COLS){out[k]=b[k];k++;} out[k]=0; }
    else if (in[0]=='e'&&in[1]=='c'&&in[2]=='h'&&in[3]=='o'&&in[4]==' ') { int j=0; const char*p=in+5; while(p[j]&&j<TERM_COLS){out[j]=p[j];j++;} out[j]=0; }
    else if (in[0]==0) { out[0]=0; }
    else { int j=0; const char*m="unknown command"; while(m[j]&&j<TERM_COLS){out[j]=m[j];j++;} out[j]=0; }
    term_n++;
    term_ilen = 0; term_in[0] = 0;
}

static void app_click(int a, int lx, int ly, int w, int h)
{
    (void)w; (void)h;
    if (a == APP_CALC) {
        int bw = (content_w(a) - 18) / 4, bh = 34, gy = 54;
        for (int i = 0; i < 16; i++) {
            int bx = (i % 4) * (bw + 6);
            int by = gy + (i / 4) * (bh + 6);
            if (in_r(lx, ly, bx, by, bw, bh)) { calc_key(calc_keys[i]); return; }
        }
    } else if (a == APP_SETTINGS) {
        static const uint32_t sw[6] = {
            0x006366F1u, 0x00A855F7u, 0x003B82F6u, 0x0022C55Eu, 0x00EF4444u, 0x00F59E0Bu
        };
        for (int i = 0; i < 6; i++)
            if (in_r(lx, ly, i * 44, 26, 34, 34)) { TH_accent = sw[i]; return; }
        if (in_r(lx, ly, 0, 110, 150, 34)) { TH_dark = !TH_dark; return; }
        for (int k = 0; k < wp_count; k++) {
            int col = k % 4, row = k / 4;
            if (in_r(lx, ly, col * 80, 186 + row * 66, 72, 54)) { wp_active = k; return; }
        }
    } else if (a == APP_PAINT) {
        static const uint32_t pal[6] = {
            0x00EF4444u, 0x00F59E0Bu, 0x0022C55Eu, 0x003B82F6u, 0x00A855F7u, 0x00111111u
        };
        for (int i = 0; i < 6; i++)
            if (in_r(lx, ly, i * 34, CANVAS_H + 8, 26, 26)) { paint_color = pal[i]; return; }
    }
}

/* draw into the paint canvas while the button is held over it */
static void paint_stroke(int a)
{
    int lx = cx - content_x(a), ly = cy - content_y(a);
    if (lx < 0 || ly < 0 || lx >= CANVAS_W || ly >= CANVAS_H) return;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            int px = lx + i, py = ly + j;
            if (px >= 0 && py >= 0 && px < CANVAS_W && py < CANVAS_H)
                paint_canvas[py * CANVAS_W + px] = paint_color;
        }
}

static void app_key(int a, char ch)
{
    if (a == APP_NOTEPAD) {
        if (ch == '\b') { if (np_len > 0) np_buf[--np_len] = 0; }
        else if ((ch >= ' ' || ch == '\n') && np_len < (int)sizeof(np_buf) - 1) {
            np_buf[np_len++] = ch; np_buf[np_len] = 0;
        }
    } else if (a == APP_TERM) {
        if (ch == '\b') { if (term_ilen > 0) term_in[--term_ilen] = 0; }
        else if (ch == '\n') { term_run(); }
        else if (ch >= ' ' && term_ilen < TERM_COLS) { term_in[term_ilen++] = ch; term_in[term_ilen] = 0; }
    } else if (a == APP_CALC) {
        if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            char k[2] = { ch, 0 }; calc_key(k);
        } else if (ch == '\n' || ch == '=') { char k[2] = { '=', 0 }; calc_key(k); }
        else if (ch == 'c' || ch == 'C') { char k[2] = { 'C', 0 }; calc_key(k); }
    }
}

/* ------------------------------------------------------------------ */
/* Window + chrome rendering                                          */
/* ------------------------------------------------------------------ */
static void draw_window(int a, int focused)
{
    struct win *win = &wins[a];
    fb_rounded_rect(win->x + 5, win->y + 8, win->w, win->h, 12, C_SHADOW);
    fb_rounded_rect(win->x, win->y, win->w, win->h, 12, co_win());
    /* title bar */
    fb_rounded_rect(win->x, win->y, win->w, TB, 12, focused ? TH_accent : co_sub());
    fb_fill_rect(win->x, win->y + 16, win->w, TB - 16, focused ? TH_accent : co_sub());
    fb_print(win->x + 12, win->y + 9, app_name[a], C_WHITE, 2);
    /* close button */
    int cbx = win->x + win->w - 26, cby = win->y + 8;
    fb_rounded_rect(cbx, cby, 18, 18, 5, 0x00E86A5Eu);
    fb_print(cbx + 4, cby + 2, "x", C_WHITE, 2);
    draw_content(a);
}

static void draw_taskbar(void)
{
    int ty = SH - TASKH;
    fb_fill_rect(0, ty, SW, TASKH, co_bar());
    fb_fill_rect(0, ty, SW, 1, TH_dark ? 0x00303048u : 0x00D8D8E4u);

    /* Start button */
    fb_rounded_rect(10, ty + 8, 30, 30, 8, TH_accent);
    fb_fill_rect(18, ty + 16, 6, 6, C_WHITE);
    fb_fill_rect(26, ty + 16, 6, 6, C_WHITE);
    fb_fill_rect(18, ty + 24, 6, 6, C_WHITE);
    fb_fill_rect(26, ty + 24, 6, 6, C_WHITE);

    /* running-window buttons (centered-ish) */
    int bx = 56;
    for (int i = 0; i < zn; i++) {
        int a = zlist[i];
        int focused = (i == zn - 1);
        fb_rounded_rect(bx, ty + 8, 140, 30, 8, focused ? (TH_dark ? 0x002A2A40u : 0x00DADAE8u)
                                                        : (TH_dark ? 0x001C1C2Cu : 0x00E6E6F0u));
        fb_rounded_rect(bx + 8, ty + 17, 12, 12, 3, app_color[a]);
        fb_print(bx + 26, ty + 15, app_name[a], co_barfg(), 2);
        bx += 148;
    }

    /* clock */
    struct rtc_time t; rtc_read(&t);
    char b[8];
    b[0] = (char)('0' + t.hour / 10); b[1] = (char)('0' + t.hour % 10); b[2] = ':';
    b[3] = (char)('0' + t.min / 10);  b[4] = (char)('0' + t.min % 10);  b[5] = 0;
    fb_print(SW - fb_text_width(b, 2) - 16, ty + 15, b, co_barfg(), 2);
}

static void draw_start_menu(void)
{
    int mw = 320, mh = 380;
    int mx = 10, my = SH - TASKH - mh - 8;
    fb_rounded_rect(mx + 4, my + 6, mw, mh, 14, C_SHADOW);
    fb_rounded_rect(mx, my, mw, mh, 14, co_panel());
    fb_print(mx + 20, my + 18, "Aurelian OS", TH_accent, 2);
    fb_print(mx + 20, my + 40, "All apps", co_sub(), 2);
    for (int i = 0; i < APP_COUNT; i++) {
        int col = i % 2, row = i / 2;
        int tx = mx + 18 + col * 148, tyy = my + 66 + row * 66;
        fb_rounded_rect(tx, tyy, 138, 56, 10, TH_dark ? 0x0026263Au : 0x00F0F0F6u);
        fb_rounded_rect(tx + 12, tyy + 14, 28, 28, 7, app_color[i]);
        fb_print(tx + 50, tyy + 20, app_name[i], co_fg(), 2);
    }
}

static int start_menu_hit(int px, int py)
{
    int mw = 320, mh = 380;
    int mx = 10, my = SH - TASKH - mh - 8;
    if (!in_r(px, py, mx, my, mw, mh)) return -2;    /* outside menu */
    for (int i = 0; i < APP_COUNT; i++) {
        int col = i % 2, row = i / 2;
        int tx = mx + 18 + col * 148, tyy = my + 66 + row * 66;
        if (in_r(px, py, tx, tyy, 138, 56)) return i;
    }
    return -1;   /* inside menu, no tile */
}

/* ------------------------------------------------------------------ */
/* Compositor                                                         */
/* ------------------------------------------------------------------ */
static void draw_background(void)
{
    if (wp_count > 0)
        fb_blit_raw32(wp_px[wp_active], wp_w[wp_active], wp_h[wp_active]);
    else
        fb_vgradient(0, 0, SW, SH, 0x00141230u, 0x004A2A82u);
}

static void compose(void)
{
    draw_background();
    for (int i = 0; i < zn; i++)
        draw_window(zlist[i], i == zn - 1);
    if (start_open)
        draw_start_menu();
    draw_taskbar();
    fb_draw_cursor(cx, cy, C_WHITE, 0x00141414u);
    fb_present();
}

/* ------------------------------------------------------------------ */
/* Input handling                                                     */
/* ------------------------------------------------------------------ */
static int topmost_at(int px, int py)
{
    for (int i = zn - 1; i >= 0; i--) {
        int a = zlist[i];
        if (in_r(px, py, wins[a].x, wins[a].y, wins[a].w, wins[a].h)) return a;
    }
    return -1;
}

static void on_press(void)
{
    int ty = SH - TASKH;
    /* Start button */
    if (in_r(cx, cy, 10, ty + 8, 30, 30)) { start_open = !start_open; return; }
    /* Start menu */
    if (start_open) {
        int hit = start_menu_hit(cx, cy);
        if (hit >= 0) { open_app(hit); start_open = 0; return; }
        if (hit == -1) return;                 /* inside menu, ignore */
        start_open = 0;                        /* clicked outside: close, continue */
    }
    /* taskbar buttons */
    if (cy >= ty) {
        int bx = 56;
        for (int i = 0; i < zn; i++) {
            if (in_r(cx, cy, bx, ty + 8, 140, 30)) { focus_app(zlist[i]); return; }
            bx += 148;
        }
        return;
    }
    /* windows */
    int a = topmost_at(cx, cy);
    if (a >= 0) {
        focus_app(a);
        int cbx = wins[a].x + wins[a].w - 26, cby = wins[a].y + 8;
        if (in_r(cx, cy, cbx, cby, 18, 18)) { close_app(a); return; }
        if (cy < wins[a].y + TB) {             /* title bar -> drag */
            dragging = a; drag_ox = cx - wins[a].x; drag_oy = cy - wins[a].y; return;
        }
        /* content click */
        if (a == APP_PAINT) { paint_stroke(a); return; }
        app_click(a, cx - content_x(a), cy - content_y(a), content_w(a), content_h(a));
        return;
    }
    start_open = 0;
}

void shell_run(int n, const uint64_t *wp_addr, const uint32_t *wp_size)
{
    SW = fb_width(); SH = fb_height();

    /* validate each wallpaper module (AOWP header) */
    wp_count = 0; wp_active = 0;
    for (int i = 0; i < n && wp_count < MAX_WP; i++) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)wp_addr[i];
        if (wp_size[i] > 12 && p[0]=='A'&&p[1]=='O'&&p[2]=='W'&&p[3]=='P') {
            uint32_t ww = *(const uint32_t *)(p + 4), wh = *(const uint32_t *)(p + 8);
            if ((uint64_t)ww * wh * 4 + 12 <= wp_size[i]) {
                wp_px[wp_count] = p + 12; wp_w[wp_count] = ww; wp_h[wp_count] = wh; wp_count++;
            }
        }
    }

    /* default window sizes */
    wins[APP_WELCOME]  = (struct win){ 0,0,520,300,0 };
    wins[APP_NOTEPAD]  = (struct win){ 0,0,460,340,0 };
    wins[APP_CALC]     = (struct win){ 0,0,250,320,0 };
    wins[APP_CLOCK]    = (struct win){ 0,0,360,200,0 };
    wins[APP_SETTINGS] = (struct win){ 0,0,400,430,0 };
    wins[APP_PAINT]    = (struct win){ 0,0,CANVAS_W+2*PAD, CANVAS_H+2*PAD+TB+52, 0 };
    wins[APP_TERM]     = (struct win){ 0,0,400,280,0 };

    for (int i = 0; i < CANVAS_W * CANVAS_H; i++) paint_canvas[i] = 0x00FFFFFFu;
    paint_inited = 1;

    open_app(APP_WELCOME);
    cx = SW / 2; cy = SH / 2;

    serial_write("[shell] Luma Shell running\n");
    compose();

    uint64_t last_sec = 0;
    struct input_event e;

    for (;;) {
        int changed = 0;

        while (input_poll(&e)) {
            changed = 1;
            if (e.type == INPUT_MOUSE) {
                cx = clampi(cx + e.dx, 0, SW - 1);
                cy = clampi(cy - e.dy, 0, SH - 1);
                uint8_t lb = e.buttons & 1u;
                int press = lb && !(prev_btn & 1u);

                if (dragging >= 0) {
                    if (lb) {
                        wins[dragging].x = clampi(cx - drag_ox, -wins[dragging].w + 40, SW - 40);
                        wins[dragging].y = clampi(cy - drag_oy, 0, SH - TASKH - 20);
                    } else {
                        dragging = -1;
                    }
                } else if (press) {
                    on_press();
                } else if (lb && zn > 0 && zlist[zn - 1] == APP_PAINT) {
                    paint_stroke(APP_PAINT);   /* drag-to-draw */
                }
                prev_btn = e.buttons;
            } else if (e.type == INPUT_KEY_DOWN && e.ascii) {
                if (zn > 0) app_key(zlist[zn - 1], e.ascii);
            }
        }

        uint64_t sec = timer_ticks() / 100;
        if (sec != last_sec) { last_sec = sec; changed = 1; }   /* clocks tick */

        if (changed) compose();
        __asm__ volatile ("hlt");
    }
}
