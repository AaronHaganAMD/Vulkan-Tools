/*
 * Vulkan
 *
 * Copyright (C) 2014 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

#include "vk_loader_platform.h"
#include "vk_dispatch_table_helper.h"
#include "vk_struct_string_helper_cpp.h"
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wwrite-strings"
#endif
#include "vk_struct_size_helper.h"
#include "draw_state.h"
#include "vk_layer_config.h"
#include "vk_debug_marker_layer.h"
// The following is #included again to catch certain OS-specific functions
// being used:
#include "vk_loader_platform.h"
#include "vk_layer_msg.h"
#include "vk_layer_table.h"
#include "vk_layer_debug_marker_table.h"
#include "vk_layer_data.h"
#include "vk_layer_logging.h"
#include "vk_layer_extension_utils.h"

typedef struct _layer_data {
    debug_report_data *report_data;
    // TODO: put instance data here
    VkDbgMsgCallback logging_callback;
} layer_data;

static std::unordered_map<void *, layer_data *> layer_data_map;
static device_table_map draw_state_device_table_map;
static instance_table_map draw_state_instance_table_map;

unordered_map<uint64_t, SAMPLER_NODE*> sampleMap;
unordered_map<uint64_t, VkImageViewCreateInfo> imageMap;
unordered_map<uint64_t, VkAttachmentViewCreateInfo> viewMap;
unordered_map<uint64_t, BUFFER_NODE*> bufferMap;
unordered_map<uint64_t, VkDynamicViewportStateCreateInfo> dynamicVpStateMap;
unordered_map<uint64_t, VkDynamicRasterStateCreateInfo> dynamicRsStateMap;
unordered_map<uint64_t, VkDynamicColorBlendStateCreateInfo> dynamicCbStateMap;
unordered_map<uint64_t, VkDynamicDepthStencilStateCreateInfo> dynamicDsStateMap;
unordered_map<uint64_t, PIPELINE_NODE*> pipelineMap;
unordered_map<uint64_t, POOL_NODE*> poolMap;
unordered_map<uint64_t, SET_NODE*> setMap;
unordered_map<uint64_t, LAYOUT_NODE*> layoutMap;
// Map for layout chains
unordered_map<void*, GLOBAL_CB_NODE*> cmdBufferMap;
unordered_map<uint64_t, VkRenderPassCreateInfo*> renderPassMap;
unordered_map<uint64_t, VkFramebufferCreateInfo*> frameBufferMap;

struct devExts {
    bool debug_marker_enabled;
};

static std::unordered_map<void *, struct devExts> deviceExtMap;

static LOADER_PLATFORM_THREAD_ONCE_DECLARATION(g_initOnce);

// TODO : This can be much smarter, using separate locks for separate global data
static int globalLockInitialized = 0;
static loader_platform_thread_mutex globalLock;
#define MAX_TID 513
static loader_platform_thread_id g_tidMapping[MAX_TID] = {0};
static uint32_t g_maxTID = 0;

template layer_data *get_my_data_ptr<layer_data>(
        void *data_key,
        std::unordered_map<void *, layer_data *> &data_map);

debug_report_data *mdd(void* object)
{
    dispatch_key key = get_dispatch_key(object);
    layer_data *my_data = get_my_data_ptr(key, layer_data_map);
#if DISPATCH_MAP_DEBUG
    fprintf(stderr, "MDD: map: %p, object: %p, key: %p, data: %p\n", &layer_data_map, object, key, my_data);
#endif
    return my_data->report_data;
}

debug_report_data *mid(VkInstance object)
{
    dispatch_key key = get_dispatch_key(object);
    layer_data *my_data = get_my_data_ptr(get_dispatch_key(object), layer_data_map);
#if DISPATCH_MAP_DEBUG
    fprintf(stderr, "MID: map: %p, object: %p, key: %p, data: %p\n", &layer_data_map, object, key, my_data);
#endif
    return my_data->report_data;
}
// Map actual TID to an index value and return that index
//  This keeps TIDs in range from 0-MAX_TID and simplifies compares between runs
static uint32_t getTIDIndex() {
    loader_platform_thread_id tid = loader_platform_get_thread_id();
    for (uint32_t i = 0; i < g_maxTID; i++) {
        if (tid == g_tidMapping[i])
            return i;
    }
    // Don't yet have mapping, set it and return newly set index
    uint32_t retVal = (uint32_t) g_maxTID;
    g_tidMapping[g_maxTID++] = tid;
    assert(g_maxTID < MAX_TID);
    return retVal;
}
// Return a string representation of CMD_TYPE enum
static string cmdTypeToString(CMD_TYPE cmd)
{
    switch (cmd)
    {
        case CMD_BINDPIPELINE:
            return "CMD_BINDPIPELINE";
        case CMD_BINDPIPELINEDELTA:
            return "CMD_BINDPIPELINEDELTA";
        case CMD_BINDDYNAMICVIEWPORTSTATE:
            return "CMD_BINDDYNAMICVIEWPORTSTATE";
        case CMD_BINDDYNAMICRASTERSTATE:
            return "CMD_BINDDYNAMICRASTERSTATE";
        case CMD_BINDDYNAMICCOLORBLENDSTATE:
            return "CMD_BINDDYNAMICCOLORBLENDSTATE";
        case CMD_BINDDYNAMICDEPTHSTENCILSTATE:
            return "CMD_BINDDYNAMICDEPTHSTENCILSTATE";
        case CMD_BINDDESCRIPTORSETS:
            return "CMD_BINDDESCRIPTORSETS";
        case CMD_BINDINDEXBUFFER:
            return "CMD_BINDINDEXBUFFER";
        case CMD_BINDVERTEXBUFFER:
            return "CMD_BINDVERTEXBUFFER";
        case CMD_DRAW:
            return "CMD_DRAW";
        case CMD_DRAWINDEXED:
            return "CMD_DRAWINDEXED";
        case CMD_DRAWINDIRECT:
            return "CMD_DRAWINDIRECT";
        case CMD_DRAWINDEXEDINDIRECT:
            return "CMD_DRAWINDEXEDINDIRECT";
        case CMD_DISPATCH:
            return "CMD_DISPATCH";
        case CMD_DISPATCHINDIRECT:
            return "CMD_DISPATCHINDIRECT";
        case CMD_COPYBUFFER:
            return "CMD_COPYBUFFER";
        case CMD_COPYIMAGE:
            return "CMD_COPYIMAGE";
        case CMD_BLITIMAGE:
            return "CMD_BLITIMAGE";
        case CMD_COPYBUFFERTOIMAGE:
            return "CMD_COPYBUFFERTOIMAGE";
        case CMD_COPYIMAGETOBUFFER:
            return "CMD_COPYIMAGETOBUFFER";
        case CMD_CLONEIMAGEDATA:
            return "CMD_CLONEIMAGEDATA";
        case CMD_UPDATEBUFFER:
            return "CMD_UPDATEBUFFER";
        case CMD_FILLBUFFER:
            return "CMD_FILLBUFFER";
        case CMD_CLEARCOLORIMAGE:
            return "CMD_CLEARCOLORIMAGE";
        case CMD_CLEARCOLORATTACHMENT:
            return "CMD_CLEARCOLORATTACHMENT";
        case CMD_CLEARDEPTHSTENCILIMAGE:
            return "CMD_CLEARDEPTHSTENCILIMAGE";
        case CMD_CLEARDEPTHSTENCILATTACHMENT:
            return "CMD_CLEARDEPTHSTENCILATTACHMENT";
        case CMD_RESOLVEIMAGE:
            return "CMD_RESOLVEIMAGE";
        case CMD_SETEVENT:
            return "CMD_SETEVENT";
        case CMD_RESETEVENT:
            return "CMD_RESETEVENT";
        case CMD_WAITEVENTS:
            return "CMD_WAITEVENTS";
        case CMD_PIPELINEBARRIER:
            return "CMD_PIPELINEBARRIER";
        case CMD_BEGINQUERY:
            return "CMD_BEGINQUERY";
        case CMD_ENDQUERY:
            return "CMD_ENDQUERY";
        case CMD_RESETQUERYPOOL:
            return "CMD_RESETQUERYPOOL";
        case CMD_WRITETIMESTAMP:
            return "CMD_WRITETIMESTAMP";
        case CMD_INITATOMICCOUNTERS:
            return "CMD_INITATOMICCOUNTERS";
        case CMD_LOADATOMICCOUNTERS:
            return "CMD_LOADATOMICCOUNTERS";
        case CMD_SAVEATOMICCOUNTERS:
            return "CMD_SAVEATOMICCOUNTERS";
        case CMD_BEGINRENDERPASS:
            return "CMD_BEGINRENDERPASS";
        case CMD_ENDRENDERPASS:
            return "CMD_ENDRENDERPASS";
        case CMD_DBGMARKERBEGIN:
            return "CMD_DBGMARKERBEGIN";
        case CMD_DBGMARKEREND:
            return "CMD_DBGMARKEREND";
        default:
            return "UNKNOWN";
    }
}
// Block of code at start here for managing/tracking Pipeline state that this layer cares about
// Just track 2 shaders for now
#define VK_NUM_GRAPHICS_SHADERS VK_SHADER_STAGE_COMPUTE
#define MAX_SLOTS 2048
#define NUM_COMMAND_BUFFERS_TO_DISPLAY 10

static uint64_t g_drawCount[NUM_DRAW_TYPES] = {0, 0, 0, 0};

// TODO : Should be tracking lastBound per cmdBuffer and when draws occur, report based on that cmd buffer lastBound
//   Then need to synchronize the accesses based on cmd buffer so that if I'm reading state on one cmd buffer, updates
//   to that same cmd buffer by separate thread are not changing state from underneath us
// Track the last cmd buffer touched by this thread
static VkCmdBuffer    g_lastCmdBuffer[MAX_TID] = {NULL};
// Track the last group of CBs touched for displaying to dot file
static GLOBAL_CB_NODE* g_pLastTouchedCB[NUM_COMMAND_BUFFERS_TO_DISPLAY] = {NULL};
static uint32_t        g_lastTouchedCBIndex = 0;
// Track the last global DrawState of interest touched by any thread
static GLOBAL_CB_NODE* g_lastGlobalCB = NULL;
static PIPELINE_NODE*  g_lastBoundPipeline = NULL;
static uint64_t        g_lastBoundDynamicState[VK_NUM_STATE_BIND_POINT] = {0};
static VkDescriptorSet g_lastBoundDescriptorSet = VkDescriptorSet(0);
#define MAX_BINDING 0xFFFFFFFF // Default vtxBinding value in CB Node to identify if no vtxBinding set

//static DYNAMIC_STATE_NODE* g_pDynamicStateHead[VK_NUM_STATE_BIND_POINT] = {0};

// Free all allocated nodes for Dynamic State objs
static void deleteDynamicState()
{
    for (auto ii=dynamicVpStateMap.begin(); ii!=dynamicVpStateMap.end(); ++ii) {
        delete[] (*ii).second.pScissors;
        delete[] (*ii).second.pViewports;
    }
    dynamicVpStateMap.clear();
    dynamicRsStateMap.clear();
    dynamicCbStateMap.clear();
    dynamicDsStateMap.clear();
}
// Free all sampler nodes
static void deleteSamplers()
{
    if (sampleMap.size() <= 0)
        return;
    for (auto ii=sampleMap.begin(); ii!=sampleMap.end(); ++ii) {
        delete (*ii).second;
    }
    sampleMap.clear();
}
static VkImageViewCreateInfo* getImageViewCreateInfo(VkImageView view)
{
    loader_platform_thread_lock_mutex(&globalLock);
    if (imageMap.find(view.handle) == imageMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        return NULL;
    } else {
        loader_platform_thread_unlock_mutex(&globalLock);
        return &imageMap[view.handle];
    }
}
// Free all image nodes
static void deleteImages()
{
    if (imageMap.size() <= 0)
        return;
    imageMap.clear();
}
static VkBufferViewCreateInfo* getBufferViewCreateInfo(VkBufferView view)
{
    loader_platform_thread_lock_mutex(&globalLock);
    if (bufferMap.find(view.handle) == bufferMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        return NULL;
    } else {
        loader_platform_thread_unlock_mutex(&globalLock);
        return &bufferMap[view.handle]->createInfo;
    }
}
// Free all buffer nodes
static void deleteBuffers()
{
    if (bufferMap.size() <= 0)
        return;
    for (auto ii=bufferMap.begin(); ii!=bufferMap.end(); ++ii) {
        delete (*ii).second;
    }
    bufferMap.clear();
}
static GLOBAL_CB_NODE* getCBNode(VkCmdBuffer cb);
// Update global ptrs to reflect that specified cmdBuffer has been used
static void updateCBTracking(VkCmdBuffer cb)
{
    g_lastCmdBuffer[getTIDIndex()] = cb;
    GLOBAL_CB_NODE* pCB = getCBNode(cb);
    loader_platform_thread_lock_mutex(&globalLock);
    g_lastGlobalCB = pCB;
    // TODO : This is a dumb algorithm. Need smart LRU that drops off oldest
    for (uint32_t i = 0; i < NUM_COMMAND_BUFFERS_TO_DISPLAY; i++) {
        if (g_pLastTouchedCB[i] == pCB) {
            loader_platform_thread_unlock_mutex(&globalLock);
            return;
        }
    }
    g_pLastTouchedCB[g_lastTouchedCBIndex++] = pCB;
    g_lastTouchedCBIndex = g_lastTouchedCBIndex % NUM_COMMAND_BUFFERS_TO_DISPLAY;
    loader_platform_thread_unlock_mutex(&globalLock);
}
static VkBool32 hasDrawCmd(GLOBAL_CB_NODE* pCB)
{
    for (uint32_t i=0; i<NUM_DRAW_TYPES; i++) {
        if (pCB->drawCount[i])
            return VK_TRUE;
    }
    return VK_FALSE;
}
// Check object status for selected flag state
static VkBool32 validate_status(GLOBAL_CB_NODE* pNode, CBStatusFlags enable_mask, CBStatusFlags status_mask, CBStatusFlags status_flag, VkFlags msg_flags, DRAW_STATE_ERROR error_code, const char* fail_msg)
{
    // If non-zero enable mask is present, check it against status but if enable_mask
    //  is 0 then no enable required so we should always just check status
    if ((!enable_mask) || (enable_mask & pNode->status)) {
        if ((pNode->status & status_mask) != status_flag) {
            // TODO : How to pass dispatchable objects as srcObject? Here src obj should be cmd buffer
            log_msg(mdd(pNode->cmdBuffer), msg_flags, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, error_code, "DS",
                    "CB object %#" PRIxLEAST64 ": %s", reinterpret_cast<VkUintPtrLeast64>(pNode->cmdBuffer), fail_msg);
            return VK_FALSE;
        }
    }
    return VK_TRUE;
}
// For given dynamic state handle and type, return CreateInfo for that Dynamic State
static void* getDynamicStateCreateInfo(const uint64_t handle, const DYNAMIC_STATE_BIND_POINT type)
{
    switch (type) {
        case VK_STATE_BIND_POINT_VIEWPORT:
            return (void*)&dynamicVpStateMap[handle];
        case VK_STATE_BIND_POINT_RASTER:
            return (void*)&dynamicRsStateMap[handle];
        case VK_STATE_BIND_POINT_COLOR_BLEND:
            return (void*)&dynamicCbStateMap[handle];
        case VK_STATE_BIND_POINT_DEPTH_STENCIL:
            return (void*)&dynamicDsStateMap[handle];
        default:
            return NULL;
    }
}
// Print the last bound dynamic state
static void printDynamicState(const VkCmdBuffer cb)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cb);
    if (pCB) {
        loader_platform_thread_lock_mutex(&globalLock);
        for (uint32_t i = 0; i < VK_NUM_STATE_BIND_POINT; i++) {
            void* pDynStateCI = getDynamicStateCreateInfo(pCB->lastBoundDynamicState[i], (DYNAMIC_STATE_BIND_POINT)i);
            if (pDynStateCI) {
                log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, dynamicStateBindPointToObjType((DYNAMIC_STATE_BIND_POINT)i), pCB->lastBoundDynamicState[i], 0, DRAWSTATE_NONE, "DS",
                        "Reporting CreateInfo for currently bound %s object %#" PRIxLEAST64, string_DYNAMIC_STATE_BIND_POINT((DYNAMIC_STATE_BIND_POINT)i).c_str(), pCB->lastBoundDynamicState[i]);
                log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, dynamicStateBindPointToObjType((DYNAMIC_STATE_BIND_POINT)i), pCB->lastBoundDynamicState[i], 0, DRAWSTATE_NONE, "DS",
                        dynamic_display(pDynStateCI, "  ").c_str());
                break;
            } else {
                log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                        "No dynamic state of type %s bound", string_DYNAMIC_STATE_BIND_POINT((DYNAMIC_STATE_BIND_POINT)i).c_str());
            }
        }
        loader_platform_thread_unlock_mutex(&globalLock);
    }
}
// Retrieve pipeline node ptr for given pipeline object
static PIPELINE_NODE* getPipeline(VkPipeline pipeline)
{
    loader_platform_thread_lock_mutex(&globalLock);
    if (pipelineMap.find(pipeline.handle) == pipelineMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        return NULL;
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return pipelineMap[pipeline.handle];
}
// Validate state stored as flags at time of draw call
static VkBool32 validate_draw_state_flags(GLOBAL_CB_NODE* pCB, VkBool32 indexedDraw) {
    VkBool32 result;
    result = validate_status(pCB, CBSTATUS_NONE, CBSTATUS_VIEWPORT_BOUND, CBSTATUS_VIEWPORT_BOUND, VK_DBG_REPORT_ERROR_BIT, DRAWSTATE_VIEWPORT_NOT_BOUND, "Viewport object not bound to this command buffer");
    result &= validate_status(pCB, CBSTATUS_NONE, CBSTATUS_RASTER_BOUND,   CBSTATUS_RASTER_BOUND,   VK_DBG_REPORT_ERROR_BIT, DRAWSTATE_RASTER_NOT_BOUND,   "Raster object not bound to this command buffer");
    result &= validate_status(pCB, CBSTATUS_COLOR_BLEND_WRITE_ENABLE, CBSTATUS_COLOR_BLEND_BOUND,   CBSTATUS_COLOR_BLEND_BOUND,   VK_DBG_REPORT_ERROR_BIT,  DRAWSTATE_COLOR_BLEND_NOT_BOUND,   "Color-blend object not bound to this command buffer");
    result &= validate_status(pCB, CBSTATUS_DEPTH_STENCIL_WRITE_ENABLE, CBSTATUS_DEPTH_STENCIL_BOUND, CBSTATUS_DEPTH_STENCIL_BOUND, VK_DBG_REPORT_ERROR_BIT,  DRAWSTATE_DEPTH_STENCIL_NOT_BOUND, "Depth-stencil object not bound to this command buffer");
    if (indexedDraw)
        result &= validate_status(pCB, CBSTATUS_NONE, CBSTATUS_INDEX_BUFFER_BOUND, CBSTATUS_INDEX_BUFFER_BOUND, VK_DBG_REPORT_ERROR_BIT, DRAWSTATE_INDEX_BUFFER_NOT_BOUND, "Index buffer object not bound to this command buffer when Index Draw attempted");
    return result;
}
// Validate overall state at the time of a draw call
static VkBool32 validate_draw_state(GLOBAL_CB_NODE* pCB, VkBool32 indexedDraw) {
    // First check flag states
    VkBool32 result = validate_draw_state_flags(pCB, indexedDraw);
    PIPELINE_NODE* pPipe = getPipeline(pCB->lastBoundPipeline);
    // Now complete other state checks
    if (pPipe && (pCB->lastBoundPipelineLayout != pPipe->graphicsPipelineCI.layout)) {
        result = VK_FALSE;
        log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pCB->lastBoundPipelineLayout.handle, 0, DRAWSTATE_PIPELINE_LAYOUT_MISMATCH, "DS",
                "Pipeline layout from last vkCmdBindDescriptorSets() (%s) does not match PSO Pipeline layout (%s)", pCB->lastBoundPipelineLayout, pPipe->graphicsPipelineCI.layout);
    }
    if (!pCB->activeRenderPass) {
        log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                "Draw cmd issued without an active RenderPass. vkCmdDraw*() must only be called within a RenderPass.");
    }
    return result;
}
// For given sampler, return a ptr to its Create Info struct, or NULL if sampler not found
static VkSamplerCreateInfo* getSamplerCreateInfo(const VkSampler sampler)
{
    loader_platform_thread_lock_mutex(&globalLock);
    if (sampleMap.find(sampler.handle) == sampleMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        return NULL;
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return &sampleMap[sampler.handle]->createInfo;
}
// Verify that create state for a pipeline is valid
static VkBool32 verifyPipelineCreateState(const VkDevice device, const PIPELINE_NODE* pPipeline)
{
    // VS is required
    if (!(pPipeline->active_shaders & VK_SHADER_STAGE_VERTEX_BIT)) {
        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_PIPELINE_CREATE_STATE, "DS",
                "Invalid Pipeline CreateInfo State: Vtx Shader required");
        return VK_FALSE;
    }
    // Either both or neither TC/TE shaders should be defined
    if (((pPipeline->active_shaders & VK_SHADER_STAGE_TESS_CONTROL_BIT) == 0) !=
         ((pPipeline->active_shaders & VK_SHADER_STAGE_TESS_EVALUATION_BIT) == 0) ) {
        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_PIPELINE_CREATE_STATE, "DS",
                "Invalid Pipeline CreateInfo State: TE and TC shaders must be included or excluded as a pair");
        return VK_FALSE;
    }
    // Compute shaders should be specified independent of Gfx shaders
    if ((pPipeline->active_shaders & VK_SHADER_STAGE_COMPUTE_BIT) &&
        (pPipeline->active_shaders & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESS_CONTROL_BIT |
                                     VK_SHADER_STAGE_TESS_EVALUATION_BIT | VK_SHADER_STAGE_GEOMETRY_BIT |
                                     VK_SHADER_STAGE_FRAGMENT_BIT))) {
        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_PIPELINE_CREATE_STATE, "DS",
                "Invalid Pipeline CreateInfo State: Do not specify Compute Shader for Gfx Pipeline");
        return VK_FALSE;
    }
    // VK_PRIMITIVE_TOPOLOGY_PATCH primitive topology is only valid for tessellation pipelines.
    // Mismatching primitive topology and tessellation fails graphics pipeline creation.
    if (pPipeline->active_shaders & (VK_SHADER_STAGE_TESS_CONTROL_BIT | VK_SHADER_STAGE_TESS_EVALUATION_BIT) &&
        (pPipeline->iaStateCI.topology != VK_PRIMITIVE_TOPOLOGY_PATCH)) {
        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_PIPELINE_CREATE_STATE, "DS",
                "Invalid Pipeline CreateInfo State: VK_PRIMITIVE_TOPOLOGY_PATCH must be set as IA topology for tessellation pipelines");
        return VK_FALSE;
    }
    if ((pPipeline->iaStateCI.topology == VK_PRIMITIVE_TOPOLOGY_PATCH) &&
        (~pPipeline->active_shaders & VK_SHADER_STAGE_TESS_CONTROL_BIT)) {
        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_PIPELINE_CREATE_STATE, "DS",
                "Invalid Pipeline CreateInfo State: VK_PRIMITIVE_TOPOLOGY_PATCH primitive topology is only valid for tessellation pipelines");
        return VK_FALSE;
    }
    return VK_TRUE;
}
// Init the pipeline mapping info based on pipeline create info LL tree
//  Threading note : Calls to this function should wrapped in mutex
static PIPELINE_NODE* initPipeline(const VkGraphicsPipelineCreateInfo* pCreateInfo, PIPELINE_NODE* pBasePipeline)
{
    PIPELINE_NODE* pPipeline = new PIPELINE_NODE;
    if (pBasePipeline) {
        memcpy((void*)pPipeline, (void*)pBasePipeline, sizeof(PIPELINE_NODE));
    } else {
        memset((void*)pPipeline, 0, sizeof(PIPELINE_NODE));
    }
    // First init create info
    // TODO : Validate that no create info is incorrectly replicated
    memcpy(&pPipeline->graphicsPipelineCI, pCreateInfo, sizeof(VkGraphicsPipelineCreateInfo));

    size_t bufferSize = 0;
    const VkPipelineVertexInputStateCreateInfo* pVICI = NULL;
    const VkPipelineColorBlendStateCreateInfo*          pCBCI = NULL;

    for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
        const VkPipelineShaderStageCreateInfo *pPSSCI = &pCreateInfo->pStages[i];

        switch (pPSSCI->stage) {
            case VK_SHADER_STAGE_VERTEX:
                memcpy(&pPipeline->vsCI, pPSSCI, sizeof(VkPipelineShaderStageCreateInfo));
                pPipeline->active_shaders |= VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case VK_SHADER_STAGE_TESS_CONTROL:
                memcpy(&pPipeline->tcsCI, pPSSCI, sizeof(VkPipelineShaderStageCreateInfo));
                pPipeline->active_shaders |= VK_SHADER_STAGE_TESS_CONTROL_BIT;
                break;
            case VK_SHADER_STAGE_TESS_EVALUATION:
                memcpy(&pPipeline->tesCI, pPSSCI, sizeof(VkPipelineShaderStageCreateInfo));
                pPipeline->active_shaders |= VK_SHADER_STAGE_TESS_EVALUATION_BIT;
                break;
            case VK_SHADER_STAGE_GEOMETRY:
                memcpy(&pPipeline->gsCI, pPSSCI, sizeof(VkPipelineShaderStageCreateInfo));
                pPipeline->active_shaders |= VK_SHADER_STAGE_GEOMETRY_BIT;
                break;
            case VK_SHADER_STAGE_FRAGMENT:
                memcpy(&pPipeline->fsCI, pPSSCI, sizeof(VkPipelineShaderStageCreateInfo));
                pPipeline->active_shaders |= VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case VK_SHADER_STAGE_COMPUTE:
                // TODO : Flag error, CS is specified through VkComputePipelineCreateInfo
                pPipeline->active_shaders |= VK_SHADER_STAGE_COMPUTE_BIT;
                break;
            default:
                // TODO : Flag error
                break;
        }
    }

    if (pCreateInfo->pVertexInputState != NULL) {
        memcpy((void*)&pPipeline->vertexInputCI, pCreateInfo->pVertexInputState , sizeof(VkPipelineVertexInputStateCreateInfo));
        // Copy embedded ptrs
        pVICI = pCreateInfo->pVertexInputState;
        pPipeline->vtxBindingCount = pVICI->bindingCount;
        if (pPipeline->vtxBindingCount) {
            pPipeline->pVertexBindingDescriptions = new VkVertexInputBindingDescription[pPipeline->vtxBindingCount];
            bufferSize = pPipeline->vtxBindingCount * sizeof(VkVertexInputBindingDescription);
            memcpy((void*)pPipeline->pVertexBindingDescriptions, pVICI->pVertexBindingDescriptions, bufferSize);
        }
        pPipeline->vtxAttributeCount = pVICI->attributeCount;
        if (pPipeline->vtxAttributeCount) {
            pPipeline->pVertexAttributeDescriptions = new VkVertexInputAttributeDescription[pPipeline->vtxAttributeCount];
            bufferSize = pPipeline->vtxAttributeCount * sizeof(VkVertexInputAttributeDescription);
            memcpy((void*)pPipeline->pVertexAttributeDescriptions, pVICI->pVertexAttributeDescriptions, bufferSize);
        }
        pPipeline->graphicsPipelineCI.pVertexInputState = &pPipeline->vertexInputCI;
    }
    if (pCreateInfo->pInputAssemblyState != NULL) {
        memcpy((void*)&pPipeline->iaStateCI, pCreateInfo->pInputAssemblyState, sizeof(VkPipelineInputAssemblyStateCreateInfo));
        pPipeline->graphicsPipelineCI.pInputAssemblyState = &pPipeline->iaStateCI;
    }
    if (pCreateInfo->pTessellationState != NULL) {
        memcpy((void*)&pPipeline->tessStateCI, pCreateInfo->pTessellationState, sizeof(VkPipelineTessellationStateCreateInfo));
        pPipeline->graphicsPipelineCI.pTessellationState = &pPipeline->tessStateCI;
    }
    if (pCreateInfo->pViewportState != NULL) {
        memcpy((void*)&pPipeline->vpStateCI, pCreateInfo->pViewportState, sizeof(VkPipelineViewportStateCreateInfo));
        pPipeline->graphicsPipelineCI.pViewportState = &pPipeline->vpStateCI;
    }
    if (pCreateInfo->pRasterState != NULL) {
        memcpy((void*)&pPipeline->rsStateCI, pCreateInfo->pRasterState, sizeof(VkPipelineRasterStateCreateInfo));
        pPipeline->graphicsPipelineCI.pRasterState = &pPipeline->rsStateCI;
    }
    if (pCreateInfo->pMultisampleState != NULL) {
        memcpy((void*)&pPipeline->msStateCI, pCreateInfo->pMultisampleState, sizeof(VkPipelineMultisampleStateCreateInfo));
        pPipeline->graphicsPipelineCI.pMultisampleState = &pPipeline->msStateCI;
    }
    if (pCreateInfo->pColorBlendState != NULL) {
        memcpy((void*)&pPipeline->cbStateCI, pCreateInfo->pColorBlendState, sizeof(VkPipelineColorBlendStateCreateInfo));
        // Copy embedded ptrs
        pCBCI = pCreateInfo->pColorBlendState;
        pPipeline->attachmentCount = pCBCI->attachmentCount;
        if (pPipeline->attachmentCount) {
            pPipeline->pAttachments = new VkPipelineColorBlendAttachmentState[pPipeline->attachmentCount];
            bufferSize = pPipeline->attachmentCount * sizeof(VkPipelineColorBlendAttachmentState);
            memcpy((void*)pPipeline->pAttachments, pCBCI->pAttachments, bufferSize);
        }
        pPipeline->graphicsPipelineCI.pColorBlendState = &pPipeline->cbStateCI;
    }
    if (pCreateInfo->pDepthStencilState != NULL) {
        memcpy((void*)&pPipeline->dsStateCI, pCreateInfo->pDepthStencilState, sizeof(VkPipelineDepthStencilStateCreateInfo));
        pPipeline->graphicsPipelineCI.pDepthStencilState = &pPipeline->dsStateCI;
    }

    // Copy over GraphicsPipelineCreateInfo structure embedded pointers
    if (pCreateInfo->stageCount != 0) {
        pPipeline->graphicsPipelineCI.pStages = new VkPipelineShaderStageCreateInfo[pCreateInfo->stageCount];
        bufferSize =  pCreateInfo->stageCount * sizeof(VkPipelineShaderStageCreateInfo);
        memcpy((void*)pPipeline->graphicsPipelineCI.pStages, pCreateInfo->pStages, bufferSize); 
    }

    return pPipeline;
}

// Free the Pipeline nodes
static void deletePipelines()
{
    if (pipelineMap.size() <= 0)
        return;
    for (auto ii=pipelineMap.begin(); ii!=pipelineMap.end(); ++ii) {
        if ((*ii).second->graphicsPipelineCI.stageCount != 0) {
            delete[] (*ii).second->graphicsPipelineCI.pStages;
        }
        if ((*ii).second->pVertexBindingDescriptions) {
            delete[] (*ii).second->pVertexBindingDescriptions;
        }
        if ((*ii).second->pVertexAttributeDescriptions) {
            delete[] (*ii).second->pVertexAttributeDescriptions;
        }
        if ((*ii).second->pAttachments) {
            delete[] (*ii).second->pAttachments;
        }
        delete (*ii).second;
    }
    pipelineMap.clear();
}
// For given pipeline, return number of MSAA samples, or one if MSAA disabled
static uint32_t getNumSamples(const VkPipeline pipeline)
{
    PIPELINE_NODE* pPipe = pipelineMap[pipeline.handle];
    if (VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO == pPipe->msStateCI.sType) {
        if (pPipe->msStateCI.multisampleEnable)
            return pPipe->msStateCI.rasterSamples;
    }
    return 1;
}
// Validate state related to the PSO
static void validatePipelineState(const GLOBAL_CB_NODE* pCB, const VkPipelineBindPoint pipelineBindPoint, const VkPipeline pipeline)
{
    if (VK_PIPELINE_BIND_POINT_GRAPHICS == pipelineBindPoint) {
        // Verify that any MSAA request in PSO matches sample# in bound FB
        uint32_t psoNumSamples = getNumSamples(pipeline);
        if (pCB->activeRenderPass) {
            const VkRenderPassCreateInfo* pRPCI = renderPassMap[pCB->activeRenderPass.handle];
            const VkSubpassDescription* pSD = &pRPCI->pSubpasses[pCB->activeSubpass];
            int subpassNumSamples = 0;
            uint32_t i;

            for (i = 0; i < pSD->colorCount; i++) {
                uint32_t samples;

                if (pSD->colorAttachments[i].attachment == VK_ATTACHMENT_UNUSED)
                    continue;

                samples = pRPCI->pAttachments[pSD->colorAttachments[i].attachment].samples;
                if (subpassNumSamples == 0) {
                    subpassNumSamples = samples;
                } else if (subpassNumSamples != samples) {
                    subpassNumSamples = -1;
                    break;
                }
            }
            if (pSD->depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED) {
                const uint32_t samples = pRPCI->pAttachments[pSD->depthStencilAttachment.attachment].samples;
                if (subpassNumSamples == 0)
                    subpassNumSamples = samples;
                else if (subpassNumSamples != samples)
                    subpassNumSamples = -1;
            }

            if (psoNumSamples != subpassNumSamples) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PIPELINE, pipeline.handle, 0, DRAWSTATE_NUM_SAMPLES_MISMATCH, "DS",
                        "Num samples mismatch! Binding PSO (%#" PRIxLEAST64 ") with %u samples while current RenderPass (%#" PRIxLEAST64 ") w/ %u samples!", pipeline.handle, psoNumSamples, pCB->activeRenderPass.handle, subpassNumSamples);
            }
        } else {
            // TODO : I believe it's an error if we reach this point and don't have an activeRenderPass
            //   Verify and flag error as appropriate
        }
        // TODO : Add more checks here
    } else {
        // TODO : Validate non-gfx pipeline updates
    }
}

// Block of code at start here specifically for managing/tracking DSs

// Return Pool node ptr for specified pool or else NULL
static POOL_NODE* getPoolNode(VkDescriptorPool pool)
{
    loader_platform_thread_lock_mutex(&globalLock);
    if (poolMap.find(pool.handle) == poolMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        return NULL;
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return poolMap[pool.handle];
}
// Return Set node ptr for specified set or else NULL
static SET_NODE* getSetNode(VkDescriptorSet set)
{
    loader_platform_thread_lock_mutex(&globalLock);
    if (setMap.find(set.handle) == setMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        return NULL;
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return setMap[set.handle];
}

static LAYOUT_NODE* getLayoutNode(const VkDescriptorSetLayout layout) {
    loader_platform_thread_lock_mutex(&globalLock);
    if (layoutMap.find(layout.handle) == layoutMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        return NULL;
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return layoutMap[layout.handle];
}
// Return 1 if update struct is of valid type, 0 otherwise
static VkBool32 validUpdateStruct(const VkDevice device, const GENERIC_HEADER* pUpdateStruct)
{
    char str[1024];
    switch (pUpdateStruct->sType)
    {
        case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
        case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
            return 1;
        default:
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_UPDATE_STRUCT, "DS",
                    "Unexpected UPDATE struct of type %s (value %u) in vkUpdateDescriptors() struct tree", string_VkStructureType(pUpdateStruct->sType), pUpdateStruct->sType);
            return 0;
    }
}
// For given update struct, return binding
static uint32_t getUpdateBinding(const VkDevice device, const GENERIC_HEADER* pUpdateStruct)
{
    char str[1024];
    switch (pUpdateStruct->sType)
    {
        case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
            return ((VkWriteDescriptorSet*)pUpdateStruct)->destBinding;
        case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
            return ((VkCopyDescriptorSet*)pUpdateStruct)->destBinding;
        default:
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_UPDATE_STRUCT, "DS",
                    "Unexpected UPDATE struct of type %s (value %u) in vkUpdateDescriptors() struct tree", string_VkStructureType(pUpdateStruct->sType), pUpdateStruct->sType);
            return 0xFFFFFFFF;
    }
}
// Return count for given update struct
static uint32_t getUpdateArrayIndex(const VkDevice device, const GENERIC_HEADER* pUpdateStruct)
{
    char str[1024];
    switch (pUpdateStruct->sType)
    {
        case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
            return ((VkWriteDescriptorSet*)pUpdateStruct)->destArrayElement;
        case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
            // TODO : Need to understand this case better and make sure code is correct
            return ((VkCopyDescriptorSet*)pUpdateStruct)->destArrayElement;
        default:
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_UPDATE_STRUCT, "DS",
                    "Unexpected UPDATE struct of type %s (value %u) in vkUpdateDescriptors() struct tree", string_VkStructureType(pUpdateStruct->sType), pUpdateStruct->sType);
            return 0;
    }
}
// Return count for given update struct
static uint32_t getUpdateCount(const VkDevice device, const GENERIC_HEADER* pUpdateStruct)
{
    char str[1024];
    switch (pUpdateStruct->sType)
    {
        case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
            return ((VkWriteDescriptorSet*)pUpdateStruct)->count;
        case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
            // TODO : Need to understand this case better and make sure code is correct
            return ((VkCopyDescriptorSet*)pUpdateStruct)->count;
        default:
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_UPDATE_STRUCT, "DS",
                    "Unexpected UPDATE struct of type %s (value %u) in vkUpdateDescriptors() struct tree", string_VkStructureType(pUpdateStruct->sType), pUpdateStruct->sType);
            return 0;
    }
}
// For given Layout Node and binding, return index where that binding begins
static uint32_t getBindingStartIndex(const LAYOUT_NODE* pLayout, const uint32_t binding)
{
    uint32_t offsetIndex = 0;
    for (uint32_t i = 0; i<binding; i++) {
        offsetIndex += pLayout->createInfo.pBinding[i].arraySize;
    }
    return offsetIndex;
}
// For given layout node and binding, return last index that is updated
static uint32_t getBindingEndIndex(const LAYOUT_NODE* pLayout, const uint32_t binding)
{
    uint32_t offsetIndex = 0;
    for (uint32_t i = 0; i<=binding; i++) {
        offsetIndex += pLayout->createInfo.pBinding[i].arraySize;
    }
    return offsetIndex-1;
}
// For given layout and update, return the first overall index of the layout that is update
static uint32_t getUpdateStartIndex(const VkDevice device, const LAYOUT_NODE* pLayout, const GENERIC_HEADER* pUpdateStruct)
{
    return (getBindingStartIndex(pLayout, getUpdateBinding(device, pUpdateStruct))+getUpdateArrayIndex(device, pUpdateStruct));
}
// For given layout and update, return the last overall index of the layout that is update
static uint32_t getUpdateEndIndex(const VkDevice device, const LAYOUT_NODE* pLayout, const GENERIC_HEADER* pUpdateStruct)
{
    return (getBindingStartIndex(pLayout, getUpdateBinding(device, pUpdateStruct))+getUpdateArrayIndex(device, pUpdateStruct)+getUpdateCount(device, pUpdateStruct)-1);
}
// Verify that the descriptor type in the update struct matches what's expected by the layout
static VkBool32 validateUpdateType(const VkDevice device, const LAYOUT_NODE* pLayout, const GENERIC_HEADER* pUpdateStruct)
{
    // First get actual type of update
    VkDescriptorType actualType;
    uint32_t i = 0;
    char str[1024];
    switch (pUpdateStruct->sType)
    {
        case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
            actualType = ((VkWriteDescriptorSet*)pUpdateStruct)->descriptorType;
            break;
        case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
            /* no need to validate */
            return 1;
            break;
        default:
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_UPDATE_STRUCT, "DS",
                    "Unexpected UPDATE struct of type %s (value %u) in vkUpdateDescriptors() struct tree", string_VkStructureType(pUpdateStruct->sType), pUpdateStruct->sType);
            return 0;
    }
    for (i = getUpdateStartIndex(device, pLayout, pUpdateStruct); i <= getUpdateEndIndex(device, pLayout, pUpdateStruct); i++) {
        if (pLayout->pTypes[i] != actualType)
            return 0;
    }
    return 1;
}
// Determine the update type, allocate a new struct of that type, shadow the given pUpdate
//   struct into the new struct and return ptr to shadow struct cast as GENERIC_HEADER
// NOTE : Calls to this function should be wrapped in mutex
static GENERIC_HEADER* shadowUpdateNode(const VkDevice device, GENERIC_HEADER* pUpdate)
{
    GENERIC_HEADER* pNewNode = NULL;
    VkWriteDescriptorSet* pWDS = NULL;
    VkCopyDescriptorSet* pCDS = NULL;
    size_t array_size = 0;
    size_t base_array_size = 0;
    size_t total_array_size = 0;
    size_t baseBuffAddr = 0;
    char str[1024];
    switch (pUpdate->sType)
    {
        case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
            pWDS = new VkWriteDescriptorSet;
            pNewNode = (GENERIC_HEADER*)pWDS;
            memcpy(pWDS, pUpdate, sizeof(VkWriteDescriptorSet));
            pWDS->pDescriptors = new VkDescriptorInfo[pWDS->count];
            array_size = sizeof(VkDescriptorInfo) * pWDS->count;
            memcpy((void*)pWDS->pDescriptors, ((VkWriteDescriptorSet*)pUpdate)->pDescriptors, array_size);
            break;
        case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
            pCDS = new VkCopyDescriptorSet;
            pUpdate = (GENERIC_HEADER*)pCDS;
            memcpy(pCDS, pUpdate, sizeof(VkCopyDescriptorSet));
            break;
        default:
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_UPDATE_STRUCT, "DS",
                    "Unexpected UPDATE struct of type %s (value %u) in vkUpdateDescriptors() struct tree", string_VkStructureType(pUpdate->sType), pUpdate->sType);
            return NULL;
    }
    // Make sure that pNext for the end of shadow copy is NULL
    pNewNode->pNext = NULL;
    return pNewNode;
}
// update DS mappings based on ppUpdateArray
static VkBool32 dsUpdate(VkDevice device, VkStructureType type, uint32_t updateCount, const void* pUpdateArray)
{
    const VkWriteDescriptorSet *pWDS = NULL;
    const VkCopyDescriptorSet *pCDS = NULL;
    VkBool32 result = 1;

    if (type == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
        pWDS = (const VkWriteDescriptorSet *) pUpdateArray;
    else
        pCDS = (const VkCopyDescriptorSet *) pUpdateArray;

    loader_platform_thread_lock_mutex(&globalLock);
    LAYOUT_NODE* pLayout = NULL;
    VkDescriptorSetLayoutCreateInfo* pLayoutCI = NULL;
    // TODO : If pCIList is NULL, flag error
    // Perform all updates
    for (uint32_t i = 0; i < updateCount; i++) {
        VkDescriptorSet ds = (pWDS) ? pWDS->destSet : pCDS->destSet;
        SET_NODE* pSet = setMap[ds.handle]; // getSetNode() without locking
        g_lastBoundDescriptorSet = pSet->set;
        GENERIC_HEADER* pUpdate = (pWDS) ? (GENERIC_HEADER*) &pWDS[i] : (GENERIC_HEADER*) &pCDS[i];
        pLayout = pSet->pLayout;
        // First verify valid update struct
        if (!validUpdateStruct(device, pUpdate)) {
            result = 0;
            break;
        }
        // Make sure that binding is within bounds
        if (pLayout->createInfo.count < getUpdateBinding(device, pUpdate)) {
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, ds.handle, 0, DRAWSTATE_INVALID_UPDATE_INDEX, "DS",
                    "Descriptor Set %p does not have binding to match update binding %u for update type %s!", ds, getUpdateBinding(device, pUpdate), string_VkStructureType(pUpdate->sType));
            result = 0;
        } else {
            // Next verify that update falls within size of given binding
            if (getBindingEndIndex(pLayout, getUpdateBinding(device, pUpdate)) < getUpdateEndIndex(device, pLayout, pUpdate)) {
                char str[48*1024]; // TODO : Keep count of layout CI structs and size this string dynamically based on that count
                pLayoutCI = &pLayout->createInfo;
                string DSstr = vk_print_vkdescriptorsetlayoutcreateinfo(pLayoutCI, "{DS}    ");
                log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, ds.handle, 0, DRAWSTATE_DESCRIPTOR_UPDATE_OUT_OF_BOUNDS, "DS",
                        "Descriptor update type of %s is out of bounds for matching binding %u in Layout w/ CI:\n%s!", string_VkStructureType(pUpdate->sType), getUpdateBinding(device, pUpdate), DSstr.c_str());
                result = 0;
            } else { // TODO : should we skip update on a type mismatch or force it?
                // Layout bindings match w/ update ok, now verify that update is of the right type
                if (!validateUpdateType(device, pLayout, pUpdate)) {
                    log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, ds.handle, 0, DRAWSTATE_DESCRIPTOR_TYPE_MISMATCH, "DS",
                            "Descriptor update type of %s does not match overlapping binding type!", string_VkStructureType(pUpdate->sType));
                    result = 0;
                } else {
                    // Save the update info
                    // TODO : Info message that update successful
                    // Create new update struct for this set's shadow copy
                    GENERIC_HEADER* pNewNode = shadowUpdateNode(device, pUpdate);
                    if (NULL == pNewNode) {
                        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, ds.handle, 0, DRAWSTATE_OUT_OF_MEMORY, "DS",
                                "Out of memory while attempting to allocate UPDATE struct in vkUpdateDescriptors()");
                        result = 0;
                    } else {
                        // Insert shadow node into LL of updates for this set
                        pNewNode->pNext = pSet->pUpdateStructs;
                        pSet->pUpdateStructs = pNewNode;
                        // Now update appropriate descriptor(s) to point to new Update node
                        for (uint32_t j = getUpdateStartIndex(device, pLayout, pUpdate); j <= getUpdateEndIndex(device, pLayout, pUpdate); j++) {
                            assert(j<pSet->descriptorCount);
                            pSet->ppDescriptors[j] = pNewNode;
                        }
                    }
                }
            }
        }
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}
// Free the shadowed update node for this Set
// NOTE : Calls to this function should be wrapped in mutex
static void freeShadowUpdateTree(SET_NODE* pSet)
{
    GENERIC_HEADER* pShadowUpdate = pSet->pUpdateStructs;
    pSet->pUpdateStructs = NULL;
    GENERIC_HEADER* pFreeUpdate = pShadowUpdate;
    // Clear the descriptor mappings as they will now be invalid
    memset(pSet->ppDescriptors, 0, pSet->descriptorCount*sizeof(GENERIC_HEADER*));
    while(pShadowUpdate) {
        pFreeUpdate = pShadowUpdate;
        pShadowUpdate = (GENERIC_HEADER*)pShadowUpdate->pNext;
        uint32_t index = 0;
        VkWriteDescriptorSet * pWDS = NULL;
        VkCopyDescriptorSet * pCDS = NULL;
        void** ppToFree = NULL;
        switch (pFreeUpdate->sType)
        {
            case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
                pWDS = (VkWriteDescriptorSet*)pFreeUpdate;
                if (pWDS->pDescriptors)
                    delete[] pWDS->pDescriptors;
                break;
            case VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET:
                break;
            default:
                assert(0);
                break;
        }
        delete pFreeUpdate;
    }
}
// Free all DS Pools including their Sets & related sub-structs
// NOTE : Calls to this function should be wrapped in mutex
static void deletePools()
{
    if (poolMap.size() <= 0)
        return;
    for (auto ii=poolMap.begin(); ii!=poolMap.end(); ++ii) {
        SET_NODE* pSet = (*ii).second->pSets;
        SET_NODE* pFreeSet = pSet;
        while (pSet) {
            pFreeSet = pSet;
            pSet = pSet->pNext;
            // Freeing layouts handled in deleteLayouts() function
            // Free Update shadow struct tree
            freeShadowUpdateTree(pFreeSet);
            if (pFreeSet->ppDescriptors) {
                delete[] pFreeSet->ppDescriptors;
            }
            delete pFreeSet;
        }
        if ((*ii).second->createInfo.pTypeCount) {
            delete[] (*ii).second->createInfo.pTypeCount;
        }
        delete (*ii).second;
    }
    poolMap.clear();
}
// WARN : Once deleteLayouts() called, any layout ptrs in Pool/Set data structure will be invalid
// NOTE : Calls to this function should be wrapped in mutex
static void deleteLayouts()
{
    if (layoutMap.size() <= 0)
        return;
    for (auto ii=layoutMap.begin(); ii!=layoutMap.end(); ++ii) {
        LAYOUT_NODE* pLayout = (*ii).second;
        if (pLayout->createInfo.pBinding) {
            for (uint32_t i=0; i<pLayout->createInfo.count; i++) {
                if (pLayout->createInfo.pBinding[i].pImmutableSamplers)
                    delete[] pLayout->createInfo.pBinding[i].pImmutableSamplers;
            }
            delete[] pLayout->createInfo.pBinding;
        }
        if (pLayout->pTypes) {
            delete[] pLayout->pTypes;
        }
        delete pLayout;
    }
    layoutMap.clear();
}
// Currently clearing a set is removing all previous updates to that set
//  TODO : Validate if this is correct clearing behavior
static void clearDescriptorSet(VkDescriptorSet set)
{
    SET_NODE* pSet = getSetNode(set);
    if (!pSet) {
        // TODO : Return error
    } else {
        loader_platform_thread_lock_mutex(&globalLock);
        freeShadowUpdateTree(pSet);
        loader_platform_thread_unlock_mutex(&globalLock);
    }
}

