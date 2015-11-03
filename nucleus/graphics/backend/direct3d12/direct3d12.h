/**
 * (c) 2015 Alexandro Sanchez Bach. All rights reserved.
 * Released under GPL v2 license. Read LICENSE for more details.
 */

#pragma once

#include "nucleus/common.h"

#if defined(NUCLEUS_PLATFORM_WINDOWS)
#include <d3d12.h>
#include <dxgi1_4.h>
#endif

// Declare Function
#define DECLARE_FUNCTION(type, function) extern type _##function;
#define FUNCTION DECLARE_FUNCTION
#include "direct3d12.inl"
#undef FUNCTION
#undef DECLARE_FUNCTION

namespace gfx {

// Load extensions
bool initializeDirect3D12();

}  // namespace gfx
