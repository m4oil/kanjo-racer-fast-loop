/*
 * KANJO RACER: BEAR PRODUCTS      (dedicated to Collin)
 *
 * OutRun-style pseudo-3D racer for the Game Boy Advance, written straight
 * against the hardware (Mode 4 bitmap road + hardware sprites).
 */
#include "game.h"
#include "gfx.h"

// ============================================================================ constants
#define SCR_W 240
#define SCR_H 160
#define HORIZON 78
#define SEGL 200            /* world units per road segment */
#define ROAD_HALF 1100      /* half road width, world units */
#define CAM_H 1000          /* camera height */
#define CAMD_FP 55050       /* camera depth 0.84 in 16.16 */
#define PLAYER_Z 1250       /* camera sits this far behind the player */
#define DRAW_N 80           /* segments drawn per frame */
#define SCEN_N 62           /* scenery drawn this far */
#define MAX_SEGS 2000
#define ROAD_BASE 100       /* palette index of the first road colour */
#define TOTAL_CP 9          /* three checkpoints per lap, three laps */
#define NCARS 10
#define TIME0 (40 * 60)
#define CP_BONUS (18 * 60)

enum { ST_TITLE, ST_HUB, ST_GARAGE, ST_RACE, ST_RESULT };
enum { PH_COUNT, PH_RACE, PH_END };
enum { OB_NONE, OB_LAMP, OB_BLDG_A, OB_BLDG_B, OB_BOARD, OB_GATE };
enum { K_EF, K_AE86, K_TRUCK, K_LAMP, K_BLDG_A, K_BLDG_B, K_BOARD, K_POST, K_BANNER };

// ============================================================================ small helpers
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int iabs(int a) { return a < 0 ? -a : a; }
static inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static u32 rng_state = 0x1234567;
static int rnd(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (int)((rng_state >> 16) & 0x7FFF);
}

static int tri(int f)   // triangle wave 0..63
{
    f &= 127;
    return f < 64 ? f : 127 - f;
}

// ============================================================================ state
static int state = ST_TITLE;
static int phase = PH_COUNT;
static u32 frame_no;
static u16 keys_prev;
static int menu_sel;
static int hold_gas = 1;
static u32 best_score;
static int tune[4];     // engine, grip, gearing, weight  (0..4)

// ============================================================================ track data
static s8 seg_curve[MAX_SEGS];
static s16 seg_y[MAX_SEGS + 1];
static u8 seg_near[MAX_SEGS], seg_far[MAX_SEGS];
static s8 seg_nearx[MAX_SEGS], seg_farx[MAX_SEGS];
static int num_segs, track_len, cp_len;

static void add_seg(int curve8, int ynext)
{
    if (num_segs >= MAX_SEGS - 1) return;
    seg_curve[num_segs] = (s8)curve8;
    seg_y[num_segs + 1] = (s16)ynext;
    num_segs++;
}

static int last_y(void) { return seg_y[num_segs]; }

static int ease_in(int a, int b, int t) { return a + (((b - a) * ((t * t) >> 8)) >> 8); }
static int ease_out(int a, int b, int t) { int u = 256 - t; return a + (((b - a) * (256 - ((u * u) >> 8))) >> 8); }
static int ease_io(int a, int b, int t)
{
    int t2 = (t * t) >> 8, t3 = (t2 * t) >> 8;
    return a + (((b - a) * (3 * t2 - 2 * t3)) >> 8);
}

static void add_road(int enter, int hold, int leave, int curve8, int dy)
{
    int y0 = last_y(), y1 = y0 + dy, total = enter + hold + leave, n;
    for (n = 0; n < enter; n++)
        add_seg(ease_in(0, curve8, n * 256 / enter), ease_io(y0, y1, n * 256 / total));
    for (n = 0; n < hold; n++)
        add_seg(curve8, ease_io(y0, y1, (enter + n) * 256 / total));
    for (n = 0; n < leave; n++)
        add_seg(ease_out(curve8, 0, n * 256 / (leave ? leave : 1)), ease_io(y0, y1, (enter + hold + n) * 256 / total));
}

static void add_straight(int n)
{
    int i, y = last_y();
    for (i = 0; i < n; i++) add_seg(0, y);
}

static void build_track(void)
{
    int i, rem;
    num_segs = 0;
    seg_y[0] = 0;
    // A Kanjo-style loop: tight sweepers, climbs, chicanes and a long bridge.
    add_straight(40);
    add_road(25, 40, 25, 32, 0);
    add_road(20, 30, 20, -48, 2000);
    add_road(15, 20, 15, 40, 0);
    add_road(10, 60, 10, 0, -2000);
    add_road(20, 30, 20, -56, 0);
    add_road(15, 15, 15, 64, 1500);
    add_road(15, 15, 15, -64, -1500);
    add_straight(25);
    add_road(30, 60, 30, 24, 3000);
    add_road(25, 30, 25, -48, -3000);
    add_road(10, 20, 10, 48, 0);
    add_road(10, 20, 10, -48, 0);
    add_road(10, 20, 10, 48, 0);
    add_road(30, 50, 30, -32, 0);
    add_road(20, 30, 20, 56, 2500);
    add_road(20, 30, 20, -40, -2500);
    add_straight(20);
    add_road(25, 50, 25, 40, 0);
    add_road(15, 30, 15, -64, 1800);
    add_road(15, 30, 15, 56, -1800);
    add_straight(30);
    add_road(15, 25, 15, 56, 0);
    add_road(15, 25, 15, -56, 0);
    add_road(15, 25, 15, 56, 0);
    // pad to a multiple of 6 and close the height profile
    rem = (num_segs + 20) % 6;
    add_straight(20 + (rem ? 6 - rem : 0));
    {
        int y = last_y();
        if (y != 0)
        {
            int n = 60;
            int y0 = y;
            for (i = 0; i < n; i++) add_seg(0, ease_io(y0, 0, (i + 1) * 256 / n));
            rem = num_segs % 6;
            if (rem) add_straight(6 - rem);
        }
    }
    seg_y[num_segs] = 0;
    // num_segs must be a multiple of 6 (stripe pattern) and of 3 (checkpoints)
    while (num_segs % 6) add_seg(0, 0);
    seg_y[num_segs] = 0;
    track_len = num_segs * SEGL;
    cp_len = (num_segs / 3) * SEGL;

    // ---- scenery
    {
        u32 s = 987654321u;
        for (i = 0; i < num_segs; i++)
        {
            seg_near[i] = OB_NONE;
            seg_far[i] = OB_NONE;
            if (i % 8 == 0)
            {
                seg_near[i] = OB_LAMP;
                seg_nearx[i] = ((i / 8) & 1) ? 21 : -21;
            }
            if (i % 140 == 60)
            {
                seg_near[i] = OB_BOARD;
                seg_nearx[i] = ((i / 140) & 1) ? 34 : -34;
            }
            s = s * 1664525u + 1013904223u;
            if (((s >> 16) & 3) == 0)
            {
                int side = ((s >> 20) & 1) ? 1 : -1;
                seg_far[i] = (u8)(OB_BLDG_A + ((s >> 21) & 1));
                seg_farx[i] = (s8)(side * (32 + (int)((s >> 22) % 40)));
            }
        }
        for (i = 0; i < 3; i++)
        {
            int g = i * (num_segs / 3);
            seg_near[g] = OB_GATE;
            seg_far[g] = OB_NONE;
        }
    }
}

