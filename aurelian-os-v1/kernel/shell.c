/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * shell.c — Luma Shell: window manager, taskbar, Start menu and apps
 *
 * An immediate-mode desktop drawn with the Prism design language (Fluent
 * inspired): Mica-translucent surfaces sampled from the blurred wallpaper,
 * 8px rounded windows with soft shadows, a centred taskbar and a Start menu.
 *
 * Everything is laid out from fb_width()/fb_height() and a UI scale factor, so
 * the shell adapts to whatever mode the loader gives us.
 * ==========================================================================*/

#include "shell.h"
#include "fb.h"
#include "fs.h"
#include "input.h"
#include "timer.h"
#include "rtc.h"
#include "pci.h"
#include "serial.h"
#include "string.h"
#include <stdint.h>

/* ================================================================== */
/* 1. Theme                                                            */
/* ================================================================== */
struct theme {
    uint32_t mica, layer, layer2, fg, fg2, stroke, ctrl, ctrl_hi, ctrl_lo;
    uint8_t  mica_a;
};

static const struct theme TH_LIGHT = {
    0x00F3F3F3u, 0x00FFFFFFu, 0x00F6F6F8u, 0x001A1A1Au, 0x005D5D62u,
    0x00E2E2E6u, 0x00FCFCFCu, 0x00F0F0F3u, 0x00E7E7EBu, 0xDE
};
static const struct theme TH_DARK = {
    0x00202020u, 0x002C2C2Eu, 0x00272729u, 0x00F4F4F6u, 0x00A9A9B0u,
    0x003B3B3Fu, 0x00323234u, 0x003C3C40u, 0x002A2A2Cu, 0xD6
};

static struct theme T;
static int      dark_mode;
static uint32_t accent = 0x000078D4u;   /* Windows-blue style default */

static const uint32_t ACCENTS[8] = {
    0x000078D4u, 0x006366F1u, 0x008B5CF6u, 0x00C026D3u,
    0x00E11D48u, 0x00EA580Cu, 0x00059669u, 0x000891B2u
};

static void theme_apply(void) { T = dark_mode ? TH_DARK : TH_LIGHT; }

/* ================================================================== */
/* 2. Metrics (resolution independent)                                 */
/* ================================================================== */
static int SW, SH;      /* screen size            */
static int S;           /* ui scale (1 or 2)      */
static int TBAR;        /* window title bar       */
static int TASKH;       /* taskbar height         */
static int RAD, RADS;   /* window / control radii */
static int PAD;         /* content padding        */
static int LH;          /* text line height       */

static void metrics_init(void)
{
    SW = fb_width();
    SH = fb_height();
    S  = (SH >= 1200) ? 2 : 1;
    TBAR  = 34 * S;
    TASKH = 48 * S;
    RAD   = 8 * S;
    RADS  = 4 * S;
    PAD   = 16 * S;
    LH    = 20 * S;
}

/* ================================================================== */
/* 3. Apps                                                             */
/* ================================================================== */
enum { APP_EXPLORER, APP_NOTEPAD, APP_CALC, APP_CLOCK,
       APP_PAINT, APP_TERM, APP_SETTINGS, APP_ABOUT, APP_COUNT };

static const char *app_title[APP_COUNT] = {
    "File Explorer", "Notepad", "Calculator", "Clock",
    "Paint", "Terminal", "Settings", "About Aurelian OS"
};
static const char *app_short[APP_COUNT] = {
    "Files", "Notepad", "Calc", "Clock", "Paint", "Terminal", "Settings", "About"
};

struct win { int x, y, w, h; uint8_t open, min; };
static struct win wins[APP_COUNT];
static int zlist[APP_COUNT], zn;

/* ================================================================== */
/* 4. Small helpers                                                    */
/* ================================================================== */
static int in_r(int px, int py, int x, int y, int w, int h)
{ return px >= x && px < x + w && py >= y && py < y + h; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int str_eq(const char *a, const char *b)
{ while (*a && *a == *b) { a++; b++; } return *a == *b; }

static int str_pre(const char *s, const char *pre)
{ while (*pre) { if (*s != *pre) return 0; s++; pre++; } return 1; }

static int text_bold(int x, int y, const char *s, uint32_t c, int sc)
{ fb_text(x, y, s, c, sc); return fb_text(x + 1, y, s, c, sc); }

/* 4-digit hex, for PCI ids. */
static int hex16(int x, int y, uint16_t v, uint32_t c)
{
    static const char *H = "0123456789ABCDEF";
    char b[5];
    for (int i = 0; i < 4; i++) b[i] = H[(v >> ((3 - i) * 4)) & 0xF];
    b[4] = 0;
    return fb_text(x, y, b, c, S);
}

/* cursor position, needed for hover states */
static int cx, cy;

/* ================================================================== */
/* 5. App icons (drawn from primitives)                                */
/* ================================================================== */
static void icon(int a, int x, int y, int sz)
{
    int q = sz / 4;
    switch (a) {
    case APP_EXPLORER:                                  /* folder */
        fb_round(x, y + q / 2, sz * 4 / 9, q, RADS / 2 + 1, 0x00D9A02Bu);
        fb_round(x, y + q, sz, sz - q - q / 2, RADS / 2 + 1, 0x00F5BE4Bu);
        fb_fill_a(x + q / 2, y + q + q / 3, sz - q, 1, 0x00FFFFFFu, 0x50);
        break;
    case APP_NOTEPAD: {                                 /* page + lines */
        fb_round(x, y, sz * 5 / 6, sz, RADS / 2 + 1, 0x00FBFBFDu);
        fb_round_border(x, y, sz * 5 / 6, sz, RADS / 2 + 1, 1, 0x00C8C8D0u, 0xFF);
        for (int i = 0; i < 3; i++)
            fb_fill(x + q / 2, y + q + i * (q * 2 / 3), sz / 2, 1 + S / 2, 0x006E7BE8u);
        break;
    }
    case APP_CALC:                                      /* keypad */
        fb_round(x, y, sz, sz, RADS / 2 + 1, 0x00343A46u);
        fb_fill(x + q / 2, y + q / 2, sz - q, q * 2 / 3, 0x008FD0FFu);
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 3; i++)
                fb_fill(x + q / 2 + i * (q * 3 / 4), y + q * 3 / 2 + j * (q * 3 / 4),
                        q / 2, q / 2, 0x00B9C0CCu);
        break;
    case APP_CLOCK: {                                   /* dial */
        int r = sz / 2;
        fb_round(x, y, sz, sz, r, 0x00FFFFFFu);
        fb_round_border(x, y, sz, sz, r, 1 + S / 2, 0x004A5568u, 0xFF);
        fb_fill(x + r, y + r / 2, 1 + S / 2, r / 2, 0x004A5568u);      /* hour  */
        fb_fill(x + r, y + r, r / 2, 1 + S / 2, 0x00E11D48u);          /* minute*/
        break;
    }
    case APP_PAINT: {                                   /* palette */
        int r = sz / 2;
        fb_round(x, y, sz, sz, r, 0x00FDFDFFu);
        fb_round_border(x, y, sz, sz, r, 1, 0x00C8C8D0u, 0xFF);
        fb_round(x + q / 2, y + q / 2, q, q, q / 2, 0x00E11D48u);
        fb_round(x + sz - q * 3 / 2, y + q / 2, q, q, q / 2, 0x000891B2u);
        fb_round(x + q, y + sz - q * 3 / 2, q, q, q / 2, 0x00059669u);
        break;
    }
    case APP_TERM:                                      /* console */
        fb_round(x, y, sz, sz, RADS / 2 + 1, 0x000E1116u);
        fb_round_border(x, y, sz, sz, RADS / 2 + 1, 1, 0x003A4150u, 0xFF);
        fb_text(x + q / 2, y + q, ">", 0x0055F0A0u, (sz >= 24) ? 2 : 1);
        break;
    case APP_SETTINGS: {                                /* gear */
        int r = sz / 2, nub = q * 3 / 5, half = nub / 2;
        uint32_t gc = dark_mode ? 0x00B4B4BEu : 0x007A7A86u;
        /* four nubs at N/S/W/E, then the body, then the hub cut-out */
        fb_round(x + r - half, y,              nub, q, 1, gc);
        fb_round(x + r - half, y + sz - q,     nub, q, 1, gc);
        fb_round(x,            y + r - half,   q,   nub, 1, gc);
        fb_round(x + sz - q,   y + r - half,   q,   nub, 1, gc);
        fb_round(x + q / 2, y + q / 2, sz - q, sz - q, (sz - q) / 2, gc);
        fb_round(x + r - q / 2, y + r - q / 2, q, q, q / 2, T.layer);
        break;
    }
    default:                                            /* about / logo */
        fb_round(x, y, sz, sz, RADS / 2 + 1, accent);
        fb_fill(x + sz / 2 - 1, y + q / 2, 2 + S, sz - q, 0x00FFFFFFu);
        fb_fill(x + q / 2, y + sz / 2 - 1, sz - q, 2 + S, 0x00FFFFFFu);
        break;
    }
}

