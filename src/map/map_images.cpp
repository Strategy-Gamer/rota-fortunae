#include "map_images.h"

#include "../utility/packed_color.h"

namespace rota::map {

RGBImageData MapImages::create_id_image(
    const Geography& geography
) {
    RGBImageData result;

    result.width = geography.width;
    result.height = geography.height;
    result.bytes.resize(
        geography.pixel_location.size() * 3
    );

    for (
        std::size_t pixel_index = 0;
        pixel_index < geography.pixel_location.size();
        ++pixel_index
    ) {
        write_rgb24(
            result.bytes,
            pixel_index * 3,
            geography.pixel_location[pixel_index]
        );
    }

    return result;
}

}
