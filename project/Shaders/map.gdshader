shader_type canvas_item;

uniform sampler2D id_tex;
uniform sampler2D palette_tex;
uniform int selected_id = -1;
uniform int hovered_id = -1;

int decode_id(vec4 enc) {
    int r = int(enc.r * 255.0 + 0.5);
    int g = int(enc.g * 255.0 + 0.5);
    int b = int(enc.b * 255.0 + 0.5);
    return r + (g << 8) + (b << 16);
}

void fragment() {
    ivec2 tex_size_i = textureSize(TEXTURE, 0);
    vec2 tex_size = vec2(tex_size_i);
    vec2 texel = 1.0 / vec2(textureSize(id_tex, 0));
	
    // Convert UV → pixel coords
    vec2 px = UV * tex_size;

    // Snap to texel centers
    px = floor(px) + vec2(0.5);

    // Back to UV
    vec2 uv_center = px * texel;

    // Sample IDs at *exact* texel centers
    int id      = decode_id(texture(TEXTURE, uv_center));

    // Look up color from palette: x = id / width
    int palette_width = textureSize(palette_tex, 0).x;
    float u = (float(id) + 0.5) / float(palette_width);
    vec4 col = texture(palette_tex, vec2(u, 0.5));

    // Simple highlight logic
    if (id == selected_id) {
        col.rgb = mix(col.rgb, vec3(1.0), 0.4);        // brighten
    } else if (id == hovered_id) {
        col.rgb = mix(col.rgb, vec3(1.0, 1.0, 0.7), 0.3); // slight yellow tint
    }

    COLOR = col;
}