/* ================================================================== */
/* 6. Window stack                                                     */
/* ================================================================== */
static int z_index(int a)
{ for (int i = 0; i < zn; i++) if (zlist[i] == a) return i; return -1; }

static void focus_app(int a)
{
    int i = z_index(a);
    if (i < 0) return;
    for (; i < zn - 1; i++) zlist[i] = zlist[i + 1];
    zlist[zn - 1] = a;
}

static void open_app(int a)
{
    if (!wins[a].open) {
        wins[a].open = 1;
        wins[a].min  = 0;
        /* cascade, keeping the window on-screen */
        int step = 28 * S;
        wins[a].x = clampi(60 * S + (a % 5) * step, 0, SW - wins[a].w);
        wins[a].y = clampi(50 * S + (a % 5) * step, 0, SH - TASKH - wins[a].h);
        zlist[zn++] = a;
    }
    wins[a].min = 0;
    focus_app(a);
}

static void close_app(int a)
{
    wins[a].open = 0;
    int i = z_index(a);
    if (i < 0) return;
    for (; i < zn - 1; i++) zlist[i] = zlist[i + 1];
    zn--;
}

static int top_app(void)
{
    for (int i = zn - 1; i >= 0; i--) if (!wins[zlist[i]].min) return zlist[i];
    return -1;
}

/* content origin (below the title bar) */
static int co_x(int a) { return wins[a].x; }
static int co_y(int a) { return wins[a].y + TBAR; }
static int co_w(int a) { return wins[a].w; }
static int co_h(int a) { return wins[a].h - TBAR; }

/* ================================================================== */
/* 7. App state                                                        */
/* ================================================================== */
/* -- Explorer -- */
static int ex_dir, ex_sel;

/* -- Notepad -- */
static char np_buf[1024];
static int  np_len, np_file = -1, np_dirty;

/* -- Calculator -- */
static long calc_acc, calc_cur;
static char calc_op;
static int  calc_fresh = 1;
static const char *CK[20] = {
    "C","<","%","/",
    "7","8","9","*",
    "4","5","6","-",
    "1","2","3","+",
    "0",".","+/-","="
};

/* -- Paint -- */
#define PW 620
#define PH 360
static uint32_t paint_px[PW * PH];
static uint32_t paint_col = 0x00E11D48u;
static int      paint_size = 3;

/* -- Terminal -- */
#define TROWS 14
#define TCOLS 60
static char term_out[TROWS][TCOLS + 1];
static int  term_n;
static char term_in[TCOLS + 1];
static int  term_len;

/* -- Settings -- */
static int set_page;                    /* 0 personalise 1 system 2 about */

/* -- wallpapers -- */
#define MAX_WP 12
static const uint8_t *wp_px[MAX_WP];
static uint32_t wp_w[MAX_WP], wp_h[MAX_WP];
static int wp_count, wp_active = -1;
static uint32_t g_mem_kib;

/* input counters — surfaced in Settings > System so a user can confirm the
 * PS/2 drivers are delivering events (there is no way to inject mouse input
 * from the host, so this is how the mouse gets verified). */
static uint32_t mouse_events, key_events;

static void wallpaper_select(int i)
{
    if (i < 0 || i >= wp_count || i == wp_active) return;
    wp_active = i;
    fb_set_wallpaper(wp_px[i], wp_w[i], wp_h[i]);
}

/* ================================================================== */
/* 8. Shared widgets                                                   */
/* ================================================================== */
static int button(int x, int y, int w, int h, const char *label, int accent_fill)
{
    int hov = in_r(cx, cy, x, y, w, h);
    uint32_t fill = accent_fill ? (hov ? shade(accent, 18) : accent)
                                : (hov ? T.ctrl_hi : T.ctrl);
    fb_round(x, y, w, h, RADS, fill);
    if (!accent_fill) fb_round_border(x, y, w, h, RADS, 1, T.stroke, 0xFF);
    fb_text_c(x, y + (h - 16 * S) / 2, w, label,
              accent_fill ? 0x00FFFFFFu : T.fg, S);
    return hov;
}

/* A section header inside an app. */
static void section(int x, int y, const char *s)
{ text_bold(x, y, s, T.fg2, S); }

/* ================================================================== */
/* 9. Apps — drawing                                                   */
/* ================================================================== */
static void draw_explorer(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a), h = co_h(a);
    int tool = 40 * S, side = 150 * S, row = 28 * S;

    /* toolbar */
    int bx = ox + 10 * S, by = oy + (tool - 26 * S) / 2;
    int can_up = (ex_dir != fs_root());
    int hov = in_r(cx, cy, bx, by, 30 * S, 26 * S);
    fb_round(bx, by, 30 * S, 26 * S, RADS, hov && can_up ? T.ctrl_hi : T.ctrl);
    fb_text(bx + 11 * S, by + 5 * S, "<", can_up ? T.fg : T.stroke, S);

    char path[128];
    fs_path(ex_dir, path, sizeof(path));
    int pbw = w - 68 * S;
    fb_round(bx + 38 * S, by, pbw, 26 * S, RADS, T.layer2);
    fb_round_border(bx + 38 * S, by, pbw, 26 * S, RADS, 1, T.stroke, 0xFF);
    fb_text(bx + 46 * S, by + 5 * S, path, T.fg, S);

    /* sidebar */
    fb_fill(ox, oy + tool, side, h - tool, T.layer2);
    fb_fill(ox + side, oy + tool, 1, h - tool, T.stroke);
    static const char *qn[6] = { "Home", "Documents", "Pictures", "Music", "Downloads", "System" };
    for (int i = 0; i < 6; i++) {
        int iy = oy + tool + 8 * S + i * row;
        int target = (i == 0) ? fs_root() : fs_child(fs_root(), i - 1);
        int on = (target == ex_dir);
        int hv = in_r(cx, cy, ox + 6 * S, iy, side - 12 * S, row - 2 * S);
        if (on || hv)
            fb_round(ox + 6 * S, iy, side - 12 * S, row - 2 * S, RADS,
                     on ? blend(T.layer2, accent, 0x30) : T.ctrl_hi);
        if (on) fb_round(ox + 6 * S, iy + row / 4, 3 * S, row / 2, 2, accent);
        icon(i == 0 ? APP_ABOUT : APP_EXPLORER, ox + 16 * S, iy + (row - 2 * S - 14 * S) / 2, 14 * S);
        fb_text(ox + 38 * S, iy + (row - 2 * S - 16 * S) / 2, qn[i], T.fg, S);
    }

    /* header + file list */
    int lx = ox + side + 1, ly = oy + tool, lw = w - side - 1;

    /* Be upfront about what this is: an in-memory tree, not a disk. */
    fb_fill_a(lx, ly, lw, 22 * S, 0x00F5BE4Bu, dark_mode ? 0x22 : 0x38);
    fb_text(lx + 12 * S, ly + 3 * S,
            "RAM disk - in memory only, resets on restart", T.fg2, S);
    ly += 24 * S;

    fb_text(lx + 14 * S, ly + 6 * S, "Name", T.fg2, S);
    fb_text(lx + lw - 76 * S, ly + 6 * S, "Size", T.fg2, S);
    fb_fill(lx + 10 * S, ly + 26 * S, lw - 20 * S, 1, T.stroke);

    int n = fs_child_count(ex_dir);
    for (int i = 0; i < n; i++) {
        int node = fs_child(ex_dir, i);
        int ry = ly + 32 * S + i * row;
        if (ry + row > oy + h) break;
        int hv = in_r(cx, cy, lx + 6 * S, ry, lw - 12 * S, row);
        if (i == ex_sel)
            fb_round(lx + 6 * S, ry, lw - 12 * S, row, RADS, blend(T.layer, accent, 0x28));
        else if (hv)
            fb_round(lx + 6 * S, ry, lw - 12 * S, row, RADS, T.ctrl_hi);
        icon(fs_is_dir(node) ? APP_EXPLORER : APP_NOTEPAD,
             lx + 14 * S, ry + (row - 16 * S) / 2, 16 * S);
        fb_text(lx + 38 * S, ry + (row - 16 * S) / 2, fs_name(node), T.fg, S);
        if (!fs_is_dir(node)) {
            int nx = fb_num(lx + lw - 76 * S, ry + (row - 16 * S) / 2,
                            fs_size(node), T.fg2, S);
            fb_text(nx, ry + (row - 16 * S) / 2, " B", T.fg2, S);
        } else {
            fb_text(lx + lw - 76 * S, ry + (row - 16 * S) / 2, "folder", T.fg2, S);
        }
    }
    if (n == 0)
        fb_text(lx + 20 * S, ly + 40 * S, "This folder is empty", T.fg2, S);
}

