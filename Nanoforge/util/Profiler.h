#pragma once

#define nop do {} while(0)

//Define custom profiler macros so it can be disabled or switched out easily
#ifdef TRACY_ENABLE
#include <d3d11.h>
#include <Tracy.hpp>
#include <TracyD3D11.hpp>

    #define PROFILER_EVENT(eventName) TracyMessageC(eventName, 12, 0xFFFFFFFF)
    #define PROFILER_SCOPED(blockName) ZoneScopedN(blockName)
    #define PROFILER_SCOPED_AUTO ZoneScoped
    #define PROFILER_FUNCTION() ZoneScoped
    #define PROFILER_D3D11_CONTEXT(device, deviceContext) TracyD3D11Context(device, deviceContext);
    #define PROFILER_D3D11_CONTEXT_DESTROY(ctx) TracyD3D11Destroy(ctx);
    #define PROFILER_D3D11_ZONE(ctx, name) TracyD3D11Zone(ctx, name);
    #define PROFILER_D3D11_COLLECT(ctx) TracyD3D11Collect(ctx);
    #define PROFILER_FRAME_END() FrameMark
    #define PROFILER_SET_THREAD_NAME(threadName) tracy::SetThreadName(threadName)
    #define PROFILER_BLOCK(blockName) ZoneNamedN(#blockName, blockName, true)
    #define PROFILER_BLOCK_WRAP(blockName, code) \
    { \
        ZoneScopedN(blockName); \
        code; \
    } \

#else
    #define PROFILER_EVENT(eventName) nop;
    #define PROFILER_SCOPED(blockName) nop;
    #define PROFILER_SCOPED_AUTO(blockName) nop;
    #define PROFILER_FUNCTION() nop;
    #define PROFILER_D3D11_CONTEXT(device, deviceContext) nop;
    #define PROFILER_D3D11_CONTEXT_DESTROY(ctx) nop;
    #define PROFILER_D3D11_ZONE(name) nop;
    #define PROFILER_D3D11_COLLECT(ctx) nop;
    #define PROFILER_FRAME_END() nop;
    #define PROFILER_SET_THREAD_NAME(threadName) nop;
    #define PROFILER_BLOCK(blockName) nop;
    #define PROFILER_BLOCK_WRAP(blockName, code) nop;
#endif