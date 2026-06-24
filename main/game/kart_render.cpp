/*
===============================================================================
  kart_render.cpp — Rendu pseudo-3D de la course (route, décor, concurrents)
-------------------------------------------------------------------------------
  Améliorations apportées :
    - La courbure des segments (Segment::curve) influence enfin l'affichage :
      la route se courbe réellement à l'écran (accumulation classique
      "dx += curve; x += dx" des moteurs pseudo-3D façon OutRun).
    - La caméra suit la position latérale du joueur (cam.offset_x), donc la
      route se décale visuellement quand on dirige le kart.
    - Bandes herbe / rumble strips / route dessinées à chaque tranche, pour
      que la piste soit clairement identifiable.
    - Décor de bord de piste (arbres/rochers, Segment::decor_left/right)
      dessiné en billboards, triés du plus loin au plus proche.
    - Karts adverses dessinés en billboards à leur position 3D réelle.
===============================================================================
*/
#include "kart_render.h"
#include "../core/graphics.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

namespace kart {

static const int W = 320;
static const int H = 240;

// A garder en sync avec kart_engine.cpp
static const float MAX_SPEED = 1.25f;

// Echelle visuelle de la courbure (en pixels par "unité de courbure
// accumulée"). Valeur de départ raisonnable, à ajuster au feeling souhaité
// une fois testé sur la console.
static const float CURVE_VISUAL_SCALE = 200.0f;

struct Proj {
    float y;
    float h;
    float road_w;
    float x;
};

// dz = distance (le long de la piste) entre la caméra et ce point.
// world_curve = décalage latéral "monde" accumulé par la courbure jusqu'ici.
static Proj project(const Camera& cam, float dz, float world_curve) {
    if (dz < 0.1f) dz = 0.1f;

    float p = cam.fov / dz;

    Proj pr{};
    pr.y = H * 0.5f + cam.height - p * 100.0f;
    pr.h = p * 120.0f;
    pr.road_w = p * 200.0f;
    pr.x = W * 0.5f + cam.offset_x * pr.road_w + world_curve * p * CURVE_VISUAL_SCALE;
    return pr;
}

// -----------------------------------------------------------------------------
//  Billboards (décor + karts adverses), dessinés du plus loin au plus proche
// -----------------------------------------------------------------------------
enum class BillKind { Tree, Rock, KartAI };

struct Billboard {
    float dz;        // distance à la caméra (clé de tri : on dessine loin -> proche)
    int   screen_x;
    int   screen_y;  // ligne de sol (pied de l'objet) à l'écran
    float scale;      // échelle visuelle (1.0 = référence)
    BillKind kind;
    int   variant;    // ex: index du kart pour varier la couleur
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