static void draw_notepad(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a), h = co_h(a);
    int tool = 36 * S;

    button(ox + 10 * S, oy + 5 * S, 62 * S, 26 * S, "Save", 1);
    button(ox + 78 * S, oy + 5 * S, 62 * S, 26 * S, "New", 0);
    const char *nm = (np_file >= 0) ? fs_name(np_file) : "untitled";
    int tx = fb_text(ox + 152 * S, oy + 10 * S, nm, T.fg2, S);
    if (np_dirty) fb_text(tx, oy + 10 * S, " *", accent, S);

    int ax = ox + 10 * S, ay = oy + tool, aw = w - 20 * S, ah = h - tool - 10 * S;
    fb_round(ax, ay, aw, ah, RADS, T.layer2);
    fb_round_border(ax, ay, aw, ah, RADS, 1, T.stroke, 0xFF);

    int cols = (aw - 16 * S) / (8 * S);
    int rows = (ah - 12 * S) / (16 * S);
    int col = 0, line = 0;
    for (int i = 0; i < np_len && line < rows; i++) {
        char ch = np_buf[i];
        if (ch == '\n') { line++; col = 0; continue; }
        if (col >= cols) { line++; col = 0; if (line >= rows) break; }
        fb_glyph(ax + 8 * S + col * 8 * S, ay + 6 * S + line * 16 * S, ch, T.fg, S);
        col++;
    }
    if (line < rows && (timer_ticks() / 50) % 2)
        fb_fill(ax + 8 * S + col * 8 * S, ay + 6 * S + line * 16 * S, 1 + S, 16 * S, accent);
}

static const char *calc_text(char *buf)
{
    long v = calc_cur;
    int i = 0, neg = v < 0;
    unsigned long u = neg ? (unsigned long)(-v) : (unsigned long)v;
    char t[24]; int n = 0;
    if (u == 0) t[n++] = '0';
    while (u && n < 20) { t[n++] = (char)('0' + u % 10); u /= 10; }
    if (neg) buf[i++] = '-';
    while (n) buf[i++] = t[--n];
    buf[i] = 0;
    return buf;
}

static void draw_calc(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a), h = co_h(a);
    int disp = 60 * S;
    char b[24];
    const char *s = calc_text(b);
    fb_text(ox + w - fb_text_w(s, 2 * S) - 16 * S, oy + disp - 34 * S - 4 * S,
            s, T.fg, 2 * S);
    if (calc_op)
        fb_text(ox + 16 * S, oy + 12 * S,
                calc_op == '+' ? "+" : calc_op == '-' ? "-" :
                calc_op == '*' ? "x" : calc_op == '/' ? "/" : "%", T.fg2, S);

    int gx = ox + 8 * S, gy = oy + disp, gw = w - 16 * S, gh = h - disp - 8 * S;
    int bw = (gw - 3 * 6 * S) / 4, bh = (gh - 4 * 6 * S) / 5;
    for (int i = 0; i < 20; i++) {
        int c = i % 4, r = i / 4;
        int bx = gx + c * (bw + 6 * S), by = gy + r * (bh + 6 * S);
        int op = (c == 3) || i == 0 || i == 1;
        int eq = (i == 19);
        int hv = in_r(cx, cy, bx, by, bw, bh);
        uint32_t fill = eq ? (hv ? shade(accent, 18) : accent)
                           : op ? (hv ? shade(T.ctrl_lo, dark_mode ? 18 : -12) : T.ctrl_lo)
                                : (hv ? T.ctrl_hi : T.ctrl);
        fb_round(bx, by, bw, bh, RADS, fill);
        if (!eq) fb_round_border(bx, by, bw, bh, RADS, 1, T.stroke, 0xFF);
        fb_text_c(bx, by + (bh - 16 * S) / 2, bw, CK[i],
                  eq ? 0x00FFFFFFu : T.fg, S);
    }
}

static void draw_clock(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a);
    struct rtc_time t; rtc_read(&t);

    char b[10]; int i = 0;
    b[i++] = (char)('0' + t.hour / 10); b[i++] = (char)('0' + t.hour % 10); b[i++] = ':';
    b[i++] = (char)('0' + t.min / 10);  b[i++] = (char)('0' + t.min % 10);  b[i++] = ':';
    b[i++] = (char)('0' + t.sec / 10);  b[i++] = (char)('0' + t.sec % 10);  b[i] = 0;
    fb_text_c(ox, oy + 18 * S, w, b, T.fg, 3 * S);

    int dx = ox + (w - 10 * 8 * S) / 2, dy = oy + 18 * S + 52 * S;
    int p = fb_num(dx, dy, t.year, T.fg2, S);
    p = fb_text(p, dy, "-", T.fg2, S);
    p = fb_num2(p, dy, t.month, T.fg2, S);
    p = fb_text(p, dy, "-", T.fg2, S);
    fb_num2(p, dy, t.day, T.fg2, S);

    uint32_t sec = (uint32_t)(timer_ticks() / 100);
    int uy = dy + LH + 6 * S;
    int q = ox + (w - 14 * 8 * S) / 2;
    q = fb_text(q, uy, "up ", T.fg2, S);
    q = fb_num(q, uy, sec / 60, accent, S);
    q = fb_text(q, uy, "m ", T.fg2, S);
    q = fb_num(q, uy, sec % 60, accent, S);
    fb_text(q, uy, "s", T.fg2, S);
}

static void draw_paint(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a), h = co_h(a);
    int bar = 40 * S;
    static const uint32_t pal[8] = {
        0x00E11D48u, 0x00EA580Cu, 0x00F59E0Bu, 0x00059669u,
        0x000891B2u, 0x000078D4u, 0x008B5CF6u, 0x00161616u
    };
    for (int i = 0; i < 8; i++) {
        int bx = ox + 10 * S + i * 30 * S, by = oy + 8 * S;
        fb_round(bx, by, 24 * S, 24 * S, RADS, pal[i]);
        if (pal[i] == paint_col)
            fb_round_border(bx - 2 * S, by - 2 * S, 28 * S, 28 * S, RADS + 2, 2 * S, T.fg, 0xFF);
    }
    for (int i = 0; i < 3; i++) {
        int r = 2 + i * 3;
        int bx = ox + 262 * S + i * 28 * S, by = oy + 8 * S;
        int on = (paint_size == 1 + i * 3);
        fb_round(bx, by, 24 * S, 24 * S, RADS, on ? blend(T.ctrl, accent, 0x40) : T.ctrl);
        fb_round(bx + 12 * S - r * S / 2, by + 12 * S - r * S / 2, r * S, r * S, r * S / 2, T.fg);
    }
    button(ox + w - 76 * S, oy + 8 * S, 66 * S, 24 * S, "Clear", 0);

    int caw = w - 20 * S, cah = h - bar - 10 * S;
    if (caw > PW) caw = PW;
    if (cah > PH) cah = PH;
    int ax = ox + 10 * S, ay = oy + bar;
    fb_round_border(ax - 1, ay - 1, caw + 2, cah + 2, 2, 1, T.stroke, 0xFF);
    for (int j = 0; j < cah; j++)
        for (int i = 0; i < caw; i++)
            fb_put_pixel(ax + i, ay + j, paint_px[j * PW + i]);
}

