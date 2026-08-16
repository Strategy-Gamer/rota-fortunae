#include "sim_world.h"

#include "../map/map_builder.h"
#include "../map/map_images.h"
#include "../map/map_modes.h"
#include "../core/command.h"
#include "../core/hash.h"

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

void SimWorld::_bind_methods() {
    // Dirty Types
    BIND_ENUM_CONSTANT(DIRTY_ALL);
    BIND_ENUM_CONSTANT(DIRTY_POLITICAL);
    
    // Command Types
    BIND_ENUM_CONSTANT(CMD_NONE);
    BIND_ENUM_CONSTANT(CMD_STEP_CLOCK);
    BIND_ENUM_CONSTANT(CMD_TOGGLE_PAUSE);
    BIND_ENUM_CONSTANT(CMD_SET_GAME_SPEED);
    BIND_ENUM_CONSTANT(CMD_ADD_DATE);
    BIND_ENUM_CONSTANT(CMD_SET_DATE);
    BIND_ENUM_CONSTANT(CMD_SET_LOCATION_OWNER);

    // Method Binds
    ClassDB::bind_method( D_METHOD("build_from_image", "image"), &SimWorld::build_from_image);

    ClassDB::bind_method(D_METHOD("clear"), &SimWorld::clear);

    ClassDB::bind_method(D_METHOD("execute_command", "type_id", "actor_civ", "payload"), &SimWorld::execute_command);
    
    ClassDB::bind_method(D_METHOD("step_tick"), &SimWorld::step_tick);
    ClassDB::bind_method(D_METHOD("get_tick"), &SimWorld::get_tick);
    
    ClassDB::bind_method(D_METHOD("get_year"), &SimWorld::get_year);
    ClassDB::bind_method(D_METHOD("get_day"), &SimWorld::get_day);
    ClassDB::bind_method(D_METHOD("get_tick_accumulation"), &SimWorld::get_tick_accumulation);
    ClassDB::bind_method(D_METHOD("get_speed"), &SimWorld::get_speed);
    ClassDB::bind_method(D_METHOD("is_paused"), &SimWorld::is_paused);


    ClassDB::bind_method(D_METHOD("is_loaded"), &SimWorld::is_loaded);

    ClassDB::bind_method(D_METHOD("get_width"), &SimWorld::get_width);

    ClassDB::bind_method(D_METHOD("get_height"), &SimWorld::get_height);

    ClassDB::bind_method(D_METHOD("get_location_count"), &SimWorld::get_location_count);

    ClassDB::bind_method(D_METHOD("get_location_id_at_pixel", "pixel"), &SimWorld::get_location_id_at_pixel);

    ClassDB::bind_method(D_METHOD("get_location_color", "location_id"), &SimWorld::get_location_color);

    ClassDB::bind_method(D_METHOD("create_country","name","color"), &SimWorld::create_country);

    ClassDB::bind_method(D_METHOD("get_country_count"), &SimWorld::get_country_count);

    ClassDB::bind_method(D_METHOD("get_country_name","country_id"), &SimWorld::get_country_name);

    ClassDB::bind_method(D_METHOD("get_country_color","country_id"), &SimWorld::get_country_color);

    ClassDB::bind_method(D_METHOD("get_location_owner","location_id"), &SimWorld::get_location_owner);

    ClassDB::bind_method(D_METHOD("create_map_mode_palette","map_mode"), &SimWorld::create_map_mode_palette);

    ClassDB::bind_method(D_METHOD("get_location_area", "location_id"), &SimWorld::get_location_area);

    ClassDB::bind_method(D_METHOD("get_location_centroid", "location_id"), &SimWorld::get_location_centroid);

    ClassDB::bind_method(D_METHOD("create_id_image"), &SimWorld::create_id_image);

    ClassDB::bind_method(D_METHOD("create_palette_image"), &SimWorld::create_palette_image);

    ClassDB::bind_method(D_METHOD("take_render_dirty"), &SimWorld::take_render_dirty);
    ClassDB::bind_method(D_METHOD("get_state_hash"), &SimWorld::get_state_hash);
}



bool SimWorld::execute_command(CommandType type_id, int actor_civ, const Dictionary& payload){
    return rota::core::Command::execute_command(world, actor_civ, type_id, payload);
}

bool SimWorld::step_tick(){
    // Carbon copy of cmd_step_clock

    world.synch_clock.step();
    world.calendar.advance();
    return true;
}

int SimWorld::get_tick(){
    return static_cast<int>(world.synch_clock.get_current_tick());
}


int SimWorld::get_year(){
    return static_cast<int>(world.calendar.get_year());
}
int SimWorld::get_day(){
    return static_cast<int>(world.calendar.get_day_of_year());
}
int SimWorld::get_tick_accumulation(){
    return static_cast<int>(world.calendar.get_tick_accumulation());
}
int SimWorld::get_speed(){
    return static_cast<int>(world.calendar.get_tick_multiplier());
}
bool SimWorld::is_paused(){
    return world.calendar.is_paused();
}

int SimWorld::create_country(
    const String& name,
    const Color& color
) {
    const std::string native_name =
        name.utf8().get_data();

    const rota::countries::CountryID id =
        world.countries.create_country(
            native_name,
            pack_color(color)
        );

    return static_cast<int>(id);
}

