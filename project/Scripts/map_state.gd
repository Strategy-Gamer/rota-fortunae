extends Node
class_name MapState

# Handles the state of the map, including location data and textures used for rendering
# Does not manage the locations themselves, only stores and provides access to them
# All game-related map data management is done outside this class
#   Think of it as handling only the map geography itself, not how countries/pops interact with it

var width: int
var height: int

var locations: Array[LocationData] = [] # index = location id

var id_texture: ImageTexture
#var height_texture: ImageTexture # Not implemented yet
var palette_texture: ImageTexture

var id_bytes: PackedByteArray

func prepare_new_map_state(
	map_hex_image: Image,
	_id_bytes: PackedByteArray,
	location_count: int
) -> void:
	width = map_hex_image.get_width()
	height = map_hex_image.get_height()
	id_bytes = _id_bytes

	locations.clear()
	locations.resize(location_count)

	id_texture = generate_id_texture()
	generate_locations(map_hex_image)
	palette_texture = create_palette_textures()

func create_location(
	id: int,
	hex_color: Color
) -> LocationData:
	
	var location = LocationData.new(id, hex_color)

	if locations.size() <= id:
		locations.resize(id + 1)

	locations[id] = location

	return location

func get_location_by_id(location_id: int) -> LocationData:
	if location_id < 0 or location_id >= locations.size():
		return null

	return locations[location_id]

func get_location_id_at_px(px: Vector2i) -> int:
	if px.x < 0 or px.y < 0 or px.x >= width or px.y >= height:
		return -1
	var index = (px.y * width + px.x)*3
	var r = id_bytes[index]
	var g = id_bytes[index + 1]
	var b = id_bytes[index + 2]
	var id = int(r) + (int(g) << 8) + (int(b) << 16)
	
	return id

func generate_id_texture() -> ImageTexture:
	var id_img = Image.create_from_data(
		width,
		height,
		false,
		Image.FORMAT_RGB8,
		id_bytes
	)

	return ImageTexture.create_from_image(id_img)

func generate_locations(map_hex_image: Image) -> void:
	# Create & fill out LocationData instances based on unique colors in the id_bytes
	var prov_id_used = {}
	for y in range(height):
		for x in range(width):
			var id = get_location_id_at_px(Vector2i(x,y))

			if not prov_id_used.has(id):
				prov_id_used[id] = true
				create_location(id, map_hex_image.get_pixel(x, y))

func create_palette_textures() -> ImageTexture:
	var palette_image = Image.create(locations.size(), 1, false, Image.FORMAT_RGB8)
	for location in locations:
		palette_image.set_pixel(location.id, 0, location.hex_color)
	return ImageTexture.create_from_image(palette_image)