static void draw_term(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a), h = co_h(a);
    fb_round(ox + 8 * S, oy + 6 * S, w - 16 * S, h - 14 * S, RADS, 0x000C0F14u);
    fb_round_border(ox + 8 * S, oy + 6 * S, w - 16 * S, h - 14 * S, RADS, 1, 0x00303844u, 0xFF);
    int tx = ox + 18 * S, ty = oy + 14 * S;
    for (int i = 0; i < term_n; i++, ty += 16 * S)
        fb_text(tx, ty, term_out[i], 0x0088E0B0u, S);
    int p = fb_text(tx, ty, "aurelian> ", 0x0078B4FFu, S);
    p = fb_text(p, ty, term_in, 0x00EAEAF0u, S);
    if ((timer_ticks() / 50) % 2) fb_fill(p, ty, 8 * S, 16 * S, 0x0088E0B0u);
}

static void draw_settings(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a), h = co_h(a);
    int nav = 140 * S;
    fb_fill(ox, oy, nav, h, T.layer2);
    fb_fill(ox + nav, oy, 1, h, T.stroke);
    static const char *pages[4] = { "Personalise", "System", "Devices", "About" };
    for (int i = 0; i < 4; i++) {
        int iy = oy + 12 * S + i * 32 * S;
        int on = (set_page == i);
        int hv = in_r(cx, cy, ox + 6 * S, iy, nav - 12 * S, 28 * S);
        if (on || hv)
            fb_round(ox + 6 * S, iy, nav - 12 * S, 28 * S, RADS,
                     on ? blend(T.layer2, accent, 0x30) : T.ctrl_hi);
        if (on) fb_round(ox + 6 * S, iy + 7 * S, 3 * S, 14 * S, 2, accent);
        fb_text(ox + 20 * S, iy + 6 * S, pages[i], T.fg, S);
    }

    int px = ox + nav + PAD, py = oy + PAD, pw = w - nav - 2 * PAD;
    if (set_page == 0) {
        section(px, py, "Accent colour");
        for (int i = 0; i < 8; i++) {
            int bx = px + i * 34 * S, by = py + LH + 4 * S;
            fb_round(bx, by, 26 * S, 26 * S, RADS, ACCENTS[i]);
            if (ACCENTS[i] == accent)
                fb_round_border(bx - 3 * S, by - 3 * S, 32 * S, 32 * S, RADS + 2, 2 * S, T.fg, 0xFF);
        }
        int ty2 = py + LH + 48 * S;
        section(px, ty2, "Theme");
        int sw = 84 * S;
        for (int i = 0; i < 2; i++) {
            int bx = px + i * (sw + 8 * S), by = ty2 + LH + 4 * S;
            int on = (dark_mode == i);
            int hv = in_r(cx, cy, bx, by, sw, 28 * S);
            fb_round(bx, by, sw, 28 * S, RADS, on ? accent : (hv ? T.ctrl_hi : T.ctrl));
            if (!on) fb_round_border(bx, by, sw, 28 * S, RADS, 1, T.stroke, 0xFF);
            fb_text_c(bx, by + 6 * S, sw, i ? "Dark" : "Light", on ? 0x00FFFFFFu : T.fg, S);
        }
        int wy = ty2 + LH + 44 * S;
        if (wp_count > 0) {
            section(px, wy, "Wallpaper");
            int tw = 76 * S, th = 54 * S, gap = 8 * S;
            int perrow = pw / (tw + gap);
            if (perrow < 1) perrow = 1;
            for (int k = 0; k < wp_count; k++) {
                int c = k % perrow, r = k / perrow;
                int bx = px + c * (tw + gap), by = wy + LH + 4 * S + r * (th + gap);
                if (by + th > oy + h) break;
                for (int j = 0; j < th; j++)
                    for (int i = 0; i < tw; i++) {
                        uint32_t sxp = (uint32_t)i * wp_w[k] / (uint32_t)tw;
                        uint32_t syp = (uint32_t)j * wp_h[k] / (uint32_t)th;
                        const uint8_t *pp = wp_px[k] + ((uint64_t)syp * wp_w[k] + sxp) * 4;
                        fb_put_pixel(bx + i, by + j,
                            (uint32_t)pp[0] | ((uint32_t)pp[1] << 8) | ((uint32_t)pp[2] << 16));
                    }
                if (k == wp_active)
                    fb_round_border(bx - 2 * S, by - 2 * S, tw + 4 * S, th + 4 * S, RADS, 2 * S, accent, 0xFF);
                else
                    fb_round_border(bx, by, tw, th, RADS / 2, 1, T.stroke, 0xFF);
            }
        }
    } else if (set_page == 1) {
        section(px, py, "Display");
        int y2 = py + LH + 2 * S;
        int p = fb_num(px, y2, (uint32_t)SW, T.fg, S);
        p = fb_text(p, y2, " x ", T.fg, S);
        p = fb_num(p, y2, (uint32_t)SH, T.fg, S);
        fb_text(p, y2, " - 32 bpp", T.fg, S);
        y2 += LH;
        p = fb_text(px, y2, "UI scale ", T.fg2, S);
        p = fb_num(p, y2, (uint32_t)S, T.fg, S);
        fb_text(p, y2, "x", T.fg, S);

        y2 += LH + 12 * S;
        section(px, y2, "Memory");
        y2 += LH + 2 * S;
        p = fb_num(px, y2, g_mem_kib / 1024, T.fg, S);
        fb_text(p, y2, " MiB usable", T.fg, S);

        y2 += LH + 12 * S;
        section(px, y2, "Uptime");
        y2 += LH + 2 * S;
        uint32_t sec = (uint32_t)(timer_ticks() / 100);
        p = fb_num(px, y2, sec / 60, T.fg, S);
        p = fb_text(p, y2, "m ", T.fg, S);
        p = fb_num(p, y2, sec % 60, T.fg, S);
        fb_text(p, y2, "s", T.fg, S);

        y2 += LH + 12 * S;
        section(px, y2, "Input (PS/2)");
        y2 += LH + 2 * S;
        p = fb_text(px, y2, "mouse ", T.fg2, S);
        p = fb_num(p, y2, mouse_events, mouse_events ? 0x00059669u : 0x00C42B1Cu, S);
        p = fb_text(p, y2, "  keys ", T.fg2, S);
        fb_num(p, y2, key_events, key_events ? 0x00059669u : 0x00C42B1Cu, S);
    } else if (set_page == 2) {
        int n = pci_count();
        int q = fb_text(px, py, "PCI bus - ", T.fg2, S);
        q = fb_num(q, py, (uint32_t)n, T.fg, S);
        fb_text(q, py, " devices found", T.fg2, S);

        int y2 = py + LH + 6 * S, rowh = 32 * S;
        for (int i = 0; i < n && y2 + rowh < oy + h - 4 * S; i++) {
            const struct pci_dev *d = pci_get(i);
            if (d->class_code == 0x02)           /* highlight the NIC */
                fb_round(px - 6 * S, y2 - 4 * S, pw + 12 * S, rowh, RADS,
                         blend(T.layer, accent, 0x22));
            const char *dn = pci_device_name(d->vendor, d->device);
            fb_text(px, y2, dn ? dn : pci_class_name(d->class_code, d->subclass),
                    T.fg, S);
            int p2 = fb_text(px, y2 + 15 * S, pci_vendor_name(d->vendor), T.fg2, S);
            p2 = fb_text(p2, y2 + 15 * S, "  ", T.fg2, S);
            p2 = hex16(p2, y2 + 15 * S, d->vendor, T.fg2);
            p2 = fb_text(p2, y2 + 15 * S, ":", T.fg2, S);
            hex16(p2, y2 + 15 * S, d->device, T.fg2);
            y2 += rowh;
        }
        int nic = pci_find_network();
        fb_text(px, oy + h - 22 * S,
                nic >= 0 ? "Ethernet controller present - no driver yet"
                         : "no network controller found", T.fg2, S);
    } else {
        icon(APP_ABOUT, px, py, 40 * S);
        text_bold(px + 52 * S, py + 2 * S, "Aurelian OS", T.fg, 2 * S);
        fb_text(px + 52 * S, py + 36 * S, "1.0.0-dev  (Luma)", T.fg2, S);
        int y2 = py + 64 * S;
        fb_text(px, y2, "Kernel    Aurelion x86-64", T.fg, S); y2 += LH;
        fb_text(px, y2, "Shell     Luma", T.fg, S);             y2 += LH;
        fb_text(px, y2, "Design    Prism UI", T.fg, S);         y2 += LH;
        fb_text(px, y2, "Boot      Multiboot2 / GRUB", T.fg, S); y2 += LH + 8 * S;
        fb_text(px, y2, "Clean-room original work.", T.fg2, S);
    }
}

