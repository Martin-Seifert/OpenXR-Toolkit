// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
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

#include "interfaces.h"

namespace toolkit {

    class OpenXrApi;

    namespace utilities {

        std::optional<int> RegGetDword(HKEY hKey, const std::wstring& subKey, const std::wstring& value);
        void RegSetDword(HKEY hKey, const std::wstring& subKey, const std::wstring& value, DWORD dwordValue);
        void
        RegSetString(HKEY hKey, const std::wstring& subKey, const std::wstring& value, const std::string& stringValue);
        void RegDeleteValue(HKEY hKey, const std::wstring& subKey, const std::wstring& value);
        void RegDeleteKey(HKEY hKey, const std::wstring& subKey);

        std::shared_ptr<ICpuTimer> CreateCpuTimer();

        uint32_t GetScaledInputSize(uint32_t outputSize, int scalePercent, uint32_t blockSize);

        bool UpdateKeyState(bool& keyState, const std::vector<int>& vkModifiers, int vkKey, bool isRepeat);

        void ToggleWindowsMixedRealityReprojection(config::MotionReprojection enable);
        void UpdateWindowsMixedRealityReprojectionRate(config::MotionReprojectionRate rate);

        void EnableHighPrecisionTimer();
        void RestoreTimerPrecision();

        bool IsServiceRunning(const std::string& name);

        void GetVRAMUsage(ComPtr<IDXGIAdapter> adapter, uint64_t& usage, uint8_t& percentUsed);

        bool GetProjectedGaze(const XrView* eyeInViewSpace, const XrVector3f& gazeDirection, XrVector2f* gazePosition);

    } // namespace utilities

    namespace config {

        std::shared_ptr<IConfigManager> CreateConfigManager(const std::string& appName);

        std::pair<uint32_t, uint32_t> GetScaledDimensions(
            int settingScaling, int settingAnamophic, uint32_t outputWidth, uint32_t outputHeight, uint32_t blockSize);
        std::pair<float, float> GetScalingFactors(int settingScaling, int settingAnamophic);

    } // namespace config

    namespace graphics {

        void HookForD3D11DebugLayer();
        void UnhookForD3D11DebugLayer();
        std::shared_ptr<IDevice> WrapD3D11Device(ID3D11Device* device,
                                                 std::shared_ptr<config::IConfigManager> configManager,
                                                 bool enableOculusQuirk = false);
        std::shared_ptr<IDevice> WrapD3D11TextDevice(ID3D11Device* device,
                                                     std::shared_ptr<config::IConfigManager> configManager);
        std::shared_ptr<ITexture> WrapD3D11Texture(std::shared_ptr<IDevice> device,
                                                   const XrSwapchainCreateInfo& info,
                                                   ID3D11Texture2D* texture,
                                                   std::string_view debugName);

        void EnableD3D12DebugLayer();
        std::shared_ptr<IDevice> WrapD3D12Device(ID3D12Device* device,
                                                 ID3D12CommandQueue* queue,
                                                 std::shared_ptr<config::IConfigManager> configManager,
                                                 bool enableVarjoQuirk = false);
        std::shared_ptr<ITexture> WrapD3D12Texture(std::shared_ptr<IDevice> device,
                                                   const XrSwapchainCreateInfo& info,
                                                   ID3D12Resource* texture,
                                                   D3D12_RESOURCE_STATES initialState,
                                                   std::string_view debugName);

        std::shared_ptr<IImageProcessor> CreateImageProcessor(
            std::shared_ptr<toolkit::config::IConfigManager> configManager, std::shared_ptr<IDevice> graphicsDevice);

        std::shared_ptr<IFrameAnalyzer>
        CreateFrameAnalyzer(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                            std::shared_ptr<IDevice> graphicsDevice,
                            uint32_t displayWidth,
                            uint32_t displayHeight,
                            FrameAnalyzerHeuristic heuristic = FrameAnalyzerHeuristic::Unknown);

        std::shared_ptr<IImageProcessor>
        CreateNISUpscaler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                          std::shared_ptr<IDevice> graphicsDevice,
                          int settingScaling,
                          int settingAnamorphic);

        std::shared_ptr<IImageProcessor>
        CreateFSRUpscaler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                          std::shared_ptr<IDevice> graphicsDevice,
                          int settingScaling,
                          int settingAnamorphic);

        std::shared_ptr<IImageProcessor>
        CreateCASUpscaler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                          std::shared_ptr<IDevice> graphicsDevice,
                          int settingScaling,
                          int settingAnamorphic);

        std::shared_ptr<IVariableRateShader>
        CreateVariableRateShader(toolkit::OpenXrApi& openXR,
                                 std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                 std::shared_ptr<IDevice> graphicsDevice,
                                 std::shared_ptr<input::IEyeTracker> eyeTracker,
                                 uint32_t renderWidth,
                                 uint32_t renderHeight,
                                 uint32_t displayWidth,
                                 uint32_t displayHeight,
                                 bool hasVisibilityMask,
                                 bool needMirroredPattern);

        bool IsDeviceSupportingFP16(std::shared_ptr<IDevice> device);

        GpuArchitecture GetGpuArchitecture(UINT VendorId);
        GpuArchitecture GetGpuArchitecture(std::shared_ptr<IDevice> device);

    } // namespace graphics

    namespace input {

        std::shared_ptr<input::IHandTracker>
        CreateHandTracker(toolkit::OpenXrApi& openXR, std::shared_ptr<toolkit::config::IConfigManager> configManager);

        std::shared_ptr<input::IEyeTracker>
        CreateEyeTracker(toolkit::OpenXrApi& openXR, std::shared_ptr<toolkit::config::IConfigManager> configManager);
        std::shared_ptr<input::IEyeTracker>
        CreateEyeTrackerFB(toolkit::OpenXrApi& openXR, std::shared_ptr<toolkit::config::IConfigManager> configManager);
        //std::shared_ptr<input::IEyeTracker>
        //CreateOmniceptEyeTracker(toolkit::OpenXrApi& openXR,
        //                         std::shared_ptr<toolkit::config::IConfigManager> configManager,
        //                         std::unique_ptr<HP::Omnicept::Client> omniceptClient);
        std::shared_ptr<input::IEyeTracker> CreatePimaxEyeTracker(
            toolkit::OpenXrApi& openXR, std::shared_ptr<toolkit::config::IConfigManager> configManager);

    } // namespace input

    namespace menu {

        struct MenuInfo {
            uint32_t displayWidth;
            uint32_t displayHeight;
            std::vector<int> keyModifiers;
            bool isHandTrackingSupported;
            bool isPredictionDampeningSupported;
            uint32_t maxDisplayHeight;
            float resolutionHeightRatio;
            bool isMotionReprojectionRateSupported;
            uint8_t displayRefreshRate;
            uint8_t variableRateShaderMaxRate;
            bool isEyeTrackingSupported;
            bool isEyeTrackingProjectionDistanceSupported;
            bool isVisibilityMaskSupported;
            bool isVisibilityMaskOverrideSupported;
            bool isCACorrectionNeed;
            std::string runtimeName;
        };

        std::shared_ptr<IMenuHandler> CreateMenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                        std::shared_ptr<toolkit::graphics::IDevice> device,
                                                        const MenuInfo& menuInfo);
    } // namespace menu

} // namespace toolkit