static void clearDescriptorPool(VkDevice device, VkDescriptorPool pool)
{
    POOL_NODE* pPool = getPoolNode(pool);
    if (!pPool) {
        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_POOL, pool.handle, 0, DRAWSTATE_INVALID_POOL, "DS",
                "Unable to find pool node for pool %#" PRIxLEAST64 " specified in vkResetDescriptorPool() call", pool.handle);
    } else {
        // For every set off of this pool, clear it
        SET_NODE* pSet = pPool->pSets;
        while (pSet) {
            clearDescriptorSet(pSet->set);
        }
    }
}
// For given CB object, fetch associated CB Node from map
static GLOBAL_CB_NODE* getCBNode(VkCmdBuffer cb)
{
    loader_platform_thread_lock_mutex(&globalLock);
    if (cmdBufferMap.find(cb) == cmdBufferMap.end()) {
        loader_platform_thread_unlock_mutex(&globalLock);
        // TODO : How to pass cb as srcObj here?
        log_msg(mdd(cb), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_INVALID_CMD_BUFFER, "DS",
                "Attempt to use CmdBuffer %#" PRIxLEAST64 " that doesn't exist!", reinterpret_cast<VkUintPtrLeast64>(cb));
        return NULL;
    }
    loader_platform_thread_unlock_mutex(&globalLock);
    return cmdBufferMap[cb];
}
// Free all CB Nodes
// NOTE : Calls to this function should be wrapped in mutex
static void deleteCmdBuffers()
{
    if (cmdBufferMap.size() <= 0)
        return;
    for (auto ii=cmdBufferMap.begin(); ii!=cmdBufferMap.end(); ++ii) {
        vector<CMD_NODE*> cmd_node_list = (*ii).second->pCmds;
        while (!cmd_node_list.empty()) {
            CMD_NODE* cmd_node = cmd_node_list.back();
            delete cmd_node;
            cmd_node_list.pop_back();
        }
        delete (*ii).second;
    }
    cmdBufferMap.clear();
}
static void report_error_no_cb_begin(const VkCmdBuffer cb, const char* caller_name)
{
    // TODO : How to pass cb as srcObj here?
    log_msg(mdd(cb), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_NO_BEGIN_CMD_BUFFER, "DS",
            "You must call vkBeginCommandBuffer() before this call to %s", (void*)caller_name);
}
static void addCmd(GLOBAL_CB_NODE* pCB, const CMD_TYPE cmd)
{
    CMD_NODE* pCmd = new CMD_NODE;
    if (pCmd) {
        // init cmd node and append to end of cmd LL
        memset(pCmd, 0, sizeof(CMD_NODE));
        pCmd->cmdNumber = ++pCB->numCmds;
        pCmd->type = cmd;
        pCB->pCmds.push_back(pCmd);
    } else {
        // TODO : How to pass cb as srcObj here?
        log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_OUT_OF_MEMORY, "DS",
                "Out of memory while attempting to allocate new CMD_NODE for cmdBuffer %#" PRIxLEAST64, reinterpret_cast<VkUintPtrLeast64>(pCB->cmdBuffer));
    }
}
static void resetCB(const VkCmdBuffer cb)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cb);
    if (pCB) {
        vector<CMD_NODE*> cmd_list = pCB->pCmds;
        while (!cmd_list.empty()) {
            delete cmd_list.back();
            cmd_list.pop_back();
        }
        pCB->pCmds.clear();
        // Reset CB state
        VkFlags saveFlags = pCB->flags;
        uint32_t saveQueueNodeIndex = pCB->queueNodeIndex;
        memset(pCB, 0, sizeof(GLOBAL_CB_NODE));
        pCB->cmdBuffer = cb;
        pCB->flags = saveFlags;
        pCB->queueNodeIndex = saveQueueNodeIndex;
        pCB->lastVtxBinding = MAX_BINDING;
    }
}
// Set PSO-related status bits for CB
static void set_cb_pso_status(GLOBAL_CB_NODE* pCB, const PIPELINE_NODE* pPipe)
{
    for (uint32_t i = 0; i < pPipe->cbStateCI.attachmentCount; i++) {
        if (0 != pPipe->pAttachments[i].channelWriteMask) {
            pCB->status |= CBSTATUS_COLOR_BLEND_WRITE_ENABLE;
        }
    }
    if (pPipe->dsStateCI.depthWriteEnable) {
        pCB->status |= CBSTATUS_DEPTH_STENCIL_WRITE_ENABLE;
    }
}
// Set dyn-state related status bits for an object node
static void set_cb_dyn_status(GLOBAL_CB_NODE* pNode, DYNAMIC_STATE_BIND_POINT stateBindPoint) {
    if (stateBindPoint == VK_STATE_BIND_POINT_VIEWPORT) {
        pNode->status |= CBSTATUS_VIEWPORT_BOUND;
    } else if (stateBindPoint == VK_STATE_BIND_POINT_RASTER) {
        pNode->status |= CBSTATUS_RASTER_BOUND;
    } else if (stateBindPoint == VK_STATE_BIND_POINT_COLOR_BLEND) {
        pNode->status |= CBSTATUS_COLOR_BLEND_BOUND;
    } else if (stateBindPoint == VK_STATE_BIND_POINT_DEPTH_STENCIL) {
        pNode->status |= CBSTATUS_DEPTH_STENCIL_BOUND;
    }
}
// Print the last bound Gfx Pipeline
static void printPipeline(const VkCmdBuffer cb)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cb);
    if (pCB) {
        PIPELINE_NODE *pPipeTrav = getPipeline(pCB->lastBoundPipeline);
        if (!pPipeTrav) {
            // nothing to print
        } else {
            log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                    vk_print_vkgraphicspipelinecreateinfo(&pPipeTrav->graphicsPipelineCI, "{DS}").c_str());
        }
    }
}
// Verify bound Pipeline State Object
static bool validateBoundPipeline(const VkCmdBuffer cb)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cb);
    if (pCB && pCB->lastBoundPipeline) {
        // First verify that we have a Node for bound pipeline
        PIPELINE_NODE *pPipeTrav = getPipeline(pCB->lastBoundPipeline);
        char str[1024];
        if (!pPipeTrav) {
            log_msg(mdd(cb), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_PIPELINE_BOUND, "DS",
                    "Can't find last bound Pipeline %#" PRIxLEAST64 "!", pCB->lastBoundPipeline.handle);
            return false;
        } else {
            // Verify Vtx binding
            if (MAX_BINDING != pCB->lastVtxBinding) {
                if (pCB->lastVtxBinding >= pPipeTrav->vtxBindingCount) {
                    if (0 == pPipeTrav->vtxBindingCount) {
                        log_msg(mdd(cb), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_VTX_INDEX_OUT_OF_BOUNDS, "DS",
                                "Vtx Buffer Index %u was bound, but no vtx buffers are attached to PSO.", pCB->lastVtxBinding);
                        return false;
                    }
                    else {
                        log_msg(mdd(cb), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_VTX_INDEX_OUT_OF_BOUNDS, "DS",
                                "Vtx binding Index of %u exceeds PSO pVertexBindingDescriptions max array index of %u.", pCB->lastVtxBinding, (pPipeTrav->vtxBindingCount - 1));
                        return false;
                    }
                }
                else {
                    log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                            vk_print_vkvertexinputbindingdescription(&pPipeTrav->pVertexBindingDescriptions[pCB->lastVtxBinding], "{DS}INFO : ").c_str());
                }
            }
        }
        return true;
    }
    return false;
}
// Print details of DS config to stdout
static void printDSConfig(const VkCmdBuffer cb)
{
    char tmp_str[1024];
    char ds_config_str[1024*256] = {0}; // TODO : Currently making this buffer HUGE w/o overrun protection.  Need to be smarter, start smaller, and grow as needed.
    GLOBAL_CB_NODE* pCB = getCBNode(cb);
    if (pCB && pCB->lastBoundDescriptorSet) {
        SET_NODE* pSet = getSetNode(pCB->lastBoundDescriptorSet);
        POOL_NODE* pPool = getPoolNode(pSet->pool);
        // Print out pool details
        log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                "Details for pool %#" PRIxLEAST64 ".", pPool->pool.handle);
        string poolStr = vk_print_vkdescriptorpoolcreateinfo(&pPool->createInfo, " ");
        log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                "%s", poolStr.c_str());
        // Print out set details
        char prefix[10];
        uint32_t index = 0;
        log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                "Details for descriptor set %#" PRIxLEAST64 ".", pSet->set.handle);
        LAYOUT_NODE* pLayout = pSet->pLayout;
        // Print layout details
        log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                "Layout #%u, (object %#" PRIxLEAST64 ") for DS %#" PRIxLEAST64 ".", index+1, (void*)pLayout->layout.handle, (void*)pSet->set.handle);
        sprintf(prefix, "  [L%u] ", index);
        string DSLstr = vk_print_vkdescriptorsetlayoutcreateinfo(&pLayout->createInfo, prefix).c_str();
        log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                "%s", DSLstr.c_str());
        index++;
        GENERIC_HEADER* pUpdate = pSet->pUpdateStructs;
        if (pUpdate) {
            log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                    "Update Chain [UC] for descriptor set %#" PRIxLEAST64 ":", pSet->set.handle);
            sprintf(prefix, "  [UC] ");
            log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                    dynamic_display(pUpdate, prefix).c_str());
            // TODO : If there is a "view" associated with this update, print CI for that view
        } else {
            if (0 != pSet->descriptorCount) {
                log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                        "No Update Chain for descriptor set %#" PRIxLEAST64 " which has %u descriptors (vkUpdateDescriptors has not been called)", pSet->set.handle, pSet->descriptorCount);
            } else {
                log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                        "FYI: No descriptors in descriptor set %#" PRIxLEAST64 ".", pSet->set.handle);
            }
        }
    }
}