static void draw_about(int a)
{
    int ox = co_x(a), oy = co_y(a), w = co_w(a);
    int px = ox + PAD + 8 * S, py = oy + PAD;
    icon(APP_ABOUT, px, py, 44 * S);
    text_bold(px + 58 * S, py, "Aurelian OS", T.fg, 2 * S);
    fb_text(px + 58 * S, py + 34 * S, "codename \"Luma\"", T.fg2, S);

    int y2 = py + 68 * S;
    fb_text(px, y2, "A from-scratch x86-64 operating system:", T.fg, S); y2 += LH;
    fb_text(px, y2, "own kernel, graphics stack and shell.", T.fg, S);   y2 += LH + 8 * S;
    section(px, y2, "Keyboard");                                          y2 += LH;
    fb_text(px, y2, "F1-F8  launch apps    Esc  Start", T.fg2, S);        y2 += LH;
    fb_text(px, y2, "F9  theme   F10  wallpaper", T.fg2, S);              y2 += LH;
    fb_text(px, y2, "F11 next window       F12  close", T.fg2, S);

    (void)w;
}

/* ================================================================== */
/* 10. Window chrome                                                   */
/* ================================================================== */
static void draw_window(int a, int focused)
{
    struct win *W = &wins[a];
    int x = W->x, y = W->y, w = W->w, h = W->h;

    fb_shadow(x, y, w, h, RAD, 9 * S);
    fb_mica_round(x, y, w, h, RAD, T.mica, T.mica_a);

    /* content surface: rounded bottom, square top (meets the title bar) */
    int cyy = y + TBAR, chh = h - TBAR;
    fb_round(x, cyy, w, chh, RAD, T.layer);
    fb_fill(x, cyy, w, RAD, T.layer);
    fb_fill(x, cyy, w, 1, T.stroke);

    /* title bar */
    icon(a, x + 12 * S, y + (TBAR - 16 * S) / 2, 16 * S);
    fb_text(x + 36 * S, y + (TBAR - 16 * S) / 2, app_title[a],
            focused ? T.fg : T.fg2, S);

    /* caption buttons: minimise, close */
    int bw = 40 * S, bh = TBAR;
    int mnx = x + w - 2 * bw, clx = x + w - bw;
    if (in_r(cx, cy, mnx, y, bw, bh)) fb_fill_a(mnx, y, bw, bh, T.fg, 0x18);
    fb_fill(mnx + bw / 2 - 5 * S, y + bh / 2, 10 * S, 1 + S / 2, focused ? T.fg : T.fg2);
    int chov = in_r(cx, cy, clx, y, bw, bh);
    if (chov) fb_fill(clx, y, bw, bh, 0x00C42B1Cu);
    uint32_t xc = chov ? 0x00FFFFFFu : (focused ? T.fg : T.fg2);
    for (int i = 0; i < 9 * S; i++) {
        fb_fill(clx + bw / 2 - 4 * S + i, y + bh / 2 - 4 * S + i, 1 + S / 2, 1 + S / 2, xc);
        fb_fill(clx + bw / 2 - 4 * S + i, y + bh / 2 + 4 * S - i, 1 + S / 2, 1 + S / 2, xc);
    }

    switch (a) {
    case APP_EXPLORER: draw_explorer(a); break;
    case APP_NOTEPAD:  draw_notepad(a);  break;
    case APP_CALC:     draw_calc(a);     break;
    case APP_CLOCK:    draw_clock(a);    break;
    case APP_PAINT:    draw_paint(a);    break;
    case APP_TERM:     draw_term(a);     break;
    case APP_SETTINGS: draw_settings(a); break;
    default:           draw_about(a);    break;
    }

    /* frame last, so app content can't paint over the window edge */
    fb_round_border(x, y, w, h, RAD, 1,
                    focused ? blend(T.stroke, accent, 0x70) : T.stroke, 0xFF);
}

/* ================================================================== */
/* 11. Taskbar + Start                                                 */
/* ================================================================== */
static int start_open;

static int tb_btn = 44;      /* button pitch, scaled below */

static int taskbar_cluster_x(void)
{
    int pitch = tb_btn * S;
    int total = (APP_COUNT + 1) * pitch;
    return (SW - total) / 2;
}

static void draw_taskbar(void)
{
    int ty = SH - TASKH;
    fb_mica_round(0, ty, SW, TASKH, 0, T.mica, 0xE8);
    fb_fill_a(0, ty, SW, 1, T.fg, 0x1A);

    int pitch = tb_btn * S, bs = 38 * S;
    int bx = taskbar_cluster_x(), byy = ty + (TASKH - bs) / 2;

    /* Start */
    int hv = in_r(cx, cy, bx, byy, bs, bs);
    if (start_open || hv)
        fb_round(bx, byy, bs, bs, RADS, start_open ? blend(T.mica, accent, 0x40) : T.ctrl_hi);
    int q = bs / 2 - 5 * S;
    for (int i = 0; i < 4; i++)
        fb_round(bx + q + (i % 2) * 6 * S, byy + q + (i / 2) * 6 * S,
                 5 * S, 5 * S, 1 * S, accent);
    bx += pitch;

    /* pinned + running apps */
    for (int i = 0; i < APP_COUNT; i++, bx += pitch) {
        int running = wins[i].open;
        int focused = (top_app() == i);
        int h2 = in_r(cx, cy, bx, byy, bs, bs);
        if (h2 || (running && focused))
            fb_round(bx, byy, bs, bs, RADS,
                     (running && focused) ? blend(T.mica, T.fg, 0x1C) : T.ctrl_hi);
        icon(i, bx + (bs - 22 * S) / 2, byy + (bs - 22 * S) / 2, 22 * S);
        if (running) {
            int pw = (focused && !wins[i].min) ? 16 * S : 7 * S;
            fb_round(bx + (bs - pw) / 2, byy + bs + 1 * S, pw, 3 * S, 2 * S, accent);
        }
    }

    /* clock: time over date, right aligned */
    struct rtc_time t; rtc_read(&t);
    int rx = SW - 12 * S;
    char hm[6];
    hm[0] = (char)('0' + t.hour / 10); hm[1] = (char)('0' + t.hour % 10);
    hm[2] = ':';
    hm[3] = (char)('0' + t.min / 10);  hm[4] = (char)('0' + t.min % 10); hm[5] = 0;
    fb_text(rx - fb_text_w(hm, S), ty + TASKH / 2 - 17 * S, hm, T.fg, S);
    int dw = 10 * 8 * S;
    int dx = rx - dw;
    int p = fb_num(dx, ty + TASKH / 2 + 2 * S, t.year, T.fg2, S);
    p = fb_text(p, ty + TASKH / 2 + 2 * S, "-", T.fg2, S);
    p = fb_num2(p, ty + TASKH / 2 + 2 * S, t.month, T.fg2, S);
    p = fb_text(p, ty + TASKH / 2 + 2 * S, "-", T.fg2, S);
    fb_num2(p, ty + TASKH / 2 + 2 * S, t.day, T.fg2, S);
}

static void start_rect(int *x, int *y, int *w, int *h)
{
    *w = 400 * S;
    *h = 480 * S;
    if (*w > SW - 40 * S) *w = SW - 40 * S;
    if (*h > SH - TASKH - 40 * S) *h = SH - TASKH - 40 * S;
    *x = (SW - *w) / 2;
    *y = SH - TASKH - *h - 10 * S;
}

