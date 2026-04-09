#include "render_postfx.hpp"

#include "settings.hpp"
#include "state.hpp"

#include "render_shaders/render_crt_frag.spv.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_render.h>

namespace splonks {

namespace {

struct CrtUniforms {
    float texture_width = 0.0F;
    float texture_height = 0.0F;
    float scan_line_amount = 0.5F;
    float scan_line_edge_start = 0.35F;
    float scan_line_edge_falloff = 0.25F;
    float scan_line_edge_strength = 1.0F;
    float zoom = 1.0F;
    float warp_amount = 0.05F;
    float vignette_amount = 0.5F;
    float vignette_intensity = 0.3F;
    float grille_amount = 0.05F;
    float brightness_boost = 1.2F;
    float padding0 = 0.0F;
    float padding1 = 0.0F;
    float padding2 = 0.0F;
};

bool SetCrtUniforms(
    SDL_GPURenderState* crt_state,
    SDL_Texture* render_texture,
    const PostProcessSettings& settings
) {
    if (crt_state == nullptr || render_texture == nullptr) {
        return false;
    }

    float texture_width = 0.0F;
    float texture_height = 0.0F;
    if (!SDL_GetTextureSize(render_texture, &texture_width, &texture_height)) {
        return false;
    }

    const CrtUniforms uniforms{
        .texture_width = texture_width,
        .texture_height = texture_height,
        .scan_line_amount = settings.crt_scanline_amount,
        .scan_line_edge_start = settings.crt_scanline_edge_start,
        .scan_line_edge_falloff = settings.crt_scanline_edge_falloff,
        .scan_line_edge_strength = settings.crt_scanline_edge_strength,
        .zoom = settings.crt_zoom,
        .warp_amount = settings.crt_warp_amount,
        .vignette_amount = settings.crt_vignette_amount,
        .vignette_intensity = settings.crt_vignette_intensity,
        .grille_amount = settings.crt_grille_amount,
        .brightness_boost = settings.crt_brightness_boost,
    };
    return SDL_SetGPURenderStateFragmentUniforms(crt_state, 0, &uniforms, sizeof(uniforms));
}

} // namespace

RenderPostFx RenderPostFx::New() {
    return RenderPostFx{};
}

void RenderPostFx::Shutdown() {
    if (crt_state != nullptr) {
        SDL_DestroyGPURenderState(crt_state);
        crt_state = nullptr;
    }
    if (gpu_device != nullptr && crt_shader != nullptr) {
        SDL_ReleaseGPUShader(gpu_device, crt_shader);
        crt_shader = nullptr;
    }
    gpu_device = nullptr;
    gpu_renderer_active = false;
}

bool InitRenderPostFx(
    RenderPostFx& post_fx,
    SDL_Renderer* renderer,
    SDL_Texture* render_texture,
    const PostProcessSettings& settings
) {
    post_fx.Shutdown();
    if (renderer == nullptr) {
        return false;
    }

    post_fx.gpu_device = SDL_GetGPURendererDevice(renderer);
    post_fx.gpu_renderer_active = post_fx.gpu_device != nullptr;
    if (!post_fx.gpu_renderer_active) {
        return false;
    }

    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(post_fx.gpu_device);
    if (formats == SDL_GPU_SHADERFORMAT_INVALID) {
        post_fx.Shutdown();
        return false;
    }

    SDL_GPUShaderCreateInfo shader_info{};
    if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) == 0U) {
        post_fx.Shutdown();
        return false;
    }
    shader_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    shader_info.code = render_crt_frag_spv;
    shader_info.code_size = sizeof(render_crt_frag_spv);

    shader_info.num_samplers = 1;
    shader_info.num_uniform_buffers = 1;
    shader_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;

    post_fx.crt_shader = SDL_CreateGPUShader(post_fx.gpu_device, &shader_info);
    if (post_fx.crt_shader == nullptr) {
        post_fx.Shutdown();
        return false;
    }

    SDL_GPURenderStateCreateInfo state_info{};
    state_info.fragment_shader = post_fx.crt_shader;
    post_fx.crt_state = SDL_CreateGPURenderState(renderer, &state_info);
    if (post_fx.crt_state == nullptr) {
        post_fx.Shutdown();
        return false;
    }

    if (!SetCrtUniforms(post_fx.crt_state, render_texture, settings)) {
        post_fx.Shutdown();
        return false;
    }

    return true;
}

void RefreshRenderPostFx(
    RenderPostFx& post_fx,
    SDL_Texture* render_texture,
    const PostProcessSettings& settings
) {
    if (post_fx.crt_state == nullptr) {
        return;
    }

    SetCrtUniforms(post_fx.crt_state, render_texture, settings);
}

SDL_GPURenderState* GetActivePostFxState(const RenderPostFx& post_fx, const State& state) {
    if (!post_fx.gpu_renderer_active) {
        return nullptr;
    }

    switch (state.settings.post_process.effect) {
    case PostProcessEffect::None:
        return nullptr;
    case PostProcessEffect::Crt:
        return post_fx.crt_state;
    }

    return nullptr;
}

} // namespace splonks
