#include "map_state.h"

#include "../map/map_builder.h"
#include "../map/map_images.h"
#include "../map/map_modes.h"

#include <algorithm>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>


namespace {

std::uint32_t pack_color(
    const godot::Color& color
) {
    const std::uint32_t r =
        static_cast<std::uint32_t>(
            color.r * 255.0F
        );

    const std::uint32_t g =
        static_cast<std::uint32_t>(
            color.g * 255.0F
        );

    const std::uint32_t b =
        static_cast<std::uint32_t>(
            color.b * 255.0F
        );

    return r |
           (g << 8U) |
           (b << 16U);
}

godot::Color unpack_color(
    std::uint32_t packed
) {
    const float r =
        static_cast<float>(
            packed & 0xFFU
        ) / 255.0F;

    const float g =
        static_cast<float>(
            (packed >> 8U) & 0xFFU
        ) / 255.0F;

    const float b =
        static_cast<float>(
            (packed >> 16U) & 0xFFU
        ) / 255.0F;

    return godot::Color(r, g, b);
}

godot::Ref<godot::Image> image_from_rgb_data(
    const rota::map::RGBImageData& data
) {
    godot::PackedByteArray bytes;

    bytes.resize(
        static_cast<std::int64_t>(
            data.bytes.size()
        )
    );

    std::copy(
        data.bytes.begin(),
        data.bytes.end(),
        bytes.ptrw()
    );

    return godot::Image::create_from_data(
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        false,
        godot::Image::FORMAT_RGB8,
        bytes
    );
}
}

namespace godot {

void MapState::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("build_from_image", "image"),
        &MapState::build_from_image
    );

    ClassDB::bind_method(
        D_METHOD("clear"),
        &MapState::clear
    );

    ClassDB::bind_method(
        D_METHOD("is_loaded"),
        &MapState::is_loaded
    );

    ClassDB::bind_method(   
        D_METHOD("get_width"),
        &MapState::get_width
    );

    ClassDB::bind_method(
        D_METHOD("get_height"),
        &MapState::get_height
    );

    ClassDB::bind_method(
        D_METHOD("get_location_count"),
        &MapState::get_location_count
    );

    ClassDB::bind_method(
        D_METHOD("get_location_id_at_pixel", "pixel"),
        &MapState::get_location_id_at_pixel
    );

    ClassDB::bind_method(
        D_METHOD("get_location_color", "location_id"),
        &MapState::get_location_color
    );

    ClassDB::bind_method(
        D_METHOD(
            "create_country",
            "name",
            "color"
        ),
        &MapState::create_country
    );

    ClassDB::bind_method(
        D_METHOD("get_country_count"),
        &MapState::get_country_count
    );

    ClassDB::bind_method(
        D_METHOD(
            "get_country_name",
            "country_id"
        ),
        &MapState::get_country_name
    );

    ClassDB::bind_method(
        D_METHOD(
            "get_country_color",
            "country_id"
        ),
        &MapState::get_country_color
    );

    ClassDB::bind_method(
        D_METHOD(
            "set_location_owner",
            "location_id",
            "country_id"
        ),
        &MapState::set_location_owner
    );

    ClassDB::bind_method(
        D_METHOD(
            "get_location_owner",
            "location_id"
        ),
        &MapState::get_location_owner
    );

    ClassDB::bind_method(
        D_METHOD(
            "create_map_mode_palette",
            "map_mode"
        ),
        &MapState::create_map_mode_palette
    );

    ClassDB::bind_method(
        D_METHOD("get_location_area", "location_id"),
        &MapState::get_location_area
    );

    ClassDB::bind_method(
        D_METHOD("get_location_centroid", "location_id"),
        &MapState::get_location_centroid
    );

    ClassDB::bind_method(
        D_METHOD("create_id_image"),
        &MapState::create_id_image
    );

    ClassDB::bind_method(
        D_METHOD("create_palette_image"),
        &MapState::create_palette_image
    );
}

int MapState::create_country(
    const String& name,
    const Color& color
) {
    const std::string native_name =
        name.utf8().get_data();

    const rota::countries::CountryID id =
        countries_.create_country(
            native_name,
            pack_color(color)
        );

    return static_cast<int>(id);
}

int MapState::get_country_count() const {
    return static_cast<int>(
        countries_.count()
    );
}

String MapState::get_country_name(
    int country_id
) const {
    if (!validate_country_id(country_id)) {
        return String();
    }

    return String(
        countries_.names[country_id].c_str()
    );
}

Color MapState::get_country_color(
    int country_id
) const {
    if (!validate_country_id(country_id)) {
        return Color();
    }

    return unpack_color(
        countries_.display_color_rgb[country_id]
    );
}

void MapState::set_location_owner(
    int location_id,
    int country_id
) {
    if (!validate_location_id(location_id)) {
        return;
    }

    if (country_id < 0) {
        politics_.set_owner(
            static_cast<rota::map::LocationID>(
                location_id
            ),
            rota::countries::INVALID_COUNTRY_ID
        );

        return;
    }

    if (!validate_country_id(country_id)) {
        return;
    }

    politics_.set_owner(
        static_cast<rota::map::LocationID>(
            location_id
        ),
        static_cast<rota::countries::CountryID>(
            country_id
        )
    );
}