static void draw_start(void)
{
    int x, y, w, h;
    start_rect(&x, &y, &w, &h);
    fb_shadow(x, y, w, h, RAD, 12 * S);
    fb_mica_round(x, y, w, h, RAD, T.mica, 0xE4);
    fb_round_border(x, y, w, h, RAD, 1, T.stroke, 0xFF);

    /* search field */
    int sx = x + PAD, sy = y + PAD, sw = w - 2 * PAD;
    fb_round(sx, sy, sw, 32 * S, RADS, T.layer);
    fb_round_border(sx, sy, sw, 32 * S, RADS, 1, T.stroke, 0xFF);
    fb_round_border(sx, sy, sw, 32 * S, RADS, 1, accent, 0x40);
    fb_text(sx + 12 * S, sy + 8 * S, "Search apps and files", T.fg2, S);

    section(sx, sy + 46 * S, "Pinned");

    /* app grid */
    int cols = 4;
    int cw = sw / cols, chh = 82 * S;
    int gy = sy + 46 * S + LH + 6 * S;
    for (int i = 0; i < APP_COUNT; i++) {
        int c = i % cols, r = i / cols;
        int bx = sx + c * cw, byy = gy + r * chh;
        if (in_r(cx, cy, bx, byy, cw - 4 * S, chh - 6 * S))
            fb_round(bx, byy, cw - 4 * S, chh - 6 * S, RADS, T.ctrl_hi);
        icon(i, bx + (cw - 4 * S - 36 * S) / 2, byy + 12 * S, 36 * S);
        fb_text_c(bx, byy + 56 * S, cw - 4 * S, app_short[i], T.fg, S);
    }

    /* footer: user + power */
    int fy = y + h - 46 * S;
    fb_fill_a(x + PAD, fy - 8 * S, w - 2 * PAD, 1, T.fg, 0x18);
    fb_round(x + PAD, fy + 4 * S, 26 * S, 26 * S, 13 * S, accent);
    fb_text_c(x + PAD, fy + 9 * S, 26 * S, "A", 0x00FFFFFFu, S);
    fb_text(x + PAD + 36 * S, fy + 9 * S, "Aurelian User", T.fg, S);
    int pxb = x + w - PAD - 30 * S;
    if (in_r(cx, cy, pxb, fy + 4 * S, 30 * S, 26 * S))
        fb_round(pxb, fy + 4 * S, 30 * S, 26 * S, RADS, T.ctrl_hi);
    fb_round_border(pxb + 9 * S, fy + 10 * S, 13 * S, 13 * S, 7 * S, 2 * S, T.fg, 0xFF);
    fb_fill(pxb + 15 * S, fy + 8 * S, 2 * S, 8 * S, T.mica);
    fb_fill(pxb + 15 * S, fy + 8 * S, 2 * S, 6 * S, T.fg);
}

/* ================================================================== */
/* 12. Compositor                                                      */
/* ================================================================== */
static void compose(void)
{
    fb_draw_background();
    for (int i = 0; i < zn; i++) {
        int a = zlist[i];
        if (!wins[a].min) draw_window(a, i == zn - 1);
    }
    if (start_open) draw_start();
    draw_taskbar();
    fb_cursor(cx, cy);
    fb_present();
}

/* ================================================================== */
/* 13. App logic                                                       */
/* ================================================================== */
static void np_open(int node)
{
    np_file = node;
    np_len  = 0;
    const char *s = fs_read(node);
    while (s[np_len] && np_len < (int)sizeof(np_buf) - 1) { np_buf[np_len] = s[np_len]; np_len++; }
    np_buf[np_len] = 0;
    np_dirty = 0;
}

static void np_save(void)
{
    if (np_file < 0) {
        int docs = fs_child(fs_root(), 0);
        np_file = fs_create(docs, "new.txt");
        if (np_file < 0) return;
    }
    fs_write(np_file, np_buf, (uint32_t)np_len);
    np_dirty = 0;
}

static void calc_apply(void)
{
    switch (calc_op) {
    case '+': calc_acc += calc_cur; break;
    case '-': calc_acc -= calc_cur; break;
    case '*': calc_acc *= calc_cur; break;
    case '/': calc_acc = calc_cur ? calc_acc / calc_cur : 0; break;
    case '%': calc_acc = calc_cur ? calc_acc % calc_cur : 0; break;
    default:  calc_acc = calc_cur; break;
    }
    calc_cur = calc_acc;
}

static void calc_key(const char *k)
{
    char c = k[0];
    if (c >= '0' && c <= '9') {
        if (calc_fresh) { calc_cur = 0; calc_fresh = 0; }
        calc_cur = calc_cur * 10 + (c - '0');
    } else if (c == 'C') {
        calc_acc = calc_cur = 0; calc_op = 0; calc_fresh = 1;
    } else if (c == '<') {
        calc_cur /= 10;
    } else if (c == '=') {
        calc_apply(); calc_op = 0; calc_fresh = 1;
    } else if (c == '+' && k[1] == '/') {
        calc_cur = -calc_cur;
    } else if (c == '.') {
        /* integer calculator: no-op */
    } else {
        calc_apply(); calc_op = c; calc_fresh = 1;
    }
}

static void term_puts(const char *s)
{
    if (term_n >= TROWS) {
        for (int i = 1; i < TROWS; i++)
            for (int j = 0; j <= TCOLS; j++) term_out[i - 1][j] = term_out[i][j];
        term_n--;
    }
    int j = 0;
    while (s[j] && j < TCOLS) { term_out[term_n][j] = s[j]; j++; }
    term_out[term_n][j] = 0;
    term_n++;
}

static void term_run(void)
{
    const char *in = term_in;
    if (in[0] == 0) { term_puts(""); }
    else if (str_eq(in, "help")) {
        term_puts("help ver about clear ls cat <f> date mem uptime");
    } else if (str_eq(in, "ver")) {
        term_puts("Aurelian OS 1.0.0-dev / Aurelion kernel x86-64");
    } else if (str_eq(in, "about")) {
        term_puts("Original from-scratch OS. Prism UI, Luma Shell.");
    } else if (str_eq(in, "clear")) {
        term_n = 0;
    } else if (str_eq(in, "ls")) {
        int n = fs_child_count(fs_root());
        for (int i = 0; i < n; i++) {
            int node = fs_child(fs_root(), i);
            char line[TCOLS + 1]; int p = 0;
            const char *nm = fs_name(node);
            for (int k = 0; nm[k] && p < TCOLS - 8; k++) line[p++] = nm[k];
            if (fs_is_dir(node) && p < TCOLS - 1) line[p++] = '/';
            line[p] = 0;
            term_puts(line);
        }
    } else if (str_pre(in, "cat ")) {
        const char *want = in + 4;
        int found = -1;
        for (int d = 0; d < fs_child_count(fs_root()) && found < 0; d++) {
            int dir = fs_child(fs_root(), d);
            for (int i = 0; i < fs_child_count(dir); i++) {
                int node = fs_child(dir, i);
                if (str_eq(fs_name(node), want)) { found = node; break; }
            }
        }
        if (found < 0) { term_puts("file not found"); }
        else {
            const char *s = fs_read(found);
            char line[TCOLS + 1]; int p = 0;
            for (int i = 0; s[i]; i++) {
                if (s[i] == '\n') { line[p] = 0; term_puts(line); p = 0; continue; }
                if (p >= TCOLS)   { line[p] = 0; term_puts(line); p = 0; }
                line[p++] = s[i];
            }
            if (p) { line[p] = 0; term_puts(line); }
        }
    } else if (str_eq(in, "date")) {
        struct rtc_time t; rtc_read(&t);
        char b[32]; int p = 0;
        b[p++] = (char)('0' + t.year / 1000 % 10);
        b[p++] = (char)('0' + t.year / 100 % 10);
        b[p++] = (char)('0' + t.year / 10 % 10);
        b[p++] = (char)('0' + t.year % 10);
        b[p++] = '-';
        b[p++] = (char)('0' + t.month / 10); b[p++] = (char)('0' + t.month % 10);
        b[p++] = '-';
        b[p++] = (char)('0' + t.day / 10);   b[p++] = (char)('0' + t.day % 10);
        b[p++] = ' ';
        b[p++] = (char)('0' + t.hour / 10);  b[p++] = (char)('0' + t.hour % 10);
        b[p++] = ':';
        b[p++] = (char)('0' + t.min / 10);   b[p++] = (char)('0' + t.min % 10);
        b[p] = 0;
        term_puts(b);
    } else if (str_eq(in, "mem")) {
        char b[32]; int p = 0;
        uint32_t m = g_mem_kib / 1024;
        char t[12]; int n = 0;
        if (!m) t[n++] = '0';
        while (m) { t[n++] = (char)('0' + m % 10); m /= 10; }
        while (n) b[p++] = t[--n];
        b[p++] = ' '; b[p++] = 'M'; b[p++] = 'i'; b[p++] = 'B'; b[p] = 0;
        term_puts(b);
    } else if (str_eq(in, "uptime")) {
        uint32_t s2 = (uint32_t)(timer_ticks() / 100);
        char b[32]; int p = 0; char t[12]; int n = 0;
        uint32_t m = s2 / 60;
        if (!m) t[n++] = '0';
        while (m) { t[n++] = (char)('0' + m % 10); m /= 10; }
        while (n) b[p++] = t[--n];
        b[p++] = 'm'; b[p++] = ' ';
        m = s2 % 60; n = 0;
        if (!m) t[n++] = '0';
        while (m) { t[n++] = (char)('0' + m % 10); m /= 10; }
        while (n) b[p++] = t[--n];
        b[p++] = 's'; b[p] = 0;
        term_puts(b);
    } else {
        term_puts("unknown command - try 'help'");
    }
    term_len = 0; term_in[0] = 0;
}