static void printCB(const VkCmdBuffer cb)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cb);
    if (pCB && pCB->pCmds.size() > 0) {
        log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NONE, "DS",
                "Cmds in CB %p", (void*)cb);
        vector<CMD_NODE*> pCmds = pCB->pCmds;
        for (auto ii=pCmds.begin(); ii!=pCmds.end(); ++ii) {
            // TODO : Need to pass cb as srcObj here
            log_msg(mdd(cb), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_NONE, "DS",
                "  CMD#%lu: %s", (*ii)->cmdNumber, cmdTypeToString((*ii)->type).c_str());
        }
    } else {
        // Nothing to print
    }
}


static void synchAndPrintDSConfig(const VkCmdBuffer cb)
{
    // TODO : Re-enable these print funcs
//    printDSConfig(cb);
//    printPipeline(cb);
//    printDynamicState(cb);
}

static void init_draw_state(layer_data *my_data)
{
    uint32_t report_flags = 0;
    uint32_t debug_action = 0;
    FILE *log_output = NULL;
    const char *option_str;
    // initialize DrawState options
    report_flags = getLayerOptionFlags("DrawStateReportFlags", 0);
    getLayerOptionEnum("DrawStateDebugAction", (uint32_t *) &debug_action);

    if (debug_action & VK_DBG_LAYER_ACTION_LOG_MSG)
    {
        option_str = getLayerOption("DrawStateLogFilename");
        if (option_str)
        {
            log_output = fopen(option_str, "w");
        }
        if (log_output == NULL)
            log_output = stdout;

        layer_create_msg_callback(my_data->report_data, report_flags, log_callback, (void *) log_output, &my_data->logging_callback);
    }

    if (!globalLockInitialized)
    {
        // TODO/TBD: Need to delete this mutex sometime.  How???  One
        // suggestion is to call this during vkCreateInstance(), and then we
        // can clean it up during vkDestroyInstance().  However, that requires
        // that the layer have per-instance locks.  We need to come back and
        // address this soon.
        loader_platform_thread_create_mutex(&globalLock);
        globalLockInitialized = 1;
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance)
{
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(draw_state_instance_table_map,*pInstance);
    VkResult result = pTable->CreateInstance(pCreateInfo, pInstance);

    if (result == VK_SUCCESS) {
        layer_data *my_data = get_my_data_ptr(get_dispatch_key(*pInstance), layer_data_map);
        my_data->report_data = debug_report_create_instance(
                                   pTable,
                                   *pInstance,
                                   pCreateInfo->extensionCount,
                                   pCreateInfo->ppEnabledExtensionNames);

        init_draw_state(my_data);
    }
    return result;
}

/* hook DestroyInstance to remove tableInstanceMap entry */
VK_LAYER_EXPORT VkResult VKAPI vkDestroyInstance(VkInstance instance)
{
    dispatch_key key = get_dispatch_key(instance);
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(draw_state_instance_table_map, instance);
    VkResult res = pTable->DestroyInstance(instance);

    // Clean up logging callback, if any
    layer_data *my_data = get_my_data_ptr(key, layer_data_map);
    if (my_data->logging_callback) {
        layer_destroy_msg_callback(my_data->report_data, my_data->logging_callback);
    }

    layer_debug_report_destroy_instance(my_data->report_data);
    layer_data_map.erase(pTable);

    draw_state_instance_table_map.erase(key);
    return res;
}

static void createDeviceRegisterExtensions(const VkDeviceCreateInfo* pCreateInfo, VkDevice device)
{
    uint32_t i, ext_idx;
    VkLayerDispatchTable *pDisp =  get_dispatch_table(draw_state_device_table_map, device);
    deviceExtMap[pDisp].debug_marker_enabled = false;

    for (i = 0; i < pCreateInfo->extensionCount; i++) {
        if (strcmp(pCreateInfo->ppEnabledExtensionNames[i], DEBUG_MARKER_EXTENSION_NAME) == 0) {
            /* Found a matching extension name, mark it enabled and init dispatch table*/
            initDebugMarkerTable(device);
            deviceExtMap[pDisp].debug_marker_enabled = true;
        }

    }
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDevice(VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice)
{
    VkLayerDispatchTable *pDeviceTable = get_dispatch_table(draw_state_device_table_map, *pDevice);
    VkResult result = pDeviceTable->CreateDevice(gpu, pCreateInfo, pDevice);
    if (result == VK_SUCCESS) {
        layer_data *my_instance_data = get_my_data_ptr(get_dispatch_key(gpu), layer_data_map);
        VkLayerDispatchTable *pTable = get_dispatch_table(draw_state_device_table_map, *pDevice);
        layer_data *my_device_data = get_my_data_ptr(get_dispatch_key(*pDevice), layer_data_map);
        my_device_data->report_data = layer_debug_report_create_device(my_instance_data->report_data, *pDevice);
        createDeviceRegisterExtensions(pCreateInfo, *pDevice);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDevice(VkDevice device)
{
    // Free all the memory
    loader_platform_thread_lock_mutex(&globalLock);
    deletePipelines();
    deleteSamplers();
    deleteImages();
    deleteBuffers();
    deleteCmdBuffers();
    deleteDynamicState();
    deletePools();
    deleteLayouts();
    loader_platform_thread_unlock_mutex(&globalLock);

    dispatch_key key = get_dispatch_key(device);
    VkLayerDispatchTable *pDisp =  get_dispatch_table(draw_state_device_table_map, device);
    VkResult result = pDisp->DestroyDevice(device);
    deviceExtMap.erase(pDisp);
    draw_state_device_table_map.erase(key);
    tableDebugMarkerMap.erase(pDisp);
    return result;
}

static const VkLayerProperties ds_global_layers[] = {
    {
        "DrawState",
        VK_API_VERSION,
        VK_MAKE_VERSION(0, 1, 0),
        "Validation layer: DrawState",
    }
};

VK_LAYER_EXPORT VkResult VKAPI vkGetGlobalExtensionProperties(
        const char *pLayerName,
        uint32_t *pCount,
        VkExtensionProperties* pProperties)
{
    /* DrawState does not have any global extensions */
    return util_GetExtensionProperties(0, NULL, pCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI vkGetGlobalLayerProperties(
        uint32_t *pCount,
        VkLayerProperties*    pProperties)
{
    return util_GetLayerProperties(ARRAY_SIZE(ds_global_layers),
                                   ds_global_layers,
                                   pCount, pProperties);
}

static const VkExtensionProperties ds_device_extensions[] = {
    {
        DEBUG_MARKER_EXTENSION_NAME,
        VK_MAKE_VERSION(0, 1, 0),
        VK_API_VERSION
    }
};

static const VkLayerProperties ds_device_layers[] = {
    {
        "DrawState",
        VK_API_VERSION,
        VK_MAKE_VERSION(0, 1, 0),
        "Validation layer: DrawState",
    }
};

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceExtensionProperties(
        VkPhysicalDevice                            physicalDevice,
        const char*                                 pLayerName,
        uint32_t*                                   pCount,
        VkExtensionProperties*                      pProperties)
{
    /* Mem tracker does not have any physical device extensions */
    return util_GetExtensionProperties(ARRAY_SIZE(ds_device_extensions), ds_device_extensions,
                                       pCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceLayerProperties(
        VkPhysicalDevice                            physicalDevice,
        uint32_t*                                   pCount,
        VkLayerProperties*                          pProperties)
{
    /* Mem tracker's physical device layers are the same as global */
    return util_GetLayerProperties(ARRAY_SIZE(ds_device_layers), ds_device_layers,
                                   pCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI vkQueueSubmit(VkQueue queue, uint32_t cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence)
{
    GLOBAL_CB_NODE* pCB = NULL;
    for (uint32_t i=0; i < cmdBufferCount; i++) {
        // Validate that cmd buffers have been updated
        pCB = getCBNode(pCmdBuffers[i]);
        loader_platform_thread_lock_mutex(&globalLock);
        if (CB_UPDATE_COMPLETE != pCB->state) {
            // Flag error for using CB w/o vkEndCommandBuffer() called
            // TODO : How to pass cb as srcObj?
            log_msg(mdd(queue), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_NO_END_CMD_BUFFER, "DS",
                    "You must call vkEndCommandBuffer() on CB %#" PRIxLEAST64 " before this call to vkQueueSubmit()!", reinterpret_cast<VkUintPtrLeast64>(pCB->cmdBuffer));
            loader_platform_thread_unlock_mutex(&globalLock);
            return VK_ERROR_UNKNOWN;
        }
        loader_platform_thread_unlock_mutex(&globalLock);
    }

    VkResult result = get_dispatch_table(draw_state_device_table_map, queue)->QueueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyFence(VkDevice device, VkFence fence)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyFence(device, fence);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroySemaphore(VkDevice device, VkSemaphore semaphore)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroySemaphore(device, semaphore);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyEvent(VkDevice device, VkEvent event)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyEvent(device, event);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyQueryPool(device, queryPool);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyBuffer(VkDevice device, VkBuffer buffer)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyBuffer(device, buffer);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyBufferView(VkDevice device, VkBufferView bufferView)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyBufferView(device, bufferView);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyImage(VkDevice device, VkImage image)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyImage(device, image);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyImageView(VkDevice device, VkImageView imageView)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyImageView(device, imageView);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyAttachmentView(VkDevice device, VkAttachmentView attachmentView)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyAttachmentView(device, attachmentView);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyShaderModule(device, shaderModule);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyShader(VkDevice device, VkShader shader)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyShader(device, shader);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyPipeline(VkDevice device, VkPipeline pipeline)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyPipeline(device, pipeline);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyPipelineLayout(device, pipelineLayout);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroySampler(VkDevice device, VkSampler sampler)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroySampler(device, sampler);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyDescriptorSetLayout(device, descriptorSetLayout);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyDescriptorPool(device, descriptorPool);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDynamicViewportState(VkDevice device, VkDynamicViewportState dynamicViewportState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyDynamicViewportState(device, dynamicViewportState);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDynamicRasterState(VkDevice device, VkDynamicRasterState dynamicRasterState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyDynamicRasterState(device, dynamicRasterState);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDynamicColorBlendState(VkDevice device, VkDynamicColorBlendState dynamicColorBlendState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyDynamicColorBlendState(device, dynamicColorBlendState);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDynamicDepthStencilState(VkDevice device, VkDynamicDepthStencilState dynamicDepthStencilState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyDynamicDepthStencilState(device, dynamicDepthStencilState);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyCommandBuffer(VkDevice device, VkCmdBuffer commandBuffer)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyCommandBuffer(device, commandBuffer);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyFramebuffer(device, framebuffer);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyRenderPass(device, renderPass);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateBufferView(device, pCreateInfo, pView);
    if (VK_SUCCESS == result) {
        loader_platform_thread_lock_mutex(&globalLock);
        BUFFER_NODE* pNewNode = new BUFFER_NODE;
        pNewNode->buffer = *pView;
        pNewNode->createInfo = *pCreateInfo;
        bufferMap[pView->handle] = pNewNode;
        loader_platform_thread_unlock_mutex(&globalLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateImageView(device, pCreateInfo, pView);
    if (VK_SUCCESS == result) {
        loader_platform_thread_lock_mutex(&globalLock);
        imageMap[pView->handle] = *pCreateInfo;
        loader_platform_thread_unlock_mutex(&globalLock);
    }
    return result;
}

VkResult VKAPI vkCreateAttachmentView(
    VkDevice                                    device,
    const VkAttachmentViewCreateInfo*           pCreateInfo,
    VkAttachmentView*                           pView)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateAttachmentView(device, pCreateInfo, pView);
    if (VK_SUCCESS == result) {
        loader_platform_thread_lock_mutex(&globalLock);
        viewMap[pView->handle] = *pCreateInfo;
        loader_platform_thread_unlock_mutex(&globalLock);
    }
    return result;
}

//TODO handle pipeline caches
VkResult VKAPI vkCreatePipelineCache(
    VkDevice                                    device,
    const VkPipelineCacheCreateInfo*            pCreateInfo,
    VkPipelineCache*                            pPipelineCache)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreatePipelineCache(device, pCreateInfo, pPipelineCache);
    return result;
}

VkResult VKAPI vkDestroyPipelineCache(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->DestroyPipelineCache(device, pipelineCache);
    return result;
}

size_t VKAPI vkGetPipelineCacheSize(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache)
{
    size_t size = get_dispatch_table(draw_state_device_table_map, device)->GetPipelineCacheSize(device, pipelineCache);
    return size;
}

VkResult VKAPI vkGetPipelineCacheData(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    void*                                       pData)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->GetPipelineCacheData(device, pipelineCache, pData);
    return result;
}

VkResult VKAPI vkMergePipelineCaches(
    VkDevice                                    device,
    VkPipelineCache                             destCache,
    uint32_t                                    srcCacheCount,
    const VkPipelineCache*                      pSrcCaches)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->MergePipelineCaches(device, destCache, srcCacheCount, pSrcCaches);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines)
{
    VkResult result = VK_ERROR_BAD_PIPELINE_DATA;
    //TODO handle count > 1  and handle pipelineCache
    // The order of operations here is a little convoluted but gets the job done
    //  1. Pipeline create state is first shadowed into PIPELINE_NODE struct
    //  2. Create state is then validated (which uses flags setup during shadowing)
    //  3. If everything looks good, we'll then create the pipeline and add NODE to pipelineMap
    loader_platform_thread_lock_mutex(&globalLock);
    PIPELINE_NODE* pPipeNode = initPipeline(pCreateInfos, NULL);
    VkBool32 valid = verifyPipelineCreateState(device, pPipeNode);
    loader_platform_thread_unlock_mutex(&globalLock);
    if (VK_TRUE == valid) {
        result = get_dispatch_table(draw_state_device_table_map, device)->CreateGraphicsPipelines(device, pipelineCache, count, pCreateInfos, pPipelines);
        log_msg(mdd(device), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_PIPELINE, (*pPipelines).handle, 0, DRAWSTATE_NONE, "DS",
                "Created Gfx Pipeline %#" PRIxLEAST64, (*pPipelines).handle);
        loader_platform_thread_lock_mutex(&globalLock);
        pPipeNode->pipeline = *pPipelines;
        pipelineMap[pPipeNode->pipeline.handle] = pPipeNode;
        loader_platform_thread_unlock_mutex(&globalLock);
    } else {
        if (pPipeNode) {
            // If we allocated a pipeNode, need to clean it up here
            delete[] pPipeNode->pVertexBindingDescriptions;
            delete[] pPipeNode->pVertexAttributeDescriptions;
            delete[] pPipeNode->pAttachments;
            delete pPipeNode;
        }
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateSampler(device, pCreateInfo, pSampler);
    if (VK_SUCCESS == result) {
        loader_platform_thread_lock_mutex(&globalLock);
        SAMPLER_NODE* pNewNode = new SAMPLER_NODE;
        pNewNode->sampler = *pSampler;
        pNewNode->createInfo = *pCreateInfo;
        sampleMap[pSampler->handle] = pNewNode;
        loader_platform_thread_unlock_mutex(&globalLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateDescriptorSetLayout(device, pCreateInfo, pSetLayout);
    if (VK_SUCCESS == result) {
        LAYOUT_NODE* pNewNode = new LAYOUT_NODE;
        if (NULL == pNewNode) {
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (*pSetLayout).handle, 0, DRAWSTATE_OUT_OF_MEMORY, "DS",
                    "Out of memory while attempting to allocate LAYOUT_NODE in vkCreateDescriptorSetLayout()");
        }
        memset(pNewNode, 0, sizeof(LAYOUT_NODE));
        memcpy((void*)&pNewNode->createInfo, pCreateInfo, sizeof(VkDescriptorSetLayoutCreateInfo));
        pNewNode->createInfo.pBinding = new VkDescriptorSetLayoutBinding[pCreateInfo->count];
        memcpy((void*)pNewNode->createInfo.pBinding, pCreateInfo->pBinding, sizeof(VkDescriptorSetLayoutBinding)*pCreateInfo->count);
        uint32_t totalCount = 0;
        for (uint32_t i=0; i<pCreateInfo->count; i++) {
            totalCount += pCreateInfo->pBinding[i].arraySize;
            if (pCreateInfo->pBinding[i].pImmutableSamplers) {
                VkSampler** ppIS = (VkSampler**)&pNewNode->createInfo.pBinding[i].pImmutableSamplers;
                *ppIS = new VkSampler[pCreateInfo->pBinding[i].arraySize];
                memcpy(*ppIS, pCreateInfo->pBinding[i].pImmutableSamplers, pCreateInfo->pBinding[i].arraySize*sizeof(VkSampler));
            }
        }
        if (totalCount > 0) {
            pNewNode->pTypes = new VkDescriptorType[totalCount];
            uint32_t offset = 0;
            uint32_t j = 0;
            for (uint32_t i=0; i<pCreateInfo->count; i++) {
                for (j = 0; j < pCreateInfo->pBinding[i].arraySize; j++) {
                    pNewNode->pTypes[offset + j] = pCreateInfo->pBinding[i].descriptorType;
                }
                offset += j;
            }
        }
        pNewNode->layout = *pSetLayout;
        pNewNode->startIndex = 0;
        pNewNode->endIndex = pNewNode->startIndex + totalCount - 1;
        assert(pNewNode->endIndex >= pNewNode->startIndex);
        // Put new node at Head of global Layer list
        loader_platform_thread_lock_mutex(&globalLock);
        layoutMap[pSetLayout->handle] = pNewNode;
        loader_platform_thread_unlock_mutex(&globalLock);
    }
    return result;
}

VkResult VKAPI vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreatePipelineLayout(device, pCreateInfo, pPipelineLayout);
    if (VK_SUCCESS == result) {
        // TODO : Need to capture the pipeline layout
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDescriptorPool(VkDevice device, VkDescriptorPoolUsage poolUsage, uint32_t maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);
    if (VK_SUCCESS == result) {
        // Insert this pool into Global Pool LL at head
        log_msg(mdd(device), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (*pDescriptorPool).handle, 0, DRAWSTATE_OUT_OF_MEMORY, "DS",
                "Created Descriptor Pool %#" PRIxLEAST64, (*pDescriptorPool).handle);
        loader_platform_thread_lock_mutex(&globalLock);
        POOL_NODE* pNewNode = new POOL_NODE;
        if (NULL == pNewNode) {
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (*pDescriptorPool).handle, 0, DRAWSTATE_OUT_OF_MEMORY, "DS",
                    "Out of memory while attempting to allocate POOL_NODE in vkCreateDescriptorPool()");
        } else {
            memset(pNewNode, 0, sizeof(POOL_NODE));
            VkDescriptorPoolCreateInfo* pCI = (VkDescriptorPoolCreateInfo*)&pNewNode->createInfo;
            memcpy((void*)pCI, pCreateInfo, sizeof(VkDescriptorPoolCreateInfo));
            if (pNewNode->createInfo.count) {
                size_t typeCountSize = pNewNode->createInfo.count * sizeof(VkDescriptorTypeCount);
                pNewNode->createInfo.pTypeCount = new VkDescriptorTypeCount[typeCountSize];
                memcpy((void*)pNewNode->createInfo.pTypeCount, pCreateInfo->pTypeCount, typeCountSize);
            }
            pNewNode->poolUsage  = poolUsage;
            pNewNode->maxSets      = maxSets;
            pNewNode->pool       = *pDescriptorPool;
            poolMap[pDescriptorPool->handle] = pNewNode;
        }
        loader_platform_thread_unlock_mutex(&globalLock);
    } else {
        // Need to do anything if pool create fails?
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->ResetDescriptorPool(device, descriptorPool);
    if (VK_SUCCESS == result) {
        clearDescriptorPool(device, descriptorPool);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkAllocDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, uint32_t count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets, uint32_t* pCount)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->AllocDescriptorSets(device, descriptorPool, setUsage, count, pSetLayouts, pDescriptorSets, pCount);
    if ((VK_SUCCESS == result) || (*pCount > 0)) {
        POOL_NODE *pPoolNode = getPoolNode(descriptorPool);
        if (!pPoolNode) {
            log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_POOL, descriptorPool.handle, 0, DRAWSTATE_INVALID_POOL, "DS",
                    "Unable to find pool node for pool %#" PRIxLEAST64 " specified in vkAllocDescriptorSets() call", descriptorPool.handle);
        } else {
            for (uint32_t i = 0; i < *pCount; i++) {
                log_msg(mdd(device), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, pDescriptorSets[i].handle, 0, DRAWSTATE_NONE, "DS",
                        "Created Descriptor Set %#" PRIxLEAST64, pDescriptorSets[i].handle);
                // Create new set node and add to head of pool nodes
                SET_NODE* pNewNode = new SET_NODE;
                if (NULL == pNewNode) {
                    log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, pDescriptorSets[i].handle, 0, DRAWSTATE_OUT_OF_MEMORY, "DS",
                            "Out of memory while attempting to allocate SET_NODE in vkAllocDescriptorSets()");
                } else {
                    memset(pNewNode, 0, sizeof(SET_NODE));
                    // Insert set at head of Set LL for this pool
                    pNewNode->pNext = pPoolNode->pSets;
                    pPoolNode->pSets = pNewNode;
                    LAYOUT_NODE* pLayout = getLayoutNode(pSetLayouts[i]);
                    if (NULL == pLayout) {
                        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, pSetLayouts[i].handle, 0, DRAWSTATE_INVALID_LAYOUT, "DS",
                                "Unable to find set layout node for layout %#" PRIxLEAST64 " specified in vkAllocDescriptorSets() call", pSetLayouts[i].handle);
                    }
                    pNewNode->pLayout = pLayout;
                    pNewNode->pool = descriptorPool;
                    pNewNode->set = pDescriptorSets[i];
                    pNewNode->setUsage = setUsage;
                    pNewNode->descriptorCount = pLayout->endIndex + 1;
                    if (pNewNode->descriptorCount) {
                        size_t descriptorArraySize = sizeof(GENERIC_HEADER*)*pNewNode->descriptorCount;
                        pNewNode->ppDescriptors = new GENERIC_HEADER*[descriptorArraySize];
                        memset(pNewNode->ppDescriptors, 0, descriptorArraySize);
                    }
                    setMap[pDescriptorSets[i].handle] = pNewNode;
                }
            }
        }
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->FreeDescriptorSets(device, descriptorPool, count, pDescriptorSets);
    // TODO : Clean up any internal data structures using this obj.
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkUpdateDescriptorSets(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies)
{
    if (dsUpdate(device, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, writeCount, pDescriptorWrites) &&
        dsUpdate(device, VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, copyCount, pDescriptorCopies)) {
        return get_dispatch_table(draw_state_device_table_map, device)->UpdateDescriptorSets(device, writeCount, pDescriptorWrites, copyCount, pDescriptorCopies);
    }
    return VK_ERROR_UNKNOWN;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicViewportState(VkDevice device, const VkDynamicViewportStateCreateInfo* pCreateInfo, VkDynamicViewportState* pState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateDynamicViewportState(device, pCreateInfo, pState);
    VkDynamicViewportStateCreateInfo local_ci;
    memcpy(&local_ci, pCreateInfo, sizeof(VkDynamicViewportStateCreateInfo));
    local_ci.pViewports = new VkViewport[pCreateInfo->viewportAndScissorCount];
    local_ci.pScissors = new VkRect2D[pCreateInfo->viewportAndScissorCount];
    loader_platform_thread_lock_mutex(&globalLock);
    dynamicVpStateMap[pState->handle] = local_ci;
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicRasterState(VkDevice device, const VkDynamicRasterStateCreateInfo* pCreateInfo, VkDynamicRasterState* pState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateDynamicRasterState(device, pCreateInfo, pState);
    //insertDynamicState(*pState, (GENERIC_HEADER*)pCreateInfo, VK_STATE_BIND_POINT_RASTER);
    loader_platform_thread_lock_mutex(&globalLock);
    dynamicRsStateMap[pState->handle] = *pCreateInfo;
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicColorBlendState(VkDevice device, const VkDynamicColorBlendStateCreateInfo* pCreateInfo, VkDynamicColorBlendState* pState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateDynamicColorBlendState(device, pCreateInfo, pState);
    //insertDynamicState(*pState, (GENERIC_HEADER*)pCreateInfo, VK_STATE_BIND_POINT_COLOR_BLEND);
    loader_platform_thread_lock_mutex(&globalLock);
    dynamicCbStateMap[pState->handle] = *pCreateInfo;
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicDepthStencilState(VkDevice device, const VkDynamicDepthStencilStateCreateInfo* pCreateInfo, VkDynamicDepthStencilState* pState)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateDynamicDepthStencilState(device, pCreateInfo, pState);
    //insertDynamicState(*pState, (GENERIC_HEADER*)pCreateInfo, VK_STATE_BIND_POINT_DEPTH_STENCIL);
    loader_platform_thread_lock_mutex(&globalLock);
    dynamicDsStateMap[pState->handle] = *pCreateInfo;
    loader_platform_thread_unlock_mutex(&globalLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateCommandBuffer(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateCommandBuffer(device, pCreateInfo, pCmdBuffer);
    if (VK_SUCCESS == result) {
        loader_platform_thread_lock_mutex(&globalLock);
        GLOBAL_CB_NODE* pCB = new GLOBAL_CB_NODE;
        memset(pCB, 0, sizeof(GLOBAL_CB_NODE));
        pCB->cmdBuffer = *pCmdBuffer;
        pCB->flags = pCreateInfo->flags;
        pCB->queueNodeIndex = pCreateInfo->queueNodeIndex;
        pCB->lastVtxBinding = MAX_BINDING;
        cmdBufferMap[*pCmdBuffer] = pCB;
        loader_platform_thread_unlock_mutex(&globalLock);
        updateCBTracking(*pCmdBuffer);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkBeginCommandBuffer(VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, cmdBuffer)->BeginCommandBuffer(cmdBuffer, pBeginInfo);
    if (VK_SUCCESS == result) {
        GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
        if (pCB) {
            if (CB_NEW != pCB->state)
                resetCB(cmdBuffer);
            pCB->state = CB_UPDATE_ACTIVE;
        } else {
            // TODO : Need to pass cmdBuffer as objType here
            log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_INVALID_CMD_BUFFER, "DS",
                    "In vkBeginCommandBuffer() and unable to find CmdBuffer Node for CB %p!", (void*)cmdBuffer);
        }
        updateCBTracking(cmdBuffer);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkEndCommandBuffer(VkCmdBuffer cmdBuffer)
{
    VkResult result = VK_ERROR_BUILDING_COMMAND_BUFFER;
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            result = get_dispatch_table(draw_state_device_table_map, cmdBuffer)->EndCommandBuffer(cmdBuffer);
            if (VK_SUCCESS == result) {
                updateCBTracking(cmdBuffer);
                pCB->state = CB_UPDATE_COMPLETE;
                // Reset CB status flags
                pCB->status = 0;
                printCB(cmdBuffer);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkEndCommandBuffer()");
        }
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkResetCommandBuffer(VkCmdBuffer cmdBuffer)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, cmdBuffer)->ResetCommandBuffer(cmdBuffer);
    if (VK_SUCCESS == result) {
        resetCB(cmdBuffer);
        updateCBTracking(cmdBuffer);
    }
    return result;
}

VK_LAYER_EXPORT void VKAPI vkCmdBindPipeline(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDPIPELINE);
            if ((VK_PIPELINE_BIND_POINT_COMPUTE == pipelineBindPoint) && (pCB->activeRenderPass)) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PIPELINE, pipeline.handle, 0, DRAWSTATE_INVALID_RENDERPASS_CMD, "DS",
                        "Incorrectly binding compute pipeline (%#" PRIxLEAST64 ") during active RenderPass (%#" PRIxLEAST64 ")", pipeline.handle, pCB->activeRenderPass.handle);
            } else if ((VK_PIPELINE_BIND_POINT_GRAPHICS == pipelineBindPoint) && (!pCB->activeRenderPass)) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PIPELINE, pipeline.handle, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrectly binding graphics pipeline (%#" PRIxLEAST64 ") without an active RenderPass", pipeline.handle);
            } else {
                PIPELINE_NODE* pPN = getPipeline(pipeline);
                if (pPN) {
                    pCB->lastBoundPipeline = pipeline;
                    loader_platform_thread_lock_mutex(&globalLock);
                    set_cb_pso_status(pCB, pPN);
                    g_lastBoundPipeline = pPN;
                    loader_platform_thread_unlock_mutex(&globalLock);
                    validatePipelineState(pCB, pipelineBindPoint, pipeline);
                    get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);
                } else {
                    log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_PIPELINE, pipeline.handle, 0, DRAWSTATE_INVALID_PIPELINE, "DS",
                            "Attempt to bind Pipeline %#" PRIxLEAST64 " that doesn't exist!", (void*)pipeline.handle);
                }
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindPipeline()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdBindDynamicViewportState(VkCmdBuffer cmdBuffer, VkDynamicViewportState dynamicViewportState)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDDYNAMICVIEWPORTSTATE);
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrect call to vkCmdBindDynamicViewportState() without an active RenderPass.");
            }
            loader_platform_thread_lock_mutex(&globalLock);
            pCB->status |= CBSTATUS_VIEWPORT_BOUND;
            if (dynamicVpStateMap.find(dynamicViewportState.handle) == dynamicVpStateMap.end()) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DYNAMIC_VIEWPORT_STATE, dynamicViewportState.handle, 0, DRAWSTATE_INVALID_DYNAMIC_STATE_OBJECT, "DS",
                        "Unable to find VkDynamicViewportState object %#" PRIxLEAST64 ", was it ever created?", dynamicViewportState.handle);
            } else {
                pCB->lastBoundDynamicState[VK_STATE_BIND_POINT_VIEWPORT] = dynamicViewportState.handle;
                g_lastBoundDynamicState[VK_STATE_BIND_POINT_VIEWPORT] = dynamicViewportState.handle;
            }
            loader_platform_thread_unlock_mutex(&globalLock);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindDynamicViewportState()");
        }
    }
}
VK_LAYER_EXPORT void VKAPI vkCmdBindDynamicRasterState(VkCmdBuffer cmdBuffer, VkDynamicRasterState dynamicRasterState)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDDYNAMICRASTERSTATE);
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrect call to vkCmdBindDynamicRasterState() without an active RenderPass.");
            }
            loader_platform_thread_lock_mutex(&globalLock);
            pCB->status |= CBSTATUS_RASTER_BOUND;
            if (dynamicRsStateMap.find(dynamicRasterState.handle) == dynamicRsStateMap.end()) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DYNAMIC_RASTER_STATE, dynamicRasterState.handle, 0, DRAWSTATE_INVALID_DYNAMIC_STATE_OBJECT, "DS",
                        "Unable to find VkDynamicRasterState object %#" PRIxLEAST64 ", was it ever created?", dynamicRasterState.handle);
            } else {
                pCB->lastBoundDynamicState[VK_STATE_BIND_POINT_RASTER] = dynamicRasterState.handle;
                g_lastBoundDynamicState[VK_STATE_BIND_POINT_RASTER] = dynamicRasterState.handle;
            }
            loader_platform_thread_unlock_mutex(&globalLock);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindDynamicRasterState()");
        }
    }
}
VK_LAYER_EXPORT void VKAPI vkCmdBindDynamicColorBlendState(VkCmdBuffer cmdBuffer, VkDynamicColorBlendState dynamicColorBlendState)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDDYNAMICCOLORBLENDSTATE);
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrect call to vkCmdBindDynamicColorBlendState() without an active RenderPass.");
            }
            loader_platform_thread_lock_mutex(&globalLock);
            pCB->status |= CBSTATUS_COLOR_BLEND_BOUND;
            if (dynamicCbStateMap.find(dynamicColorBlendState.handle) == dynamicCbStateMap.end()) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DYNAMIC_COLOR_BLEND_STATE, dynamicColorBlendState.handle, 0, DRAWSTATE_INVALID_DYNAMIC_STATE_OBJECT, "DS",
                        "Unable to find VkDynamicColorBlendState object %#" PRIxLEAST64 ", was it ever created?", dynamicColorBlendState.handle);
            } else {
                pCB->lastBoundDynamicState[VK_STATE_BIND_POINT_COLOR_BLEND] = dynamicColorBlendState.handle;
                g_lastBoundDynamicState[VK_STATE_BIND_POINT_COLOR_BLEND] = dynamicColorBlendState.handle;
            }
            loader_platform_thread_unlock_mutex(&globalLock);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindDynamicColorBlendState()");
        }
    }
}
VK_LAYER_EXPORT void VKAPI vkCmdBindDynamicDepthStencilState(VkCmdBuffer cmdBuffer, VkDynamicDepthStencilState dynamicDepthStencilState)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDDYNAMICDEPTHSTENCILSTATE);
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrect call to vkCmdBindDynamicDepthStencilState() without an active RenderPass.");
            }
            loader_platform_thread_lock_mutex(&globalLock);
            pCB->status |= CBSTATUS_DEPTH_STENCIL_BOUND;
            if (dynamicDsStateMap.find(dynamicDepthStencilState.handle) == dynamicDsStateMap.end()) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DYNAMIC_DEPTH_STENCIL_STATE, dynamicDepthStencilState.handle, 0, DRAWSTATE_INVALID_DYNAMIC_STATE_OBJECT, "DS",
                        "Unable to find VkDynamicDepthStencilState object %#" PRIxLEAST64 ", was it ever created?", dynamicDepthStencilState.handle);
            } else {
                pCB->lastBoundDynamicState[VK_STATE_BIND_POINT_DEPTH_STENCIL] = dynamicDepthStencilState.handle;
                g_lastBoundDynamicState[VK_STATE_BIND_POINT_DEPTH_STENCIL] = dynamicDepthStencilState.handle;
            }
            loader_platform_thread_unlock_mutex(&globalLock);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindDynamicDepthStencilState()");
        }
    }
}
VK_LAYER_EXPORT void VKAPI vkCmdBindDescriptorSets(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDDESCRIPTORSETS);
            if ((VK_PIPELINE_BIND_POINT_COMPUTE == pipelineBindPoint) && (pCB->activeRenderPass)) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_RENDERPASS_CMD, "DS",
                        "Incorrectly binding compute DescriptorSets during active RenderPass (%#" PRIxLEAST64 ")", pCB->activeRenderPass.handle);
            } else if ((VK_PIPELINE_BIND_POINT_GRAPHICS == pipelineBindPoint) && (!pCB->activeRenderPass)) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrectly binding graphics DescriptorSets without an active RenderPass");
            } else if (validateBoundPipeline(cmdBuffer)) {
                for (uint32_t i=0; i<setCount; i++) {
                    SET_NODE* pSet = getSetNode(pDescriptorSets[i]);
                    if (pSet) {
                        loader_platform_thread_lock_mutex(&globalLock);
                        pCB->lastBoundDescriptorSet = pDescriptorSets[i];
                        pCB->lastBoundPipelineLayout = layout;
                        pCB->boundDescriptorSets.push_back(pDescriptorSets[i]);
                        g_lastBoundDescriptorSet = pDescriptorSets[i];
                        loader_platform_thread_unlock_mutex(&globalLock);
                        log_msg(mdd(cmdBuffer), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, pDescriptorSets[i].handle, 0, DRAWSTATE_NONE, "DS",
                                "DS %#" PRIxLEAST64 " bound on pipeline %s", pDescriptorSets[i].handle, string_VkPipelineBindPoint(pipelineBindPoint));
                        if (!pSet->pUpdateStructs)
                            log_msg(mdd(cmdBuffer), VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, pDescriptorSets[i].handle, 0, DRAWSTATE_DESCRIPTOR_SET_NOT_UPDATED, "DS",
                                    "DS %#" PRIxLEAST64 " bound but it was never updated. You may want to either update it or not bind it.", pDescriptorSets[i].handle);
                    } else {
                        log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_DESCRIPTOR_SET, pDescriptorSets[i].handle, 0, DRAWSTATE_INVALID_SET, "DS",
                                "Attempt to bind DS %#" PRIxLEAST64 " that doesn't exist!", pDescriptorSets[i].handle);
                    }
                }
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindDescriptorSets()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdBindIndexBuffer(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDINDEXBUFFER);
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrect call to vkCmdBindIndexBuffer() without an active RenderPass.");
            } else {
                // TODO : Can be more exact in tracking/validating details for Idx buffer, for now just make sure *something* was bound
                pCB->status |= CBSTATUS_INDEX_BUFFER_BOUND;
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            /* TODO: Need to track all the vertex buffers, not just last one */
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BINDVERTEXBUFFER);
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Incorrect call to vkCmdBindVertexBuffers() without an active RenderPass.");
            } else {
                pCB->lastVtxBinding = startBinding + bindingCount -1;
                if (validateBoundPipeline(cmdBuffer)) {
                    get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);
                }
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdDraw(VkCmdBuffer cmdBuffer, uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    VkBool32 valid = VK_FALSE;
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_DRAW);
            pCB->drawCount[DRAW]++;
            valid = validate_draw_state(pCB, VK_FALSE);
            // TODO : Need to pass cmdBuffer as srcObj here
            log_msg(mdd(cmdBuffer), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_NONE, "DS",
                    "vkCmdDraw() call #%lu, reporting DS state:", g_drawCount[DRAW]++);
            synchAndPrintDSConfig(cmdBuffer);
            if (valid) {
               get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdDraw(cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdDrawIndexed(VkCmdBuffer cmdBuffer, uint32_t firstIndex, uint32_t indexCount, int32_t vertexOffset, uint32_t firstInstance, uint32_t instanceCount)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    VkBool32 valid = VK_FALSE;
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_DRAWINDEXED);
            pCB->drawCount[DRAW_INDEXED]++;
            valid = validate_draw_state(pCB, VK_TRUE);
            // TODO : Need to pass cmdBuffer as srcObj here
            log_msg(mdd(cmdBuffer), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_NONE, "DS",
                    "vkCmdDrawIndexed() call #%lu, reporting DS state:", g_drawCount[DRAW_INDEXED]++);
            synchAndPrintDSConfig(cmdBuffer);
            if (valid) {
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdDrawIndexed(cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdDrawIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    VkBool32 valid = VK_FALSE;
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_DRAWINDIRECT);
            pCB->drawCount[DRAW_INDIRECT]++;
            valid = validate_draw_state(pCB, VK_FALSE);
            // TODO : Need to pass cmdBuffer as srcObj here
            log_msg(mdd(cmdBuffer), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_NONE, "DS",
                    "vkCmdDrawIndirect() call #%lu, reporting DS state:", g_drawCount[DRAW_INDIRECT]++);
            synchAndPrintDSConfig(cmdBuffer);
            if (valid) {
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdDrawIndirect(cmdBuffer, buffer, offset, count, stride);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdDrawIndexedIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    VkBool32 valid = VK_FALSE;
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_DRAWINDEXEDINDIRECT);
            pCB->drawCount[DRAW_INDEXED_INDIRECT]++;
            valid = validate_draw_state(pCB, VK_TRUE);
            // TODO : Need to pass cmdBuffer as srcObj here
            log_msg(mdd(cmdBuffer), VK_DBG_REPORT_INFO_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_NONE, "DS",
                    "vkCmdDrawIndexedIndirect() call #%lu, reporting DS state:", g_drawCount[DRAW_INDEXED_INDIRECT]++);
            synchAndPrintDSConfig(cmdBuffer);
            if (valid) {
               get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdDrawIndexedIndirect(cmdBuffer, buffer, offset, count, stride);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdDispatch(VkCmdBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_DISPATCH);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdDispatch(cmdBuffer, x, y, z);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdDispatchIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_DISPATCHINDIRECT);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdDispatchIndirect(cmdBuffer, buffer, offset);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyBuffer(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_COPYBUFFER);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyImage(VkCmdBuffer cmdBuffer,
                                             VkImage srcImage,
                                             VkImageLayout srcImageLayout,
                                             VkImage destImage,
                                             VkImageLayout destImageLayout,
                                             uint32_t regionCount, const VkImageCopy* pRegions)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_COPYIMAGE);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdCopyImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdBlitImage(VkCmdBuffer cmdBuffer,
                                             VkImage srcImage, VkImageLayout srcImageLayout,
                                             VkImage destImage, VkImageLayout destImageLayout,
                                             uint32_t regionCount, const VkImageBlit* pRegions,
                                             VkTexFilter filter)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BLITIMAGE);
            if (pCB->activeRenderPass) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_RENDERPASS_CMD, "DS",
                        "Incorrectly issuing CmdBlitImage during active RenderPass (%#" PRIxLEAST64 ")", pCB->activeRenderPass.handle);
            }
            else
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBlitImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyBufferToImage(VkCmdBuffer cmdBuffer,
                                                     VkBuffer srcBuffer,
                                                     VkImage destImage, VkImageLayout destImageLayout,
                                                     uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_COPYBUFFERTOIMAGE);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyImageToBuffer(VkCmdBuffer cmdBuffer,
                                                     VkImage srcImage, VkImageLayout srcImageLayout,
                                                     VkBuffer destBuffer,
                                                     uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_COPYIMAGETOBUFFER);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdUpdateBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_UPDATEBUFFER);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdUpdateBuffer(cmdBuffer, destBuffer, destOffset, dataSize, pData);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdFillBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_FILLBUFFER);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdFillBuffer(cmdBuffer, destBuffer, destOffset, fillSize, data);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdClearColorAttachment(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    colorAttachment,
    VkImageLayout                               imageLayout,
    const VkClearColorValue*                    pColor,
    uint32_t                                    rectCount,
    const VkRect3D*                             pRects)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            // Warn if this is issued prior to Draw Cmd
            if (!hasDrawCmd(pCB)) {
                // TODO : cmdBuffer should be srcObj
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_CLEAR_CMD_BEFORE_DRAW, "DS",
                        "vkCmdClearColorAttachment() issued on CB object 0x%" PRIxLEAST64 " prior to any Draw Cmds."
                        " It is recommended you use RenderPass LOAD_OP_CLEAR on Color Attachments prior to any Draw.", reinterpret_cast<VkUintPtrLeast64>(cmdBuffer));
            }
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Clear*Attachment cmd issued without an active RenderPass. vkCmdClearColorAttachment() must only be called inside of a RenderPass."
                        " vkCmdClearColorImage() should be used outside of a RenderPass.");
            } else {
                updateCBTracking(cmdBuffer);
                addCmd(pCB, CMD_CLEARCOLORATTACHMENT);
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdClearColorAttachment(cmdBuffer, colorAttachment, imageLayout, pColor, rectCount, pRects);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdClearDepthStencilAttachment(
    VkCmdBuffer                                 cmdBuffer,
    VkImageAspectFlags                          imageAspectMask,
    VkImageLayout                               imageLayout,
    float                                       depth,
    uint32_t                                    stencil,
    uint32_t                                    rectCount,
    const VkRect3D*                             pRects)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            // Warn if this is issued prior to Draw Cmd
            if (!hasDrawCmd(pCB)) {
                // TODO : cmdBuffer should be srcObj
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_WARN_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_CLEAR_CMD_BEFORE_DRAW, "DS",
                        "vkCmdClearDepthStencilAttachment() issued on CB object 0x%" PRIxLEAST64 " prior to any Draw Cmds."
                        " It is recommended you use RenderPass LOAD_OP_CLEAR on DS Attachment prior to any Draw.", reinterpret_cast<VkUintPtrLeast64>(cmdBuffer));
            }
            if (!pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                        "Clear*Attachment cmd issued without an active RenderPass. vkCmdClearDepthStencilAttachment() must only be called inside of a RenderPass."
                        " vkCmdClearDepthStencilImage() should be used outside of a RenderPass.");
            } else {
                updateCBTracking(cmdBuffer);
                addCmd(pCB, CMD_CLEARDEPTHSTENCILATTACHMENT);
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdClearDepthStencilAttachment(cmdBuffer, imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdClearColorImage(
        VkCmdBuffer cmdBuffer,
        VkImage image, VkImageLayout imageLayout,
        const VkClearColorValue *pColor,
        uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            if (pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_RENDERPASS_CMD, "DS",
                        "Clear*Image cmd issued with an active RenderPass. vkCmdClearColorImage() must only be called outside of a RenderPass."
                        " vkCmdClearColorAttachment() should be used within a RenderPass.");
            } else {
                updateCBTracking(cmdBuffer);
                addCmd(pCB, CMD_CLEARCOLORIMAGE);
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdClearColorImage(cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdClearDepthStencilImage(VkCmdBuffer cmdBuffer,
                                                     VkImage image, VkImageLayout imageLayout,
                                                     float depth, uint32_t stencil,
                                                     uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            if (pCB->activeRenderPass) {
                log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_RENDERPASS_CMD, "DS",
                        "Clear*Image cmd issued with an active RenderPass. vkCmdClearDepthStencilImage() must only be called outside of a RenderPass."
                        " vkCmdClearDepthStencilAttachment() should be used within a RenderPass.");
            } else {
                updateCBTracking(cmdBuffer);
                addCmd(pCB, CMD_CLEARDEPTHSTENCILIMAGE);
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdClearDepthStencilImage(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdResolveImage(VkCmdBuffer cmdBuffer,
                                                VkImage srcImage, VkImageLayout srcImageLayout,
                                                VkImage destImage, VkImageLayout destImageLayout,
                                                uint32_t regionCount, const VkImageResolve* pRegions)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            if (pCB->activeRenderPass) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_RENDERPASS_CMD, "DS",
                        "Cannot call vkCmdResolveImage() during an active RenderPass (%#" PRIxLEAST64 ").", pCB->activeRenderPass.handle);
            } else {
                updateCBTracking(cmdBuffer);
                addCmd(pCB, CMD_RESOLVEIMAGE);
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdResolveImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
            }
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdSetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_SETEVENT);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdSetEvent(cmdBuffer, event, stageMask);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdResetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_RESETEVENT);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdResetEvent(cmdBuffer, event, stageMask);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdWaitEvents(VkCmdBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags sourceStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void** ppMemBarriers)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_WAITEVENTS);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdWaitEvents(cmdBuffer, eventCount, pEvents, sourceStageMask, destStageMask, memBarrierCount, ppMemBarriers);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBindIndexBuffer()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdPipelineBarrier(VkCmdBuffer cmdBuffer, VkPipelineStageFlags sourceStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, uint32_t memBarrierCount, const void** ppMemBarriers)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_PIPELINEBARRIER);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdPipelineBarrier(cmdBuffer, sourceStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdPipelineBarrier()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdBeginQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkFlags flags)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_BEGINQUERY);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBeginQuery(cmdBuffer, queryPool, slot, flags);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdBeginQuery()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdEndQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_ENDQUERY);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdEndQuery(cmdBuffer, queryPool, slot);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdEndQuery()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdResetQueryPool(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_RESETQUERYPOOL);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdResetQueryPool(cmdBuffer, queryPool, startQuery, queryCount);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdResetQueryPool()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdWriteTimestamp(VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pCB->state == CB_UPDATE_ACTIVE) {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_WRITETIMESTAMP);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdWriteTimestamp(cmdBuffer, timestampType, destBuffer, destOffset);
        } else {
            report_error_no_cb_begin(cmdBuffer, "vkCmdWriteTimestamp()");
        }
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateFramebuffer(device, pCreateInfo, pFramebuffer);
    if (VK_SUCCESS == result) {
        // Shadow create info and store in map
        VkFramebufferCreateInfo* localFBCI = new VkFramebufferCreateInfo(*pCreateInfo);
        if (pCreateInfo->pAttachments) {
            localFBCI->pAttachments = new VkAttachmentBindInfo[localFBCI->attachmentCount];
            memcpy((void*)localFBCI->pAttachments, pCreateInfo->pAttachments, localFBCI->attachmentCount*sizeof(VkAttachmentBindInfo));
        }
        frameBufferMap[pFramebuffer->handle] = localFBCI;
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass)
{
    VkResult result = get_dispatch_table(draw_state_device_table_map, device)->CreateRenderPass(device, pCreateInfo, pRenderPass);
    if (VK_SUCCESS == result) {
        // Shadow create info and store in map
        VkRenderPassCreateInfo* localRPCI = new VkRenderPassCreateInfo(*pCreateInfo);
        if (pCreateInfo->pAttachments) {
            localRPCI->pAttachments = new VkAttachmentDescription[localRPCI->attachmentCount];
            memcpy((void*)localRPCI->pAttachments, pCreateInfo->pAttachments, localRPCI->attachmentCount*sizeof(VkAttachmentDescription));
        }
        if (pCreateInfo->pSubpasses) {
            localRPCI->pSubpasses = new VkSubpassDescription[localRPCI->subpassCount];
            memcpy((void*)localRPCI->pSubpasses, pCreateInfo->pSubpasses, localRPCI->subpassCount*sizeof(VkSubpassDescription));

            for (uint32_t i = 0; i < localRPCI->subpassCount; i++) {
                VkSubpassDescription *subpass = (VkSubpassDescription *) &localRPCI->pSubpasses[i];
                const uint32_t attachmentCount = subpass->inputCount +
                    subpass->colorCount * (1 + (bool) subpass->resolveAttachments) +
                    subpass->preserveCount;
                VkAttachmentReference *attachments = new VkAttachmentReference[attachmentCount];

                memcpy(attachments, subpass->inputAttachments,
                        sizeof(attachments[0]) * subpass->inputCount);
                subpass->inputAttachments = attachments;
                attachments += subpass->inputCount;

                memcpy(attachments, subpass->colorAttachments,
                        sizeof(attachments[0]) * subpass->colorCount);
                subpass->colorAttachments = attachments;
                attachments += subpass->colorCount;

                if (subpass->resolveAttachments) {
                    memcpy(attachments, subpass->resolveAttachments,
                            sizeof(attachments[0]) * subpass->colorCount);
                    subpass->resolveAttachments = attachments;
                    attachments += subpass->colorCount;
                }

                memcpy(attachments, subpass->preserveAttachments,
                        sizeof(attachments[0]) * subpass->preserveCount);
                subpass->preserveAttachments = attachments;
            }
        }
        if (pCreateInfo->pDependencies) {
            localRPCI->pDependencies = new VkSubpassDependency[localRPCI->dependencyCount];
            memcpy((void*)localRPCI->pDependencies, pCreateInfo->pDependencies, localRPCI->dependencyCount*sizeof(VkSubpassDependency));
        }
        renderPassMap[pRenderPass->handle] = localRPCI;
    }
    return result;
}

