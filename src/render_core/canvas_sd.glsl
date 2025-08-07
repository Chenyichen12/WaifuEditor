layout(binding = 0, std140) uniform UniformBufferObject{
    float region_scale;
    vec2 region_offset;
    vec2 screen_size;
} ubo; // v

layout(binding = 1) uniform sampler2D main_tex; // f

#ifdef VERTEX
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 out_uv;

void main(){
    vec2 pos = in_pos * ubo.region_scale + ubo.region_offset;
    pos.x = pos.x * 2.0 / ubo.screen_size.x - 1.0;
    pos.y = pos.y * 2.0 / ubo.screen_size.y - 1.0;
    gl_Position = vec4(pos.xy, 0.0, 1.0);
    out_uv = in_uv;
}
#endif

#ifdef FRAGMENT
layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color; /*
{
    "format": "srgba32",
}
*/

void main(){
    vec4 result_color = texture(main_tex, in_uv);
    if (result_color.a < 0.01) {
        discard; // 丢弃透明像素
    }
    out_color = result_color;   
}

#endif