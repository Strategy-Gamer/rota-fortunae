#pragma once

#include "../core/world.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class SimWorld : public Node {
    GDCLASS(SimWorld, Node)

public:
    SimWorld() = default;
    ~SimWorld() override = default;

    void build_from_image(const Ref<Image>& image);
    void clear();

    enum RenderDirty : std::uint32_t { 
        DIRTY_ALL = 1 << 0,
        DIRTY_POLITICAL = 1 << 1
        // DIRTY_POPULATION = 1 << 2... 
    };

    enum CommandType {
        CMD_NONE = 0,
        
        // Time & Debug (1-999)
        // Time Controls (10-19)

        CMD_TOGGLE_CLOCK = 10,
        CMD_STEP_CLOCK = 11,

        CMD_TOGGLE_PAUSE = 12,
        CMD_SET_GAME_SPEED = 13,
        CMD_ADD_DATE = 14,
        CMD_SET_DATE = 15,

        // Locations (50-199)

        CMD_SET_LOCATION_OWNER = 20,

        // Countries (200-299)

        // Civilizations (300-399)

        // In-Game Commands (1_000+)

        

        
    };

    bool execute_command(CommandType type_id, int actor_civ, const Dictionary& payload);

    bool step_tick();
    int get_tick();

    int get_year();
    int get_day();
    int get_tick_accumulation();
    int get_speed();
    bool is_paused();

    bool is_loaded() const;
    int get_width() const;
    int get_height() const;

    int get_location_count() const;
    int get_location_id_at_pixel(Vector2i pixel) const;
    Color get_location_color(int location_id) const;

    int create_country(
        const godot::String& name,
        const godot::Color& color
    );
    int get_country_count() const;

    godot::String get_country_name(int country_id) const;
    godot::Color get_country_color(int country_id) const;

    int get_location_owner(int location_id) const;

    godot::Ref<godot::Image> create_map_mode_palette(int map_mode) const;

    int get_location_area(int location_id) const;
    Vector2i get_location_centroid(int location_id) const;

    Ref<Image> create_id_image() const;
    Ref<Image> create_palette_image() const;

    int take_render_dirty();
    int64_t get_state_hash();

protected:
    static void _bind_methods();

private:
    // Authoritative world state. Aggregates all stores
    // Private so no temptation to reference this outside this class -> preserves separation
    rota::core::World world;

    bool validate_location_id(int location_id) const;
    bool validate_country_id(int country_id) const;
};

}
VARIANT_ENUM_CAST(SimWorld::RenderDirty);
VARIANT_ENUM_CAST(SimWorld::CommandType);
