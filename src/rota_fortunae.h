#pragma once

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"

using namespace godot;

class MapReader : public RefCounted {
	GDCLASS(MapReader, RefCounted)

protected:
	static void _bind_methods();

public:
	MapReader() = default;
	~MapReader() override = default;

	// void print_type(const Variant &p_variant) const;
	Dictionary analyze_rgb8(const PackedByteArray &p_image_data, int32_t p_width, int32_t p_height);
};
