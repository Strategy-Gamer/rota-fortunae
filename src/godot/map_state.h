#pragma once

#include "../map/geography.h"
#include "../countries/countries.h"
#include "../countries/location_politics.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class MapState : public Node {
    GDCLASS(MapState, Node)

public:
    MapState() = default;
    ~MapState() override = default;

    void build_from_image(const Ref<Image>& image);
    void clear();

    [[nodiscard]]
    bool is_loaded() const;

    [[nodiscard]]
    int get_width() const;

    [[nodiscard]]
    int get_height() const;

    [[nodiscard]]
    int get_location_count() const;

    [[nodiscard]]
    int get_location_id_at_pixel(Vector2i pixel) const;

    [[nodiscard]]
    Color get_location_color(int location_id) const;

    int create_country(
        const godot::String& name,
        const godot::Color& color
    );

    int get_country_count() const;

    godot::String get_country_name(
        int country_id
    ) const;

    godot::Color get_country_color(
        int country_id
    ) const;

    void set_location_owner(
        int location_id,
        int country_id
    );

    int get_location_owner(
        int location_id
    ) const;

    godot::Ref<godot::Image>
    create_map_mode_palette(
        int map_mode
    ) const;

    [[nodiscard]]
    int get_location_area(int location_id) const;

    [[nodiscard]]
    Vector2i get_location_centroid(int location_id) const;

    [[nodiscard]]
    Ref<Image> create_id_image() const;

    [[nodiscard]]
    Ref<Image> create_palette_image() const;

protected:
    static void _bind_methods();

private:
    rota::map::Geography geography_;

    rota::countries::CountryStore countries_;
    rota::countries::LocationPolitics politics_;

    bool validate_location_id(int location_id) const;
    bool validate_country_id(int country_id) const;
};

}