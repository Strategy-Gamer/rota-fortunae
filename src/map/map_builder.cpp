#include "map_builder.h"

#include "../utility/FixedMath.h"
#include "../utility/packed_color.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace rota::map {

namespace {

// "#rrggbb" -> packed like display_color_rgb
std::uint32_t parse_color(const std::string& text) {
    if (text.size() != 7 || text[0] != '#') {
        throw std::invalid_argument("Bad location color: " + text);
    }
    const unsigned long hex = std::stoul(text.substr(1), nullptr, 16);
    return pack_rgb(
        static_cast<std::uint8_t>(hex >> 16U),
        static_cast<std::uint8_t>(hex >> 8U),
        static_cast<std::uint8_t>(hex)
    );
}

// Rows after the header, each split on ';'
std::vector<std::vector<std::string>> parse_rows(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream lines(text);
    std::string line;
    bool header = true;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (header || line.empty()) {
            header = false;
            continue;
        }
        std::vector<std::string> fields;
        std::istringstream cells(line);
        std::string field;
        while (std::getline(cells, field, ';')) fields.push_back(field);
        rows.push_back(fields);
    }
    return rows;
}

}

LocationData MapBuilder::parse_location_data(
    const std::string& locations_text,
    const std::string& straits_text
) {
    LocationData data;

    static const std::pair<const char*, Terrain> terrain_names[] = {
        { "grasslands", Terrain::Grasslands }, { "farmlands", Terrain::Farmlands },
        { "steppe", Terrain::Steppe }, { "savannah", Terrain::Savannah },
        { "drylands", Terrain::Drylands }, { "coastline", Terrain::Coastline },
        { "coastal_desert", Terrain::CoastalDesert }, { "desert", Terrain::Desert },
        { "woods", Terrain::Woods }, { "forest", Terrain::Forest },
        { "jungle", Terrain::Jungle }, { "marsh", Terrain::Marsh },
        { "hills", Terrain::Hills }, { "highlands", Terrain::Highlands },
        { "mountain", Terrain::Mountain }, { "glacier", Terrain::Glacier },
    };
    static_assert(sizeof(terrain_names) / sizeof(terrain_names[0]) == static_cast<std::size_t>(Terrain::Count) - 1, "name every terrain");

    for (const auto& row : parse_rows(locations_text)) {
        if (row.size() != 6) {
            throw std::invalid_argument("Location rows need color;name;kind;terrain;metres_per_pixel_x;metres_per_pixel_y");
        }
        LocationData::Location location{ parse_color(row[0]), LocationKind::Land, Terrain::None, 0, 0 };
        if (row[2] == "sea") location.kind = LocationKind::Sea;
        else if (row[2] == "impassable") location.kind = LocationKind::Impassable;
        else if (row[2] != "land") throw std::invalid_argument("Unknown location kind: " + row[2]);

        if (location.kind == LocationKind::Land) {
            for (const auto& [name, terrain] : terrain_names) {
                if (row[3] == name) location.terrain = terrain;
            }
            if (location.terrain == Terrain::None) throw std::invalid_argument("Unknown terrain: " + row[3]);
        }

        location.metres_per_pixel_x = static_cast<std::uint32_t>(std::stoul(row[4]));
        location.metres_per_pixel_y = static_cast<std::uint32_t>(std::stoul(row[5]));
        data.locations.push_back(location);
    }

    for (const auto& row : parse_rows(straits_text)) {
        if (row.size() < 2) {
            throw std::invalid_argument("Strait rows need from;to");
        }
        data.straits.emplace_back(parse_color(row[0]), parse_color(row[1]));
    }

    return data;
}

