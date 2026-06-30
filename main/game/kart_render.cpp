/*
===============================================================================
  kart_render.cpp — Rendu pseudo-3D de la course (route, décor, concurrents)
-------------------------------------------------------------------------------
  Portage fidèle de l'algorithme du tutoriel de référence "javascript-racer"
  (Jake Gordon), qui est la base technique standard pour ce type de rendu
  façon OutRun. Points clés (différents de nos tentatives précédentes) :

    - PAS de position "monde" précalculée par segment pour la courbure :
      l'accumulation (x += dx; dx += curve) se fait à CHAQUE FRAME, en
      repartant du segment courant de la caméra, avec une interpolation
      sous-segment (basePercent) pour une transition continue — c'est ce
      qui rend le rendu cohérent quel que soit l'endroit où on se trouve.
    - Le kart du joueur reste à une position FIXE à l'écran : c'est la
      route (x/dx) qui se décale pour donner l'impression du virage.
    - Le déplacement latéral lié au volant (cf. kart_engine.cpp) est
      proportionnel à la vitesse : à l'arrêt, rien ne bouge.
    - Les concurrents/décor réutilisent les positions écran déjà projetées
      du segment (p1/p2), avec une simple interpolation — pas de re-projection
      indépendante (qui désynchronisait facilement les calculs).
===============================================================================
*/

#include "kart_render.h"
#include "../core/graphics.h"
#include "../core/input.h"
#include "../core/kart_loader.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <utility>

