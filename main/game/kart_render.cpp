/*
===============================================================================
  kart_render.cpp — Rendu pseudo-3D OutRun (Akart, Gamebuino AKA)
===============================================================================
*/

#include "kart_render.h"
#include "../core/graphics.h"
#include "../core/input.h"
#include "../core/kart_loader.h"

// Décors
#include "../assets/gfx/decor_palm.h"
#include "../assets/gfx/decor_tree1.h"
#include "../assets/gfx/decor_tree2.h"
#include "../assets/gfx/decor_tree3.h"
#include "../assets/gfx/decor_tree4.h"
#include "../assets/gfx/decor_cactus1.h"
#include "../assets/gfx/decor_cactus2.h"
#include "../assets/gfx/decor_cactus3.h"
#include "../assets/gfx/decor_tire.h"
#include "../assets/gfx/decor_flag.h"
#include "../assets/gfx/decor_rock.h"
#include "../assets/gfx/decor_start_gate.h"
#include "../assets/gfx/decor_finish_gate.h"
#include "../assets/gfx/decor_house.h"
#include "../assets/gfx/decor_sky_bg.h"
#include "../assets/gfx/decor_sky_bg_city.h"
#include "../assets/gfx/decor_sky_bg_desert.h"
#include "../assets/gfx/decor_sign_left.h"
#include "../assets/gfx/decor_sign_right.h"
#include "../assets/gfx/decor_oil_slick.h"
#include "../assets/gfx/decor_bonus_boost.h"
#include "../assets/gfx/decor_bonus_shield.h"
#include "../assets/gfx/decor_bonus_shock.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <utility>