int SimWorld::get_country_count() const {
    return static_cast<int>(
        world.countries.count()
    );
}

String SimWorld::get_country_name(
    int country_id
) const {
    if (!validate_country_id(country_id)) {
        return String();
    }

    return String(
        world.countries.names[country_id].c_str()
    );
}

Color SimWorld::get_country_color(
    int country_id
) const {
    if (!validate_country_id(country_id)) {
        return Color();
    }

    return unpack_color(
        world.countries.display_color_rgb[country_id]
    );
}

int SimWorld::get_location_owner(
    int location_id
) const {
    if (!validate_location_id(location_id)) {
        return -1;
    }

    const rota::countries::CountryID owner =
        world.location_politics.get_owner(
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

Ref<Image> SimWorld::create_map_mode_palette(
    int map_mode
) const {
    rota::map::MapMode mode =
        rota::map::MapMode::LocationColor;

    if (map_mode == 1) {
        mode = rota::map::MapMode::Political;
    }

    const rota::map::RGBImageData data =
        rota::map::create_map_mode_palette(
            world.geography,
            world.countries,
            world.location_politics,
            mode
        );

    return image_from_rgb_data(data);
}

bool SimWorld::validate_country_id(
    int country_id
) const {
    if (country_id < 0) {
        return false;
    }

    return world.countries.is_valid(
        static_cast<rota::countries::CountryID>(
            country_id
        )
    );
}

void SimWorld::build_from_image(const Ref<Image>& image) {
    if (image.is_null() || image->is_empty()) {
        UtilityFunctions::push_error(
            "SimWorld received an empty image."
        );
        return;
    }

    if (image->get_format() != Image::FORMAT_RGB8) {
        UtilityFunctions::push_error(
            "SimWorld requires an RGB8 image."
        );
        return;
    }

    const PackedByteArray godot_bytes = image->get_data();

    try {
        rota::map::MapBuilder::build_from_rgb8(
            world.geography,
            static_cast<std::uint32_t>(image->get_width()),
            static_cast<std::uint32_t>(image->get_height()),
            godot_bytes.ptr(),
            static_cast<std::size_t>(godot_bytes.size())
        );
        world.location_politics.initialize(
            world.geography.location_count()
        );
    } catch (const std::exception& exception) {
        UtilityFunctions::push_error(exception.what());
    }
}

void SimWorld::clear() {
    world.clear();
}

bool SimWorld::is_loaded() const {
    return world.geography.width > 0 &&
           world.geography.height > 0 &&
           !world.geography.pixel_location.empty();
}

int SimWorld::get_width() const {
    return static_cast<int>(world.geography.width);
}

int SimWorld::get_height() const {
    return static_cast<int>(world.geography.height);
}

int SimWorld::get_location_count() const {
    return static_cast<int>(world.geography.location_count());
}

int SimWorld::get_location_id_at_pixel(Vector2i pixel) const {
    const rota::map::LocationID id =
        world.geography.location_at(pixel.x, pixel.y);

    if (id == rota::map::INVALID_LOCATION_ID) {
        return -1;
    }

    return static_cast<int>(id);
}

Color SimWorld::get_location_color(int location_id) const {
    if (!validate_location_id(location_id)) {
        return Color();
    }

    const std::uint32_t packed =
        world.geography.display_color_rgb[location_id];

    const float r =
        static_cast<float>(packed & 0xFFU) / 255.0F;

    const float g =
        static_cast<float>((packed >> 8U) & 0xFFU) / 255.0F;

    const float b =
        static_cast<float>((packed >> 16U) & 0xFFU) / 255.0F;

    return Color(r, g, b);
}


int SimWorld::get_location_area(int location_id) const {
    if (!validate_location_id(location_id)) {
        return 0;
    }

    return static_cast<int>(
        world.geography.area[location_id]
    );
}

Vector2i SimWorld::get_location_centroid(
    int location_id
) const {
    if (!validate_location_id(location_id)) {
        return Vector2i();
    }

    return Vector2i(
        world.geography.centroid_x[location_id],
        world.geography.centroid_y[location_id]
    );
}

Ref<Image> SimWorld::create_id_image() const {
    const rota::map::RGBImageData data =
        rota::map::MapImages::create_id_image(world.geography);

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

Ref<Image> SimWorld::create_palette_image() const {
    const rota::map::RGBImageData data =
        rota::map::MapImages::create_palette_image(
            world.geography
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

bool SimWorld::validate_location_id(
    int location_id
) const {
    if (location_id < 0) {
        return false;
    }

    return world.geography.is_valid_location(
        static_cast<rota::map::LocationID>(location_id)
    );
}

int SimWorld::take_render_dirty(){
    int d = world.render_dirty;
    world.render_dirty = 0;
    return d;
}

int64_t SimWorld::get_state_hash(){
    rota::core::Hasher h;

    world.synch_clock.feed_hash(h);
    world.calendar.feed_hash(h);

    for (auto owner : world.location_politics.owner_country) h.feed(owner);

    h.feed(world.countries.count());
    for (auto alive : world.countries.alive)                h.feed(alive);
    for (auto color : world.countries.display_color_rgb)    h.feed(color);

    return static_cast<int64_t>(h.value());
}

}