namespace kart {

static const int W = 320;
static const int H = 240;

// A garder en sync avec kart_engine.cpp
static const float MAX_SPEED = 1.25f;

// Demi-largeur de route, en unités "monde" (mêmes unités que Segment::length).
// Ratio choisi pour rester cohérent avec le tutoriel de référence
// (roadWidth/segmentLength = 10 chez eux) : avec nos segments de longueur 8,
// cela donne 80. C'est ce qui calibre la largeur de route à l'écran —
// à ajuster ici si besoin, plutôt que dans la formule de projection.
static const float ROAD_HALF_WIDTH = 80.0f;

// Nombre de segments dessinés devant la caméra.
// 150 donne un horizon suffisamment lointain même à pleine vitesse.
static const int DRAW_DISTANCE = 150;

// -----------------------------------------------------------------------------
//  Projection pseudo-3D — fidèle à Util.project() de la référence.
//  cam.depth et cam.player_z sont précalculés une fois (cf. camera_setup,
//  appelé depuis kart_game.cpp) à partir du champ de vision et de la
//  hauteur de caméra : depth = 1/tan((fov/2)*pi/180).
// -----------------------------------------------------------------------------
struct Proj {
    float x, y;     // position écran
    float w;        // demi-largeur de route à l'écran (pixels)
    float scale;    // facteur de perspective brut (réutilisé pour les billboards)
};

static Proj project(const Camera& cam, float camera_x, float camera_height, float camera_z) {
    float scale = cam.depth / camera_z;

    // camera_x  : décalage latéral monde, NÉGATIF à gauche → doit donner
    //             un x écran À GAUCHE du centre (valeur < W/2).
    //             Référence JS : destX = (width/2) + (scale * worldX * width/2)
    //             — mais avec la convention que x monde positif = droite écran.
    //             Notre convention physique est inversée (x>0 = à droite de la
    //             piste, vu par le joueur) donc on SOUSTRAIT pour que l'effet
    //             de virage soit dans le bon sens.
    float cam_rel_y = -camera_height; // route à y=0, caméra en haut → différence négative

    Proj pr{};
    pr.scale = scale;
    pr.x = (W * 0.5f) - (scale * camera_x * W * 0.5f); // signe MOINS = bon sens gauche/droite
    pr.y = (H * 0.5f) - (scale * cam_rel_y * H * 0.5f);
    pr.w = scale * ROAD_HALF_WIDTH * W * 0.5f;
    return pr;
}

// -----------------------------------------------------------------------------
//  Billboards (décor + karts adverses), dessinés du plus loin au plus proche
// -----------------------------------------------------------------------------
enum class BillKind { Tree, Rock, KartAI };

struct Billboard {
    float dz;
    int   screen_x;
    int   screen_y;
    float scale;
    BillKind kind;
    int   variant;
};

static void draw_tree(int x, int base_y, float scale) {
    using namespace core;
    int trunk_w = std::max(2, (int)(4 * scale));
    int trunk_h = std::max(3, (int)(10 * scale));
    int leaf_r  = std::max(3, (int)(12 * scale));

    graphics_fill_rect(x - trunk_w / 2, base_y - trunk_h, trunk_w, trunk_h, Color::Brown);
    graphics_fill_circle(x, base_y - trunk_h - leaf_r / 2, leaf_r, Color::Green);
    graphics_draw_circle(x, base_y - trunk_h - leaf_r / 2, leaf_r, Color::DarkGray);
}

static void draw_rock(int x, int base_y, float scale) {
    using namespace core;
    int w = std::max(4, (int)(14 * scale));
    int h = std::max(3, (int)(9 * scale));
    graphics_fill_rect(x - w / 2, base_y - h, w, h, Color::Gray);
    graphics_draw_rect(x - w / 2, base_y - h, w, h, Color::DarkGray);
}

static void draw_kart_billboard(int x, int base_y, float scale, int variant) {
    using namespace core;
    static const Color kColors[3] = { Color::Yellow, Color::Orange, Color::SkyBlue };
    Color body = kColors[variant % 3];

    int w = std::max(4, (int)(18 * scale));
    int h = std::max(3, (int)(12 * scale));

    graphics_fill_circle(x, base_y, w / 2, Color::DarkGray);
    graphics_fill_rect(x - w / 2, base_y - h, w, h, body);
    graphics_draw_rect(x - w / 2, base_y - h, w, h, Color::Black);
    graphics_fill_circle(x, base_y - h - h / 4, std::max(1, h / 3), Color::White);
}

static void draw_billboard(const Billboard& b) {
    switch (b.kind) {
        case BillKind::Tree:   draw_tree(b.screen_x, b.screen_y, b.scale); break;
        case BillKind::Rock:   draw_rock(b.screen_x, b.screen_y, b.scale); break;
        case BillKind::KartAI: draw_kart_billboard(b.screen_x, b.screen_y, b.scale, b.variant); break;
    }
}

// -----------------------------------------------------------------------------
//  Minimap (vue du dessus, intégration de la courbure — indépendante du
//  rendu 3D, juste pour situer les karts sur le tracé global)
// -----------------------------------------------------------------------------
struct TrackShape {
    std::vector<std::pair<float, float>> points;
    float min_x = 0, max_x = 0, min_y = 0, max_y = 0;
};

static const float MINIMAP_HEADING_SCALE = 0.05f;

static const TrackShape& get_track_shape(const Track& t) {
    static TrackShape shape;
    static bool computed = false;
    if (computed) return shape;
    computed = true;

    float heading = 0.0f, x = 0.0f, y = 0.0f;
    shape.points.reserve(t.segs.size());

    for (const auto& s : t.segs) {
        heading += s.curve * MINIMAP_HEADING_SCALE;
        x += std::cos(heading) * s.length;
        y += std::sin(heading) * s.length;
        shape.points.push_back({x, y});
        shape.min_x = std::min(shape.min_x, x);
        shape.max_x = std::max(shape.max_x, x);
        shape.min_y = std::min(shape.min_y, y);
        shape.max_y = std::max(shape.max_y, y);
    }
    return shape;
}

static void draw_minimap(const Track& t,
                         const std::vector<KartState>& karts)
{
    using namespace core;

    int map_x = W - 80;
    int map_y = 8;
    int map_w = 72;
    int map_h = 72;
    int pad = 6;

    graphics_fill_rect(map_x, map_y, map_w, map_h, Color::DarkBlue);
    graphics_draw_rect(map_x, map_y, map_w, map_h, Color::White);

    const TrackShape& shape = get_track_shape(t);
    if (shape.points.empty()) return;

    float span_x = std::max(1.0f, shape.max_x - shape.min_x);
    float span_y = std::max(1.0f, shape.max_y - shape.min_y);
    float scale = std::min((map_w - pad * 2) / span_x, (map_h - pad * 2) / span_y);

    auto to_screen = [&](float wx, float wy) {
        int sx = map_x + pad + (int)((wx - shape.min_x) * scale);
        int sy = map_y + pad + (int)((wy - shape.min_y) * scale);
        return std::pair<int, int>(sx, sy);
    };

    auto prev = to_screen(shape.points[0].first, shape.points[0].second);
    for (size_t i = 1; i < shape.points.size(); ++i) {
        auto cur = to_screen(shape.points[i].first, shape.points[i].second);
        graphics_draw_line(prev.first, prev.second, cur.first, cur.second, Color::LightGray);
        prev = cur;
    }
    auto first_pt = to_screen(shape.points.front().first, shape.points.front().second);
    graphics_draw_line(prev.first, prev.second, first_pt.first, first_pt.second, Color::LightGray);

    for (size_t i = 0; i < karts.size(); ++i) {
        size_t idx = std::min((size_t)karts[i].seg_index, shape.points.size() - 1);
        auto p = to_screen(shape.points[idx].first, shape.points[idx].second);

        Color c = (i == 0) ? Color::Yellow : Color::Red;
        graphics_fill_rect(p.first - 2, p.second - 2, 4, 4, c);
    }
}

// -----------------------------------------------------------------------------
//  camera_setup() — calcule UNE FOIS cam.depth et cam.player_z à partir du
//  champ de vision et de la hauteur de caméra. À appeler après avoir réglé
//  cam.fov / cam.height (cf. kart_game.cpp), avant le premier rendu.
// -----------------------------------------------------------------------------
void camera_setup(Camera& cam) {
    cam.depth = 1.0f / std::tan((cam.fov * 0.5f) * 3.14159265f / 180.0f);
    cam.player_z = cam.height * cam.depth;
}

// -----------------------------------------------------------------------------
//  draw_race()
// -----------------------------------------------------------------------------
void draw_race(const Track& t,
               const std::vector<KartState>& karts,
               const Camera& cam)
{
    using namespace core;

    graphics_clear(Color::SkyBlue);
    graphics_fill_rect(0, H/2, W, H/2, Color::Green); // sol par défaut sous l'horizon

    const KartState& p = karts[0];
    const int seg_count = (int)t.segs.size();

    int base_idx = p.seg_index;
    const Segment& base_seg = t.segs[base_idx];
    float base_percent = (base_seg.length > 0.0f)
                        ? (p.z - base_seg.start_z) / base_seg.length
                        : 0.0f;

    float shake = cam.shake * std::sin(p.speed * 20.0f);
    graphics_push();
    graphics_translate(0, (int)shake);

    // Accumulation par frame, façon OutRun : x/dx repartent de zéro à
    // CHAQUE image, depuis le segment courant de la caméra — avec une
    // interpolation sous-segment (base_percent) pour que ça reste continu
    // pendant qu'on avance, sans à-coup au changement de segment.
    float x  = 0.0f;
    float dx = -(base_seg.curve * base_percent);

    float world_z = base_seg.start_z; // début du segment courant, en z monde

    std::vector<Billboard> billboards;
    billboards.reserve(32);

    int idx = base_idx;
    Proj p1{}, p2{};

    for (int n = 0; n < DRAW_DISTANCE; ++n) {
        const Segment& seg = t.segs[idx];

        bool looped = world_z < p.z;
        float seg_world_z = world_z + (looped ? t.total_length : 0.0f);

        float dz1 = seg_world_z - p.z;
        float dz2 = seg_world_z + seg.length - p.z;

        float cam_x1 = (p.x * ROAD_HALF_WIDTH) - x;
        float cam_x2 = (p.x * ROAD_HALF_WIDTH) - x - dx;

        x  += dx;
        dx += seg.curve;

        // Segment trop proche (quasi sous la caméra) : on l'ignore plutôt
        // que de risquer une échelle de projection qui explose.
        bool skip = (dz1 <= cam.depth);

        if (!skip) {
            p1 = project(cam, cam_x1, cam.height, dz1);
            p2 = project(cam, cam_x2, cam.height, dz2);

            if (p2.y < p1.y) { // évite tout artefact si jamais l'ordre s'inverse
                int y1 = (int)p1.y, y2 = (int)p2.y;

                Color grass = ((idx / 3) % 2 == 0) ? Color::Green : Color::DarkGray;
                graphics_fill_rect(0, y2, W, (y1 - y2) + 1, grass);

                Color rumble = ((idx / 3) % 2 == 0) ? Color::Red : Color::White;
                graphics_fill_trapezoid(
                    p1.x - p1.w * 1.12f, p1.x + p1.w * 1.12f, y1,
                    p2.x - p2.w * 1.12f, p2.x + p2.w * 1.12f, y2,
                    rumble
                );

                graphics_fill_trapezoid(
                    p1.x - p1.w, p1.x + p1.w, y1,
                    p2.x - p2.w, p2.x + p2.w, y2,
                    Color::DarkGray
                );

                if ((idx / 2) % 2 == 0) {
                    graphics_fill_trapezoid(
                        p1.x - p1.w * 0.04f, p1.x + p1.w * 0.04f, y1,
                        p2.x - p2.w * 0.04f, p2.x + p2.w * 0.04f, y2,
                        Color::White
                    );
                }

                // Décor de bord de piste : utilise uniquement p1 (point fixe
                // du segment), comme les sprites statiques de la référence.
                if ((n % 2) == 0) {
                    float decor_scale = std::clamp(p1.scale * 40.0f, 0.15f, 3.0f);
                    float side_offset = p1.w * 1.3f;

                    if (seg.decor_left != 0) {
                        Billboard b;
                        b.dz = dz1;
                        b.screen_x = (int)(p1.x - side_offset);
                        b.screen_y = (int)p1.y;
                        b.scale = decor_scale;
                        b.kind = (seg.decor_left == 1) ? BillKind::Tree : BillKind::Rock;
                        b.variant = 0;
                        billboards.push_back(b);
                    }
                    if (seg.decor_right != 0) {
                        Billboard b;
                        b.dz = dz1;
                        b.screen_x = (int)(p1.x + side_offset);
                        b.screen_y = (int)p1.y;
                        b.scale = decor_scale;
                        b.kind = (seg.decor_right == 1) ? BillKind::Tree : BillKind::Rock;
                        b.variant = 0;
                        billboards.push_back(b);
                    }
                }

                // Karts adverses situés sur ce segment : interpolés entre
                // p1 et p2 selon leur avancement exact dans le segment —
                // jamais re-projetés indépendamment (cf. en-tête du fichier).
                for (size_t k = 1; k < karts.size(); ++k) {
                    const KartState& ai = karts[k];
                    if (ai.seg_index != idx) continue;

                    float percent = (seg.length > 0.0f)
                                   ? (ai.z - seg.start_z) / seg.length
                                   : 0.0f;
                    percent = std::clamp(percent, 0.0f, 1.0f);

                    float ix = p1.x + (p2.x - p1.x) * percent;
                    float iy = p1.y + (p2.y - p1.y) * percent;
                    float iw = p1.w + (p2.w - p1.w) * percent;
                    float isc = p1.scale + (p2.scale - p1.scale) * percent;

                    Billboard b;
                    b.dz = dz1 + (dz2 - dz1) * percent;
                    b.screen_x = (int)(ix + ai.x * iw);
                    b.screen_y = (int)iy;
                    b.scale = std::clamp(isc * 60.0f, 0.1f, 2.5f);
                    b.kind = BillKind::KartAI;
                    b.variant = (int)k;
                    billboards.push_back(b);
                }
            }
        }

        world_z += seg.length;
        idx = (idx + 1) % seg_count;
    }

    std::sort(billboards.begin(), billboards.end(),
              [](const Billboard& a, const Billboard& b) { return a.dz > b.dz; });

    for (const auto& b : billboards)
        draw_billboard(b);

    graphics_pop();

    // Vue cockpit ou 3e personne — le kart du joueur reste TOUJOURS à une
    // position fixe à l'écran (cf. en-tête du fichier).
    if (!cam.cockpit_view) {
        float tilt = p.drift * 12.0f;

        KartSpriteId frame_id = KartSpriteId::Back;
        if (std::abs(p.x) > 1.0f)
            frame_id = KartSpriteId::Crash;
        else if (input_is_held(Button::JoystickLeft))
            frame_id = KartSpriteId::Left;
        else if (input_is_held(Button::JoystickRight))
            frame_id = KartSpriteId::Right;

        const KartSprites& sprites = get_kart_sprites(KartColor::Red);
        int sx = W/2 - KART_SPRITE_W/2;
        int sy = H - 48 - (int)(p.y * 20.0f);

        draw_kart_sprite(sprites, frame_id, sx + (int)tilt, sy);
    } else {
        graphics_draw_sprite(SpriteId::Dashboard, 0, H - 70);

        int wheel_x = W/2 - 32;
        int wheel_y = H - 64;
        float ang = p.angle * 40.0f;

        graphics_draw_sprite_rotated(SpriteId::Wheel, wheel_x, wheel_y, ang);

        int bar_w = (int)(p.speed / MAX_SPEED * 200);
        graphics_fill_rect(60, H - 20, bar_w, 8, Color::Yellow);
    }

    char buf[32];
    std::snprintf(buf, sizeof(buf), "Vitesse: %d", (int)(p.speed * 100));
    graphics_draw_text(4, 4, buf);

    std::snprintf(buf, sizeof(buf), "Tour: %d", p.lap + 1);
    graphics_draw_text(4, 16, buf);

    std::snprintf(buf, sizeof(buf), "Rang: %d", p.rank);
    graphics_draw_text(4, 28, buf);

    if (p.has_boost)
        graphics_draw_text(W - 80, 4, "BOOST!");

    if (std::abs(p.x) > 1.0f)
        graphics_draw_text_center(H - 90, "HORS PISTE !", Color::Red);

    draw_minimap(t, karts);
}

} // namespace kart
