#pragma once

struct SDL_GPUDevice;
struct SDL_GPURenderState;
struct SDL_GPUShader;
struct SDL_Renderer;
struct SDL_Texture;

namespace splonks {

struct Graphics;
struct PostProcessSettings;
struct State;

struct RenderPostFx {
    bool gpu_renderer_active = false;
    SDL_GPUDevice* gpu_device = nullptr;
    SDL_GPUShader* crt_shader = nullptr;
    SDL_GPURenderState* crt_state = nullptr;

    static RenderPostFx New();
    void Shutdown();
};

bool InitRenderPostFx(
    RenderPostFx& post_fx,
    SDL_Renderer* renderer,
    SDL_Texture* render_texture,
    const PostProcessSettings& settings
);
void RefreshRenderPostFx(
    RenderPostFx& post_fx,
    SDL_Texture* render_texture,
    const PostProcessSettings& settings
);
SDL_GPURenderState* GetActivePostFxState(const RenderPostFx& post_fx, const State& state);

} // namespace splonks