    // Ombre
    graphics_fill_circle(x, base_y, w / 2, Color::DarkGray);
    // Corps
    graphics_fill_rect(x - w / 2, base_y - h, w, h, body);
    graphics_draw_rect(x - w / 2, base_y - h, w, h, Color::Black);
    // "Casque" du pilote
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
//  Minimap (inchangée)
// -----------------------------------------------------------------------------
static void draw_minimap(const Track& t,
                         const std::vector<KartState>& karts)
{
    using namespace core;

    int map_x = W - 80;
    int map_y = 8;
    int map_w = 72;
    int map_h = 72;

    graphics_draw_rect(map_x, map_y, map_w, map_h, Color::White);

    for (size_t i = 0; i < karts.size(); ++i) {
        float ratio = karts[i].z / t.total_length;
        float angle = ratio * 2.0f * 3.14159f;

        float cx = map_x + map_w / 2;
        float cy = map_y + map_h / 2;
        float r  = (map_w / 2) - 4;

        int px = (int)(cx + std::cos(angle) * r);
        int py = (int)(cy + std::sin(angle) * r);

        Color c = (i == 0) ? Color::Yellow : Color::Red;
        graphics_fill_rect(px - 2, py - 2, 4, 4, c);
    }
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

    const KartState& p = karts[0];
    float cam_z = p.z + 5.0f;

    const int VISIBLE_SEGMENTS = 80;

    // Optionnel : petite vibration caméra
    float shake = cam.shake * std::sin(p.speed * 20.0f);
    graphics_push();
    graphics_translate(0, (int)shake);

    float z_acc = 0.0f;
    int idx = p.seg_index;

    float curve_dx = 0.0f;     // dérivée de courbure accumulée
    float world_curve = 0.0f;  // décalage latéral "monde" accumulé

    Proj prev{};
    bool have_prev = false;

    std::vector<Billboard> billboards;
    billboards.reserve(32);

    for (int i = 0; i < VISIBLE_SEGMENTS; ++i) {
        const Segment& s = t.segs[idx];
        z_acc += s.length;

        // Accumulation classique de la courbure (façon OutRun) : chaque
        // segment plus loin dans un virage dévie un peu plus la route.
        curve_dx    += s.curve;
        world_curve += curve_dx;

        float dz = z_acc - cam_z;
        Proj pr = project(cam, dz, world_curve);

        if (have_prev && dz > 0.0f) {
            int y1 = (int)prev.y;
            int y2 = (int)pr.y;

            // --- Herbe (toute la largeur, alternée pour le sens de vitesse) ---
            int gy1 = std::min(y1, y2), gy2 = std::max(y1, y2);
            Color grass = ((idx / 3) % 2 == 0) ? Color::Green : Color::DarkGray;
            graphics_fill_rect(0, gy1, W, (gy2 - gy1) + 1, grass);

            // --- Bas-côtés (rumble strips), légèrement plus larges que la route ---
            Color rumble = ((idx / 3) % 2 == 0) ? Color::Red : Color::White;
            graphics_fill_trapezoid(
                prev.x - prev.road_w * 1.12f, prev.x + prev.road_w * 1.12f, y1,
                pr.x   - pr.road_w   * 1.12f, pr.x   + pr.road_w   * 1.12f, y2,
                rumble
            );

            // --- Route ---
            graphics_fill_trapezoid(
                prev.x - prev.road_w, prev.x + prev.road_w, y1,
                pr.x   - pr.road_w,   pr.x   + pr.road_w,   y2,
                Color::DarkGray
            );

            // --- Ligne centrale en tirets ---
            if ((idx / 2) % 2 == 0) {
                graphics_fill_trapezoid(
                    prev.x - prev.road_w * 0.04f, prev.x + prev.road_w * 0.04f, y1,
                    pr.x   - pr.road_w   * 0.04f, pr.x   + pr.road_w   * 0.04f, y2,
                    Color::White
                );
            }

            // --- Décor de bord de piste (un billboard tous les 2 segments) ---
            if ((i % 2) == 0) {
                float decor_scale = std::clamp(pr.h / 600.0f, 0.15f, 3.0f);
                float side_offset = pr.road_w * 1.3f;

                if (s.decor_left != 0) {
                    Billboard b;
                    b.dz = dz;
                    b.screen_x = (int)(pr.x - side_offset);
                    b.screen_y = (int)pr.y;
                    b.scale = decor_scale;
                    b.kind = (s.decor_left == 1) ? BillKind::Tree : BillKind::Rock;
                    b.variant = 0;
                    billboards.push_back(b);
                }
                if (s.decor_right != 0) {
                    Billboard b;
                    b.dz = dz;
                    b.screen_x = (int)(pr.x + side_offset);
                    b.screen_y = (int)pr.y;
                    b.scale = decor_scale;
                    b.kind = (s.decor_right == 1) ? BillKind::Tree : BillKind::Rock;
                    b.variant = 0;
                    billboards.push_back(b);
                }
            }
        }

        prev = pr;
        have_prev = true;
        idx = (idx + 1) % (int)t.segs.size();
    }

    float visible_depth = z_acc - cam_z;

    // --- Karts adverses : projetés à leur propre position le long de la piste ---
    for (size_t k = 1; k < karts.size(); ++k) {
        const KartState& ai = karts[k];

        float dz_ai = ai.z - p.z;
        if (dz_ai < 0.0f) dz_ai += t.total_length; // tour de boucle
        dz_ai += 5.0f; // même décalage caméra que cam_z

        if (dz_ai < 0.2f || dz_ai > visible_depth) continue; // hors champ de vision

        // Approxime le décalage de courbure à cette profondeur en interpolant
        // sur la proportion de la distance visible déjà parcourue.
        float ratio = (visible_depth > 0.01f) ? (dz_ai / visible_depth) : 0.0f;
        float approx_world_curve = world_curve * ratio;

        Proj pr_ai = project(cam, dz_ai, approx_world_curve);
        float scale = std::clamp(pr_ai.h / 900.0f, 0.1f, 2.5f);

        Billboard b;
        b.dz = dz_ai;
        b.screen_x = (int)(pr_ai.x + ai.x * pr_ai.road_w);
        b.screen_y = (int)pr_ai.y;
        b.scale = scale;
        b.kind = BillKind::KartAI;
        b.variant = (int)k;
        billboards.push_back(b);
    }

    // Tri loin -> proche, pour que les objets proches recouvrent les lointains.
    std::sort(billboards.begin(), billboards.end(),
              [](const Billboard& a, const Billboard& b) { return a.dz > b.dz; });

    for (const auto& b : billboards)
        draw_billboard(b);

    graphics_pop();

    // Vue cockpit ou 3e personne
    if (!cam.cockpit_view) {
        graphics_draw_sprite(SpriteId::KartPlayer,
                             W/2 - 16,
                             H - 48 - (int)(p.y * 20.0f));
    } else {
        graphics_draw_sprite(SpriteId::Dashboard, 0, H - 70);

        int wheel_x = W/2 - 32;
        int wheel_y = H - 64;
        float ang = p.angle * 40.0f;

        graphics_draw_sprite_rotated(SpriteId::Wheel, wheel_x, wheel_y, ang);

        int bar_w = (int)(p.speed / MAX_SPEED * 200);
        graphics_fill_rect(60, H - 20, bar_w, 8, Color::Yellow);
    }

    // HUD
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
