#include "rota_fortunae.h"

void MapReader::_bind_methods() {
	godot::ClassDB::bind_method(D_METHOD("analyze_rgb8", "image_data", "width", "height"), &MapReader::analyze_rgb8);
}

Dictionary MapReader::analyze_rgb8(const PackedByteArray &p_image_data, int32_t p_width, int32_t p_height) {
	// Implementation of the analyze_rgb8 method

	return Dictionary();
}