static void paint_clear(void)
{
    for (int i = 0; i < PW * PH; i++) paint_px[i] = 0x00FFFFFFu;
}

static void paint_stroke(int a)
{
    int lx = cx - (co_x(a) + 10 * S);
    int ly = cy - (co_y(a) + 40 * S);
    if (lx < 0 || ly < 0 || lx >= PW || ly >= PH) return;
    int r = paint_size;
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            if (i * i + j * j > r * r) continue;
            int px = lx + i, py = ly + j;
            if (px >= 0 && py >= 0 && px < PW && py < PH)
                paint_px[py * PW + px] = paint_col;
        }
}

/* ================================================================== */
/* 14. App click handling (local coords: origin = content origin)      */
/* ================================================================== */
static void app_click(int a, int lx, int ly)
{
    int w = co_w(a), h = co_h(a);

    if (a == APP_EXPLORER) {
        int tool = 40 * S, side = 150 * S, row = 28 * S;
        if (in_r(lx, ly, 10 * S, (tool - 26 * S) / 2, 30 * S, 26 * S)) {
            int p = fs_parent(ex_dir);
            if (p >= 0) { ex_dir = p; ex_sel = -1; }
            return;
        }
        for (int i = 0; i < 6; i++) {
            int iy = tool + 8 * S + i * row;
            if (in_r(lx, ly, 6 * S, iy, side - 12 * S, row - 2 * S)) {
                ex_dir = (i == 0) ? fs_root() : fs_child(fs_root(), i - 1);
                ex_sel = -1;
                return;
            }
        }
        int n = fs_child_count(ex_dir);
        for (int i = 0; i < n; i++) {
            int ry = tool + 56 * S + i * row;      /* +24 for the RAM-disk note */
            if (in_r(lx, ly, side + 6 * S, ry, w - side - 12 * S, row)) {
                int node = fs_child(ex_dir, i);
                if (fs_is_dir(node)) { ex_dir = node; ex_sel = -1; }
                else { ex_sel = i; np_open(node); open_app(APP_NOTEPAD); }
                return;
            }
        }
    } else if (a == APP_NOTEPAD) {
        if (in_r(lx, ly, 10 * S, 5 * S, 62 * S, 26 * S)) { np_save(); return; }
        if (in_r(lx, ly, 78 * S, 5 * S, 62 * S, 26 * S)) {
            np_file = -1; np_len = 0; np_buf[0] = 0; np_dirty = 0; return;
        }
    } else if (a == APP_CALC) {
        int disp = 60 * S;
        int gw = w - 16 * S, gh = h - disp - 8 * S;
        int bw = (gw - 3 * 6 * S) / 4, bh = (gh - 4 * 6 * S) / 5;
        for (int i = 0; i < 20; i++) {
            int c = i % 4, r = i / 4;
            if (in_r(lx, ly, 8 * S + c * (bw + 6 * S), disp + r * (bh + 6 * S), bw, bh)) {
                calc_key(CK[i]); return;
            }
        }
    } else if (a == APP_PAINT) {
        for (int i = 0; i < 8; i++)
            if (in_r(lx, ly, 10 * S + i * 30 * S, 8 * S, 24 * S, 24 * S)) {
                static const uint32_t pal[8] = {
                    0x00E11D48u, 0x00EA580Cu, 0x00F59E0Bu, 0x00059669u,
                    0x000891B2u, 0x000078D4u, 0x008B5CF6u, 0x00161616u
                };
                paint_col = pal[i]; return;
            }
        for (int i = 0; i < 3; i++)
            if (in_r(lx, ly, 262 * S + i * 28 * S, 8 * S, 24 * S, 24 * S)) {
                paint_size = 1 + i * 3; return;
            }
        if (in_r(lx, ly, w - 76 * S, 8 * S, 66 * S, 24 * S)) { paint_clear(); return; }
        paint_stroke(a);
    } else if (a == APP_SETTINGS) {
        int nav = 140 * S;
        for (int i = 0; i < 4; i++)
            if (in_r(lx, ly, 6 * S, 12 * S + i * 32 * S, nav - 12 * S, 28 * S)) {
                set_page = i; return;
            }
        if (set_page != 0) return;
        int px = nav + PAD, py = PAD, pw = w - nav - 2 * PAD;
        for (int i = 0; i < 8; i++)
            if (in_r(lx, ly, px + i * 34 * S, py + LH + 4 * S, 26 * S, 26 * S)) {
                accent = ACCENTS[i]; return;
            }
        int ty2 = py + LH + 48 * S, sw = 84 * S;
        for (int i = 0; i < 2; i++)
            if (in_r(lx, ly, px + i * (sw + 8 * S), ty2 + LH + 4 * S, sw, 28 * S)) {
                dark_mode = i; theme_apply(); return;
            }
        int wy = ty2 + LH + 44 * S;
        int tw = 76 * S, th = 54 * S, gap = 8 * S;
        int perrow = pw / (tw + gap); if (perrow < 1) perrow = 1;
        for (int k = 0; k < wp_count; k++) {
            int c = k % perrow, r = k / perrow;
            if (in_r(lx, ly, px + c * (tw + gap), wy + LH + 4 * S + r * (th + gap), tw, th)) {
                wallpaper_select(k); return;
            }
        }
    }
}

/* Non-text keys for the focused app. Returns 1 if the key was consumed.
 * Extended (E0-prefixed) keys arrive with bit 7 set — see keyboard.c. */
static int app_keycode(int a, uint8_t k)
{
    if (a == APP_SETTINGS) {
        /* 1..4 jump to a Settings page (it has no text fields to conflict) */
        if (k >= 0x02 && k <= 0x05) { set_page = k - 0x02; return 1; }
        return 0;
    }
    if (a == APP_EXPLORER) {
        int n = fs_child_count(ex_dir);
        switch (k) {
        case 0xC8:                                  /* up    */
            if (n) ex_sel = (ex_sel <= 0) ? n - 1 : ex_sel - 1;
            return 1;
        case 0xD0:                                  /* down  */
            if (n) ex_sel = (ex_sel < 0 || ex_sel >= n - 1) ? 0 : ex_sel + 1;
            return 1;
        case 0x1C:                                  /* enter */
            if (ex_sel >= 0 && ex_sel < n) {
                int node = fs_child(ex_dir, ex_sel);
                if (fs_is_dir(node)) { ex_dir = node; ex_sel = -1; }
                else { np_open(node); open_app(APP_NOTEPAD); }
            }
            return 1;
        case 0x0E:                                  /* backspace: up a level */
        case 0xCB: {                                /* left */
            int p = fs_parent(ex_dir);
            if (p >= 0) { ex_dir = p; ex_sel = -1; }
            return 1;
        }
        default: return 0;
        }
    }
    return 0;
}

static void app_key(int a, char ch)
{
    if (a == APP_NOTEPAD) {
        if (ch == '\b') { if (np_len > 0) { np_buf[--np_len] = 0; np_dirty = 1; } }
        else if ((ch >= ' ' || ch == '\n') && np_len < (int)sizeof(np_buf) - 1) {
            np_buf[np_len++] = ch; np_buf[np_len] = 0; np_dirty = 1;
        }
    } else if (a == APP_TERM) {
        if (ch == '\b') { if (term_len > 0) term_in[--term_len] = 0; }
        else if (ch == '\n') term_run();
        else if (ch >= ' ' && term_len < TCOLS) { term_in[term_len++] = ch; term_in[term_len] = 0; }
    } else if (a == APP_CALC) {
        if ((ch >= '0' && ch <= '9')) { char k[2] = { ch, 0 }; calc_key(k); }
        else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') { char k[2] = { ch, 0 }; calc_key(k); }
        else if (ch == '\n' || ch == '=') calc_key("=");
        else if (ch == '\b') calc_key("<");
        else if (ch == 'c' || ch == 'C') calc_key("C");
    } else if (a == APP_EXPLORER) {
        if (ch == '\b') { int p = fs_parent(ex_dir); if (p >= 0) { ex_dir = p; ex_sel = -1; } }
    }
}

