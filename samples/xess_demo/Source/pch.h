//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//

#pragma once

#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable:4328) // nonstandard extension used : class rvalue used as lvalue
#pragma warning(disable:4324) // structure was padded due to __declspec(align())

#include <winapifamily.h>        // for WINAPI_FAMILY
#include <wrl.h>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
// For constants like CLSID_D3D12DSRDeviceFactory to be initialized with a value,
// to avoid linker error like "LNK2001: unresolved external symbol CLSID_D3D12DSRDeviceFactory".
#define INITGUID

#include <windows.h>

#ifdef USE_PREVIEW_AGILITY_SDK
#include "Microsoft.Direct3D.D3D12.1.714.0-preview/build/native/include/d3d12.h"
#include "Microsoft.Direct3D.D3D12.1.714.0-preview/build/native/include/directsr.h"
#else
#include <d3d12.h>
#endif


#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define MY_IID_PPV_ARGS IID_PPV_ARGS

#if _MSC_VER >= 1800
    #include <d3d11_2.h>
    #include <pix3.h>
#endif

#include "d3dx12.h"

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <queue>
#include <array>
#include <memory>
#include <map>
#include <unordered_map>
#include <string>
#include <exception>

#include <wrl.h>
#include <ppltasks.h>

#include "Utility.h"
#include "VectorMath.h"
#include "EngineTuning.h"
#include "EngineProfiling.h"
