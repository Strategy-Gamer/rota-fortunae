#include "geography.h"

namespace rota::map {

void Geography::clear() {
    width = 0;
    height = 0;

    pixel_location.clear();

    display_color_rgb.clear();
    owner_id.clear();
    area.clear();
    centroid_x.clear();
    centroid_y.clear();
}

}