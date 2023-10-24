#ifndef IMGUI_API
#define IMGUI_API
#endif
#ifndef IMGUI_IMPL_API
#define IMGUI_IMPL_API              IMGUI_API
#endif

namespace ImGui
{
    IMGUI_API bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format);
    IMGUI_API bool SliderIntWithSteps(const char* label, int* v, int v_min, int v_max, int v_step, const char* display_format);
}