VK_LAYER_EXPORT void VKAPI vkCmdBeginRenderPass(VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo *pRenderPassBegin, VkRenderPassContents contents)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (pRenderPassBegin && pRenderPassBegin->renderPass) {
            if (pCB->activeRenderPass) {
                log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_RENDERPASS_CMD, "DS",
                        "Cannot call vkCmdBeginRenderPass() during an active RenderPass (%#" PRIxLEAST64 "). You must first call vkCmdEndRenderPass().", pCB->activeRenderPass.handle);
            } else {
                updateCBTracking(cmdBuffer);
                addCmd(pCB, CMD_BEGINRENDERPASS);
                pCB->activeRenderPass = pRenderPassBegin->renderPass;
                pCB->activeSubpass = 0;
                pCB->framebuffer = pRenderPassBegin->framebuffer;
                if (pCB->lastBoundPipeline) {
                    validatePipelineState(pCB, VK_PIPELINE_BIND_POINT_GRAPHICS, pCB->lastBoundPipeline);
                }
                get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);
            }
        } else {
            log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_INVALID_RENDERPASS, "DS",
                    "You cannot use a NULL RenderPass object in vkCmdBeginRenderPass()");
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdNextSubpass(VkCmdBuffer cmdBuffer, VkRenderPassContents contents)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (!pCB->activeRenderPass) {
            log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                    "Incorrect call to vkCmdNextSubpass() without an active RenderPass.");
        } else {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_NEXTSUBPASS);
            pCB->activeSubpass++;
            if (pCB->lastBoundPipeline) {
                validatePipelineState(pCB, VK_PIPELINE_BIND_POINT_GRAPHICS, pCB->lastBoundPipeline);
            }
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdNextSubpass(cmdBuffer, contents);
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdEndRenderPass(VkCmdBuffer cmdBuffer)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (!pCB->activeRenderPass) {
            log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                    "Incorrect call to vkCmdEndRenderPass() without an active RenderPass.");
        } else {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_ENDRENDERPASS);
            pCB->activeRenderPass = 0;
            pCB->activeSubpass = 0;
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdEndRenderPass(cmdBuffer);
        }
    }
}

