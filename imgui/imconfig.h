//-----------------------------------------------------------------------------
// COMPILE-TIME OPTIONS FOR DEAR IMGUI
// Runtime options (clipboard callbacks, enabling various features, etc.) can generally be set via the ImGuiIO structure.
// You can use ImGui::SetAllocatorFunctions() before calling ImGui::CreateContext() to rewire memory allocation functions.
//-----------------------------------------------------------------------------
// A) You may edit imconfig.h (and not overwrite it when updating Dear ImGui, or maintain a patch/rebased branch with your modifications to it)
// B) or '#define IMGUI_USER_CONFIG "my_imgui_config.h"' in your project and then add directives in your own file without touching this template.
//-----------------------------------------------------------------------------
// You need to make sure that configuration settings are defined consistently _everywhere_ Dear ImGui is used, which include the imgui*.cpp
// files but also _any_ of your code that uses Dear ImGui. This is because some compile-time options have an affect on data structures.
// Defining those options in imconfig.h will ensure every compilation unit gets to see the same data structure layouts.
// Call IMGUI_CHECKVERSION() from your .cpp files to verify that the data structures your files are using are matching the ones imgui.cpp is using.
//-----------------------------------------------------------------------------

#pragma once

// Add this to your imconfig.h

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>

#include "imgui-SFML_export.h"

#define IM_VEC2_CLASS_EXTRA           \
    template<typename T>              \
    ImVec2(const sf::Vector2<T>& v) { \
        x = static_cast<float>(v.x);  \
        y = static_cast<float>(v.y);  \
    }                                 \
                                      \
    template<typename T>              \
    operator sf::Vector2<T>() const { \
        return sf::Vector2<T>(x, y);  \
    }

#define IM_VEC4_CLASS_EXTRA                                                                        \
    ImVec4(const sf::Color& c) : x(c.r / 255.f), y(c.g / 255.f), z(c.b / 255.f), w(c.a / 255.f) {} \
    operator sf::Color() const {                                                                   \
        return sf::Color(static_cast<std::uint8_t>(x * 255.f),                                     \
                         static_cast<std::uint8_t>(y * 255.f),                                     \
                         static_cast<std::uint8_t>(z * 255.f),                                     \
                         static_cast<std::uint8_t>(w * 255.f));                                    \
    }