int MapState::get_location_owner(
    int location_id
) const {
    if (!validate_location_id(location_id)) {
        return -1;
    }

    const rota::countries::CountryID owner =
        politics_.get_owner(
            static_cast<rota::map::LocationID>(
                location_id
            )
        );

    if (
        owner ==
        rota::countries::INVALID_COUNTRY_ID
    ) {
        return -1;
    }

    return static_cast<int>(owner);
}

Ref<Image> MapState::create_map_mode_palette(
    int map_mode
) const {
    rota::map::MapMode mode =
        rota::map::MapMode::LocationColor;

    if (map_mode == 1) {
        mode = rota::map::MapMode::Political;
    }

    const rota::map::RGBImageData data =
        rota::map::create_map_mode_palette(
            geography_,
            countries_,
            politics_,
            mode
        );

    return image_from_rgb_data(data);
}

bool MapState::validate_country_id(
    int country_id
) const {
    if (country_id < 0) {
        return false;
    }

    return countries_.is_valid(
        static_cast<rota::countries::CountryID>(
            country_id
        )
    );
}

void MapState::build_from_image(const Ref<Image>& image) {
    if (image.is_null() || image->is_empty()) {
        UtilityFunctions::push_error(
            "MapState received an empty image."
        );
        return;
    }

    if (image->get_format() != Image::FORMAT_RGB8) {
        UtilityFunctions::push_error(
            "MapState requires an RGB8 image."
        );
        return;
    }

    const PackedByteArray godot_bytes = image->get_data();

    try {
        rota::map::MapBuilder::build_from_rgb8(
            geography_,
            static_cast<std::uint32_t>(image->get_width()),
            static_cast<std::uint32_t>(image->get_height()),
            godot_bytes.ptr(),
            static_cast<std::size_t>(godot_bytes.size())
        );
        politics_.initialize(
            geography_.location_count()
        );
    } catch (const std::exception& exception) {
        UtilityFunctions::push_error(exception.what());
    }
}

void MapState::clear() {
    geography_.clear();
    countries_.clear();
    politics_.clear();
}

bool MapState::is_loaded() const {
    return geography_.width > 0 &&
           geography_.height > 0 &&
           !geography_.pixel_location.empty();
}

int MapState::get_width() const {
    return static_cast<int>(geography_.width);
}

int MapState::get_height() const {
    return static_cast<int>(geography_.height);
}

int MapState::get_location_count() const {
    return static_cast<int>(geography_.location_count());
}

int MapState::get_location_id_at_pixel(Vector2i pixel) const {
    const rota::map::LocationID id =
        geography_.location_at(pixel.x, pixel.y);

    if (id == rota::map::INVALID_LOCATION_ID) {
        return -1;
    }

    return static_cast<int>(id);
}

Color MapState::get_location_color(int location_id) const {
    if (!validate_location_id(location_id)) {
        return Color();
    }

    const std::uint32_t packed =
        geography_.display_color_rgb[location_id];

    const float r =
        static_cast<float>(packed & 0xFFU) / 255.0F;

    const float g =
        static_cast<float>((packed >> 8U) & 0xFFU) / 255.0F;

    const float b =
        static_cast<float>((packed >> 16U) & 0xFFU) / 255.0F;

    return Color(r, g, b);
}


int MapState::get_location_area(int location_id) const {
    if (!validate_location_id(location_id)) {
        return 0;
    }

    return static_cast<int>(
        geography_.area[location_id]
    );
}

Vector2i MapState::get_location_centroid(
    int location_id
) const {
    if (!validate_location_id(location_id)) {
        return Vector2i();
    }

    return Vector2i(
        geography_.centroid_x[location_id],
        geography_.centroid_y[location_id]
    );
}

Ref<Image> MapState::create_id_image() const {
    const rota::map::RGBImageData data =
        rota::map::MapImages::create_id_image(geography_);

    PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(data.bytes.size()));

    std::copy(
        data.bytes.begin(),
        data.bytes.end(),
        bytes.ptrw()
    );

    return Image::create_from_data(
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        false,
        Image::FORMAT_RGB8,
        bytes
    );
}

Ref<Image> MapState::create_palette_image() const {
    const rota::map::RGBImageData data =
        rota::map::MapImages::create_palette_image(
            geography_
        );

    PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(data.bytes.size()));

    std::copy(
        data.bytes.begin(),
        data.bytes.end(),
        bytes.ptrw()
    );

    return Image::create_from_data(
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        false,
        Image::FORMAT_RGB8,
        bytes
    );
}

bool MapState::validate_location_id(
    int location_id
) const {
    if (location_id < 0) {
        return false;
    }

    return geography_.is_valid_location(
        static_cast<rota::map::LocationID>(location_id)
    );
}

}