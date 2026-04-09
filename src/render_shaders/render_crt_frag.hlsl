// This shader is adapted from the Lightweight CRT Effect at:
// https://godotshaders.com/shader/lightweight-crt-effect/

cbuffer Context : register(b0, space3) {
    float2 resolution;
    float scan_line_amount;
    float scan_line_edge_start;
    float scan_line_edge_falloff;
    float scan_line_edge_strength;
    float zoom;
    float warp_amount;
    float vignette_amount;
    float vignette_intensity;
    float grille_amount;
    float brightness_boost;
};

Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

static const float PI = 3.14159265f;

PSOutput main(PSInput input) {
    PSOutput output;

    float2 uv = input.v_uv;
    uv = (uv - 0.5) / max(zoom, 0.001) + 0.5;
    
    float2 delta = uv - 0.5;
    float warp_factor = dot(delta, delta) * warp_amount;
    uv += delta * warp_factor;

    float scanline = sin(uv.y * resolution.y * PI) * 0.5 + 0.5;
    float edge_distance = length(delta) * 2.0;
    float edge_mask = smoothstep(
        scan_line_edge_start,
        min(1.0, scan_line_edge_start + max(scan_line_edge_falloff, 0.0001)),
        edge_distance
    );
    float scanline_mix = scan_line_amount * lerp(1.0, edge_mask, scan_line_edge_strength);
    scanline = lerp(1.0, scanline, scanline_mix * 0.5);
    
    float grille = fmod(uv.x * resolution.x, 3.0) < 1.5 ? 0.95 : 1.05;
    grille = lerp(1.0, grille, grille_amount * 0.5);
    
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        output.o_color = float4(0.0, 0.0, 0.0, 1.0);
        return output;
    }

    float4 color = u_texture.Sample(u_sampler, uv) * input.v_color;
    color.rgb *= scanline * grille;
    
    float2 v_uv = uv * (1.0 - uv.xy);
    float vignette = v_uv.x * v_uv.y * 15.0;
    vignette = lerp(1.0, vignette, vignette_amount * vignette_intensity);

    color.rgb *= vignette * brightness_boost;

    output.o_color = color;
    return output;
}
