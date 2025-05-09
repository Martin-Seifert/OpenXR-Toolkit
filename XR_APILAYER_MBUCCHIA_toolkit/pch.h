// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

// Standard library.
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdarg>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Windows header files.
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#include <windows.h>
#include <unknwn.h>
#include <wrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wil/registry.h>

using Microsoft::WRL::ComPtr;

// Helpers for ComPtr manipulation.

template <typename T>
inline T* get(const ComPtr<T>& object) {
    return object.Get();
}

template <typename T>
inline T** set(ComPtr<T>& object) {
    return object.ReleaseAndGetAddressOf();
}

template <typename T>
void attach(ComPtr<T>& object, T* value) {
    object.Attach(value);
}

template <typename T>
T* detach(ComPtr<T>& object) {
    return object.Detach();
}

template <typename T>
constexpr inline T alignTo(T value, uint32_t pad) noexcept {
    //assert(pad && !(pad & (pad - 1)));
    return (value + (pad - 1)) & ~static_cast<T>(pad - 1);
}

template <typename T>
constexpr inline T roundUp(T value, uint32_t pad) noexcept {
    return ((value + (pad - 1)) / pad) * pad;
}

template <typename T>
constexpr inline T roundDown(T value, uint32_t pad) noexcept {
    return (value / pad) * pad;
}

// ETL tracing
#include <traceloggingactivity.h>
#include <traceloggingprovider.h>

// Direct3D.
#include <dxgi1_4.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <d3d11on12.h>
#include <d3dcompiler.h>

// OpenXR + Windows-specific definitions.
#define XR_NO_PROTOTYPES
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

// Oculus definitions.
#include <openxr/fb_eye_tracking_social.h>

// OpenXR loader interfaces.
#include <loader_interfaces.h>

// OpenXR utilities.
#include <XrUtility/XrError.h>
#include <XrUtility/XrMath.h>
#define xrStringToPath(...) XR_SUCCESS
#define xrPathToString(...) XR_SUCCESS
#include <XrUtility/XrString.h>
#undef xrStringToPath
#undef xrPathToString

// FW1FontWrapper.
#include <FW1FontWrapper.h>

// FMT formatter.
#include <fmt/format.h>

// Detours
#include <detours.h>
#include "detours_helpers.h"

// NVAPI SDK.
#include <nvapi.h>

// Omnicept SDK.
//#include <omnicept/Glia.h>

// Pimax eye tracker SDK.
#include <aSeeVRClient.h>
#include <aSeeVRTypes.h>
#include <aSeeVRUtility.h>