namespace kart {

static const int   W = 320;
static const int   H = 240;
static const float MAX_SPEED       = 7.0f;
static const float ROAD_HALF_WIDTH = 200.0f;
static const int   DRAW_DISTANCE   = 100;

static float sky_scroll_x = 0.0f;
static unsigned int g_frame_counter = 0; // pour le clignotement des bonus box

// -----------------------------------------------------------------------------
// Projection pseudo-3D optimisée
// -----------------------------------------------------------------------------
struct Proj { float x, y, w, scale; };

static inline Proj project(const Camera& cam, float cam_x, float cam_height, float cam_z) {
    if (cam_z < 0.001f) cam_z = 0.001f;
    float sc = cam.depth / cam_z;

    float halfW = W * 0.5f;
    float halfH = H * 0.5f;

    return {
        halfW - sc * cam_x * halfW,
        halfH - sc * (-cam_height) * halfH,
        sc * ROAD_HALF_WIDTH * halfW,
        sc
    };
}

void camera_setup(Camera& cam) {
    cam.depth    = 1.0f / std::tan((cam.fov * 0.5f) * 3.14159265f / 180.0f);
    cam.player_z = cam.height * cam.depth;
}

// -----------------------------------------------------------------------------
// Fond défilant optimisé (moins de modulo, moins de multiplications)
// -----------------------------------------------------------------------------
static void draw_sky(float speed_pct, SkyTheme theme) {
    sky_scroll_x += speed_pct * 0.4f;
    if (sky_scroll_x >= SKY_BG_W) sky_scroll_x -= SKY_BG_W;

    int off = (int)sky_scroll_x;

    // Les 3 thèmes (collines/ville/désert) sont tous convertis à la même
    // taille (640x120) : on peut donc juste changer le pointeur de données
    // sans toucher au reste de la logique (scroll, run-length, padding...).
    const uint16_t* sky_data = decor_sky_bg;
    switch (theme) {
        case SkyTheme::City:   sky_data = decor_sky_bg_city;   break;
        case SkyTheme::Desert: sky_data = decor_sky_bg_desert; break;
        case SkyTheme::Hills:
        default:                sky_data = decor_sky_bg;        break;
    }

    // Fond ciel plein AVANT le décor (montagnes/immeubles/dunes) : c'est ce
    // qui se voit à travers les pixels transparents (clé magenta 0xF81F).
    core::graphics_fill_rect(0, 0, W, H / 2, core::Color::SkyBlue);

    // SKY_BG_H (120) correspond pile à H/2 (l'asymptote théorique de la
    // ligne d'horizon), mais le segment le plus lointain réellement dessiné
    // (DRAW_DISTANCE fini, pas l'infini) s'arrête juste avant cette limite :
    // il reste donc une mince bande non peinte entre le bas du ciel et le
    // haut du sol, qui laisse voir la couleur de fond (vert) derrière. On
    // étend le décor de quelques pixels en réutilisant sa dernière ligne
    // (bande de brume/horizon) comme remplissage, pour que le bas du ciel
    // rejoigne le haut du sol au lieu de s'arrêter juste avant.
    const int SKY_EXTRA_PAD = 8; // pixels de plus sous SKY_BG_H, à ajuster si besoin
    int render_h = (SKY_BG_H < H/2) ? SKY_BG_H : H/2;
    int total_h  = std::min(render_h + SKY_EXTRA_PAD, H);

    const uint16_t SKY_TRANSPARENT_KEY = 0xF81F;

    for (int y = 0; y < total_h; y += 2) {
        const uint16_t* row = sky_data + std::min(y, SKY_BG_H - 1) * SKY_BG_W;
        int sx = 0;

        // idx = (sx+off) % SKY_BG_W, mais en évitant la division : sx<320
        // et off<640 donc sx+off<960<2*640, une seule soustraction suffit
        // (plus rapide qu'un modulo sur la plupart des cœurs embarqués).
        auto wrapped = [&](int v) { return (v >= SKY_BG_W) ? v - SKY_BG_W : v; };

        while (sx < W) {
            int idx = wrapped(sx + off);
            uint16_t col = row[idx];
            int run = 1;

            while (sx + run < W && row[wrapped(sx + run + off)] == col) {
                run++;
            }

            // Pixel(s) magenta : on ne dessine rien, le ciel bleu plein
            // posé juste au-dessus reste visible à travers.
            if (col != SKY_TRANSPARENT_KEY) {
                core::graphics_hline_raw(sx, y, run, col);
            }
            sx += run;
        }
    }
}

// -----------------------------------------------------------------------------
// Décors optimisés
// -----------------------------------------------------------------------------
struct DecorInfo {
    const uint16_t* data;
    int src_w, src_h;
};

static const DecorInfo& decor_info(uint8_t id, SkyTheme theme) {
    static const DecorInfo table[] = {
        { nullptr,           0,   0  }, // 0
        { decor_tree1,      48,  96  }, // 1
        { decor_tree2,      48,  96  }, // 2
        { decor_tree3,      48,  96  }, // 3
        { decor_tree4,      48,  96  }, // 4
        { decor_palm,       64,  96  }, // 5
        { decor_tire,       48,  48  }, // 6
        { decor_flag,       64,  64  }, // 7
        { decor_rock,       64,  64  }, // 8
        { decor_house,      64,  80  }, // 9
        { decor_start_gate,128,  80  }, // 10
        { decor_finish_gate,128, 80  }, // 11
        { decor_sign_left,  16,  32  }, // 12
        { decor_sign_right, 16,  32  }, // 13
    };

    // Thème désert : les 4 slots "arbre" (1-4) sont remplacés par des
    // cactus (on n'a que 3 visuels de cactus, le 4e réutilise le 1er).
    // Tout le reste (pneus, panneaux, portiques...) reste identique quel
    // que soit le thème.
    if (theme == SkyTheme::Desert && id >= 1 && id <= 4) {
        static const DecorInfo cactus_table[] = {
            { decor_cactus1, 48, 96 }, // remplace tree1
            { decor_cactus2, 48, 96 }, // remplace tree2
            { decor_cactus3, 48, 96 }, // remplace tree3
            { decor_cactus1, 48, 96 }, // remplace tree4 (réutilise cactus1)
        };
        return cactus_table[id - 1];
    }

    return (id < 14) ? table[id] : table[0];
}

// Décalque plat au sol (flaque d'huile, bonus box) : centré en (cx,cy),
// contrairement à draw_decor_side (ancré par le bas, pour un objet "debout").
static inline void draw_road_decal(float cx, float cy, float road_w, float size_frac,
                                    const uint16_t* data, int src_w, int src_h) {
    int dst_w = std::clamp((int)(road_w * size_frac), 2, W);
    if (dst_w <= 0) return;
    int dst_h = (int)(dst_w * (float)src_h / src_w);

    core::graphics_draw_bitmap565_scaled(
        (int)cx - dst_w / 2,
        (int)cy - dst_h / 2,
        dst_w, dst_h,
        data, src_w, src_h
    );
}

static inline void draw_decor_side(int cx, int ground_y, float road_w, uint8_t id, SkyTheme theme) {
    const auto& di = decor_info(id, theme);
    if (!di.data) return;

    float f = road_w * 0.55f;
    f = std::clamp(f, 4.0f, 128.0f);

    int dst_w = (int)f;
    int dst_h = (int)(f * (float)di.src_h / di.src_w);

    core::graphics_draw_bitmap565_scaled(
        cx - dst_w/2,
        ground_y - dst_h,
        dst_w, dst_h,
        di.data, di.src_w, di.src_h
    );
}

static inline void draw_decor_fullwidth(int cx, int ground_y, float road_w, uint8_t id, SkyTheme theme) {
    const auto& di = decor_info(id, theme);
    if (!di.data) return;

    // Plafond remonté (avant : W/H pile, donc atteint bien avant de passer
    // réellement sous le portique — il semblait "s'enfoncer" au lieu de
    // continuer à grossir jusqu'au bon moment). 2x l'écran laisse largement
    // le temps de grossir jusqu'à déborder franchement du cadre avant de
    // se figer, sans risquer un vrai souci de coût (le clip d'écran réel
    // dans graphics_draw_bitmap565_scaled ignore déjà les pixels hors-champ).
    const int MAX_W = W * 2;
    const int MAX_H = H * 2;

    int dst_w = std::clamp((int)(road_w * 2.0f), 4, MAX_W);
    int dst_h = (int)(dst_w * (float)di.src_h / di.src_w);
    dst_h = std::clamp(dst_h, 4, MAX_H);

    // BUG CORRIGÉ ("un pied du portique glisse vers le centre") : cx inclut
    // le décalage latéral du JOUEUR sur la route (p.x, cf. cax1 dans la
    // boucle de projection). Cette contribution est amplifiée par l'échelle
    // de proximité (sc) à mesure qu'on approche — mais alors que dst_w est
    // plafonné à W, cx ne l'était pas : dès que road_w*2 dépassait la
    // largeur d'écran, la structure entière (les deux piliers ensemble)
    // glissait d'un bloc dans la direction du décalage du joueur, donnant
    // l'impression qu'un pilier "rentrait" vers le centre pendant que
    // l'autre sortait de l'écran. On ramène donc cx progressivement vers le
    // centre écran à mesure que la structure dépasse la largeur visible :
    // à ce stade on est de toute façon en train de passer sous l'arche, la
    // position latérale exacte n'a plus besoin d'être précise au pixel.
    if (road_w * 2.0f > W) {
        float t = std::clamp((road_w * 2.0f - W) / W, 0.0f, 1.0f);
        cx = (int)(cx * (1.0f - t) + (W / 2) * t);
    }

    core::graphics_draw_bitmap565_scaled(
        cx - dst_w/2,
        ground_y - dst_h,
        dst_w, dst_h,
        di.data, di.src_w, di.src_h
    );
}

// -----------------------------------------------------------------------------
// Billboards (IA + Décors)
// -----------------------------------------------------------------------------
struct Billboard {
    float    dz;
    int      sx, sy;
    float    road_w;
    bool     full_width;
    uint8_t  decor_id;      // 0 = kart IA
    int      ai_idx;
    core::KartSpriteId kart_frame;
    int      kart_size;
    core::KartColor kart_color = core::KartColor::Blue; // couleur réelle du kart (cf. KartState::color)
};

static inline void draw_billboard(const Billboard& b, SkyTheme theme) {
    if (b.decor_id == 0) {
        // Kart IA
        int sz = std::clamp(b.kart_size, 4, 96);
        const core::KartSprites& spr = core::get_kart_sprites(b.kart_color);
        core::draw_kart_sprite_scaled(
            spr, b.kart_frame,
            b.sx - sz/2,
            b.sy - sz,
            sz
        );
    }
    else if (b.full_width) {
        draw_decor_fullwidth(b.sx, b.sy, b.road_w, b.decor_id, theme);
    }
    else {
        draw_decor_side(b.sx, b.sy, b.road_w, b.decor_id, theme);
    }
}

// -----------------------------------------------------------------------------
// Minimap optimisée (centrée + zoom auto + ratio équilibré)
// -----------------------------------------------------------------------------
struct TrackShape {
    std::vector<std::pair<float,float>> pts;
    float min_x=0, max_x=0, min_y=0, max_y=0;
};

static TrackShape g_shape;
static int g_shape_cached_generation = -1; // -1 = jamais calculé

static void build_shape(const Track& t) {
    if (g_shape_cached_generation == t.generation) return;
    g_shape_cached_generation = t.generation;
    g_shape.pts.clear();
    g_shape.min_x = g_shape.max_x = g_shape.min_y = g_shape.max_y = 0.0f;

    if (t.has_shape) {
        // ---------------------------------------------------------------
        // Piste générée par l'éditeur (track_editor.html) : chaque segment
        // porte son vrai déplacement (x,y) tel que dessiné à l'écran. On se
        // contente de les cumuler : la minimap correspond alors EXACTEMENT
        // au circuit réellement dessiné, par construction (pas de dérivation
        // approximative depuis `curve`).
        // ---------------------------------------------------------------
        float x = 0, y = 0;
        for (auto& s : t.segs) {
            x += s.shape_dx;
            y += s.shape_dy;
            g_shape.pts.push_back({x, y});
            g_shape.min_x = std::min(g_shape.min_x, x);
            g_shape.max_x = std::max(g_shape.max_x, x);
            g_shape.min_y = std::min(g_shape.min_y, y);
            g_shape.max_y = std::max(g_shape.max_y, y);
        }
        return;
    }

    // ---------------------------------------------------------------
    // Repli pour les pistes "à la main" (track_example.h) qui n'ont pas de
    // shape_dx/dy réel : on approxime depuis `curve`, qui n'est qu'un
    // artifice de rendu pseudo-3D sans lien géométrique exact avec la
    // vraie forme du circuit — la minimap qui en résulte reste donc
    // approximative (proportions des virages indicatives, mais le tracé ne
    // "colle" pas forcément au vrai circuit). Pour une minimap fidèle,
    // reconstruire la piste avec track_editor.html (cf. Track::has_shape).
    //
    // Comme un circuit fermé DOIT effectuer un virage net de ±360° sur un
    // tour complet, on calibre ici le facteur K de sorte que la somme des
    // virages fasse exactement ±2*PI, pour au moins garantir une boucle qui
    // se referme (même si elle ne "colle" pas exactement au vrai circuit).
    // ---------------------------------------------------------------
    float curve_len_sum = 0.0f;
    for (auto& s : t.segs) curve_len_sum += s.curve * s.length;

    const float TWO_PI = 6.28318530718f;
    float k = (std::abs(curve_len_sum) > 0.0001f) ? (TWO_PI / curve_len_sum) : 0.0f;

    float h = 0, x = 0, y = 0;

    for (auto& s : t.segs) {
        h += s.curve * s.length * k;
        x += std::cos(h) * s.length;
        y += std::sin(h) * s.length;

        g_shape.pts.push_back({x,y});

        g_shape.min_x = std::min(g_shape.min_x, x);
        g_shape.max_x = std::max(g_shape.max_x, x);
        g_shape.min_y = std::min(g_shape.min_y, y);
        g_shape.max_y = std::max(g_shape.max_y, y);
    }
}

static void draw_minimap(const Track& t, const std::vector<KartState>& karts) {
    build_shape(t);

    int mx = W - 80;
    int my = 8;
    int mw = 72;
    int mh = 72;
    int pad = 5;

    core::graphics_fill_rect(mx, my, mw, mh, core::Color::DarkBlue);
    core::graphics_draw_rect(mx, my, mw, mh, core::Color::White);

    if (g_shape.pts.empty()) return;

    float sx = g_shape.max_x - g_shape.min_x;
    float sy = g_shape.max_y - g_shape.min_y;
    if (sx < 1.0f) sx = 1.0f;
    if (sy < 1.0f) sy = 1.0f;

    // Le tracé se referme maintenant correctement sur lui-même (cf.
    // build_shape), donc plus besoin du hack de correction de ratio qui
    // compensait l'ancienne forme quasi rectiligne : un simple "fit"
    // proportionnel dans la boîte suffit et reste fidèle à la vraie forme.
    float sc = std::min((mw - pad*2) / sx, (mh - pad*2) / sy);

    auto ts = [&](float wx, float wy) {
        float nx = (wx - g_shape.min_x) * sc;
        float ny = (wy - g_shape.min_y) * sc;
        return std::pair<int,int>(
            mx + pad + (int)nx,
            my + pad + (int)ny
        );
    };

    // Ruban de route : passe épaisse (asphalte) + liseré central fin
    // (clair), façon plan de circuit. Les primitives dispo ne permettent pas
    // de tracer un trait épais directement, donc on triche avec un petit
    // "pinceau" en redessinant la ligne sur un carré de décalages.
    auto draw_loop = [&](core::Color col, int thickness) {
        auto prev = ts(g_shape.pts.back().first, g_shape.pts.back().second);
        int half = thickness / 2;
        for (size_t i = 0; i < g_shape.pts.size(); ++i) {
            auto cur = ts(g_shape.pts[i].first, g_shape.pts[i].second);
            for (int ox = -half; ox <= half; ++ox) {
                for (int oy = -half; oy <= half; ++oy) {
                    core::graphics_draw_line(prev.first + ox, prev.second + oy,
                                             cur.first + ox, cur.second + oy, col);
                }
            }
            prev = cur;
        }
    };

    draw_loop(core::Color::Gray, 3);   // route
    draw_loop(core::Color::White, 1);  // liseré central

    // Karts
    for (size_t i = 0; i < karts.size(); ++i) {
        size_t idx = std::min((size_t)karts[i].seg_index, g_shape.pts.size() - 1);
        auto p = ts(g_shape.pts[idx].first, g_shape.pts[idx].second);

        core::graphics_fill_rect(
            p.first - 2,
            p.second - 2,
            4, 4,
            (i == 0 ? core::Color::Yellow : core::Color::Red)
        );
    }
}

static void ground_colors(SkyTheme theme, core::Color& a, core::Color& b) {
    switch (theme) {
        case SkyTheme::Desert:
            a = core::Color::Yellow;    // sable
            b = core::Color::Orange;   // roche
            break;
        case SkyTheme::City:
            a = core::Color::Gray;      // trottoir clair
            b = core::Color::DarkGray;  // trottoir foncé
            break;
        case SkyTheme::Hills:
        default:
            a = core::Color::Green;
            b = core::Color::DarkGreen;
            break;
    }
}

// -----------------------------------------------------------------------------
// Rendu principal OutRun optimisé
// -----------------------------------------------------------------------------
void draw_race(const Track& t,
               const std::vector<KartState>& karts,
               const Camera& cam,
               const std::vector<float>& item_box_cooldowns)
{
    float speed_pct = std::clamp(karts[0].speed / MAX_SPEED, 0.0f, 1.0f);
    const KartState& p = karts[0];
    g_frame_counter++;

    const int seg_count = (int)t.segs.size();
    int draw_dist = std::min(DRAW_DISTANCE, seg_count - 1);

    // 1. Fond
    draw_sky(speed_pct, t.sky_theme);

    // 2. Sol
    core::Color ground_a, ground_b;
    ground_colors(t.sky_theme, ground_a, ground_b);
    core::graphics_fill_rect(0, H/2, W, H/2, ground_a);

    // 3. Préparation OutRun
    int base_idx = p.seg_index;
    const Segment& base_seg = t.segs[base_idx];

    float base_pct = (base_seg.length > 0.f)
                     ? (p.z - base_seg.start_z) / base_seg.length
                     : 0.f;

    float shake = cam.shake * std::sin(p.speed * 20.f) * (0.5f + speed_pct);

    core::graphics_push();
    core::graphics_translate(0, (int)shake);

    // Pré-calcul
    // -------------------------------------------------------------------
    // CURVE_RENDER_SCALE : les valeurs de Segment::curve utilisées dans
    // track_example.h sont petites (0.30 à 0.75) comparées aux moteurs
    // OutRun de référence (où ces valeurs tournent plutôt autour de 2 à 6).
    // Sans amplification, l'accumulation dx_acc += seg.curve produit un
    // décalage visuel minime sur toute la longueur d'un virage : la route
    // semble presque droite à l'écran alors que le kart est bel et bien en
    // train de négocier un virage côté physique (k.x, centrifugalité...).
    // Ce facteur n'affecte QUE le rendu (route + décor, qui partagent cette
    // même accumulation cx_acc/dx_acc) : il ne touche pas Segment::curve
    // lui-même, donc ni la physique (kart_engine.cpp) ni le calibrage de la
    // minimap (qui renormalise de toute façon sur ±2*PI, donc insensible à
    // ce facteur). Valeur de départ raisonnable, à ajuster sur hardware.
    // -------------------------------------------------------------------
    const float CURVE_RENDER_SCALE = 18.0f;

    float cx_acc = 0.f;
    float dx_acc = -(base_seg.curve * CURVE_RENDER_SCALE * base_pct);
    float wz_acc = base_seg.start_z;
    int   idx_acc = base_idx;

    struct SegProj { Proj p1, p2; int idx; float dz1; bool skip; };
    std::vector<SegProj> projs;
    projs.reserve(draw_dist);

    // 4. Accumulation des projections
    for (int n = 0; n < draw_dist; ++n) {
        const Segment& seg = t.segs[idx_acc];

        // BUG CORRIGÉ : wz_acc est une simple somme continue (wz_acc +=
        // seg.length à chaque itération, cf. fin de boucle) qui ne se
        // réinitialise JAMAIS, même quand idx_acc boucle via %seg_count.
        // Elle représente donc déjà, par construction, la vraie distance
        // monde "dépliée" — y compris quand on tourne un tour complet en
        // cours de frame (idx_acc revient à 0 mais wz_acc continue de
        // grandir normalement).
        //
        // L'ancien test "wz_acc < p.z" tentait de détecter ce rebouclage
        // pour ajouter total_length "à la main", mais :
        //   - il était FAUX pour les segments proches du joueur : par
        //     définition base_seg.start_z <= p.z (c'est le segment qui
        //     CONTIENT p.z), donc ce test valait "vrai" dès n=0 à CHAQUE
        //     frame — leur dz1 explosait à ~total_length (un tour entier)
        //     au lieu de ~player_z (quelques dizaines d'unités). Résultat :
        //     la route proche était projetée à l'horizon au lieu du bas de
        //     l'écran ("la route commence trop haut" + bande de fond verte
        //     visible en dessous, rien n'étant dessiné à cet endroit).
        //   - pour le vrai rebouclage (loin devant), wz_acc avait de toute
        //     façon déjà la bonne valeur par simple accumulation continue :
        //     ajouter total_length en plus revenait à compter un tour en
        //     trop.
        // Dans les deux cas, la bonne valeur est simplement wz_acc lui-même,
        // sans aucune correction conditionnelle.
        float seg_wz = wz_acc;

        float dz1 = (seg_wz - p.z) + cam.player_z;
        // SECOND BUG (même famille) : dz1 est la distance jusqu'au DÉBUT du
        // segment. Sur le segment où se trouve le joueur, dès qu'il est
        // parcouru à plus de ~72% (cam.player_z/seg.length), dz1 devient
        // négatif (le "début" du segment est désormais derrière la
        // caméra) — alors que le segment lui-même est toujours bien là,
        // sous et devant le kart. L'ancien test de skip se basait
        // uniquement sur dz1 et sautait TOUT le segment dans ce cas,
        // laissant un trou (bande de fond visible) sous le kart pendant
        // ~28% du parcours de CHAQUE segment. Le bon test est sur dz2 (le
        // bord LOIN, qui lui reste devant la caméra tant qu'on n'a pas
        // atteint le segment suivant) ; dz1 est simplement plafonné à une
        // petite valeur positive pour la projection (ce qui revient à
        // dessiner ce bord tout en bas de l'écran, comme il se doit).
        float dz2 = (seg_wz + seg.length - p.z) + cam.player_z;
        float dz1_for_proj = std::max(dz1, cam.depth * 0.5f);

        float cax1 = (p.x * ROAD_HALF_WIDTH) - cx_acc;
        float cax2 = cax1 - dx_acc;

        cx_acc += dx_acc;
        dx_acc += seg.curve * CURVE_RENDER_SCALE;

        bool skip = (dz2 <= cam.depth * 0.5f);

        Proj pr1{}, pr2{};
        if (!skip) {
            pr1 = project(cam, cax1, cam.height, dz1_for_proj);
            pr2 = project(cam, cax2, cam.height, dz2);
            if (pr2.y > pr1.y) std::swap(pr1, pr2);
        }

        projs.push_back({pr1, pr2, idx_acc, dz1, skip});

        wz_acc += seg.length;
        idx_acc = (idx_acc + 1) % seg_count;
    }

    // 5. Rendu de loin → proche
    std::vector<Billboard> billboards;
    billboards.reserve(32);

    // Bas du dernier segment (le plus proche) effectivement dessiné, et sa
    // couleur d'herbe — utilisés après la boucle pour combler l'écran
    // jusqu'en bas (cf. note plus bas : évite la "bande verte" fixe).
    bool  near_drawn = false;
    int   near_y1 = H;
    core::Color near_grass = ground_a;

    for (int n = draw_dist - 1; n >= 0; --n) {
        auto& sp = projs[n];
        if (sp.skip) continue;

        const Segment& seg = t.segs[sp.idx];
        Proj& pr1 = sp.p1;
        Proj& pr2 = sp.p2;

        int y1 = (int)pr1.y;
        int y2 = (int)pr2.y;

        // IMPORTANT (fix clignotement des décors lointains) :
        // près de l'horizon, y1 et y2 sont très proches et, à cause des
        // arrondis flottant→int, il arrive qu'un même segment bascule d'une
        // frame à l'autre entre y2<y1 (trapèze de sol dessiné) et y2>=y1
        // (dégénéré). Comme TOUT — sol, route, ET décors/portique — était
        // conditionné par ce test avec un `continue` global, un décor
        // lointain apparaissait/disparaissait à chaque bascule, d'où le
        // clignotement "visible / pas visible / visible". Le sol dégénéré
        // n'a de toute façon aucun impact visuel (hauteur ~0 pixel) : on se
        // contente maintenant de ne pas le dessiner dans ce cas précis, SANS
        // sauter le reste du segment (décors, portique, bonus...), qui eux
        // restent valides tant que pr1/pr2 sont valides (segment non "skip").
        bool ground_valid = (y2 < y1);

        if (ground_valid) {
        // Herbe / sol (couleur selon le thème de la piste)
        core::Color grass = ((sp.idx / 3) % 2 == 0) ? ground_a : ground_b;

        core::graphics_fill_rect(0, y2, W, y1 - y2, grass);

        near_drawn = true;
        near_y1 = y1;
        near_grass = grass;

        // Rumble
        core::Color rumble = ((sp.idx / 3) % 2 == 0)
                             ? core::Color::Red
                             : core::Color::White;

        core::graphics_fill_trapezoid(
            pr1.x - pr1.w * 1.12f, pr1.x + pr1.w * 1.12f, y1,
            pr2.x - pr2.w * 1.12f, pr2.x + pr2.w * 1.12f, y2,
            rumble
        );

        // Route
        core::graphics_fill_trapezoid(
            pr1.x - pr1.w, pr1.x + pr1.w, y1,
            pr2.x - pr2.w, pr2.x + pr2.w, y2,
            core::Color::DarkGray
        );

        // Ligne centrale
        if ((sp.idx / 2) % 2 == 0) {
            core::graphics_fill_trapezoid(
                pr1.x - pr1.w * 0.04f, pr1.x + pr1.w * 0.04f, y1,
                pr2.x - pr2.w * 0.04f, pr2.x + pr2.w * 0.04f, y2,
                core::Color::White
            );
        }

        // Tremplin
        if (seg.jump_pad) {
            core::graphics_fill_trapezoid(
                pr1.x - pr1.w * 0.7f, pr1.x - pr1.w * 0.5f, y1,
                pr2.x - pr2.w * 0.7f, pr2.x - pr2.w * 0.5f, y2,
                core::Color::Yellow
            );
            core::graphics_fill_trapezoid(
                pr1.x + pr1.w * 0.5f, pr1.x + pr1.w * 0.7f, y1,
                pr2.x + pr2.w * 0.5f, pr2.x + pr2.w * 0.7f, y2,
                core::Color::Yellow
            );
        }

        // Flaque d'huile : sprite dédié, décalé latéralement selon
        // seg.oil_offset_x (cf. kart_engine.cpp — évitable, pas un mur).
        if (seg.has_oil) {
            float ocx = pr1.x + seg.oil_offset_x * pr1.w;
            draw_road_decal(ocx, pr1.y, pr1.w, 0.7f,
                             decor_oil_slick, 48, 24);
        }

        // Bonus box : icône clignotante au centre de la route (cf.
        // Segment::has_item_box — donne un bonus aléatoire au passage).
        // IMPORTANT : ne pas la dessiner pendant son cooldown (juste
        // collectée par un kart, en attente de réapparition) — sinon elle
        // restait affichée en permanence même "vide", ce qui ne reflétait
        // pas son état réel côté logique de jeu (item_box_cooldowns, déjà
        // suivi dans kart_engine.cpp mais jamais consulté ici jusqu'ici).
        bool box_active =
            seg.has_item_box &&
            sp.idx < (int)item_box_cooldowns.size() &&
            item_box_cooldowns[sp.idx] <= 0.0f;

        if (box_active) {
            static const uint16_t* const kBonusIcons[3] = {
                decor_bonus_boost, decor_bonus_shield, decor_bonus_shock
            };
            const uint16_t* box_data = kBonusIcons[(g_frame_counter / 8) % 3];
            draw_road_decal(pr1.x, pr1.y, pr1.w, 0.35f, box_data, 32, 32);
        }
        } // if (ground_valid)

        // Décors
        // IMPORTANT : le filtre doit porter sur l'index ABSOLU du segment
        // (sp.idx), pas sur son rang relatif au joueur (n). `n` représente
        // la distance en nombre de segments jusqu'au joueur ; à chaque fois
        // que celui-ci franchit un segment, TOUTES les valeurs de n se
        // décalent de 1, ce qui faisait entrer/sortir un même décor du
        // filtre "(n % 4) == 0" de façon quasi aléatoire (le décor
        // apparaissait/disparaissait par sauts de 4 segments d'un coup —
        // c'est ce qui donnait l'impression d'un sprite "2 bandes trop haut"
        // puis "revenant en haut" au lieu d'avancer continûment). En testant
        // sp.idx (fixe pour un segment donné, indépendant de la position du
        // joueur), un décor donné est TOUJOURS dessiné ou JAMAIS, de façon
        // stable, et se rapproche progressivement sans saut.
        if ((sp.idx % 4) == 0 && (seg.decor_left || seg.decor_right)) {
            bool is_gate =
                (seg.decor_left == 10 || seg.decor_left == 11 ||
                 seg.decor_right == 10 || seg.decor_right == 11);

            if (is_gate) {
                // Même principe que DECOR_MAX_W plus bas : sans coupure, le
                // portique continue d'être ajouté à la liste de billboards
                // même une fois le segment très proche de la caméra, et
                // pr1.w/pr1.y grandissent de façon disproportionnée — au lieu
                // de disparaître proprement une fois franchi, il semblait
                // "descendre" indéfiniment à l'écran. Seuil plus large que
                // DECOR_MAX_W (90) car l'arche est censée être vue nettement
                // plus grande/plus proche qu'un simple décor de bord de piste
                // avant de disparaître (on passe littéralement dessous).
                const float GATE_MAX_W = 260.0f;
                if (pr1.w < GATE_MAX_W) {
                    uint8_t gid = seg.decor_left ? seg.decor_left : seg.decor_right;

                    // Le même portique (un seul segment, la piste est une
                    // boucle) sert à la fois de ligne de départ et d'arrivée.
                    // La piste l'encode toujours en "START" (id 10) : on ne
                    // bascule vers "FINISH" (id 11) qu'à l'affichage, pendant
                    // le dernier tour du joueur (t.laps - 1, lap 0-indexé).
                    if (gid == 10 || gid == 11) {
                        gid = (p.lap >= t.laps - 1) ? 11 : 10;
                    }

                    billboards.push_back({
                        sp.dz1,
                        (int)pr1.x,
                        (int)pr1.y,
                        pr1.w,
                        true,
                        gid,
                        0,
                        core::KartSpriteId::Back,
                        0
                    });
                }
            }
            else {
                // Le plafond appliqué précédemment (clamp du décalage
                // latéral) empêchait bien l'explosion de position, mais
                // "figeait" le décor près du bord d'écran pendant plusieurs
                // frames au lieu de le laisser sortir — visuellement, ça
                // donnait l'impression que le sprite REVENAIT vers le centre
                // plutôt que de disparaître normalement.
                //
                // Le bon réglage n'est pas de plafonner la position, mais
                // de cesser purement et simplement d'ajouter le décor une
                // fois que le segment est trop proche de la caméra (pr1.w
                // devient franchement disproportionné). La route elle-même
                // continue d'utiliser pr1.w brut (non affecté), donc le sol
                // reste rempli jusqu'en bas — seul le décor latéral cesse
                // d'être dessiné un peu plus tôt, proprement.
                const float DECOR_MAX_W = 90.0f;

                if (pr1.w < DECOR_MAX_W) {
                    if (seg.decor_left) {
                        billboards.push_back({
                            sp.dz1,
                            (int)(pr1.x - pr1.w * 1.45f),
                            (int)pr1.y,
                            pr1.w,
                            false,
                            seg.decor_left,
                            0,
                            core::KartSpriteId::Back,
                            0
                        });
                    }
                    if (seg.decor_right) {
                        billboards.push_back({
                            sp.dz1,
                            (int)(pr1.x + pr1.w * 1.45f),
                            (int)pr1.y,
                            pr1.w,
                            false,
                            seg.decor_right,
                            0,
                            core::KartSpriteId::Back,
                            0
                        });
                    }
                }
            }
        }

        // IA
        for (size_t k = 1; k < karts.size(); ++k) {
            const KartState& ai = karts[k];
            if (ai.faded) continue; // disparu après l'arrivée, cf. kart_engine.cpp
            if (ai.seg_index != sp.idx) continue;

            float pct = (seg.length > 0.f)
                        ? std::clamp((ai.z - seg.start_z) / seg.length, 0.f, 1.f)
                        : 0.f;

            float ix = pr1.x + (pr2.x - pr1.x) * pct;
            float iy = pr1.y + (pr2.y - pr1.y) * pct;
            float iw = pr1.w + (pr2.w - pr1.w) * pct;

            billboards.push_back({
                sp.dz1 + pct * 0.01f,
                (int)(ix + ai.x * iw),
                (int)iy,
                iw,
                false,
                0,
                (int)k,
                (std::abs(ai.x) > 1.f || ai.spinout_timer > 0.f || ai.shock_slow_timer > 0.f)
                                       ? core::KartSpriteId::Crash :
                (ai.x < -0.15f)        ? core::KartSpriteId::Left  :
                (ai.x >  0.15f)        ? core::KartSpriteId::Right :
                                         core::KartSpriteId::Back,
                (int)std::clamp(iw * 0.25f, 4.f, 96.f),
                ai.color
            });
        }
    }

    // Comble l'éventuel espace résiduel entre le bas du segment le plus
    // proche effectivement dessiné et le bas physique de l'écran (H).
    // Sans ce comblement, ce reliquat reste affiché avec la couleur du tout
    // premier remplissage plein-écran (étape "2. Sol", toujours vert uni,
    // jamais alterné) : comme sa hauteur dépend de la position exacte du
    // joueur dans son segment courant, elle varie en continu (elle rétrécit
    // à mesure qu'on avance dans le segment) puis "réapparaît" plus grande
    // dès qu'on passe au segment suivant — c'est le défaut de bande verte
    // signalé. On la comble ici avec la MÊME couleur alternée (verte/grise)
    // que le dernier segment réellement dessiné, pour un raccord invisible.
    if (near_drawn && near_y1 < H) {
        core::graphics_fill_rect(0, near_y1, W, H - near_y1, near_grass);
    }

    // 6. Billboards triés (stable_sort : deux décors à distance quasi
    // identique gardent un ordre de dessin cohérent d'une frame à l'autre,
    // std::sort ne garantit pas ça et pourrait faire clignoter l'un
    // derrière l'autre par instabilité de tri).
    std::stable_sort(billboards.begin(), billboards.end(),
              [](const Billboard& a, const Billboard& b) {
                  return a.dz > b.dz;
              });

    for (auto& b : billboards) draw_billboard(b, t.sky_theme);

    core::graphics_pop();

    // 7. Speed lines
    if (speed_pct > 0.35f) {
        float intensity = (speed_pct - 0.35f) / 0.65f;
        float len = 14.f + intensity * 50.f;

        for (int i = 0; i < 10; ++i) {
            float a = (float)i / 10 * 2.f * 3.14159265f;
            float ox = W * 0.5f + std::cos(a) * (W * 0.62f);
            float oy = H * 0.5f + std::sin(a) * (H * 0.62f);

            float dx2 = W * 0.5f - ox;
            float dy2 = H * 0.5f - oy;
            float d = std::sqrt(dx2 * dx2 + dy2 * dy2);

            if (d < 1) continue;
            dx2 /= d;
            dy2 /= d;

            core::graphics_draw_line(
                (int)ox, (int)oy,
                (int)(ox + dx2 * len),
                (int)(oy + dy2 * len),
                core::Color::White
            );
        }
    }

    // 8. Kart joueur
    using namespace core;

    if (!cam.cockpit_view) {
        float tilt = p.drift * 12.f;

        KartSpriteId fid =
            (std::abs(p.x) > 1.f || p.spinout_timer > 0.f || p.shock_slow_timer > 0.f)
                                                  ? KartSpriteId::Crash :
            input_is_held(Button::JoystickLeft)  ? KartSpriteId::Left  :
            input_is_held(Button::JoystickRight) ? KartSpriteId::Right :
                                                   KartSpriteId::Back;

        const KartSprites& spr = get_kart_sprites(p.color);

        draw_kart_sprite(
            spr, fid,
            W/2 - KART_SPRITE_W/2 + (int)tilt,
            H - 64 - (int)(p.y * 20.f)
        );
    }
    else {
        graphics_draw_sprite(SpriteId::Dashboard, 0, H - 70);
        graphics_draw_sprite_rotated(SpriteId::Wheel, W/2 - 32, H - 64, p.angle * 40.f);
        graphics_fill_rect(60, H - 20, (int)(p.speed / MAX_SPEED * 200), 8, Color::Yellow);
    }

    // 9. HUD
    char buf[48];

    snprintf(buf, sizeof(buf), "V:%d km/h", (int)(p.speed / MAX_SPEED * 80));
    graphics_draw_text(4, 4, buf, Color::White);

    snprintf(buf, sizeof(buf), "Tour:%d/%d", std::min(p.lap + 1, t.laps), t.laps);
    graphics_draw_text(4, 16, buf, Color::White);

    snprintf(buf, sizeof(buf), "Pos:%d/%d", p.rank, (int)karts.size());
    int tw = (int)strlen(buf) * 8;
    graphics_draw_text(W - tw - 4, 4, buf, Color::Yellow);

    if (p.has_boost)  graphics_draw_text(W - 80, 16, "BOOST!", Color::Orange);
    if (p.has_shield) graphics_draw_text(W - 96, 28, "BOUCLIER", Color::Cyan);
    if (!p.on_ground) graphics_draw_text_center(50, "SAUT !", Color::Cyan);
    if (std::abs(p.x) > 1.f)
        graphics_draw_text_center(H - 90, "HORS PISTE !", Color::Red);

    // Bonus tenu (à utiliser avec L1) : icône + rappel texte de la touche.
    if (p.bonus != BonusType::None) {
        const uint16_t* icon =
            (p.bonus == BonusType::Boost)  ? decor_bonus_boost  :
            (p.bonus == BonusType::Shield) ? decor_bonus_shield :
                                              decor_bonus_shock;
        core::graphics_draw_bitmap565(4, H - 60, 32, 32, icon, true, 0xF81F);
        graphics_draw_text(40, H - 50, "L1", Color::Yellow);
    }

    // Mini-turbo en cours de charge : indique le palier atteint (façon
    // "étincelles" Mario Kart, ici juste du texte coloré par palier).
    if (p.drift_tier_reached > 0) {
        const char* tier_txt =
            (p.drift_tier_reached >= 3) ? "MINI-TURBO x3" :
            (p.drift_tier_reached == 2) ? "MINI-TURBO x2" : "MINI-TURBO x1";
        Color tier_col =
            (p.drift_tier_reached >= 3) ? Color::Red :
            (p.drift_tier_reached == 2) ? Color::Orange : Color::Cyan;
        graphics_draw_text_center(70, tier_txt, tier_col);
    }

    graphics_draw_text(4, H - 18, "Menu:Pause", Color::Gray);

    // Minimap optimisée
    draw_minimap(t, karts);
}

} // namespace kart
