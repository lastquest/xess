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
// Author:  James Stanard 
//

#pragma once

#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable:4238) // nonstandard extension used : class rvalue used as lvalue
#pragma warning(disable:4239) // A non-const reference may only be bound to an lvalue; assignment operator takes a reference to non-const
#pragma warning(disable:4324) // structure was padded due to __declspec(align())

#include <winsdkver.h>
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN
// For constants like CLSID_D3D12DSRDeviceFactory to be initialized with a value,
// to avoid linker error like "LNK2001: unresolved external symbol CLSID_D3D12DSRDeviceFactory".
#define INITGUID

#include <Windows.h>
#include <wrl/client.h>
#include <wrl/event.h>


#ifdef USE_PREVIEW_AGILITY_SDK
#include "Microsoft.Direct3D.D3D12.1.714.0-preview/build/native/include/d3d12.h"
#include "Microsoft.Direct3D.D3D12.1.714.0-preview/build/native/include/directsr.h"
#else
#include <d3d12.h>
#endif

#include <dxgi1_6.h>
#include "d3dx12.h"

#ifdef _DEBUG
    #include <dxgidebug.h>
#endif

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define MY_IID_PPV_ARGS                     IID_PPV_ARGS


#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <queue>
#include <memory>
#include <string>
#include <cwctype>
#include <exception>

#include <ppltasks.h>
#include <functional>

#include "Log.h"
#include "Utility.h"
#include "VectorMath.h"
#include "EngineTuning.h"
#include "EngineProfiling.h"
#include "Util/CommandLineArg.h"
