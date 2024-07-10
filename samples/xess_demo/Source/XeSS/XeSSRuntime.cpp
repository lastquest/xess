/*******************************************************************************
 * Copyright 2021 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "pch.h"

#include "XeSSRuntime.h"
#include "XeSSJitter.h"
#include "Utility.h"
#include "GraphicsCore.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "CommandContext.h"
#include "Log.h"
#include "Display.h"

#include "xess/xess_d3d12_debug.h"

using namespace Graphics;

namespace XeSS
{
    /// Convert from xess_result_t to string.
    const char* ResultToString(xess_result_t result)
    {
        switch (result)
        {
        case XESS_RESULT_WARNING_NONEXISTING_FOLDER: return "Warning Nonexistent Folder";
        case XESS_RESULT_WARNING_OLD_DRIVER: return "Warning Old Driver";
        case XESS_RESULT_SUCCESS: return "Success";
        case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE: return "Unsupported Device";
        case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER: return "Unsupported Driver";
        case XESS_RESULT_ERROR_UNINITIALIZED: return "Uninitialized";
        case XESS_RESULT_ERROR_INVALID_ARGUMENT: return "Invalid Argument";
        case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return "Device Out of Memory";
        case XESS_RESULT_ERROR_DEVICE: return "Device Error";
        case XESS_RESULT_ERROR_NOT_IMPLEMENTED: return "Not Implemented";
        case XESS_RESULT_ERROR_INVALID_CONTEXT: return "Invalid Context";
        case XESS_RESULT_ERROR_OPERATION_IN_PROGRESS: return "Operation in Progress";
        case XESS_RESULT_ERROR_UNSUPPORTED: return "Unsupported";
        case XESS_RESULT_ERROR_CANT_LOAD_LIBRARY: return "Cannot Load Library";
        case XESS_RESULT_ERROR_UNKNOWN:
        default: return "Unknown";
        }
    }

    /// Convert application level Quality to
    static _xess_quality_settings_t ToXeSSQuality(eQualityLevel Quality)
    {
        switch (Quality)
        {
            case kQualityUltraPerformance: return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
            case kQualityPerformance: return XESS_QUALITY_SETTING_PERFORMANCE;
            case kQualityBalanced: return XESS_QUALITY_SETTING_BALANCED;
            case kQualityQuality: return XESS_QUALITY_SETTING_QUALITY;
            case kQualityUltraQuality: return XESS_QUALITY_SETTING_ULTRA_QUALITY;
            case kQualityAA: return XESS_QUALITY_SETTING_AA;
            case kQualityUltraQualityPlus: return XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS;
            default: return XESS_QUALITY_SETTING_PERFORMANCE;
        }
    }

    void LogCallback(const char* Message, xess_logging_level_t Level)
    {
        Log::WriteFormat(static_cast<Log::LogLevel>(Level), "[XeSS Runtime]: %s", Message);
    }

    XeSSRuntime::XeSSRuntime()
        : m_Initialized(false)
        , m_PipelineBuilt(false)
        , m_PipelineBuiltFlag(0)
        , m_Context(nullptr)
        , m_PipelineLibBuilt(false)
        , m_PerfGraphData({ 0.0f })
        , m_PerfAccumTime(0.0f)
    {
    }

    bool XeSSRuntime::CreateContext()
    {
        ASSERT(g_Device);
        if (!g_Device)
            return false;

        uint32_t console = 0;
        CommandLineArgs::GetInteger(L"console", console);

        // Create an IDSRDevice to allow us to enumerate available SR variants.
        ID3D12DSRDeviceFactory* dsrDeviceFactory;
        D3D12GetInterface(CLSID_D3D12DSRDeviceFactory, IID_PPV_ARGS(&dsrDeviceFactory));
        dsrDeviceFactory->CreateDSRDevice(g_Device, 1, IID_PPV_ARGS(&g_dsrDevice));

        // Enumerate all super resolution variants available on the device.
        m_DirectSVariantCount = g_dsrDevice->GetNumSuperResVariants();
        DSR_SUPERRES_VARIANT_DESC variantDesc;
        UINT textpos = 0;
        for (UINT currentVariantIndex = 0; currentVariantIndex < m_DirectSVariantCount; currentVariantIndex++)
        {
            g_dsrDevice->GetSuperResVariantDesc(currentVariantIndex, &variantDesc);
            // Use the variant desc as we see fit...
            memcpy(m_DirectSRVariantNames + textpos, variantDesc.VariantName, strlen(variantDesc.VariantName));
            textpos += strlen(variantDesc.VariantName);
            m_DirectSRVariantNames[textpos++] = 0;
            m_DirectSRVariantIDs[currentVariantIndex] = variantDesc.VariantId;
        }


        // Get version of XeSS
        xess_version_t ver;
        xess_result_t ret = xessGetVersion(&ver);
        ASSERT(ret == XESS_RESULT_SUCCESS);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not get version information. Result - %s.", ResultToString(ret));
            return false;
        }

        char buf[128];
        sprintf_s(buf, "%u.%u.%u", ver.major, ver.minor, ver.patch);

        m_VersionStr = buf;

        LOG_INFOF("XeSS: Version - %s.", buf);

        // Create Context of XeSS
        ret = xessD3D12CreateContext(g_Device, &m_Context);
        ASSERT(ret == XESS_RESULT_SUCCESS && m_Context);
        if (ret != XESS_RESULT_SUCCESS || !m_Context)
        {
            m_Context = nullptr;

            LOG_ERRORF("XeSS: XeSS is not supported on this device. Result - %s.", ResultToString(ret));
            return false;
        }

        if (XESS_RESULT_WARNING_OLD_DRIVER == xessIsOptimalDriver(m_Context))
        {
            if (console)
            {
                LOG_ERRORF("Important notice: Please install the latest graphics driver from your vendor for optimal Intel(R) XeSS performance and visual quality.");
            }
            else
            {
                MessageBox(NULL, L"Please install the latest graphics driver from your vendor for optimal Intel(R) XeSS performance and visual quality", L"Important notice", MB_OK | MB_TOPMOST);
            }
        }

        // Set logging callback here.
        ret = xessSetLoggingCallback(m_Context, XESS_LOGGING_LEVEL_DEBUG, LogCallback);
        ASSERT(ret == XESS_RESULT_SUCCESS);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not set logging callback. Result - %s.", ResultToString(ret));
            return false;
        }

        return true;
    }

    bool XeSSRuntime::InitializePipeline(uint32_t InitFlag, bool Blocking)
    {
        // Create the Pipeline Library.
        ComPtr<ID3D12Device1> device1;
        if (FAILED(g_Device->QueryInterface(IID_PPV_ARGS(&device1))))
        {
            LOG_ERROR("XeSS: Get ID3D12Device1 failed.");
            return false;
        }

        HRESULT hr = device1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_PipelineLibrary));
        if (FAILED(hr) || !m_PipelineLibrary)
        {
            LOG_ERROR("XeSS: Create D3D Pipeline library failed.");
            return false;
        }

#ifndef RELEASE
        m_PipelineLibrary->SetName(L"XeSS Pipeline Library Object");
#endif
        xess_result_t ret = xessD3D12BuildPipelines(m_Context, m_PipelineLibrary.Get(), Blocking, InitFlag);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not build XeSS pipelines. Result - %s.", ResultToString(ret));
            return false;
        }

        LOG_INFO("XeSS: Built XeSS pipelines.");

        m_PipelineBuilt = true;
        m_PipelineBuiltBlocking = Blocking;
        m_PipelineBuiltFlag = InitFlag;

        return true;
    }

    void XeSSRuntime::Shutdown(void)
    {
        if (!m_Initialized)
            return;

        // Destroy XeSS Context
        xessDestroyContext(m_Context);

        m_Context = nullptr;

        m_PipelineLibrary = nullptr;

        m_PipelineLibBuilt = false;

        m_Initialized = false;
    }

    bool XeSSRuntime::IsInitialzed() const
    {
        return m_Initialized;
    }

    bool XeSSRuntime::Initialize(const InitArguments& Args)
    {
        m_Initialized = false;

        if (!m_Context)
            return false;

        g_CommandManager.IdleGPU();

        m_InitArguments = Args;

        if (g_dsrDevice)
        {
            // Create a DirectSR engine for the desired variant.
            DSR_SUPERRES_CREATE_ENGINE_PARAMETERS createParams;
            createParams.VariantId = m_DirectSRVariantIDs[m_DirectSVariantIndex];
            createParams.TargetFormat = DXGI_FORMAT_R11G11B10_FLOAT;
            createParams.SourceColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
            createParams.SourceDepthFormat = DXGI_FORMAT_R16_UNORM;
            createParams.ExposureScaleFormat = DXGI_FORMAT_UNKNOWN;
            createParams.Flags = DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_DRS;
            createParams.TargetSize = DSR_SIZE{ Args.OutputWidth, Args.OutputHeight };
            createParams.MaxSourceSize = DSR_SIZE{ Args.OutputWidth, Args.OutputHeight };

            IDSRSuperResEngine* dsrEngine;
            g_dsrDevice->CreateSuperResEngine(&createParams, IID_PPV_ARGS(&dsrEngine));

            // Create super resolution upscaler object to use each frame.
            dsrEngine->CreateUpscaler(g_CommandManager.GetCommandQueue(), IID_PPV_ARGS(&g_dsrUpscaler));
        }
        xess_d3d12_init_params_t params {};
        params.outputResolution.x = Args.OutputWidth;
        params.outputResolution.y = Args.OutputHeight;
        params.qualitySetting = ToXeSSQuality(Args.Quality);
        params.initFlags = Args.UseHiResMotionVectors ? XESS_INIT_FLAG_HIGH_RES_MV : 0;
        if (Args.UseJitteredMotionVectors)
        {
            params.initFlags |= XESS_INIT_FLAG_JITTERED_MV;
        }
        if (Args.UseMotionVectorsInNDC)
        {
            params.initFlags |= XESS_INIT_FLAG_USE_NDC_VELOCITY;
        }
        if (Args.UseExposureTexture)
        {
            params.initFlags |= XESS_INIT_FLAG_EXPOSURE_SCALE_TEXTURE;
        }
        if (Args.UseResponsiveMask)
        {
            params.initFlags |= XESS_INIT_FLAG_RESPONSIVE_PIXEL_MASK;
        }
        if (Args.UseAutoExposure)
        {
            params.initFlags |= XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE;
        }
        if (Args.EnableProfiling)
        {
            params.initFlags |= XESS_D3D12_DEBUG_ENABLE_PROFILING;
        }

        params.pPipelineLibrary = m_PipelineBuilt ? m_PipelineLibrary.Get() : nullptr;

        xess_result_t ret = xessD3D12Init(m_Context, &params);
        ASSERT(ret == XESS_RESULT_SUCCESS);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not initialize. Result - %s.", ResultToString(ret));
            return false;
        }

        LOG_INFO("XeSS: Initialized.");

        m_Initialized = true;

        return true;
    }

    void XeSSRuntime::Execute(ExecuteArguments& ExeArgs)
    {
        if (!m_Initialized)
            return;

        ASSERT(ExeArgs.ColorTexture && ExeArgs.VelocityTexture && ExeArgs.OutputTexture
            && (m_InitArguments.UseHiResMotionVectors || ExeArgs.DepthTexture)
            && (!m_InitArguments.UseExposureTexture || ExeArgs.ExposureTexture)
            && (!m_InitArguments.UseResponsiveMask || ExeArgs.ResponsiveMask));

        if (!ExeArgs.ColorTexture || !ExeArgs.VelocityTexture || !ExeArgs.OutputTexture
            || (!m_InitArguments.UseHiResMotionVectors && !ExeArgs.DepthTexture)
            || (m_InitArguments.UseExposureTexture && !ExeArgs.ExposureTexture)
            || (m_InitArguments.UseResponsiveMask && !ExeArgs.ResponsiveMask))
            return;

        if (m_InitArguments.EnableProfiling)
        {
            m_PerfAccumTime += Graphics::GetFrameTime();
        }

        float jitterX, jitterY;
        XeSSJitter::GetJitterValues(jitterX, jitterY);

        if (g_dsrUpscaler)
        {
            DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS params;
            params.pTargetTexture = ExeArgs.OutputTexture->GetResource();
            params.TargetRegion = D3D12_RECT{ 0, 0, (LONG)ExeArgs.OutputTexture->GetWidth(), (LONG)ExeArgs.OutputTexture->GetHeight() };
            params.pSourceColorTexture = ExeArgs.ColorTexture->GetResource();
            params.SourceColorRegion = D3D12_RECT{ 0, 0, (LONG)ExeArgs.ColorTexture->GetWidth(), (LONG)ExeArgs.ColorTexture->GetHeight() };
            
            if (m_InitArguments.UseHiResMotionVectors)
            {
                params.pSourceDepthTexture = nullptr;
            }
            else
            {
                params.pSourceDepthTexture = ExeArgs.DepthTexture->GetResource();
                params.SourceDepthRegion = D3D12_RECT{ 0, 0, (LONG)ExeArgs.DepthTexture->GetWidth(), (LONG)ExeArgs.DepthTexture->GetHeight() };
            }

            params.pMotionVectorsTexture = ExeArgs.VelocityTexture->GetResource();
            params.MotionVectorsRegion = D3D12_RECT{ 0, 0, (LONG)ExeArgs.VelocityTexture->GetWidth(), (LONG)ExeArgs.VelocityTexture->GetHeight() };
            params.MotionVectorScale = DSR_FLOAT2{ 1.f, 1.f };
            params.CameraJitter = DSR_FLOAT2{ jitterX, jitterY };
            params.ExposureScale = 1.f;
            params.PreExposure = 1.f;
            params.Sharpness = 1.f;
            params.CameraNear = 0.f;
            params.CameraFar = INFINITY;
            params.CameraFovAngleVert = 1.f;
            params.pExposureScaleTexture = nullptr;
            params.pIgnoreHistoryMaskTexture = nullptr;
            params.IgnoreHistoryMaskRegion = D3D12_RECT{ 0, 0, (LONG)ExeArgs.ColorTexture->GetWidth(), (LONG)ExeArgs.ColorTexture->GetHeight() };
            params.pReactiveMaskTexture = nullptr;
            params.ReactiveMaskRegion = D3D12_RECT{ 0, 0,(LONG)ExeArgs.ColorTexture->GetWidth(), (LONG)ExeArgs.ColorTexture->GetHeight() };

            DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS executeFlags =
                ExeArgs.ResetHistory ? DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_RESET_HISTORY
                : DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_NONE;
            
            float frameDeltaInSeconds = 0.016f;
            
            HRESULT exe_hr = g_dsrUpscaler->Execute(&params, frameDeltaInSeconds, executeFlags);

            ASSERT(exe_hr == S_OK);

            return;
        }

        xess_d3d12_execute_params_t params {};
        params.jitterOffsetX = jitterX;
        params.jitterOffsetY = jitterY;
        params.inputWidth = ExeArgs.InputWidth;
        params.inputHeight = ExeArgs.InputHeight;
        params.resetHistory = ExeArgs.ResetHistory ? 1 : 0;
        params.exposureScale = 1.0f;

        params.pColorTexture = ExeArgs.ColorTexture->GetResource();
        params.pVelocityTexture = ExeArgs.VelocityTexture->GetResource();
        params.pOutputTexture = ExeArgs.OutputTexture->GetResource();
        params.pDepthTexture = m_InitArguments.UseHiResMotionVectors ? nullptr : ExeArgs.DepthTexture->GetResource();
        params.pExposureScaleTexture = !m_InitArguments.UseExposureTexture ? nullptr : ExeArgs.ExposureTexture->GetResource();
        params.pResponsivePixelMaskTexture = !m_InitArguments.UseResponsiveMask ? nullptr : ExeArgs.ResponsiveMask->GetResource();

#if _XESS_DEBUG_JITTER_
        LOG_DEBUGF("XeSS Jitter: Input = %f, %f.", params.jitterOffsetX, params.jitterOffsetY);
#endif

        xess_result_t ret = xessD3D12Execute(m_Context, ExeArgs.CommandList, &params);

        // Trigger error report once.
        if (ret != XESS_RESULT_SUCCESS)
        {
            static bool s_Reported = false;
            if (!s_Reported)
            {
                s_Reported = true;
                ASSERT(ret != XESS_RESULT_SUCCESS);
                LOG_ERRORF("XeSS: Failed to execute. Result - %s.", ResultToString(ret));
            }
        }
    }

    bool XeSSRuntime::GetInputResolution(uint32_t& Width, uint32_t& Height)
    {
        ASSERT(m_Context);
        if (!m_Context)
            return false;

        xess_2d_t inputRes = { 1, 1 };
        xess_2d_t outputRes = { m_InitArguments.OutputWidth, m_InitArguments.OutputHeight };

        xess_result_t ret = xessGetInputResolution(m_Context, &outputRes, ToXeSSQuality(m_InitArguments.Quality), &inputRes);
        ASSERT(ret == XESS_RESULT_SUCCESS);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not get input resolution. Result - %s.", ResultToString(ret));
            return false;
        }

        Width = inputRes.x;
        Height = inputRes.y;

        return true;
    }

    bool XeSSRuntime::SetJitterScale(float X, float Y)
    {
        xess_result_t ret = xessSetJitterScale(m_Context, X, Y);
        ASSERT(ret == XESS_RESULT_SUCCESS);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not set jitter scale. Result - %s.", ResultToString(ret));
            return false;
        }

        return true;
    }

    bool XeSSRuntime::SetVelocityScale(float X, float Y)
    {
        xess_result_t ret = xessSetVelocityScale(m_Context, X, Y);
        ASSERT(ret == XESS_RESULT_SUCCESS);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not set velocity scale. Result - %s.", ResultToString(ret));
            return false;
        }

        return true;
    }

    bool XeSSRuntime::SetExposureScale(float scale)
    {
        xess_result_t ret = xessSetExposureMultiplier(m_Context, scale);
        ASSERT(ret == XESS_RESULT_SUCCESS);
        if (ret != XESS_RESULT_SUCCESS)
        {
            LOG_ERRORF("XeSS: Could not set exposure scale. Result - %s.", ResultToString(ret));
            return false;
        }

        return true;
    }

    xess_context_handle_t XeSSRuntime::GetContext()
    {
        return m_Context;
    }

    const std::string& XeSSRuntime::GetVersionString()
    {
        return m_VersionStr;
    }

    void XeSSRuntime::SetPerfGraphRecord(const std::string& RecordName)
    {
        if (m_PerfGraphRecordName == RecordName)
            return;

        m_PerfGraphRecordName = RecordName;

        std::fill(m_PerfGraphData.begin(), m_PerfGraphData.end(), 0.0f);
    }

    void XeSSRuntime::DoGPUProfile()
    {
        if (!m_Initialized)
            return;

        auto ProcessProfilingData = [&](xess_profiling_data_t* Data)
        {
            for (uint64_t fi = 0; fi < Data->frame_count; fi++) // for each frame
            {
                auto& prof_frame = Data->frames[fi];
                auto global_frame_index = prof_frame.frame_index;

                for (uint64_t ri = 0; ri < prof_frame.gpu_duration_record_count; ri++) // for each record
                {
                    auto it = m_PerfData_.find(prof_frame.gpu_duration_names[ri]);
                    if (it == m_PerfData_.end()) // new record
                    {
                        PerfRecord entry = { 0.0, 0.0, 0.0, 0.0, 0.0, 0 };
                        it = m_PerfData_.emplace(prof_frame.gpu_duration_names[ri], entry).first;
                    }

                    // Override the time record.
                    it->second.Time = prof_frame.gpu_duration_values[ri];
                    // Mark as dirty.
                    it->second.Dirty = true;
                }
            }
        };

        xess_profiling_data_t* profilingData = nullptr;
        do
        {
            auto ret = xessD3D12GetProfilingData(m_Context, &profilingData);
            ASSERT(ret == XESS_RESULT_SUCCESS);
            if (ret != XESS_RESULT_SUCCESS || profilingData == nullptr)
            {
                LOG_ERRORF("XeSS: Could not get profiling data. Result - %s.", ResultToString(ret));
                return;
            }
            ProcessProfilingData(profilingData);
        } while (profilingData->any_profiling_data_in_flight);

        // Process the raw perf data.
        ProcessPerfData();
    }

    void XeSSRuntime::ProcessPerfData()
    {
        constexpr auto REFRESH_INTERVAL = 1.0f; // seconds.
        bool reset = (m_PerfAccumTime > REFRESH_INTERVAL);

        if (reset)
        {
            // Update display data with accumulated data.
            m_DisplayPerfData_ = m_PerfData_;
            m_PerfAccumTime = 0.0f;
        }

        for (auto& data : m_PerfData_)
        {
            // Process perf data.
            auto& name = data.first;
            auto& record = data.second;

            // Append new record to perf graph data if any.
            if (m_PerfGraphRecordName.length() && (m_PerfGraphRecordName == name) && record.Dirty)
            {
                for (size_t i = 0; i < m_PerfGraphData.size() - 1; ++i)
                {
                    m_PerfGraphData[i] = m_PerfGraphData[i + 1];
                }

                // We use microseconds.
                m_PerfGraphData[m_PerfGraphData.size() - 1] = static_cast<float>(record.Time) * 1000.0f * 1000.0f;
            }

            if (reset)
            {
                // Interval-based reset for avg/min/max calculation.
                record.Reset();
            }
            else if (record.Dirty)
            {
                // Only process the dirty entries.
                record.Accumulate();
            }

            // "Time" is in real-time, so we update it every frame.
            auto pit = m_DisplayPerfData_.find(name);
            if (pit == m_DisplayPerfData_.end())
            {
                pit = m_DisplayPerfData_.emplace(data).first;
            }

            pit->second.Time = record.Time;
        }

        assert(m_PerfData_.size() == m_DisplayPerfData_.size());
    }

    void XeSSRuntime::SetInitArguments(const InitArguments& Args)
    {
        m_InitArguments = Args;
    }

    void XeSSRuntime::ForceLegacyScaleFactors(bool force)
    {
        xessForceLegacyScaleFactors(m_Context, force);
    }

} // namespace XeSS