/* ================================================================== */
/* 15. Input dispatch                                                  */
/* ================================================================== */
static int dragging = -1, drag_ox, drag_oy;
static uint8_t prev_btn;

static int win_at(int px, int py)
{
    for (int i = zn - 1; i >= 0; i--) {
        int a = zlist[i];
        if (wins[a].min) continue;
        if (in_r(px, py, wins[a].x, wins[a].y, wins[a].w, wins[a].h)) return a;
    }
    return -1;
}

static void on_press(void)
{
    int ty = SH - TASKH;

    /* taskbar */
    if (cy >= ty) {
        int pitch = tb_btn * S, bs = 38 * S;
        int bx = taskbar_cluster_x(), byy = ty + (TASKH - bs) / 2;
        if (in_r(cx, cy, bx, byy, bs, bs)) { start_open = !start_open; return; }
        bx += pitch;
        for (int i = 0; i < APP_COUNT; i++, bx += pitch) {
            if (!in_r(cx, cy, bx, byy, bs, bs)) continue;
            start_open = 0;
            if (!wins[i].open) { open_app(i); }
            else if (top_app() == i && !wins[i].min) { wins[i].min = 1; }
            else { wins[i].min = 0; focus_app(i); }
            return;
        }
        return;
    }

    /* start menu */
    if (start_open) {
        int x, y, w, h;
        start_rect(&x, &y, &w, &h);
        if (in_r(cx, cy, x, y, w, h)) {
            int sx = x + PAD, sy = y + PAD, sw = w - 2 * PAD;
            int cols = 4, cw = sw / cols, chh = 82 * S;
            int gy = sy + 46 * S + LH + 6 * S;
            for (int i = 0; i < APP_COUNT; i++) {
                int c = i % cols, r = i / cols;
                if (in_r(cx, cy, sx + c * cw, gy + r * chh, cw - 4 * S, chh - 6 * S)) {
                    open_app(i); start_open = 0; return;
                }
            }
            return;                     /* swallow clicks inside the menu */
        }
        start_open = 0;                 /* clicked away: dismiss, keep going */
    }

    /* windows */
    int a = win_at(cx, cy);
    if (a < 0) return;
    focus_app(a);

    int bw = 40 * S;
    int mnx = wins[a].x + wins[a].w - 2 * bw, clx = wins[a].x + wins[a].w - bw;
    if (in_r(cx, cy, clx, wins[a].y, bw, TBAR)) { close_app(a); return; }
    if (in_r(cx, cy, mnx, wins[a].y, bw, TBAR)) { wins[a].min = 1; return; }
    if (cy < wins[a].y + TBAR) {
        dragging = a; drag_ox = cx - wins[a].x; drag_oy = cy - wins[a].y; return;
    }
    app_click(a, cx - co_x(a), cy - co_y(a));
}

/* ================================================================== */
/* 16. Entry                                                           */
/* ================================================================== */
void shell_run(int nwp, const uint64_t *wp_addr, const uint32_t *wp_size,
               uint32_t mem_kib)
{
    metrics_init();
    theme_apply();
    fs_init();
    pci_scan();
    g_mem_kib = mem_kib;
    tb_btn = 44;

    /* validate wallpaper modules (AOWP header + BGRX pixels) */
    wp_count = 0;
    for (int i = 0; i < nwp && wp_count < MAX_WP; i++) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)wp_addr[i];
        if (wp_size[i] > 12 && p[0]=='A' && p[1]=='O' && p[2]=='W' && p[3]=='P') {
            uint32_t ww = *(const uint32_t *)(p + 4);
            uint32_t wh = *(const uint32_t *)(p + 8);
            if (ww && wh && (uint64_t)ww * wh * 4 + 12 <= wp_size[i]) {
                wp_px[wp_count] = p + 12;
                wp_w[wp_count]  = ww;
                wp_h[wp_count]  = wh;
                wp_count++;
            }
        }
    }
    if (wp_count > 0) wallpaper_select(0);
    else              fb_set_wallpaper_gradient(0x00243B7Au, 0x000C1024u);

    /* window sizes (scaled, clamped to the screen) */
    wins[APP_EXPLORER] = (struct win){ 0,0, 620*S, 400*S, 0,0 };
    wins[APP_NOTEPAD]  = (struct win){ 0,0, 470*S, 380*S, 0,0 };
    wins[APP_CALC]     = (struct win){ 0,0, 280*S, 400*S, 0,0 };
    wins[APP_CLOCK]    = (struct win){ 0,0, 340*S, 210*S, 0,0 };
    wins[APP_PAINT]    = (struct win){ 0,0, 560*S, 380*S, 0,0 };
    wins[APP_TERM]     = (struct win){ 0,0, 540*S, 320*S, 0,0 };
    wins[APP_SETTINGS] = (struct win){ 0,0, 620*S, 430*S, 0,0 };
    wins[APP_ABOUT]    = (struct win){ 0,0, 430*S, 330*S, 0,0 };
    for (int i = 0; i < APP_COUNT; i++) {
        if (wins[i].w > SW - 20 * S) wins[i].w = SW - 20 * S;
        if (wins[i].h > SH - TASKH - 20 * S) wins[i].h = SH - TASKH - 20 * S;
    }

    paint_clear();
    ex_dir = fs_root(); ex_sel = -1;
    term_puts("Aurelian OS terminal - type 'help'");

    cx = SW / 2; cy = SH / 2;
    open_app(APP_ABOUT);

    serial_write("[shell] Luma Shell running (");
    serial_write_u64((uint64_t)SW); serial_write("x");
    serial_write_u64((uint64_t)SH); serial_write(", ");
    serial_write_u64((uint64_t)wp_count); serial_write(" wallpapers)\n");

    compose();

    uint64_t last = 0;
    struct input_event e;
    for (;;) {
        int dirty = 0;
        while (input_poll(&e)) {
            dirty = 1;
            if (e.type == INPUT_MOUSE) {
                mouse_events++;
                cx = clampi(cx + e.dx, 0, SW - 1);
                cy = clampi(cy - e.dy, 0, SH - 1);
                uint8_t lb = e.buttons & 1u;
                int press = lb && !(prev_btn & 1u);

                if (dragging >= 0) {
                    if (lb) {
                        wins[dragging].x = clampi(cx - drag_ox, -wins[dragging].w + 60 * S, SW - 60 * S);
                        wins[dragging].y = clampi(cy - drag_oy, 0, SH - TASKH - TBAR);
                    } else dragging = -1;
                } else if (press) {
                    on_press();
                } else if (lb && top_app() == APP_PAINT) {
                    int a = APP_PAINT;
                    if (in_r(cx, cy, co_x(a), co_y(a) + 40 * S, co_w(a), co_h(a) - 40 * S))
                        paint_stroke(a);
                }
                prev_btn = e.buttons;
            } else if (e.type == INPUT_KEY_DOWN) {
                key_events++;
                uint8_t k = e.keycode;
                if (k >= 0x3B && k <= 0x42) {           /* F1..F8 launch apps  */
                    open_app(k - 0x3B); start_open = 0;
                } else if (k == 0x43) {                  /* F9  theme          */
                    dark_mode = !dark_mode; theme_apply();
                } else if (k == 0x44) {                  /* F10 next wallpaper */
                    if (wp_count) wallpaper_select((wp_active + 1) % wp_count);
                } else if (k == 0x57) {                  /* F11 cycle windows  */
                    if (zn > 1) {
                        int f = zlist[zn - 1];
                        for (int i = zn - 1; i > 0; i--) zlist[i] = zlist[i - 1];
                        zlist[0] = f;
                    }
                } else if (k == 0x58) {                  /* F12 close window   */
                    int a = top_app(); if (a >= 0) close_app(a);
                } else if (k == 0x01) {                  /* Esc  Start menu    */
                    start_open = !start_open;
                } else {
                    int a = top_app();
                    if (a >= 0 && !app_keycode(a, k) && e.ascii) app_key(a, e.ascii);
                }
            }
        }

        uint64_t half = timer_ticks() / 50;      /* 2 Hz: clocks + caret blink */
        if (half != last) { last = half; dirty = 1; }

        if (dirty) compose();
        __asm__ volatile ("hlt");
    }
}
