shader_type canvas_item;


int decode_id(vec4 enc) {
    int r = int(enc.r * 255.0 + 0.5);
    int g = int(enc.g * 255.0 + 0.5);
    int b = int(enc.b * 255.0 + 0.5);
    return r + (g << 8) + (b << 16);
}

vec2 uv_to_px(vec2 uv, vec2 tex_size) {
    return uv * tex_size;
}

vec2 snap_px_center(vec2 px) {
    return floor(px) + vec2(0.5);
}

vec2 px_to_uv(vec2 px, vec2 tex_size) {
    return px / tex_size;
}

int sample_id_px(sampler2D tex, vec2 px_center, vec2 tex_size) {
    vec2 uv = px_to_uv(px_center, tex_size);
    return decode_id(texture(tex, uv));
}

void fragment() {
    vec2 tex_size = vec2(textureSize(TEXTURE, 0));

    // Work in pixel space
    vec2 px = uv_to_px(UV, tex_size);
    vec2 p0 = snap_px_center(px);

    int id      = sample_id_px(TEXTURE, p0, tex_size);
    int id_right = sample_id_px(TEXTURE, p0 + vec2(1.0, 0.0), tex_size);
    int id_down  = sample_id_px(TEXTURE, p0 + vec2(0.0, 1.0), tex_size);

    // Asymmetric 1-texel border: only right/down edges
    bool is_border = (id_right != id) || (id_down != id);

    // Default black + transparent
    vec4 col = vec4(0.0);

    if (is_border) {
		col.a = 1.0; // black border
    }

    COLOR = col;
}
