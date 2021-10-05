#pragma once

//Define custom profiler macros so it can be disabled or switched out easily
#ifdef TRACY_ENABLE
#include <d3d11.h>
#include <Tracy.hpp>
#include <TracyD3D11.hpp>
//Uncomment this line to enable tracy callstack collection. Disabled by default because it has some performance cost
//#define TRACY_CALLSTACK 5

    #define PROFILER_EVENT(eventName) TracyMessageC(eventName, 12, 0xFFFFFFFF)
    #define PROFILER_SCOPED(blockName) ZoneScopedN(blockName)
    #define PROFILER_SCOPED_AUTO ZoneScoped
    #define PROFILER_FUNCTION() ZoneScoped
    #define PROFILER_D3D11_CONTEXT(device, deviceContext) TracyD3D11Context(device, deviceContext)
    #define PROFILER_D3D11_CONTEXT_DESTROY(ctx) TracyD3D11Destroy(ctx)
    #define PROFILER_D3D11_ZONE(ctx, name) TracyD3D11Zone(ctx, name)
    #define PROFILER_D3D11_COLLECT(ctx) TracyD3D11Collect(ctx)
    #define PROFILER_FRAME_MARK() FrameMark
    #define PROFILER_FRAME_MARK_START(name) FrameMarkStart(name)
    #define PROFILER_FRAME_MARK_END(name) FrameMarkEnd(name)
    #define PROFILER_SET_THREAD_NAME(threadName) tracy::SetThreadName(threadName)
    #define PROFILER_BLOCK(blockName) ZoneNamedN(#blockName, blockName, true)
    #define PROFILER_BLOCK_WRAP(blockName, code) \
    { \
        ZoneScopedN(blockName); \
        code; \
    } \

#else
    #define PROFILER_EVENT(eventName)
    #define PROFILER_SCOPED(blockName)
    #define PROFILER_SCOPED_AUTO(blockName)
    #define PROFILER_FUNCTION()
    #define PROFILER_D3D11_CONTEXT(device, deviceContext)
    #define PROFILER_D3D11_CONTEXT_DESTROY(ctx)
    #define PROFILER_D3D11_ZONE(ctx, name)
    #define PROFILER_D3D11_COLLECT(ctx)
    #define PROFILER_FRAME_MARK()
    #define PROFILER_FRAME_MARK_START(name)
    #define PROFILER_FRAME_MARK_END(name)
    #define PROFILER_SET_THREAD_NAME(threadName)
    #define PROFILER_BLOCK(blockName)
    #define PROFILER_BLOCK_WRAP(blockName, code) \
    { \
        code \
    } \

#endif