// ============================================================================ tuning -> numbers
static int max_speed(void) { return 150 + 6 * tune[0] + 9 * tune[2] + 2 * tune[3]; }   // units per frame
static int accel_scale(void) { return 256 + 22 * tune[0] + 10 * tune[3] - 14 * tune[2]; }
static int grip_scale(void) { return 256 + 22 * tune[1]; }
static int kmh_of(int speed64) { return (speed64 * 11) / (64 * 10); }

// ============================================================================ SRAM saving
static const char save_id[] __attribute__((used)) = "SRAM_V113";

static void load_game(void)
{
    volatile u8 *s = SRAM_RAM;
    int i;
    if (s[0] == 0x4B && s[1] == 0x32)
    {
        for (i = 0; i < 4; i++) tune[i] = iclamp(s[2 + i], 0, 4);
        hold_gas = s[6] ? 1 : 0;
        best_score = (u32)s[8] | ((u32)s[9] << 8) | ((u32)s[10] << 16) | ((u32)s[11] << 24);
    }
}

static void save_game(void)
{
    volatile u8 *s = SRAM_RAM;
    int i;
    s[0] = 0x4B;
    s[1] = 0x32;
    for (i = 0; i < 4; i++) s[2 + i] = (u8)tune[i];
    s[6] = (u8)hold_gas;
    for (i = 0; i < 4; i++) s[8 + i] = (u8)((best_score >> (8 * i)) & 255);
}

// ============================================================================ sprites / OAM shadow
static u16 oam[512];
static int oam_n, aff_n;

static void oam_reset(void)
{
    int i;
    for (i = 0; i < 128; i++)
    {
        oam[i * 4 + 0] = 0x0200;
        oam[i * 4 + 1] = 0;
        oam[i * 4 + 2] = 0;
    }
    oam_n = 0;
    aff_n = 0;
}

static void text8(int x, int y, const char *s, int bank)
{
    for (; *s; s++, x += 8)
    {
        int c = (unsigned char)*s;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        if (c == ' ') continue;
        if (oam_n >= 126) return;
        oam[oam_n * 4 + 0] = (u16)(y & 0xFF);
        oam[oam_n * 4 + 1] = (u16)(x & 0x1FF);
        oam[oam_n * 4 + 2] = (u16)((OBJ_TILE_BASE + T_FONT + (c - 32)) | (bank << 12));
        oam_n++;
    }
}

static void text16(int x, int y, const char *s, int bank)   // 2x size via affine group 31
{
    for (; *s; s++, x += 16)
    {
        int c = (unsigned char)*s;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        if (c == ' ') continue;
        if (oam_n >= 126) return;
        oam[oam_n * 4 + 0] = (u16)((y & 0xFF) | 0x100 | 0x200);
        oam[oam_n * 4 + 1] = (u16)((x & 0x1FF) | (31 << 9));
        oam[oam_n * 4 + 2] = (u16)((OBJ_TILE_BASE + T_FONT + (c - 32)) | (bank << 12));
        oam_n++;
    }
}

static char *put_int(char *p, int v, int width, char pad)
{
    char tmp[12];
    int n = 0, i;
    if (v < 0) v = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 11);
    for (i = n; i < width; i++) *p++ = pad;
    while (n) *p++ = tmp[--n];
    return p;
}

