#pragma once

#include "../core/world.h"
#include "../queries/country_summary.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

namespace godot {

class SimWorld : public Node {
    GDCLASS(SimWorld, Node)

public:
    SimWorld() = default;
    ~SimWorld() override = default;

    // The data texts are the map's sidecar files (see MapBuilder::parse_location_data); empty means all land
    void build_from_image(const Ref<Image>& image, const String& location_data, const String& strait_data);
    void clear();

    // False if a payload field isn't an int or bool, or the command is illegal
    bool execute_command(int type_id, int actor_civ, const Dictionary& payload);

    bool step_tick();
    bool advance_years(int years);
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

    bool generate_random_scenario(int seed, int country_count);
    int get_country_count() const;
    PackedInt32Array get_country_ids() const;

    godot::String get_country_name(int country_id) const;
    godot::Color get_country_color(int country_id) const;

    int get_location_owner(int location_id) const;

    godot::Ref<godot::Image> create_map_mode_palette(int map_mode) const;

    int get_location_area(int location_id) const;
    Vector2i get_location_centroid(int location_id) const;

    Ref<Image> create_id_image() const;
    Ref<Image> create_palette_image() const;

    Dictionary get_location_summary(int location_id) const;
    Dictionary get_country_summary(int country_id) const;
    // Every live country's summary, keyed by ID, from one pass over the locations
    Dictionary get_country_summaries() const;

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
    Dictionary country_summary_to_dictionary(int country_id, const rota::queries::CountrySummary& s) const;
};

}