VK_LAYER_EXPORT void VKAPI vkCmdExecuteCommands(VkCmdBuffer cmdBuffer, uint32_t cmdBuffersCount, const VkCmdBuffer* pCmdBuffers)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    if (pCB) {
        if (!pCB->activeRenderPass) {
            log_msg(mdd(pCB->cmdBuffer), VK_DBG_REPORT_ERROR_BIT, (VkDbgObjectType) 0, 0, 0, DRAWSTATE_NO_ACTIVE_RENDERPASS, "DS",
                    "Incorrect call to vkCmdExecuteCommands() without an active RenderPass.");
        } else {
            updateCBTracking(cmdBuffer);
            addCmd(pCB, CMD_EXECUTECOMMANDS);
            get_dispatch_table(draw_state_device_table_map, cmdBuffer)->CmdExecuteCommands(cmdBuffer, cmdBuffersCount, pCmdBuffers);
        }
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgCreateMsgCallback(
    VkInstance                          instance,
    VkFlags                             msgFlags,
    const PFN_vkDbgMsgCallback          pfnMsgCallback,
    void*                               pUserData,
    VkDbgMsgCallback*                   pMsgCallback)
{
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(draw_state_instance_table_map, instance);
    VkResult res = pTable->DbgCreateMsgCallback(instance, msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
    if (VK_SUCCESS == res) {
        layer_data *my_data = get_my_data_ptr(get_dispatch_key(instance), layer_data_map);
        res = layer_create_msg_callback(my_data->report_data, msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
    }
    return res;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgDestroyMsgCallback(
    VkInstance                          instance,
    VkDbgMsgCallback                    msgCallback)
{
    VkLayerInstanceDispatchTable *pTable = get_dispatch_table(draw_state_instance_table_map, instance);
    VkResult res = pTable->DbgDestroyMsgCallback(instance, msgCallback);
    layer_data *my_data = get_my_data_ptr(get_dispatch_key(instance), layer_data_map);
    layer_destroy_msg_callback(my_data->report_data, msgCallback);
    return res;
}

VK_LAYER_EXPORT void VKAPI vkCmdDbgMarkerBegin(VkCmdBuffer cmdBuffer, const char* pMarker)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    VkLayerDispatchTable *pDisp =  *(VkLayerDispatchTable **) cmdBuffer;
    if (!deviceExtMap[pDisp].debug_marker_enabled) {
        // TODO : cmdBuffer should be srcObj
        log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_INVALID_EXTENSION, "DS",
                "Attempt to use CmdDbgMarkerBegin but extension disabled!");
        return;
    } else if (pCB) {
        updateCBTracking(cmdBuffer);
        addCmd(pCB, CMD_DBGMARKERBEGIN);
    }
    debug_marker_dispatch_table(cmdBuffer)->CmdDbgMarkerBegin(cmdBuffer, pMarker);
}

VK_LAYER_EXPORT void VKAPI vkCmdDbgMarkerEnd(VkCmdBuffer cmdBuffer)
{
    GLOBAL_CB_NODE* pCB = getCBNode(cmdBuffer);
    VkLayerDispatchTable *pDisp =  *(VkLayerDispatchTable **) cmdBuffer;
    if (!deviceExtMap[pDisp].debug_marker_enabled) {
        // TODO : cmdBuffer should be srcObj
        log_msg(mdd(cmdBuffer), VK_DBG_REPORT_ERROR_BIT, VK_OBJECT_TYPE_COMMAND_BUFFER, 0, 0, DRAWSTATE_INVALID_EXTENSION, "DS",
                "Attempt to use CmdDbgMarkerEnd but extension disabled!");
        return;
    } else if (pCB) {
        updateCBTracking(cmdBuffer);
        addCmd(pCB, CMD_DBGMARKEREND);
    }
    debug_marker_dispatch_table(cmdBuffer)->CmdDbgMarkerEnd(cmdBuffer);
}

//VK_LAYER_EXPORT VkResult VKAPI vkDbgSetObjectTag(VkDevice device, VkObjectType  objType, VkObject object, size_t tagSize, const void* pTag)
//{
//    VkLayerDispatchTable *pDisp  =  *(VkLayerDispatchTable **) device;
//    if (!deviceExtMap[pDisp].debug_marker_enabled) {
//        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, objType, object, 0, DRAWSTATE_INVALID_EXTENSION, "DS",
//                "Attempt to use DbgSetObjectTag but extension disabled!");
//        return VK_ERROR_UNAVAILABLE;
//    }
//    debug_marker_dispatch_table(device)->DbgSetObjectTag(device, objType, object, tagSize, pTag);
//}
//
//VK_LAYER_EXPORT VkResult VKAPI vkDbgSetObjectName(VkDevice device, VkObjectType  objType, VkObject object, size_t nameSize, const char* pName)
//{
//    VkLayerDispatchTable *pDisp  =  *(VkLayerDispatchTable **) device;
//    if (!deviceExtMap[pDisp].debug_marker_enabled) {
//        log_msg(mdd(device), VK_DBG_REPORT_ERROR_BIT, objType, object, 0, DRAWSTATE_INVALID_EXTENSION, "DS",
//                "Attempt to use DbgSetObjectName but extension disabled!");
//        return VK_ERROR_UNAVAILABLE;
//    }
//    debug_marker_dispatch_table(device)->DbgSetObjectName(device, objType, object, nameSize, pName);
//}

VK_LAYER_EXPORT void* VKAPI vkGetDeviceProcAddr(VkDevice dev, const char* funcName)
{
    if (dev == NULL)
        return NULL;

    /* loader uses this to force layer initialization; device object is wrapped */
    if (!strcmp(funcName, "vkGetDeviceProcAddr")) {
        initDeviceTable(draw_state_device_table_map, (const VkBaseLayerObject *) dev);
        return (void *) vkGetDeviceProcAddr;
    }
    if (!strcmp(funcName, "vkCreateDevice"))
        return (void*) vkCreateDevice;
    if (!strcmp(funcName, "vkDestroyDevice"))
        return (void*) vkDestroyDevice;
    if (!strcmp(funcName, "vkQueueSubmit"))
        return (void*) vkQueueSubmit;
    if (!strcmp(funcName, "vkDestroyInstance"))
        return (void*) vkDestroyInstance;
    if (!strcmp(funcName, "vkDestroyDevice"))
        return (void*) vkDestroyDevice;
    if (!strcmp(funcName, "vkDestroyFence"))
        return (void*) vkDestroyFence;
    if (!strcmp(funcName, "vkDestroySemaphore"))
        return (void*) vkDestroySemaphore;
    if (!strcmp(funcName, "vkDestroyEvent"))
        return (void*) vkDestroyEvent;
    if (!strcmp(funcName, "vkDestroyQueryPool"))
        return (void*) vkDestroyQueryPool;
    if (!strcmp(funcName, "vkDestroyBuffer"))
        return (void*) vkDestroyBuffer;
    if (!strcmp(funcName, "vkDestroyBufferView"))
        return (void*) vkDestroyBufferView;
    if (!strcmp(funcName, "vkDestroyImage"))
        return (void*) vkDestroyImage;
    if (!strcmp(funcName, "vkDestroyImageView"))
        return (void*) vkDestroyImageView;
    if (!strcmp(funcName, "vkDestroyAttachmentView"))
        return (void*) vkDestroyAttachmentView;
    if (!strcmp(funcName, "vkDestroyShaderModule"))
        return (void*) vkDestroyShaderModule;
    if (!strcmp(funcName, "vkDestroyShader"))
        return (void*) vkDestroyShader;
    if (!strcmp(funcName, "vkDestroyPipeline"))
        return (void*) vkDestroyPipeline;
    if (!strcmp(funcName, "vkDestroyPipelineLayout"))
        return (void*) vkDestroyPipelineLayout;
    if (!strcmp(funcName, "vkDestroySampler"))
        return (void*) vkDestroySampler;
    if (!strcmp(funcName, "vkDestroyDescriptorSetLayout"))
        return (void*) vkDestroyDescriptorSetLayout;
    if (!strcmp(funcName, "vkDestroyDescriptorPool"))
        return (void*) vkDestroyDescriptorPool;
    if (!strcmp(funcName, "vkDestroyDynamicViewportState"))
        return (void*) vkDestroyDynamicViewportState;
    if (!strcmp(funcName, "vkDestroyDynamicRasterState"))
        return (void*) vkDestroyDynamicRasterState;
    if (!strcmp(funcName, "vkDestroyDynamicColorBlendState"))
        return (void*) vkDestroyDynamicColorBlendState;
    if (!strcmp(funcName, "vkDestroyDynamicDepthStencilState"))
        return (void*) vkDestroyDynamicDepthStencilState;
    if (!strcmp(funcName, "vkDestroyCommandBuffer"))
        return (void*) vkDestroyCommandBuffer;
    if (!strcmp(funcName, "vkDestroyFramebuffer"))
        return (void*) vkDestroyFramebuffer;
    if (!strcmp(funcName, "vkDestroyRenderPass"))
        return (void*) vkDestroyRenderPass;
    if (!strcmp(funcName, "vkCreateBufferView"))
        return (void*) vkCreateBufferView;
    if (!strcmp(funcName, "vkCreateImageView"))
        return (void*) vkCreateImageView;
    if (!strcmp(funcName, "vkCreateAttachmentView"))
        return (void*) vkCreateAttachmentView;
    if (!strcmp(funcName, "CreatePipelineCache"))
        return (void*) vkCreatePipelineCache;
    if (!strcmp(funcName, "DestroyPipelineCache"))
        return (void*) vkDestroyPipelineCache;
    if (!strcmp(funcName, "GetPipelineCacheSize"))
        return (void*) vkGetPipelineCacheSize;
    if (!strcmp(funcName, "GetPipelineCacheData"))
        return (void*) vkGetPipelineCacheData;
    if (!strcmp(funcName, "MergePipelineCaches"))
        return (void*) vkMergePipelineCaches;
    if (!strcmp(funcName, "vkCreateGraphicsPipelines"))
        return (void*) vkCreateGraphicsPipelines;
    if (!strcmp(funcName, "vkCreateSampler"))
        return (void*) vkCreateSampler;
    if (!strcmp(funcName, "vkCreateDescriptorSetLayout"))
        return (void*) vkCreateDescriptorSetLayout;
    if (!strcmp(funcName, "vkCreatePipelineLayout"))
        return (void*) vkCreatePipelineLayout;
    if (!strcmp(funcName, "vkCreateDescriptorPool"))
        return (void*) vkCreateDescriptorPool;
    if (!strcmp(funcName, "vkResetDescriptorPool"))
        return (void*) vkResetDescriptorPool;
    if (!strcmp(funcName, "vkAllocDescriptorSets"))
        return (void*) vkAllocDescriptorSets;
    if (!strcmp(funcName, "vkUpdateDescriptorSets"))
        return (void*) vkUpdateDescriptorSets;
    if (!strcmp(funcName, "vkCreateDynamicViewportState"))
        return (void*) vkCreateDynamicViewportState;
    if (!strcmp(funcName, "vkCreateDynamicRasterState"))
        return (void*) vkCreateDynamicRasterState;
    if (!strcmp(funcName, "vkCreateDynamicColorBlendState"))
        return (void*) vkCreateDynamicColorBlendState;
    if (!strcmp(funcName, "vkCreateDynamicDepthStencilState"))
        return (void*) vkCreateDynamicDepthStencilState;
    if (!strcmp(funcName, "vkCreateCommandBuffer"))
        return (void*) vkCreateCommandBuffer;
    if (!strcmp(funcName, "vkBeginCommandBuffer"))
        return (void*) vkBeginCommandBuffer;
    if (!strcmp(funcName, "vkEndCommandBuffer"))
        return (void*) vkEndCommandBuffer;
    if (!strcmp(funcName, "vkResetCommandBuffer"))
        return (void*) vkResetCommandBuffer;
    if (!strcmp(funcName, "vkCmdBindPipeline"))
        return (void*) vkCmdBindPipeline;
    if (!strcmp(funcName, "vkCmdBindDynamicViewportState"))
        return (void*) vkCmdBindDynamicViewportState;
    if (!strcmp(funcName, "vkCmdBindDynamicRasterState"))
        return (void*) vkCmdBindDynamicRasterState;
    if (!strcmp(funcName, "vkCmdBindDynamicColorBlendState"))
        return (void*) vkCmdBindDynamicColorBlendState;
    if (!strcmp(funcName, "vkCmdBindDynamicDepthStencilState"))
        return (void*) vkCmdBindDynamicDepthStencilState;
    if (!strcmp(funcName, "vkCmdBindDescriptorSets"))
        return (void*) vkCmdBindDescriptorSets;
    if (!strcmp(funcName, "vkCmdBindVertexBuffers"))
        return (void*) vkCmdBindVertexBuffers;
    if (!strcmp(funcName, "vkCmdBindIndexBuffer"))
        return (void*) vkCmdBindIndexBuffer;
    if (!strcmp(funcName, "vkCmdDraw"))
        return (void*) vkCmdDraw;
    if (!strcmp(funcName, "vkCmdDrawIndexed"))
        return (void*) vkCmdDrawIndexed;
    if (!strcmp(funcName, "vkCmdDrawIndirect"))
        return (void*) vkCmdDrawIndirect;
    if (!strcmp(funcName, "vkCmdDrawIndexedIndirect"))
        return (void*) vkCmdDrawIndexedIndirect;
    if (!strcmp(funcName, "vkCmdDispatch"))
        return (void*) vkCmdDispatch;
    if (!strcmp(funcName, "vkCmdDispatchIndirect"))
        return (void*) vkCmdDispatchIndirect;
    if (!strcmp(funcName, "vkCmdCopyBuffer"))
        return (void*) vkCmdCopyBuffer;
    if (!strcmp(funcName, "vkCmdCopyImage"))
        return (void*) vkCmdCopyImage;
    if (!strcmp(funcName, "vkCmdCopyBufferToImage"))
        return (void*) vkCmdCopyBufferToImage;
    if (!strcmp(funcName, "vkCmdCopyImageToBuffer"))
        return (void*) vkCmdCopyImageToBuffer;
    if (!strcmp(funcName, "vkCmdUpdateBuffer"))
        return (void*) vkCmdUpdateBuffer;
    if (!strcmp(funcName, "vkCmdFillBuffer"))
        return (void*) vkCmdFillBuffer;
    if (!strcmp(funcName, "vkCmdClearColorImage"))
        return (void*) vkCmdClearColorImage;
    if (!strcmp(funcName, "vkCmdClearDepthStencilImage"))
        return (void*) vkCmdClearDepthStencilImage;
    if (!strcmp(funcName, "vkCmdClearColorAttachment"))
        return (void*) vkCmdClearColorAttachment;
    if (!strcmp(funcName, "vkCmdClearDepthStencilAttachment"))
        return (void*) vkCmdClearDepthStencilAttachment;
    if (!strcmp(funcName, "vkCmdResolveImage"))
        return (void*) vkCmdResolveImage;
    if (!strcmp(funcName, "vkCmdSetEvent"))
        return (void*) vkCmdSetEvent;
    if (!strcmp(funcName, "vkCmdResetEvent"))
        return (void*) vkCmdResetEvent;
    if (!strcmp(funcName, "vkCmdWaitEvents"))
        return (void*) vkCmdWaitEvents;
    if (!strcmp(funcName, "vkCmdPipelineBarrier"))
        return (void*) vkCmdPipelineBarrier;
    if (!strcmp(funcName, "vkCmdBeginQuery"))
        return (void*) vkCmdBeginQuery;
    if (!strcmp(funcName, "vkCmdEndQuery"))
        return (void*) vkCmdEndQuery;
    if (!strcmp(funcName, "vkCmdResetQueryPool"))
        return (void*) vkCmdResetQueryPool;
    if (!strcmp(funcName, "vkCmdWriteTimestamp"))
        return (void*) vkCmdWriteTimestamp;
    if (!strcmp(funcName, "vkCreateFramebuffer"))
        return (void*) vkCreateFramebuffer;
    if (!strcmp(funcName, "vkCreateRenderPass"))
        return (void*) vkCreateRenderPass;
    if (!strcmp(funcName, "vkCmdBeginRenderPass"))
        return (void*) vkCmdBeginRenderPass;
    if (!strcmp(funcName, "vkCmdNextSubpass"))
        return (void*) vkCmdNextSubpass;
    if (!strcmp(funcName, "vkCmdEndRenderPass"))
        return (void*) vkCmdEndRenderPass;

    VkLayerDispatchTable* pTable = get_dispatch_table(draw_state_device_table_map, dev);
    if (deviceExtMap.size() == 0 || deviceExtMap[pTable].debug_marker_enabled)
    {
        if (!strcmp(funcName, "vkCmdDbgMarkerBegin"))
            return (void*) vkCmdDbgMarkerBegin;
        if (!strcmp(funcName, "vkCmdDbgMarkerEnd"))
            return (void*) vkCmdDbgMarkerEnd;
//        if (!strcmp(funcName, "vkDbgSetObjectTag"))
//            return (void*) vkDbgSetObjectTag;
//        if (!strcmp(funcName, "vkDbgSetObjectName"))
//            return (void*) vkDbgSetObjectName;
    }
    {
        if (pTable->GetDeviceProcAddr == NULL)
            return NULL;
        return pTable->GetDeviceProcAddr(dev, funcName);
    }
}

VK_LAYER_EXPORT void * VKAPI vkGetInstanceProcAddr(VkInstance instance, const char* funcName)
{
    void *fptr;
    if (instance == NULL)
        return NULL;

    /* loader uses this to force layer initialization; instance object is wrapped */
    if (!strcmp(funcName, "vkGetInstanceProcAddr")) {
        initInstanceTable(draw_state_instance_table_map, (const VkBaseLayerObject *) instance);
        return (void *) vkGetInstanceProcAddr;
    }
    if (!strcmp(funcName, "vkCreateInstance"))
        return (void *) vkCreateInstance;
    if (!strcmp(funcName, "vkDestroyInstance"))
        return (void *) vkDestroyInstance;
    if (!strcmp(funcName, "vkGetGlobalLayerProperties"))
        return (void*) vkGetGlobalLayerProperties;
    if (!strcmp(funcName, "vkGetGlobalExtensionProperties"))
        return (void*) vkGetGlobalExtensionProperties;
    if (!strcmp(funcName, "vkGetPhysicalDeviceLayerProperties"))
        return (void*) vkGetPhysicalDeviceLayerProperties;
    if (!strcmp(funcName, "vkGetPhysicalDeviceExtensionProperties"))
        return (void*) vkGetPhysicalDeviceExtensionProperties;

    layer_data *my_data = get_my_data_ptr(get_dispatch_key(instance), layer_data_map);
    fptr = debug_report_get_instance_proc_addr(my_data->report_data, funcName);
    if (fptr)
        return fptr;

    {
        VkLayerInstanceDispatchTable* pTable = get_dispatch_table(draw_state_instance_table_map, instance);
        if (pTable->GetInstanceProcAddr == NULL)
            return NULL;
        return pTable->GetInstanceProcAddr(instance, funcName);
    }
}