static char *put_str(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

typedef struct { u8 wn, hn, shape, size; u16 tile, tile_s; } sdef_t;
static const sdef_t sdefs[] = {
    { 64, 32, 1, 3, T_EF, T_EF_S },          // K_EF
    { 64, 32, 1, 3, T_AE86, T_AE86_S },      // K_AE86
    { 64, 32, 1, 3, T_TRUCK, T_TRUCK_S },    // K_TRUCK
    { 32, 64, 2, 3, T_LAMP, 0 },             // K_LAMP
    { 32, 64, 2, 3, T_BLDG_A, 0 },           // K_BLDG_A
    { 32, 64, 2, 3, T_BLDG_B, 0 },           // K_BLDG_B
    { 64, 32, 1, 3, T_BOARD, 0 },            // K_BOARD
    { 16, 32, 2, 2, T_POST, 0 },             // K_POST
    { 64, 32, 1, 3, T_BANNER, 0 },           // K_BANNER
};

typedef struct { s32 key; s16 cx, cy; s16 scale; u8 kind, bank, mirror, small; } sq_t;
static sq_t sq[72];
static int sq_n;

static void queue(int kind, int bank, s32 key, int cx, int cy, int scale, int mirror)
{
    sq_t *q;
    if (sq_n >= 72) return;
    if (scale < 12 || scale > 512) return;
    q = &sq[sq_n++];
    q->key = key; q->cx = (s16)cx; q->cy = (s16)cy; q->scale = (s16)scale;
    q->kind = (u8)kind; q->bank = (u8)bank; q->mirror = (u8)mirror; q->small = 0;
}

static void emit_sprite(const sq_t *q)
{
    const sdef_t *d = &sdefs[q->kind];
    int scale = q->scale, tile = d->tile, wn = d->wn, hn = d->hn, shape = d->shape, size = d->size;
    int dbl, bw, bh, x, y, inv, g;
    if (oam_n >= 126 || aff_n >= 31) return;
    if (q->kind <= K_TRUCK && scale < 128)      // far cars use the small 32x16 art
    {
        tile = d->tile_s; wn = 32; hn = 16; shape = 1; size = 2; scale *= 2;
    }
    dbl = scale > 256;
    bw = dbl ? wn * 2 : wn;
    bh = dbl ? hn * 2 : hn;
    x = q->cx - bw / 2;
    y = q->cy - bh / 2;
    if (x >= SCR_W || x + bw <= 0 || y >= SCR_H || y + bh <= 0) return;
    inv = 65536 / scale;
    g = aff_n++;
    oam[g * 16 + 3] = (u16)(q->mirror ? -inv : inv);
    oam[g * 16 + 7] = 0;
    oam[g * 16 + 11] = 0;
    oam[g * 16 + 15] = (u16)inv;
    oam[oam_n * 4 + 0] = (u16)((y & 0xFF) | 0x100 | (dbl ? 0x200 : 0) | (shape << 14));
    oam[oam_n * 4 + 1] = (u16)((x & 0x1FF) | (g << 9) | (size << 14));
    oam[oam_n * 4 + 2] = (u16)((OBJ_TILE_BASE + tile) | (q->bank << 12));
    oam_n++;
}

static void emit_queue(void)
{
    int i, j;
    for (i = 1; i < sq_n; i++)           // insertion sort: nearest first (drawn on top)
    {
        sq_t t = sq[i];
        for (j = i - 1; j >= 0 && sq[j].key > t.key; j--) sq[j + 1] = sq[j];
        sq[j + 1] = t;
    }
    for (i = 0; i < sq_n; i++) emit_sprite(&sq[i]);
}

// ============================================================================ cars
typedef struct
{
    s32 z64;            // position along the track, 1/64 units
    s32 x16;            // lateral, 16.16 road fractions
    s32 spd64;
    s32 lane16;
    s32 base64;         // cruising speed
    u8 kind, bank, racer, phase;
    s8 side;            // which side of the player it was last frame
} car_t;

static car_t cars[NCARS];
#ifdef HOST_TEST
int dbg_notraffic;
#endif

// ============================================================================ player / race state
static s32 player_z64;
static s32 speed64;
static s32 px16;        // lateral position, 16.16 road fractions (-1..1 is the road)
static s32 lv16;        // lateral velocity
static s32 race_dist;
static int next_cp;
static int time_left;
static u32 score;
static int count_frames;
static int end_frames;
static int end_reason;      // 0 finish, 1 time up
static int final_rank;
static int cp_msg;
static int steer_dir;       // -1/0/1 for sprite pose
static int braking;
static int shake;
static int rank_now = 4;
static int theme_a, theme_b, theme_t = 255;
static int pal_dirty;
static int hit_flash;
static int bonus_msg;
static int new_best;

static int lap_index(void) { return (int)((race_dist + 1000) / track_len); }

static s32 wrap_diff64(s32 a64, s32 b64)   // signed a-b on the loop, in 1/64 units
{
    s32 span = track_len * 64, d = a64 - b64;
    if (d > span / 2) d -= span;
    else if (d < -span / 2) d += span;
    return d;
}

static void cars_reset(void)
{
    int i;
    s32 z0 = 1000;
    for (i = 0; i < NCARS; i++)
    {
        car_t *c = &cars[i];
        memset(c, 0, sizeof(*c));
        if (i < 3)    // rival kanjo racers on the starting grid
        {
            static const u8 kinds[3] = { K_EF, K_AE86, K_EF };
            static const u8 banks[3] = { 3, 4, 5 };
            c->kind = kinds[i]; c->bank = banks[i]; c->racer = 1;
            c->z64 = (z0 + 600 + 650 * i) * 64;
            c->lane16 = (i == 1) ? 0 : (i == 0 ? -26000 : 26000);
            c->base64 = (138 + 7 * i) * 64;
        }
        else          // traffic
        {
            static const u8 kinds[7] = { K_TRUCK, K_AE86, K_EF, K_TRUCK, K_EF, K_AE86, K_TRUCK };
            static const u8 banks[7] = { 9, 7, 8, 9, 7, 8, 9 };
            int k = i - 3;
            c->kind = kinds[k]; c->bank = banks[k];
            c->z64 = (14000 + k * ((track_len - 20000) / 7) + (rnd() % 3000)) * 64;
            c->lane16 = ((k % 3) - 1) * 36000;
            c->base64 = (66 + (k * 5) % 26) * 64;
        }
        c->x16 = c->lane16;
        c->spd64 = 0;
        c->phase = (u8)(i * 29);
        c->side = 1;
    }
}

static void race_reset(void)
{
    player_z64 = 1000 * 64;
    speed64 = 0;
    px16 = 0;
    lv16 = 0;
    race_dist = 0;
    next_cp = 1;
    time_left = TIME0;
    score = 0;
    count_frames = 0;
    end_frames = 0;
    cp_msg = 0;
    steer_dir = 0;
    braking = 0;
    shake = 0;
    theme_a = theme_b = 0;
    theme_t = 255;
    pal_dirty = 1;
    hit_flash = 0;
    bonus_msg = 0;
    phase = PH_COUNT;
    cars_reset();
}

// ============================================================================ race logic
static void race_step(u16 keys)
{
    int maxspd64 = max_speed() * 64;
    int pct, acc, i, pi, curve, dir;
    int gas = 0, brake = 0;

    if (phase == PH_COUNT)
    {
        count_frames++;
        if (count_frames >= 240) { phase = PH_RACE; }
    }
    else if (phase == PH_RACE)
    {
        gas = (keys & (KEY_A | KEY_UP)) != 0;
        brake = (keys & (KEY_B | KEY_DOWN)) != 0;
        time_left--;
        if (time_left <= 0) { time_left = 0; phase = PH_END; end_reason = 1; final_rank = rank_now; }
    }
    else
    {
        end_frames++;
    }

    // ---- speed
    pct = (speed64 * 256) / maxspd64;
    if (gas)
    {
        acc = pct < 100 ? 56 : pct < 160 ? 28 : pct < 210 ? 18 : 12;
        speed64 += acc * accel_scale() / 256;
    }
    else if (brake)
    {
        speed64 -= 64;
        braking = 1;
    }
    else if (phase == PH_END) speed64 -= 40;
    else if (!hold_gas) speed64 -= 10;
    if (!brake) braking = 0;
    if (speed64 > maxspd64) speed64 -= 24, speed64 = imax(speed64, maxspd64);
    if (speed64 < 0) speed64 = 0;
    pct = (speed64 * 256) / maxspd64;

    // ---- steering (works at every speed, even crawling)
    dir = 0;
    if (phase == PH_RACE)
    {
        if (keys & KEY_LEFT) dir = -1;
        else if (keys & KEY_RIGHT) dir = 1;
    }
    {
        int auth = (170 + (86 * imin(pct, 256)) / 256) * grip_scale() / 256;   // /256
        int latmax = 3300 * auth / 256;
        if (dir) lv16 += dir * 2100;
        else lv16 = lv16 * 5 / 8;
        lv16 = iclamp(lv16, -latmax, latmax);
        px16 += lv16 * (speed64 > 640 ? 256 : (int)(speed64 * 256 / 640)) / 256 + (dir && speed64 <= 640 ? dir * 600 : 0);
    }
    steer_dir = dir;

    // ---- road curvature pushes the car outwards
    pi = (int)((player_z64 >> 6) / SEGL) % num_segs;
    curve = seg_curve[pi];
    {
        int base = (2185 * pct) >> 8;
        int t = (base * pct) >> 8;
        int push = (t * curve * 3 / 80);
        px16 -= push * 256 / grip_scale() * 256 / 256;
    }

    // ---- walls
    if (px16 > 72000 || px16 < -72000)
    {
        int sgn = px16 > 0 ? 1 : -1;
        speed64 -= speed64 >> 4;
        if (speed64 > 3000) speed64 -= 60;
        px16 = sgn * 71000;
        lv16 = -sgn * 1500;
        shake = 6;
    }
    else if (px16 > 61000 || px16 < -61000)
    {
        speed64 -= speed64 >> 7;    // rumble strip drag
    }
    if (shake) shake--;

    // ---- advance
    player_z64 += speed64;
    race_dist += speed64 >> 6;
    if (player_z64 >= track_len * 64) player_z64 -= track_len * 64;
    if (phase == PH_RACE) score += (u32)(speed64 >> 10);

    // ---- checkpoints / finish
    while (next_cp <= TOTAL_CP && race_dist >= next_cp * cp_len - 1000)
    {
        if (next_cp == TOTAL_CP)
        {
            if (phase == PH_RACE)
            {
                phase = PH_END;
                end_reason = 0;
                final_rank = rank_now;
                score += (u32)(time_left / 6) + (final_rank == 1 ? 5000u : final_rank == 2 ? 3000u : final_rank == 3 ? 1500u : 500u);
            }
        }
        else
        {
            time_left += CP_BONUS;
            cp_msg = 150;
        }
        next_cp++;
    }
    if (cp_msg) cp_msg--;

    // ---- time-of-day follows the lap
    {
        int want = lap_index() % 3;
        if (want != theme_b)
        {
            theme_a = theme_b;
            theme_b = want;
            theme_t = 0;
        }
        if (theme_t < 255)
        {
            theme_t = imin(255, theme_t + 3);
            pal_dirty = 1;
        }
    }

    // ---- other cars
    rank_now = 1;
    for (i = 0; i < NCARS; i++)
    {
        car_t *c = &cars[i];
        s32 dz, target;
        int cseg, ccurve, dx;
#ifdef HOST_TEST
        if (dbg_notraffic && !c->racer) { c->z64 = 0; c->x16 = 900000; continue; }
#endif
        if (phase == PH_COUNT)
        {
            c->x16 = c->lane16;
            if (c->racer) rank_now++;
            continue;
        }
        cseg = (int)((c->z64 >> 6) / SEGL) % num_segs;
        ccurve = iabs(seg_curve[cseg]);
        dz = wrap_diff64(c->z64, player_z64);       // + = ahead of the player
        target = c->base64;
        if (c->racer)
        {
            if (dz > 6000 * 64) target = target * 90 / 100;           // rubber band: ease off when far ahead
            else if (dz < -3000 * 64) target = target * 112 / 100;    // catch up when behind
            target = target * (256 - ccurve * 2) / 256;
            if (dz > 0 && dz < 80000 * 64) rank_now++;
            c->x16 = c->lane16 + (tri((frame_no >> 1) + c->phase) - 32) * 380;
        }
        else
        {
            c->x16 = c->lane16;
        }
        if (c->spd64 < target) c->spd64 += 12;
        else if (c->spd64 > target) c->spd64 -= 12;
        c->z64 += c->spd64;
        if (c->z64 >= track_len * 64) c->z64 -= track_len * 64;

        // collisions with the player
        dz = wrap_diff64(c->z64, player_z64);
        dx = c->x16 - px16;
        if (phase == PH_RACE && iabs(dz) < 230 * 64 && iabs(dx) < 26000)
        {
            if (speed64 > c->spd64)
            {
                speed64 = c->spd64 * 3 / 4;
                lv16 = (dx > 0 ? -1 : 1) * 2600;
                px16 += (dx > 0 ? -1 : 1) * 4000;
                shake = 10;
                hit_flash = 10;
            }
        }
        // overtaking bonus
        {
            int side = dz > 0 ? 1 : -1;
            if (c->side > 0 && side < 0 && phase == PH_RACE && iabs(dz) < 3000 * 64)
            {
                score += c->racer ? 500u : 150u;
                bonus_msg = 40;
            }
            c->side = (s8)side;
        }
    }
    if (bonus_msg) bonus_msg--;
    if (hit_flash) hit_flash--;
}

// ============================================================================ palettes
static u16 race_pal[256];
static const u16 *const sky_pals[3] = { sky_pal_0, sky_pal_1, sky_pal_2 };

// OUT_A, OUT_B, WALL_A, WALL_B, RUM_A, RUM_B, ROAD_A, ROAD_B, LANE
static const u8 theme_road[3][9][3] = {
    { {26, 12, 44}, {34, 16, 52}, {150, 150, 172}, {112, 112, 134}, {232, 50, 72}, {242, 242, 246}, {78, 70, 94}, {66, 60, 82}, {244, 232, 200} },
    { {10, 8, 28}, {16, 10, 36}, {112, 112, 138}, {82, 82, 108}, {200, 30, 62}, {222, 222, 238}, {52, 50, 72}, {42, 40, 62}, {226, 216, 170} },
    { {96, 104, 140}, {84, 96, 128}, {196, 196, 214}, {164, 164, 190}, {242, 72, 92}, {252, 252, 252}, {118, 112, 136}, {104, 98, 122}, {252, 242, 212} },
};

static int c5(u16 c, int sh) { return (c >> sh) & 31; }

static void build_race_pal(void)
{
    int i, fog, c, t = theme_t;
    const u16 *sa = sky_pals[theme_a], *sb = sky_pals[theme_b];
    int hr, hg, hb;
    for (i = 0; i < 96; i++)
    {
        int r = c5(sa[i], 0) + ((c5(sb[i], 0) - c5(sa[i], 0)) * t >> 8);
        int g = c5(sa[i], 5) + ((c5(sb[i], 5) - c5(sa[i], 5)) * t >> 8);
        int b = c5(sa[i], 10) + ((c5(sb[i], 10) - c5(sa[i], 10)) * t >> 8);
        race_pal[i] = (u16)(r | (g << 5) | (b << 10));
    }
    hr = c5(race_pal[27], 0) << 3;
    hg = c5(race_pal[27], 5) << 3;
    hb = c5(race_pal[27], 10) << 3;
    for (fog = 0; fog < 4; fog++)
        for (c = 0; c < 9; c++)
        {
            int r = theme_road[theme_a][c][0] + ((theme_road[theme_b][c][0] - theme_road[theme_a][c][0]) * t >> 8);
            int g = theme_road[theme_a][c][1] + ((theme_road[theme_b][c][1] - theme_road[theme_a][c][1]) * t >> 8);
            int b = theme_road[theme_a][c][2] + ((theme_road[theme_b][c][2] - theme_road[theme_a][c][2]) * t >> 8);
            int m = fog * 62;
            r += ((hr - r) * m) >> 8;
            g += ((hg - g) * m) >> 8;
            b += ((hb - b) * m) >> 8;
            race_pal[ROAD_BASE + fog * 10 + c] = (u16)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
        }
}

// ============================================================================ road renderer
static s16 L_x[DRAW_N + 2], L_y[DRAW_N + 2], L_w[DRAW_N + 2], L_clip[DRAW_N + 2];
static s32 L_scale[DRAW_N + 2];
static s16 row_x[SCR_H], row_w[SCR_H];
static u8 row_info[SCR_H];
static u8 linebuf[SCR_W] __attribute__((aligned(4)));
static int cam_base, cam_rem, cam_top;
static int sky_x, sky16;

IWRAM_CODE static void fill(u8 *buf, int a, int b, u8 c)
{
    u32 w;
    if (a < 0) a = 0;
    if (b > SCR_W) b = SCR_W;
    if (a >= b) return;
    while (a < b && (a & 3)) buf[a++] = c;
    w = c * 0x01010101u;
    while (a + 4 <= b) { *(u32 *)(buf + a) = w; a += 4; }
    while (a < b) buf[a++] = c;
}

IWRAM_CODE static void compose_row(u8 *buf, int cx, int hw, int info)
{
    int fog = info & 3, alt = (info >> 2) & 1, dash = (info >> 3) & 1;
    int base = ROAD_BASE + fog * 10;
    int rw, ww, xl, xr, lw, a;
    if (hw < 1) hw = 1;
    rw = (hw >> 4) + 1;
    ww = (hw >> 3) + 1;
    xl = cx - hw;
    xr = cx + hw;
    fill(buf, 0, SCR_W, (u8)(base + alt));
    fill(buf, xl - rw - ww, xl - rw, (u8)(base + 2 + alt));
    fill(buf, xr + rw, xr + rw + ww, (u8)(base + 2 + alt));
    fill(buf, xl - rw, xl, (u8)(base + 4 + alt));
    fill(buf, xr, xr + rw, (u8)(base + 4 + alt));
    fill(buf, xl, xr, (u8)(base + 6 + alt));
    lw = (hw >> 6) + 1;
    fill(buf, xl + 1, xl + 1 + lw, (u8)(base + 8));       // edge lines
    fill(buf, xr - 1 - lw, xr - 1, (u8)(base + 8));
    if (dash)
    {
        a = hw / 3;
        fill(buf, cx - a - (lw >> 1), cx - a - (lw >> 1) + lw, (u8)(base + 8));
        fill(buf, cx + a - (lw >> 1), cx + a - (lw >> 1) + lw, (u8)(base + 8));
    }
}

static inline void copy32(volatile u32 *d, const u32 *s, int n)
{
    int i;
    for (i = 0; i < n; i++) d[i] = s[i];
}

IWRAM_CODE static void sky_row(volatile u16 *dst, int r, int sx)
{
    const u16 *src;
    int first, i, n;
    if (r >= SKY_H) r = SKY_H - 1;
    src = (const u16 *)(sky_img + r * SKY_W);
    first = SKY_W - sx;
    if (first > SCR_W) first = SCR_W;
    n = first >> 1;
    for (i = 0; i < n; i++) dst[i] = src[(sx >> 1) + i];
    for (i = n; i < SCR_W / 2; i++) dst[i] = src[i - n];
}

IWRAM_CODE static void project_road(void)
{
    int n, camz, pseg, ppct, camy, pxw, x8, dx8, top = SCR_H, i;
    int playerz = (int)(player_z64 >> 6);
    camz = playerz - PLAYER_Z;
    if (camz < 0) camz += track_len;
    cam_base = camz / SEGL;
    cam_rem = camz % SEGL;
    pseg = (playerz / SEGL) % num_segs;
    ppct = (playerz % SEGL) * 256 / SEGL;
    camy = seg_y[pseg] + (((seg_y[pseg + 1] - seg_y[pseg]) * ppct) >> 8) + CAM_H;
    pxw = (int)(((s64)px16 * ROAD_HALF) >> 16);
    x8 = 0;
    {
    int idx = cam_base % num_segs;
    dx8 = -((seg_curve[idx] * (cam_rem * 256 / SEGL)) >> 8);

    for (n = 0; n <= DRAW_N; n++)
    {
        if (n >= 1)
        {
            int dz = n * SEGL - cam_rem;
            s32 sc;
            s64 sx, sy, sw;
            if (dz < 1) dz = 1;
            sc = (s32)((CAMD_FP * 256) / dz);
            sx = 120 + (((s64)sc * ((x8 >> 3) - pxw) * 120) >> 24);
            sy = HORIZON - (((s64)sc * (seg_y[idx] - camy) * 120) >> 24);
            sw = ((s64)sc * ROAD_HALF * 120) >> 24;
            L_scale[n] = sc;
            L_x[n] = (s16)iclamp((int)sx, -30000, 30000);
            L_y[n] = (s16)iclamp((int)sy, -30000, 30000);
            L_w[n] = (s16)iclamp((int)sw, 0, 30000);
        }
        x8 += dx8;
        dx8 += seg_curve[idx];
        if (++idx >= num_segs) idx = 0;
    }
    }

    for (n = 1; n < DRAW_N; n++)
    {
        int y1 = L_y[n], y2 = L_y[n + 1], rs, r, dy, num;
        int abs_idx = cam_base + n;
        L_clip[n] = (s16)top;
        if (y2 >= y1 || y2 >= top) continue;
        rs = y1 < top ? y1 : top;
        dy = y1 - y2;
        {
            s32 slx = ((L_x[n] - L_x[n + 1]) * 4096) / dy;
            s32 slw = ((L_w[n] - L_w[n + 1]) * 4096) / dy;
            int fog = n < 22 ? 0 : n < 42 ? 1 : n < 62 ? 2 : 3;
            int alt = (abs_idx / 3) & 1;
            int dash = ((abs_idx >> 2) & 1) ? 0 : 1;
            u8 info = (u8)(fog | (alt << 2) | (dash << 3));
            for (r = (y2 < 0 ? 0 : y2); r < rs; r++)
            {
                num = r - y2;
                row_x[r] = (s16)(L_x[n + 1] + ((slx * num) >> 12));
                row_w[r] = (s16)(L_w[n + 1] + ((slw * num) >> 12));
                row_info[r] = info;
            }
        }
        top = y2 < 0 ? 0 : y2;
        if (top <= 0) { for (i = n + 1; i < DRAW_N; i++) L_clip[i] = 0; break; }
    }
    cam_top = top;
    L_clip[DRAW_N] = (s16)top;
}

IWRAM_CODE static void draw_world(volatile u8 *page)
{
    int r;
    project_road();
    for (r = 0; r < SCR_H; r++)
    {
        volatile u16 *dst = (volatile u16 *)(page + r * SCR_W);
        if (r < cam_top)
        {
            sky_row(dst, r, sky_x & ~1);
        }
        else
        {
            compose_row(linebuf, row_x[r], row_w[r], row_info[r]);
            copy32((volatile u32 *)dst, (const u32 *)linebuf, SCR_W / 4);
        }
    }
}

// ============================================================================ world sprites
static void interp_line(int n, int pct, int *sx, int *sy, s32 *sc)
{
    *sx = L_x[n] + (((L_x[n + 1] - L_x[n]) * pct) >> 8);
    *sy = L_y[n] + (((L_y[n + 1] - L_y[n]) * pct) >> 8);
    *sc = L_scale[n] + (s32)(((s64)(L_scale[n + 1] - L_scale[n]) * pct) >> 8);
}

static int ppu_of(s32 sc) { return (int)(((s64)sc * 120) >> 8); }   // pixels per world unit, 16.16

static void queue_scenery(void)
{
    int n;
    int idx = (cam_base + 4) % num_segs;
    for (n = 4; n < SCEN_N; n++, idx = (idx + 1 >= num_segs ? 0 : idx + 1))
    {
        int dz = n * SEGL - cam_rem;
        int ppu = ppu_of(L_scale[n]);
        int bx, by;
        if (L_y[n] > L_clip[n] + 2) continue;
        bx = L_x[n];
        by = L_y[n];
        if (seg_near[idx])
        {
            int t = seg_near[idx];
            int lat = seg_nearx[idx] * ROAD_HALF / 16;
            int sx = bx + (int)(((s64)ppu * lat) >> 16);
            if (t == OB_LAMP)
            {
                int sc = ppu * 1500 / 16384;          // native 64px = 1500 units
                int right = lat > 0;
                int ax = (23 * sc) >> 9;
                queue(K_LAMP, 14, dz, right ? sx - ax : sx + ax, by - ((64 * sc) >> 9), sc, right);
            }
            else if (t == OB_BOARD)
            {
                int sc = ppu * 1900 / 16384;
                queue(K_BOARD, 13, dz, sx, by - ((32 * sc) >> 9), sc, 0);
            }
            else if (t == OB_GATE)
            {
                int pl = (int)(((s64)ppu * 1235) >> 16);
                int scp = ppu * 1900 / 8192;                       // post: native 32px = 1900 units
                int scb = ppu * 2700 / 16384;                      // banner spans the road
                int midy = by - (int)(((s64)ppu * 1750) >> 16);
                queue(K_POST, 13, dz, bx - pl, by - ((32 * scp) >> 9), scp, 0);
                queue(K_POST, 13, dz, bx + pl, by - ((32 * scp) >> 9), scp, 0);
                queue(K_BANNER, 13, dz - 1, bx, midy + ((8 * scb) >> 8), scb, 0);
            }
        }
        if (seg_far[idx] && n < 52)
        {
            int t = seg_far[idx];
            int lat = seg_farx[idx] * ROAD_HALF / 16;
            int sx = bx + (int)(((s64)ppu * lat) >> 16);
            int sc = ppu * (t == OB_BLDG_A ? 3800 : 2900) / 16384;
            queue(t == OB_BLDG_A ? K_BLDG_A : K_BLDG_B, 14, dz + 5, sx, by - ((64 * sc) >> 9), sc, 0);
        }
    }
}

static void queue_cars(void)
{
    int i;
    int camz64 = (int)player_z64 - PLAYER_Z * 64;
    int span = track_len * 64;
    for (i = 0; i < NCARS; i++)
    {
        car_t *c = &cars[i];
        int d64 = c->z64 - camz64, d, n, pct, sx, sy, ppu, sc, lat;
        s32 scl;
        if (d64 < 0) d64 += span;
        if (d64 >= span) d64 -= span;
        d = d64 >> 6;
        d += cam_rem;
        n = d / SEGL;
        if (n < 3 || n >= DRAW_N - 1) continue;
        pct = (d % SEGL) * 256 / SEGL;
        interp_line(n, pct, &sx, &sy, &scl);
        if (sy > L_clip[n] + 2) continue;
        ppu = ppu_of(scl);
        lat = (int)(((s64)c->x16 * ROAD_HALF) >> 16);
        sx += (int)(((s64)ppu * lat) >> 16);
        sc = ppu * 560 / 16384;
        queue(c->kind, c->bank, d, sx, sy - ((32 * sc) >> 9), sc, 0);
    }
}

// ============================================================================ HUD
static void hud_race(void)
{
    char b[40], *p;
    int pct = (speed64 * 256) / (max_speed() * 64);
    int gear = imin(5, 1 + pct * 5 / 257);
    int rev = ((pct * 5) & 255) * 12 / 256 + (speed64 ? 1 : 0) + (frame_no & 1);
    int i, secs = time_left / 60, lap = imin(3, lap_index() + 1);

    p = put_str(b, "SCORE ");
    p = put_int(p, (int)score, 7, '0');
    *p = 0;
    text8(4, 3, b, 0);

    p = put_str(b, "TIME ");
    p = put_int(p, secs, 2, ' ');
    *p = 0;
    text8(4, 13, b, secs <= 10 ? 12 : 15);

    p = put_str(b, "LAP ");
    p = put_int(p, lap, 1, ' ');
    p = put_str(p, "/3");
    *p = 0;
    text8(180, 3, b, 0);

    p = put_str(b, "POS ");
    p = put_int(p, rank_now, 1, ' ');
    p = put_str(p, "/4");
    *p = 0;
    text8(180, 13, b, 15);

    // tachometer
    for (i = 0; i < 12; i++)
    {
        int lit = i < imin(12, rev);
        text8(72 + i * 8, 24, lit ? "#" : "_", lit ? (i < 7 ? 10 : i < 10 ? 11 : 12) : 0);
    }

    p = put_int(b, kmh_of(speed64), 3, ' ');
    *p = 0;
    text16(6, 126, b, 15);
    text8(8, 146, "KM/H", 0);
    b[0] = 'G'; b[1] = (char)('0' + gear); b[2] = 0;
    text16(206, 126, b, 15);
    text8(204, 146, "GEAR", 0);
}

static void hud_overlay(void)
{
    char b[40], *p;
    if (phase == PH_COUNT)
    {
        int c = 3 - count_frames / 60;
        if (c > 0) { b[0] = (char)('0' + c); b[1] = 0; text16(112, 52, b, 15); }
        else text16(104, 52, "GO!", 15);
    }
    else if (phase == PH_RACE && count_frames < 300) { /* GO fades */ }
    if (phase == PH_RACE && cp_msg > 0)
    {
        text16(56, 44, "CHECKPOINT", 15);
        text8(84, 64, "TIME EXTENDED", 11);
    }
    if (phase == PH_RACE && bonus_msg > 0 && cp_msg == 0) text8(92, 36, "NICE PASS!", 11);
    if (phase == PH_END)
        text16(end_reason ? 64 : 56, 44, end_reason ? "TIME UP" : "FINISH!", 15);
    (void)b; (void)p;
}

// ============================================================================ static screens / menus
static const u16 *pending_pal;
static int scr_dirty;
static int flip_pending;
static int disp_page;

static void hub_text(void)
{
    char b[40], *p;
    int i, pct;
    static const char *const names[4] = { "ENGINE", "GRIP", "GEARS", "WEIGHT" };
    if (state == ST_HUB)
    {
        text8(128, 36, "BEAR GARAGE", 11);
        text8(136, 52, "RACE", menu_sel == 0 ? 15 : 0);
        text8(136, 64, "GARAGE", menu_sel == 1 ? 15 : 0);
        text8(136, 76, hold_gas ? "GAS: HOLD" : "GAS: NORMAL", menu_sel == 2 ? 15 : 0);
        text8(128, 52 + menu_sel * 12, ">", 15);
        p = put_str(b, "TOP  ");
        p = put_int(p, kmh_of(max_speed() * 64), 3, ' ');
        p = put_str(p, "KM/H");
        *p = 0;
        text8(128, 100, b, 0);
        p = put_str(b, "0-TOP ");
        pct = 750 * 256 / accel_scale();
        p = put_int(p, pct / 100, 2, ' ');
        *p++ = '.';
        p = put_int(p, (pct / 10) % 10, 1, '0');
        p = put_str(p, " S");
        *p = 0;
        text8(128, 110, b, 0);
        p = put_str(b, "BEST ");
        p = put_int(p, (int)best_score, 7, '0');
        *p = 0;
        text8(128, 128, b, 15);
    }
    else if (state == ST_RESULT)
    {
        text8(128, 36, "RESULT", 11);
        text8(128, 52, end_reason ? "TIME UP!" : "GOAL!", 15);
        if (!end_reason)
        {
            p = put_str(b, "POS ");
            p = put_int(p, final_rank, 1, ' ');
            p = put_str(p, " OF 4");
            *p = 0;
            text8(128, 68, b, 0);
        }
        text8(128, 84, "SCORE", 0);
        p = put_int(b, (int)score, 7, '0');
        *p = 0;
        text8(136, 94, b, 15);
        if (new_best) text8(128, 108, "NEW BEST!", ((frame_no >> 3) & 1) ? 11 : 12);
        else
        {
            p = put_str(b, "BEST ");
            p = put_int(p, (int)best_score, 7, '0');
            *p = 0;
            text8(128, 108, b, 0);
        }
        if ((frame_no >> 4) & 1) text8(128, 128, "PRESS A", 11);
    }
    else if (state == ST_GARAGE)
    {
        text8(128, 36, "TUNING", 11);
        for (i = 0; i < 4; i++)
        {
            int k;
            text8(134, 52 + i * 14, names[i], menu_sel == i ? 15 : 0);
            for (k = 0; k < 5; k++)
                text8(188 + k * 8, 52 + i * 14, k < tune[i] ? "#" : "_", k < tune[i] ? 11 : 0);
        }
        text8(124, 52 + menu_sel * 14, ">", 15);
        p = put_str(b, "TOP  ");
        p = put_int(p, kmh_of(max_speed() * 64), 3, ' ');
        p = put_str(p, "KM/H");
        *p = 0;
        text8(128, 112, b, 0);
        text8(128, 128, "A:BACK", 0);
    }
}

static void enter_state(int s)
{
    state = s;
    menu_sel = 0;
    scr_dirty = 1;
    if (s == ST_TITLE) pending_pal = title_pal;
    else if (s == ST_HUB || s == ST_GARAGE || s == ST_RESULT) pending_pal = hub_pal;
}

// ============================================================================ sound (simple PSG engine tone)
static void sound_update(void)
{
    int on = (state == ST_RACE && phase != PH_END);
    if (!on)
    {
        REG16(0x80) = 0x0077;
        return;
    }
    {
        int maxspd64 = max_speed() * 64;
        int pct = (speed64 * 256) / maxspd64;
        int gear = imin(5, 1 + pct * 5 / 257);
        int rpm = ((pct * 5) & 255);
        int hz = 70 + rpm / 2 + gear * 18 + (speed64 ? 10 : 0);
        int f1 = 2048 - 131072 / hz;
        int f2 = 2048 - 131072 / (hz * 3 / 2 + 3);
        REG16(0x84) = 0x80;
        REG16(0x82) = 0x0002;
        REG16(0x80) = 0x3377;
        REG16(0x62) = 0x2080;
        REG16(0x68) = 0x1080;
        REG16(0x64) = (u16)((f1 & 0x7FF) | ((frame_no & 31) == 0 ? 0x8000 : 0));
        REG16(0x6C) = (u16)((f2 & 0x7FF) | ((frame_no & 31) == 0 ? 0x8000 : 0));
    }
}

// ============================================================================ public API
void game_init(void)
{
    int i;
    volatile u32 *objv = (volatile u32 *)(VRAM_BASE + OBJ_VRAM_OFFSET);
    REG_DISPCNT = 0x80;                     // forced blank while loading
    REG16(0x20) = 0x100;                    // BG2 identity matrix (Mode 4 uses the affine BG2 registers)
    REG16(0x22) = 0;
    REG16(0x24) = 0;
    REG16(0x26) = 0x100;
    REG32(0x28) = 0;
    REG32(0x2C) = 0;
    for (i = 0; i < OBJ_TILE_COUNT * 8; i++) objv[i] = obj_tiles[i];
    for (i = 0; i < 256; i++) PAL_OBJ[i] = obj_pal[i];
    build_track();
    load_game();
    oam_reset();
    oam[31 * 16 + 3] = 128;                 // affine group 31 = 2x text
    oam[31 * 16 + 7] = 0;
    oam[31 * 16 + 11] = 0;
    oam[31 * 16 + 15] = 128;
    for (i = 0; i < 128 * 4; i++) OAM_RAM[i] = oam[i];
    enter_state(ST_TITLE);
    disp_page = 0;
    flip_pending = 0;
    REG_DISPCNT = DCNT_BASE;
}

void game_step(u16 keys)
{
    u16 pressed = (u16)(keys & ~keys_prev);
    keys_prev = keys;
    frame_no++;
    switch (state)
    {
    case ST_TITLE:
        if (pressed & (KEY_START | KEY_A)) enter_state(ST_HUB);
        break;
    case ST_HUB:
        if (pressed & KEY_UP) menu_sel = (menu_sel + 2) % 3;
        if (pressed & KEY_DOWN) menu_sel = (menu_sel + 1) % 3;
        if (pressed & KEY_B) enter_state(ST_TITLE);
        else if (pressed & (KEY_A | KEY_START))
        {
            if (menu_sel == 0)
            {
                race_reset();
                state = ST_RACE;
                scr_dirty = 1;
            }
            else if (menu_sel == 1)
            {
                enter_state(ST_GARAGE);
            }
            else
            {
                hold_gas = !hold_gas;
                save_game();
            }
        }
        break;
    case ST_GARAGE:
        if (pressed & KEY_UP) menu_sel = (menu_sel + 3) % 4;
        if (pressed & KEY_DOWN) menu_sel = (menu_sel + 1) % 4;
        if (pressed & KEY_LEFT) tune[menu_sel] = imax(0, tune[menu_sel] - 1);
        if (pressed & KEY_RIGHT) tune[menu_sel] = imin(4, tune[menu_sel] + 1);
        if (pressed & (KEY_A | KEY_B | KEY_START))
        {
            save_game();
            enter_state(ST_HUB);
            menu_sel = 1;
        }
        break;
    case ST_RACE:
        race_step(keys);
        sky16 = (sky16 + 7680 + ((seg_curve[(int)((player_z64 >> 6) / SEGL) % num_segs] * (speed64 >> 6)) >> 9)) % 7680;
        sky_x = sky16 >> 4;
        if (phase == PH_END && end_frames > 100)
        {
            new_best = score > best_score;
            if (new_best) { best_score = score; save_game(); }
            enter_state(ST_RESULT);
        }
        break;
    case ST_RESULT:
        if (pressed & (KEY_A | KEY_START | KEY_B)) enter_state(ST_HUB);
        break;
    }
    sound_update();
}

void game_draw(void)
{
    volatile u8 *back = VRAM_BASE + (disp_page ? 0 : PAGE1_OFFSET);
    int i;
    oam_reset();
    sq_n = 0;

    if (state == ST_RACE)
    {
        int bank = braking ? 2 : 1, frm = steer_dir > 0 ? 1 : steer_dir < 0 ? 2 : 0;
        int bob = (speed64 > 640 && ((frame_no >> 2) & 1)) ? 1 : 0;
        int sh = shake ? ((frame_no & 1) ? 2 : -2) : 0;
        static sq_t pl;
        if (hit_flash) bank = (frame_no & 2) ? 2 : 1;
        // HUD first (lowest OAM slots = on top)
        hud_race();
        hud_overlay();
        // player car (always nearest)
        pl.cx = (s16)(120 + sh);
        pl.cy = (s16)(154 - 16 + bob);
        pl.scale = 256;
        pl.kind = K_EF; pl.bank = (u8)bank; pl.mirror = 0; pl.key = -1;
        // draw the player with its own tile set (3 steering poses)
        if (oam_n < 126 && aff_n < 31)
        {
            int g = aff_n++;
            oam[g * 16 + 3] = 256; oam[g * 16 + 7] = 0; oam[g * 16 + 11] = 0; oam[g * 16 + 15] = 256;
            oam[oam_n * 4 + 0] = (u16)((((int)pl.cy - 16) & 0xFF) | 0x100 | (1 << 14));
            oam[oam_n * 4 + 1] = (u16)((((int)pl.cx - 32) & 0x1FF) | (g << 9) | (3 << 14));
            oam[oam_n * 4 + 2] = (u16)((OBJ_TILE_BASE + T_PLAYER0 + frm * 32) | (bank << 12));
            oam_n++;
        }
        if (pal_dirty) { build_race_pal(); pending_pal = race_pal; pal_dirty = 0; }
        if (scr_dirty) { pending_pal = race_pal; build_race_pal(); scr_dirty = 0; }
        draw_world(back);
        queue_scenery();
        queue_cars();
        emit_queue();
        flip_pending = 1;
    }
    else
    {
        if (scr_dirty)
        {
            const u8 *img = (state == ST_TITLE) ? title_img : hub_img;
            volatile u16 *d = (volatile u16 *)back;
            const u16 *s = (const u16 *)img;
            for (i = 0; i < SCR_W * SCR_H / 2; i++) d[i] = s[i];
            flip_pending = 1;
            scr_dirty = 0;
        }
        if (state == ST_TITLE)
        {
            if ((frame_no >> 5) & 1) text8(84, 142, "PRESS START", 15);
        }
        else
        {
            hub_text();
        }
    }
}

void game_present(void)
{
    int i;
    if (pending_pal)
    {
        int n = (pending_pal == race_pal) ? 256 : 256;
        for (i = 0; i < n; i++) PAL_BG[i] = pending_pal[i];
        pending_pal = 0;
    }
    if (flip_pending)
    {
        disp_page ^= 1;
        REG_DISPCNT = (u16)(DCNT_BASE | (disp_page ? 0x10 : 0));
        flip_pending = 0;
    }
    for (i = 0; i < 128 * 4; i++) OAM_RAM[i] = oam[i];
}

int game_frames_wanted(void) { return 1; }

#ifdef HOST_TEST
int game_dbg(int what)
{
    switch (what)
    {
    case 0: return px16;
    case 1: return speed64;
    case 2: return race_dist;
    case 3: return seg_curve[(int)((player_z64 >> 6) / SEGL) % num_segs];
    case 4: return phase;
    case 5: return state;
    case 6: return time_left;
    case 7: return (int)score;
    case 8: return num_segs;
    case 9: return next_cp;
    case 10: return rank_now;
    case 11: return end_reason;
    case 12: return (int)((player_z64 >> 6));
    }
    return 0;
}
#endif