void MapBuilder::build_from_rgb8(
    Geography& geography,
    std::uint32_t width,
    std::uint32_t height,
    const std::uint8_t* rgb_bytes,
    std::size_t rgb_byte_count,
    const LocationData& data
) {
    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * height;

    const std::size_t expected_byte_count = pixel_count * 3;

    if (rgb_bytes == nullptr) {
        throw std::invalid_argument("RGB byte pointer is null.");
    }

    if (rgb_byte_count != expected_byte_count) {
        throw std::invalid_argument(
            "RGB byte count does not match map dimensions."
        );
    }

    geography.clear();

    geography.width = width;
    geography.height = height;
    geography.pixel_location.resize(pixel_count);

    // Maps the original RGB color to the LocationID assigned to it.
    std::unordered_map<std::uint32_t, LocationID> color_to_location;

    std::vector<std::uint64_t> centroid_sum_x;
    std::vector<std::uint64_t> centroid_sum_y;

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t pixel_index =
                static_cast<std::size_t>(y) * width + x;

            const std::size_t byte_index = pixel_index * 3;

            const std::uint8_t r = rgb_bytes[byte_index];
            const std::uint8_t g = rgb_bytes[byte_index + 1];
            const std::uint8_t b = rgb_bytes[byte_index + 2];

            const std::uint32_t packed_color = pack_rgb(r, g, b);

            LocationID location_id;

            const auto existing = color_to_location.find(packed_color);

            if (existing == color_to_location.end()) {
                location_id = geography.location_count();

                color_to_location.emplace(packed_color, location_id);

                geography.display_color_rgb.push_back(packed_color);
                geography.area.push_back(0);
                geography.centroid_x.push_back(0);
                geography.centroid_y.push_back(0);

                centroid_sum_x.push_back(0);
                centroid_sum_y.push_back(0);
            } else {
                location_id = existing->second;
            }

            geography.pixel_location[pixel_index] = location_id;

            geography.area[location_id] += 1;
            centroid_sum_x[location_id] += x;
            centroid_sum_y[location_id] += y;
        }
    }

    finalize_centroids(
        geography,
        centroid_sum_x,
        centroid_sum_y
    );

    const auto location_of = [&](std::uint32_t color) {
        const auto found = color_to_location.find(color);
        if (found == color_to_location.end()) {
            throw std::invalid_argument("Location data names a color that isn't on the map.");
        }
        return found->second;
    };

    const std::uint32_t count = geography.location_count();
    geography.kind.assign(count, LocationKind::Land);
    geography.terrain.assign(count, Terrain::Grasslands);
    geography.metres_per_pixel_x.assign(count, 1000);
    geography.metres_per_pixel_y.assign(count, 1000);
    for (const LocationData::Location& location : data.locations) {
        const LocationID id = location_of(location.color);
        geography.kind[id] = location.kind;
        geography.terrain[id] = location.terrain;
        geography.metres_per_pixel_x[id] = location.metres_per_pixel_x;
        geography.metres_per_pixel_y[id] = location.metres_per_pixel_y;
    }

    std::vector<std::pair<LocationID, LocationID>> straits;
    for (const auto& [from, to] : data.straits) {
        straits.emplace_back(location_of(from), location_of(to));
    }
    build_adjacency(geography, straits);
}

void MapBuilder::finalize_centroids(
    Geography& geography,
    const std::vector<std::uint64_t>& centroid_sum_x,
    const std::vector<std::uint64_t>& centroid_sum_y
) {
    const std::uint32_t count = geography.location_count();

    for (LocationID id = 0; id < count; ++id) {
        const std::uint32_t location_area = geography.area[id];

        if (location_area == 0) {
            continue;
        }

        geography.centroid_x[id] =
            static_cast<std::int32_t>(
                centroid_sum_x[id] / location_area
            );

        geography.centroid_y[id] =
            static_cast<std::int32_t>(
                centroid_sum_y[id] / location_area
            );
    }
}

void MapBuilder::build_adjacency(
    Geography& geography,
    const std::vector<std::pair<LocationID, LocationID>>& straits
) {
    // Both directions of every edge, as (from << 32 | to), so sorting gives each row in order
    std::vector<std::uint64_t> edges;
    const auto add_edge = [&](LocationID a, LocationID b) {
        const std::uint64_t forward = (static_cast<std::uint64_t>(a) << 32U) | b;
        if (a == b || (!edges.empty() && edges[edges.size() - 2] == forward)) return; // runs along one border repeat a pair
        edges.push_back(forward);
        edges.push_back((static_cast<std::uint64_t>(b) << 32U) | a);
    };

    const std::uint32_t width = geography.width;
    const std::uint32_t height = geography.height;
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * width;
        for (std::uint32_t x = 0; x < width; ++x) {
            const LocationID here = geography.pixel_location[row + x];
            add_edge(here, geography.pixel_location[row + (x + 1) % width]); // the world wraps east-west
            if (y + 1 < height) {
                add_edge(here, geography.pixel_location[row + width + x]);
            }
        }
    }
    for (const auto& [a, b] : straits) {
        add_edge(a, b);
    }

    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

    const std::uint32_t count = geography.location_count();
    geography.neighbor_offset.assign(count + 1, 0);
    geography.neighbors.resize(edges.size());
    for (std::size_t i = 0; i < edges.size(); ++i) {
        geography.neighbor_offset[(edges[i] >> 32U) + 1] += 1;
        geography.neighbors[i] = static_cast<LocationID>(edges[i] & 0xFFFFFFFFU);
    }
    for (std::uint32_t id = 0; id < count; ++id) {
        geography.neighbor_offset[id + 1] += geography.neighbor_offset[id];
    }

    geography.neighbor_distance.resize(edges.size());
    for (LocationID id = 0; id < count; ++id) {
        for (std::uint32_t i = geography.neighbor_offset[id]; i < geography.neighbor_offset[id + 1]; ++i) {
            const LocationID other = geography.neighbors[i];
            std::int64_t dx = std::abs(static_cast<std::int64_t>(geography.centroid_x[id]) - geography.centroid_x[other]);
            dx = std::min<std::int64_t>(dx, width - dx); // across the east-west seam
            const std::int64_t dy = static_cast<std::int64_t>(geography.centroid_y[id]) - geography.centroid_y[other];
            // each end's scale covers its half of the edge
            const std::int64_t mx = dx * (geography.metres_per_pixel_x[id] + geography.metres_per_pixel_x[other]) / 2;
            const std::int64_t my = dy * (geography.metres_per_pixel_y[id] + geography.metres_per_pixel_y[other]) / 2;
            geography.neighbor_distance[i] = static_cast<std::uint32_t>(
                (rota::fmath::isqrt(static_cast<std::uint64_t>(mx * mx + my * my)) + 500) / 1000); // integer, since distances feed the sim
        }
    }
}

}
