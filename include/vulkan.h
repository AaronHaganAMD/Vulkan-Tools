//
// File: vulkan.h
//
/*
** Copyright (c) 2014-2015 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

#ifndef __VULKAN_H__
#define __VULKAN_H__

#define VK_MAKE_VERSION(major, minor, patch) \
    ((major << 22) | (minor << 12) | patch)

#include "vk_platform.h"

// Vulkan API version supported by this file
#define VK_API_VERSION VK_MAKE_VERSION(0, 132, 1)

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*
***************************************************************************************************
*   Core Vulkan API
***************************************************************************************************
*/

#define VK_DEFINE_HANDLE(obj) typedef struct obj##_T* obj;


#if defined(__cplusplus)
    #if (_MSC_VER >= 1800 || __cplusplus >= 201103L)
        // The bool operator only works if there are no implicit conversions from an obj to
        // a bool-compatible type, which can then be used to unintentially violate type safety.
        // C++11 and above supports the "explicit" keyword on conversion operators to stop this
        // from happening. Otherwise users of C++ below C++11 won't get direct access to evaluating
        // the object handle as a bool in expressions like:
        //     if (obj) vkDestroy(obj);
        #define VK_NONDISP_HANDLE_OPERATOR_BOOL() explicit operator bool() const { return handle != 0; }
    #else
        #define VK_NONDISP_HANDLE_OPERATOR_BOOL()
    #endif
    #define VK_DEFINE_NONDISP_HANDLE(obj) \
        struct obj { \
            obj() { } \
            obj(uint64_t x) { handle = x; } \
            obj& operator =(uint64_t x) { handle = x; return *this; } \
            bool operator==(const obj& other) const { return handle == other.handle; } \
            bool operator!=(const obj& other) const { return handle != other.handle; } \
            bool operator!() const { return !handle; } \
            VK_NONDISP_HANDLE_OPERATOR_BOOL() \
            uint64_t handle; \
        };
#else
    #define VK_DEFINE_NONDISP_HANDLE(obj) typedef struct obj##_T { uint64_t handle; } obj;
#endif

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkQueue)
VK_DEFINE_HANDLE(VkCmdBuffer)
VK_DEFINE_NONDISP_HANDLE(VkFence)
VK_DEFINE_NONDISP_HANDLE(VkDeviceMemory)
VK_DEFINE_NONDISP_HANDLE(VkBuffer)
VK_DEFINE_NONDISP_HANDLE(VkImage)
VK_DEFINE_NONDISP_HANDLE(VkSemaphore)
VK_DEFINE_NONDISP_HANDLE(VkEvent)
VK_DEFINE_NONDISP_HANDLE(VkQueryPool)
VK_DEFINE_NONDISP_HANDLE(VkBufferView)
VK_DEFINE_NONDISP_HANDLE(VkImageView)
VK_DEFINE_NONDISP_HANDLE(VkAttachmentView)
VK_DEFINE_NONDISP_HANDLE(VkShaderModule)
VK_DEFINE_NONDISP_HANDLE(VkShader)
VK_DEFINE_NONDISP_HANDLE(VkPipelineCache)
VK_DEFINE_NONDISP_HANDLE(VkPipelineLayout)
VK_DEFINE_NONDISP_HANDLE(VkPipeline)
VK_DEFINE_NONDISP_HANDLE(VkDescriptorSetLayout)
VK_DEFINE_NONDISP_HANDLE(VkSampler)
VK_DEFINE_NONDISP_HANDLE(VkDescriptorPool)
VK_DEFINE_NONDISP_HANDLE(VkDescriptorSet)
VK_DEFINE_NONDISP_HANDLE(VkDynamicViewportState)
VK_DEFINE_NONDISP_HANDLE(VkDynamicRasterState)
VK_DEFINE_NONDISP_HANDLE(VkDynamicColorBlendState)
VK_DEFINE_NONDISP_HANDLE(VkDynamicDepthStencilState)
VK_DEFINE_NONDISP_HANDLE(VkRenderPass)
VK_DEFINE_NONDISP_HANDLE(VkFramebuffer)

#define VK_MAX_PHYSICAL_DEVICE_NAME 256
#define VK_MAX_EXTENSION_NAME       256
#define VK_MAX_DESCRIPTION          256
#define VK_MAX_MEMORY_TYPES         32
#define VK_MAX_MEMORY_HEAPS         16
#define VK_UUID_LENGTH              16
#define VK_LOD_CLAMP_NONE           MAX_FLOAT
#define VK_LAST_MIP_LEVEL           UINT32_MAX
#define VK_LAST_ARRAY_SLICE         UINT32_MAX


#define VK_WHOLE_SIZE           UINT64_MAX
#define VK_ATTACHMENT_UNUSED    UINT32_MAX

#define VK_TRUE  1
#define VK_FALSE 0

#define VK_NULL_HANDLE 0

// This macro defines INT_MAX in enumerations to force compilers to use 32 bits
// to represent them. This may or may not be necessary on some compilers. The
// option to compile it out may allow compilers that warn about missing enumerants
// in switch statements to be silenced.
// Using this macro is not needed for flag bit enums because those aren't used
// as storage type anywhere.
#define VK_MAX_ENUM(Prefix) VK_##Prefix##_MAX_ENUM = 0x7FFFFFFF

// This macro defines the BEGIN_RANGE, END_RANGE, NUM, and MAX_ENUM constants for
// the enumerations.
#define VK_ENUM_RANGE(Prefix, First, Last) \
    VK_##Prefix##_BEGIN_RANGE                               = VK_##Prefix##_##First, \
    VK_##Prefix##_END_RANGE                                 = VK_##Prefix##_##Last, \
    VK_NUM_##Prefix                                         = (VK_##Prefix##_END_RANGE - VK_##Prefix##_BEGIN_RANGE + 1), \
    VK_MAX_ENUM(Prefix)

// This is a helper macro to define the value of flag bit enum values.
#define VK_BIT(bit)     (1 << (bit))

// ------------------------------------------------------------------------------------------------
// Enumerations

typedef enum VkImageLayout_
{
    VK_IMAGE_LAYOUT_UNDEFINED                               = 0x00000000,   // Implicit layout an image is when its contents are undefined due to various reasons (e.g. right after creation)
    VK_IMAGE_LAYOUT_GENERAL                                 = 0x00000001,   // General layout when image can be used for any kind of access
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL                = 0x00000002,   // Optimal layout when image is only used for color attachment read/write
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL        = 0x00000003,   // Optimal layout when image is only used for depth/stencil attachment read/write
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL         = 0x00000004,   // Optimal layout when image is used for read only depth/stencil attachment and shader access
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                = 0x00000005,   // Optimal layout when image is used for read only shader access
    VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL                 = 0x00000006,   // Optimal layout when image is used only as source of transfer operations
    VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL            = 0x00000007,   // Optimal layout when image is used only as destination of transfer operations

    VK_ENUM_RANGE(IMAGE_LAYOUT, UNDEFINED, TRANSFER_DESTINATION_OPTIMAL)
} VkImageLayout;

typedef enum VkAttachmentLoadOp_
{
    VK_ATTACHMENT_LOAD_OP_LOAD                              = 0x00000000,
    VK_ATTACHMENT_LOAD_OP_CLEAR                             = 0x00000001,
    VK_ATTACHMENT_LOAD_OP_DONT_CARE                         = 0x00000002,

    VK_ENUM_RANGE(ATTACHMENT_LOAD_OP, LOAD, DONT_CARE)
} VkAttachmentLoadOp;

typedef enum VkAttachmentStoreOp_
{
    VK_ATTACHMENT_STORE_OP_STORE                            = 0x00000000,
    VK_ATTACHMENT_STORE_OP_DONT_CARE                        = 0x00000001,

    VK_ENUM_RANGE(ATTACHMENT_STORE_OP, STORE, DONT_CARE)
} VkAttachmentStoreOp;

typedef enum VkImageType_
{
    VK_IMAGE_TYPE_1D                                        = 0x00000000,
    VK_IMAGE_TYPE_2D                                        = 0x00000001,
    VK_IMAGE_TYPE_3D                                        = 0x00000002,

    VK_ENUM_RANGE(IMAGE_TYPE, 1D, 3D)
} VkImageType;

typedef enum VkImageTiling_
{
    VK_IMAGE_TILING_LINEAR                                  = 0x00000000,
    VK_IMAGE_TILING_OPTIMAL                                 = 0x00000001,

    VK_ENUM_RANGE(IMAGE_TILING, LINEAR, OPTIMAL)
} VkImageTiling;

typedef enum VkImageViewType_
{
    VK_IMAGE_VIEW_TYPE_1D                                   = 0x00000000,
    VK_IMAGE_VIEW_TYPE_2D                                   = 0x00000001,
    VK_IMAGE_VIEW_TYPE_3D                                   = 0x00000002,
    VK_IMAGE_VIEW_TYPE_CUBE                                 = 0x00000003,
    VK_IMAGE_VIEW_TYPE_1D_ARRAY                             = 0x00000004,
    VK_IMAGE_VIEW_TYPE_2D_ARRAY                             = 0x00000005,
    VK_IMAGE_VIEW_TYPE_CUBE_ARRAY                           = 0x00000006,

    VK_ENUM_RANGE(IMAGE_VIEW_TYPE, 1D, CUBE_ARRAY)
} VkImageViewType;

typedef enum VkImageAspect_
{
    VK_IMAGE_ASPECT_COLOR                                   = 0x00000000,
    VK_IMAGE_ASPECT_DEPTH                                   = 0x00000001,
    VK_IMAGE_ASPECT_STENCIL                                 = 0x00000002,
    VK_IMAGE_ASPECT_METADATA                                = 0x00000003,

    VK_ENUM_RANGE(IMAGE_ASPECT, COLOR, METADATA)
} VkImageAspect;

typedef enum VkBufferViewType_
{
    VK_BUFFER_VIEW_TYPE_RAW                                 = 0x00000000,   // Raw buffer without special structure (UBO, SSBO)
    VK_BUFFER_VIEW_TYPE_FORMATTED                           = 0x00000001,   // Buffer with format (TBO, IBO)

    VK_ENUM_RANGE(BUFFER_VIEW_TYPE, RAW, FORMATTED)
} VkBufferViewType;

typedef enum VkChannelSwizzle_
{
    VK_CHANNEL_SWIZZLE_ZERO                                 = 0x00000000,
    VK_CHANNEL_SWIZZLE_ONE                                  = 0x00000001,
    VK_CHANNEL_SWIZZLE_R                                    = 0x00000002,
    VK_CHANNEL_SWIZZLE_G                                    = 0x00000003,
    VK_CHANNEL_SWIZZLE_B                                    = 0x00000004,
    VK_CHANNEL_SWIZZLE_A                                    = 0x00000005,

    VK_ENUM_RANGE(CHANNEL_SWIZZLE, ZERO, A)
} VkChannelSwizzle;

typedef enum VkDescriptorType_
{
    VK_DESCRIPTOR_TYPE_SAMPLER                              = 0x00000000,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER               = 0x00000001,
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE                        = 0x00000002,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE                        = 0x00000003,
    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER                 = 0x00000004,
    VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER                 = 0x00000005,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER                       = 0x00000006,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER                       = 0x00000007,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC               = 0x00000008,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC               = 0x00000009,

    VK_ENUM_RANGE(DESCRIPTOR_TYPE, SAMPLER, STORAGE_BUFFER_DYNAMIC)
} VkDescriptorType;

typedef enum VkDescriptorPoolUsage_
{
    VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT                       = 0x00000000,
    VK_DESCRIPTOR_POOL_USAGE_DYNAMIC                        = 0x00000001,

    VK_ENUM_RANGE(DESCRIPTOR_POOL_USAGE, ONE_SHOT, DYNAMIC)
} VkDescriptorPoolUsage;

typedef enum VkDescriptorSetUsage_
{
    VK_DESCRIPTOR_SET_USAGE_ONE_SHOT                        = 0x00000000,
    VK_DESCRIPTOR_SET_USAGE_STATIC                          = 0x00000001,

    VK_ENUM_RANGE(DESCRIPTOR_SET_USAGE, ONE_SHOT, STATIC)
} VkDescriptorSetUsage;

typedef enum VkQueryType_
{
    VK_QUERY_TYPE_OCCLUSION                                 = 0x00000000,
    VK_QUERY_TYPE_PIPELINE_STATISTICS                       = 0x00000001, // Optional

    VK_ENUM_RANGE(QUERY_TYPE, OCCLUSION, PIPELINE_STATISTICS)
} VkQueryType;

typedef enum VkTimestampType_
{
    VK_TIMESTAMP_TYPE_TOP                                   = 0x00000000,
    VK_TIMESTAMP_TYPE_BOTTOM                                = 0x00000001,

    VK_ENUM_RANGE(TIMESTAMP_TYPE, TOP, BOTTOM)
} VkTimestampType;

typedef enum VkRenderPassContents_
{
    VK_RENDER_PASS_CONTENTS_INLINE                          = 0x00000000,
    VK_RENDER_PASS_CONTENTS_SECONDARY_CMD_BUFFERS           = 0x00000001,

    VK_ENUM_RANGE(RENDER_PASS_CONTENTS, INLINE, SECONDARY_CMD_BUFFERS)
} VkRenderPassContents;

typedef enum VkBorderColor_
{
    VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK                 = 0x00000000,
    VK_BORDER_COLOR_INT_TRANSPARENT_BLACK                   = 0x00000001,
    VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK                      = 0x00000002,
    VK_BORDER_COLOR_INT_OPAQUE_BLACK                        = 0x00000003,
    VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE                      = 0x00000004,
    VK_BORDER_COLOR_INT_OPAQUE_WHITE                        = 0x00000005,

    VK_ENUM_RANGE(BORDER_COLOR, FLOAT_TRANSPARENT_BLACK, INT_OPAQUE_WHITE)
} VkBorderColor;

typedef enum VkPipelineBindPoint_
{
    VK_PIPELINE_BIND_POINT_COMPUTE                          = 0x00000000,
    VK_PIPELINE_BIND_POINT_GRAPHICS                         = 0x00000001,

    VK_ENUM_RANGE(PIPELINE_BIND_POINT, COMPUTE, GRAPHICS)
} VkPipelineBindPoint;

typedef enum VkPrimitiveTopology_
{
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST                        = 0x00000000,
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST                         = 0x00000001,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                        = 0x00000002,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST                     = 0x00000003,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP                    = 0x00000004,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN                      = 0x00000005,
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST_ADJ                     = 0x00000006,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_ADJ                    = 0x00000007,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_ADJ                 = 0x00000008,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_ADJ                = 0x00000009,
    VK_PRIMITIVE_TOPOLOGY_PATCH                             = 0x0000000a,

    VK_ENUM_RANGE(PRIMITIVE_TOPOLOGY, POINT_LIST, PATCH)
} VkPrimitiveTopology;

typedef enum VkCmdBufferLevel_
{
    VK_CMD_BUFFER_LEVEL_PRIMARY                             = 0x00000000,
    VK_CMD_BUFFER_LEVEL_SECONDARY                           = 0x00000001,

    VK_ENUM_RANGE(CMD_BUFFER_LEVEL, PRIMARY, SECONDARY)
} VkCmdBufferLevel;

typedef enum VkIndexType_
{
    VK_INDEX_TYPE_UINT16                                    = 0x00000000,
    VK_INDEX_TYPE_UINT32                                    = 0x00000001,

    VK_ENUM_RANGE(INDEX_TYPE, UINT16, UINT32)
} VkIndexType;

typedef enum VkTexFilter_
{
    VK_TEX_FILTER_NEAREST                                   = 0x00000000,
    VK_TEX_FILTER_LINEAR                                    = 0x00000001,

    VK_ENUM_RANGE(TEX_FILTER, NEAREST, LINEAR)
} VkTexFilter;

typedef enum VkTexMipmapMode_
{
    VK_TEX_MIPMAP_MODE_BASE                                 = 0x00000000,   // Always choose base level
    VK_TEX_MIPMAP_MODE_NEAREST                              = 0x00000001,   // Choose nearest mip level
    VK_TEX_MIPMAP_MODE_LINEAR                               = 0x00000002,   // Linear filter between mip levels

    VK_ENUM_RANGE(TEX_MIPMAP_MODE, BASE, LINEAR)
} VkTexMipmapMode;

typedef enum VkTexAddress_
{
    VK_TEX_ADDRESS_WRAP                                     = 0x00000000,
    VK_TEX_ADDRESS_MIRROR                                   = 0x00000001,
    VK_TEX_ADDRESS_CLAMP                                    = 0x00000002,
    VK_TEX_ADDRESS_MIRROR_ONCE                              = 0x00000003,
    VK_TEX_ADDRESS_CLAMP_BORDER                             = 0x00000004,

    VK_ENUM_RANGE(TEX_ADDRESS, WRAP, CLAMP_BORDER)
} VkTexAddress;

typedef enum VkCompareOp_
{
    VK_COMPARE_OP_NEVER                                     = 0x00000000,
    VK_COMPARE_OP_LESS                                      = 0x00000001,
    VK_COMPARE_OP_EQUAL                                     = 0x00000002,
    VK_COMPARE_OP_LESS_EQUAL                                = 0x00000003,
    VK_COMPARE_OP_GREATER                                   = 0x00000004,
    VK_COMPARE_OP_NOT_EQUAL                                 = 0x00000005,
    VK_COMPARE_OP_GREATER_EQUAL                             = 0x00000006,
    VK_COMPARE_OP_ALWAYS                                    = 0x00000007,

    VK_ENUM_RANGE(COMPARE_OP, NEVER, ALWAYS)
} VkCompareOp;

typedef enum VkFillMode_
{
    VK_FILL_MODE_POINTS                                     = 0x00000000,
    VK_FILL_MODE_WIREFRAME                                  = 0x00000001,
    VK_FILL_MODE_SOLID                                      = 0x00000002,

    VK_ENUM_RANGE(FILL_MODE, POINTS, SOLID)
} VkFillMode;

typedef enum VkCullMode_
{
    VK_CULL_MODE_NONE                                       = 0x00000000,
    VK_CULL_MODE_FRONT                                      = 0x00000001,
    VK_CULL_MODE_BACK                                       = 0x00000002,
    VK_CULL_MODE_FRONT_AND_BACK                             = 0x00000003,

    VK_ENUM_RANGE(CULL_MODE, NONE, FRONT_AND_BACK)
} VkCullMode;

typedef enum VkFrontFace_
{
    VK_FRONT_FACE_CCW                                       = 0x00000000,
    VK_FRONT_FACE_CW                                        = 0x00000001,

    VK_ENUM_RANGE(FRONT_FACE, CCW, CW)
} VkFrontFace;

typedef enum VkProvokingVertex_
{
    VK_PROVOKING_VERTEX_FIRST                               = 0x00000000,
    VK_PROVOKING_VERTEX_LAST                                = 0x00000001,

    VK_ENUM_RANGE(PROVOKING_VERTEX, FIRST, LAST)
} VkProvokingVertex;

typedef enum VkCoordinateOrigin_
{
    VK_COORDINATE_ORIGIN_UPPER_LEFT                         = 0x00000000,
    VK_COORDINATE_ORIGIN_LOWER_LEFT                         = 0x00000001,

    VK_ENUM_RANGE(COORDINATE_ORIGIN, UPPER_LEFT, LOWER_LEFT)
} VkCoordinateOrigin;

typedef enum VkDepthMode_
{
    VK_DEPTH_MODE_ZERO_TO_ONE                               = 0x00000000,
    VK_DEPTH_MODE_NEGATIVE_ONE_TO_ONE                       = 0x00000001,

    VK_ENUM_RANGE(DEPTH_MODE, ZERO_TO_ONE, NEGATIVE_ONE_TO_ONE)
} VkDepthMode;

typedef enum VkBlend_
{
    VK_BLEND_ZERO                                           = 0x00000000,
    VK_BLEND_ONE                                            = 0x00000001,
    VK_BLEND_SRC_COLOR                                      = 0x00000002,
    VK_BLEND_ONE_MINUS_SRC_COLOR                            = 0x00000003,
    VK_BLEND_DEST_COLOR                                     = 0x00000004,
    VK_BLEND_ONE_MINUS_DEST_COLOR                           = 0x00000005,
    VK_BLEND_SRC_ALPHA                                      = 0x00000006,
    VK_BLEND_ONE_MINUS_SRC_ALPHA                            = 0x00000007,
    VK_BLEND_DEST_ALPHA                                     = 0x00000008,
    VK_BLEND_ONE_MINUS_DEST_ALPHA                           = 0x00000009,
    VK_BLEND_CONSTANT_COLOR                                 = 0x0000000a,
    VK_BLEND_ONE_MINUS_CONSTANT_COLOR                       = 0x0000000b,
    VK_BLEND_CONSTANT_ALPHA                                 = 0x0000000c,
    VK_BLEND_ONE_MINUS_CONSTANT_ALPHA                       = 0x0000000d,
    VK_BLEND_SRC_ALPHA_SATURATE                             = 0x0000000e,
    VK_BLEND_SRC1_COLOR                                     = 0x0000000f,
    VK_BLEND_ONE_MINUS_SRC1_COLOR                           = 0x00000010,
    VK_BLEND_SRC1_ALPHA                                     = 0x00000011,
    VK_BLEND_ONE_MINUS_SRC1_ALPHA                           = 0x00000012,

    VK_ENUM_RANGE(BLEND, ZERO, ONE_MINUS_SRC1_ALPHA)
} VkBlend;

typedef enum VkBlendOp_
{
    VK_BLEND_OP_ADD                                         = 0x00000000,
    VK_BLEND_OP_SUBTRACT                                    = 0x00000001,
    VK_BLEND_OP_REVERSE_SUBTRACT                            = 0x00000002,
    VK_BLEND_OP_MIN                                         = 0x00000003,
    VK_BLEND_OP_MAX                                         = 0x00000004,

    VK_ENUM_RANGE(BLEND_OP, ADD, MAX)
} VkBlendOp;

typedef enum VkStencilOp_
{
    VK_STENCIL_OP_KEEP                                      = 0x00000000,
    VK_STENCIL_OP_ZERO                                      = 0x00000001,
    VK_STENCIL_OP_REPLACE                                   = 0x00000002,
    VK_STENCIL_OP_INC_CLAMP                                 = 0x00000003,
    VK_STENCIL_OP_DEC_CLAMP                                 = 0x00000004,
    VK_STENCIL_OP_INVERT                                    = 0x00000005,
    VK_STENCIL_OP_INC_WRAP                                  = 0x00000006,
    VK_STENCIL_OP_DEC_WRAP                                  = 0x00000007,

    VK_ENUM_RANGE(STENCIL_OP, KEEP, DEC_WRAP)
} VkStencilOp;

typedef enum VkLogicOp_
{
    VK_LOGIC_OP_CLEAR                                       = 0x00000000,
    VK_LOGIC_OP_AND                                         = 0x00000001,
    VK_LOGIC_OP_AND_REVERSE                                 = 0x00000002,
    VK_LOGIC_OP_COPY                                        = 0x00000003,
    VK_LOGIC_OP_AND_INVERTED                                = 0x00000004,
    VK_LOGIC_OP_NOOP                                        = 0x00000005,
    VK_LOGIC_OP_XOR                                         = 0x00000006,
    VK_LOGIC_OP_OR                                          = 0x00000007,
    VK_LOGIC_OP_NOR                                         = 0x00000008,
    VK_LOGIC_OP_EQUIV                                       = 0x00000009,
    VK_LOGIC_OP_INVERT                                      = 0x0000000a,
    VK_LOGIC_OP_OR_REVERSE                                  = 0x0000000b,
    VK_LOGIC_OP_COPY_INVERTED                               = 0x0000000c,
    VK_LOGIC_OP_OR_INVERTED                                 = 0x0000000d,
    VK_LOGIC_OP_NAND                                        = 0x0000000e,
    VK_LOGIC_OP_SET                                         = 0x0000000f,

    VK_ENUM_RANGE(LOGIC_OP, CLEAR, SET)
} VkLogicOp;

typedef enum VkSystemAllocType_
{
    VK_SYSTEM_ALLOC_TYPE_API_OBJECT                         = 0x00000000,
    VK_SYSTEM_ALLOC_TYPE_INTERNAL                           = 0x00000001,
    VK_SYSTEM_ALLOC_TYPE_INTERNAL_TEMP                      = 0x00000002,
    VK_SYSTEM_ALLOC_TYPE_INTERNAL_SHADER                    = 0x00000003,
    VK_SYSTEM_ALLOC_TYPE_DEBUG                              = 0x00000004,

    VK_ENUM_RANGE(SYSTEM_ALLOC_TYPE, API_OBJECT, DEBUG)
} VkSystemAllocType;

typedef enum VkPhysicalDeviceType_
{
    VK_PHYSICAL_DEVICE_TYPE_OTHER                           = 0x00000000,
    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU                  = 0x00000001,
    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU                    = 0x00000002,
    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU                     = 0x00000003,
    VK_PHYSICAL_DEVICE_TYPE_CPU                             = 0x00000004,

    VK_ENUM_RANGE(PHYSICAL_DEVICE_TYPE, OTHER, CPU)
} VkPhysicalDeviceType;

typedef enum VkVertexInputStepRate_
{
    VK_VERTEX_INPUT_STEP_RATE_VERTEX                        = 0x0,
    VK_VERTEX_INPUT_STEP_RATE_INSTANCE                      = 0x1,
    VK_VERTEX_INPUT_STEP_RATE_DRAW                          = 0x2,  //Optional

    VK_ENUM_RANGE(VERTEX_INPUT_STEP_RATE, VERTEX, DRAW)
} VkVertexInputStepRate;

// Vulkan format definitions
typedef enum VkFormat_
{
    VK_FORMAT_UNDEFINED                                     = 0x00000000,
    VK_FORMAT_R4G4_UNORM                                    = 0x00000001,
    VK_FORMAT_R4G4_USCALED                                  = 0x00000002,
    VK_FORMAT_R4G4B4A4_UNORM                                = 0x00000003,
    VK_FORMAT_R4G4B4A4_USCALED                              = 0x00000004,
    VK_FORMAT_R5G6B5_UNORM                                  = 0x00000005,
    VK_FORMAT_R5G6B5_USCALED                                = 0x00000006,
    VK_FORMAT_R5G5B5A1_UNORM                                = 0x00000007,
    VK_FORMAT_R5G5B5A1_USCALED                              = 0x00000008,
    VK_FORMAT_R8_UNORM                                      = 0x00000009,
    VK_FORMAT_R8_SNORM                                      = 0x0000000A,
    VK_FORMAT_R8_USCALED                                    = 0x0000000B,
    VK_FORMAT_R8_SSCALED                                    = 0x0000000C,
    VK_FORMAT_R8_UINT                                       = 0x0000000D,
    VK_FORMAT_R8_SINT                                       = 0x0000000E,
    VK_FORMAT_R8_SRGB                                       = 0x0000000F,
    VK_FORMAT_R8G8_UNORM                                    = 0x00000010,
    VK_FORMAT_R8G8_SNORM                                    = 0x00000011,
    VK_FORMAT_R8G8_USCALED                                  = 0x00000012,
    VK_FORMAT_R8G8_SSCALED                                  = 0x00000013,
    VK_FORMAT_R8G8_UINT                                     = 0x00000014,
    VK_FORMAT_R8G8_SINT                                     = 0x00000015,
    VK_FORMAT_R8G8_SRGB                                     = 0x00000016,
    VK_FORMAT_R8G8B8_UNORM                                  = 0x00000017,
    VK_FORMAT_R8G8B8_SNORM                                  = 0x00000018,
    VK_FORMAT_R8G8B8_USCALED                                = 0x00000019,
    VK_FORMAT_R8G8B8_SSCALED                                = 0x0000001A,
    VK_FORMAT_R8G8B8_UINT                                   = 0x0000001B,
    VK_FORMAT_R8G8B8_SINT                                   = 0x0000001C,
    VK_FORMAT_R8G8B8_SRGB                                   = 0x0000001D,
    VK_FORMAT_R8G8B8A8_UNORM                                = 0x0000001E,
    VK_FORMAT_R8G8B8A8_SNORM                                = 0x0000001F,
    VK_FORMAT_R8G8B8A8_USCALED                              = 0x00000020,
    VK_FORMAT_R8G8B8A8_SSCALED                              = 0x00000021,
    VK_FORMAT_R8G8B8A8_UINT                                 = 0x00000022,
    VK_FORMAT_R8G8B8A8_SINT                                 = 0x00000023,
    VK_FORMAT_R8G8B8A8_SRGB                                 = 0x00000024,
    VK_FORMAT_R10G10B10A2_UNORM                             = 0x00000025,
    VK_FORMAT_R10G10B10A2_SNORM                             = 0x00000026,
    VK_FORMAT_R10G10B10A2_USCALED                           = 0x00000027,
    VK_FORMAT_R10G10B10A2_SSCALED                           = 0x00000028,
    VK_FORMAT_R10G10B10A2_UINT                              = 0x00000029,
    VK_FORMAT_R10G10B10A2_SINT                              = 0x0000002A,
    VK_FORMAT_R16_UNORM                                     = 0x0000002B,
    VK_FORMAT_R16_SNORM                                     = 0x0000002C,
    VK_FORMAT_R16_USCALED                                   = 0x0000002D,
    VK_FORMAT_R16_SSCALED                                   = 0x0000002E,
    VK_FORMAT_R16_UINT                                      = 0x0000002F,
    VK_FORMAT_R16_SINT                                      = 0x00000030,
    VK_FORMAT_R16_SFLOAT                                    = 0x00000031,
    VK_FORMAT_R16G16_UNORM                                  = 0x00000032,
    VK_FORMAT_R16G16_SNORM                                  = 0x00000033,
    VK_FORMAT_R16G16_USCALED                                = 0x00000034,
    VK_FORMAT_R16G16_SSCALED                                = 0x00000035,
    VK_FORMAT_R16G16_UINT                                   = 0x00000036,
    VK_FORMAT_R16G16_SINT                                   = 0x00000037,
    VK_FORMAT_R16G16_SFLOAT                                 = 0x00000038,
    VK_FORMAT_R16G16B16_UNORM                               = 0x00000039,
    VK_FORMAT_R16G16B16_SNORM                               = 0x0000003A,
    VK_FORMAT_R16G16B16_USCALED                             = 0x0000003B,
    VK_FORMAT_R16G16B16_SSCALED                             = 0x0000003C,
    VK_FORMAT_R16G16B16_UINT                                = 0x0000003D,
    VK_FORMAT_R16G16B16_SINT                                = 0x0000003E,
    VK_FORMAT_R16G16B16_SFLOAT                              = 0x0000003F,
    VK_FORMAT_R16G16B16A16_UNORM                            = 0x00000040,
    VK_FORMAT_R16G16B16A16_SNORM                            = 0x00000041,
    VK_FORMAT_R16G16B16A16_USCALED                          = 0x00000042,
    VK_FORMAT_R16G16B16A16_SSCALED                          = 0x00000043,
    VK_FORMAT_R16G16B16A16_UINT                             = 0x00000044,
    VK_FORMAT_R16G16B16A16_SINT                             = 0x00000045,
    VK_FORMAT_R16G16B16A16_SFLOAT                           = 0x00000046,
    VK_FORMAT_R32_UINT                                      = 0x00000047,
    VK_FORMAT_R32_SINT                                      = 0x00000048,
    VK_FORMAT_R32_SFLOAT                                    = 0x00000049,
    VK_FORMAT_R32G32_UINT                                   = 0x0000004A,
    VK_FORMAT_R32G32_SINT                                   = 0x0000004B,
    VK_FORMAT_R32G32_SFLOAT                                 = 0x0000004C,
    VK_FORMAT_R32G32B32_UINT                                = 0x0000004D,
    VK_FORMAT_R32G32B32_SINT                                = 0x0000004E,
    VK_FORMAT_R32G32B32_SFLOAT                              = 0x0000004F,
    VK_FORMAT_R32G32B32A32_UINT                             = 0x00000050,
    VK_FORMAT_R32G32B32A32_SINT                             = 0x00000051,
    VK_FORMAT_R32G32B32A32_SFLOAT                           = 0x00000052,
    VK_FORMAT_R64_SFLOAT                                    = 0x00000053,
    VK_FORMAT_R64G64_SFLOAT                                 = 0x00000054,
    VK_FORMAT_R64G64B64_SFLOAT                              = 0x00000055,
    VK_FORMAT_R64G64B64A64_SFLOAT                           = 0x00000056,
    VK_FORMAT_R11G11B10_UFLOAT                              = 0x00000057,
    VK_FORMAT_R9G9B9E5_UFLOAT                               = 0x00000058,
    VK_FORMAT_D16_UNORM                                     = 0x00000059,
    VK_FORMAT_D24_UNORM                                     = 0x0000005A,
    VK_FORMAT_D32_SFLOAT                                    = 0x0000005B,
    VK_FORMAT_S8_UINT                                       = 0x0000005C,
    VK_FORMAT_D16_UNORM_S8_UINT                             = 0x0000005D,
    VK_FORMAT_D24_UNORM_S8_UINT                             = 0x0000005E,
    VK_FORMAT_D32_SFLOAT_S8_UINT                            = 0x0000005F,
    VK_FORMAT_BC1_RGB_UNORM                                 = 0x00000060,
    VK_FORMAT_BC1_RGB_SRGB                                  = 0x00000061,
    VK_FORMAT_BC1_RGBA_UNORM                                = 0x00000062,
    VK_FORMAT_BC1_RGBA_SRGB                                 = 0x00000063,
    VK_FORMAT_BC2_UNORM                                     = 0x00000064,
    VK_FORMAT_BC2_SRGB                                      = 0x00000065,
    VK_FORMAT_BC3_UNORM                                     = 0x00000066,
    VK_FORMAT_BC3_SRGB                                      = 0x00000067,
    VK_FORMAT_BC4_UNORM                                     = 0x00000068,
    VK_FORMAT_BC4_SNORM                                     = 0x00000069,
    VK_FORMAT_BC5_UNORM                                     = 0x0000006A,
    VK_FORMAT_BC5_SNORM                                     = 0x0000006B,
    VK_FORMAT_BC6H_UFLOAT                                   = 0x0000006C,
    VK_FORMAT_BC6H_SFLOAT                                   = 0x0000006D,
    VK_FORMAT_BC7_UNORM                                     = 0x0000006E,
    VK_FORMAT_BC7_SRGB                                      = 0x0000006F,
    VK_FORMAT_ETC2_R8G8B8_UNORM                             = 0x00000070,
    VK_FORMAT_ETC2_R8G8B8_SRGB                              = 0x00000071,
    VK_FORMAT_ETC2_R8G8B8A1_UNORM                           = 0x00000072,
    VK_FORMAT_ETC2_R8G8B8A1_SRGB                            = 0x00000073,
    VK_FORMAT_ETC2_R8G8B8A8_UNORM                           = 0x00000074,
    VK_FORMAT_ETC2_R8G8B8A8_SRGB                            = 0x00000075,
    VK_FORMAT_EAC_R11_UNORM                                 = 0x00000076,
    VK_FORMAT_EAC_R11_SNORM                                 = 0x00000077,
    VK_FORMAT_EAC_R11G11_UNORM                              = 0x00000078,
    VK_FORMAT_EAC_R11G11_SNORM                              = 0x00000079,
    VK_FORMAT_ASTC_4x4_UNORM                                = 0x0000007A,
    VK_FORMAT_ASTC_4x4_SRGB                                 = 0x0000007B,
    VK_FORMAT_ASTC_5x4_UNORM                                = 0x0000007C,
    VK_FORMAT_ASTC_5x4_SRGB                                 = 0x0000007D,
    VK_FORMAT_ASTC_5x5_UNORM                                = 0x0000007E,
    VK_FORMAT_ASTC_5x5_SRGB                                 = 0x0000007F,
    VK_FORMAT_ASTC_6x5_UNORM                                = 0x00000080,
    VK_FORMAT_ASTC_6x5_SRGB                                 = 0x00000081,
    VK_FORMAT_ASTC_6x6_UNORM                                = 0x00000082,
    VK_FORMAT_ASTC_6x6_SRGB                                 = 0x00000083,
    VK_FORMAT_ASTC_8x5_UNORM                                = 0x00000084,
    VK_FORMAT_ASTC_8x5_SRGB                                 = 0x00000085,
    VK_FORMAT_ASTC_8x6_UNORM                                = 0x00000086,
    VK_FORMAT_ASTC_8x6_SRGB                                 = 0x00000087,
    VK_FORMAT_ASTC_8x8_UNORM                                = 0x00000088,
    VK_FORMAT_ASTC_8x8_SRGB                                 = 0x00000089,
    VK_FORMAT_ASTC_10x5_UNORM                               = 0x0000008A,
    VK_FORMAT_ASTC_10x5_SRGB                                = 0x0000008B,
    VK_FORMAT_ASTC_10x6_UNORM                               = 0x0000008C,
    VK_FORMAT_ASTC_10x6_SRGB                                = 0x0000008D,
    VK_FORMAT_ASTC_10x8_UNORM                               = 0x0000008E,
    VK_FORMAT_ASTC_10x8_SRGB                                = 0x0000008F,
    VK_FORMAT_ASTC_10x10_UNORM                              = 0x00000090,
    VK_FORMAT_ASTC_10x10_SRGB                               = 0x00000091,
    VK_FORMAT_ASTC_12x10_UNORM                              = 0x00000092,
    VK_FORMAT_ASTC_12x10_SRGB                               = 0x00000093,
    VK_FORMAT_ASTC_12x12_UNORM                              = 0x00000094,
    VK_FORMAT_ASTC_12x12_SRGB                               = 0x00000095,
    VK_FORMAT_B4G4R4A4_UNORM                                = 0x00000096,
    VK_FORMAT_B5G5R5A1_UNORM                                = 0x00000097,
    VK_FORMAT_B5G6R5_UNORM                                  = 0x00000098,
    VK_FORMAT_B5G6R5_USCALED                                = 0x00000099,
    VK_FORMAT_B8G8R8_UNORM                                  = 0x0000009A,
    VK_FORMAT_B8G8R8_SNORM                                  = 0x0000009B,
    VK_FORMAT_B8G8R8_USCALED                                = 0x0000009C,
    VK_FORMAT_B8G8R8_SSCALED                                = 0x0000009D,
    VK_FORMAT_B8G8R8_UINT                                   = 0x0000009E,
    VK_FORMAT_B8G8R8_SINT                                   = 0x0000009F,
    VK_FORMAT_B8G8R8_SRGB                                   = 0x000000A0,
    VK_FORMAT_B8G8R8A8_UNORM                                = 0x000000A1,
    VK_FORMAT_B8G8R8A8_SNORM                                = 0x000000A2,
    VK_FORMAT_B8G8R8A8_USCALED                              = 0x000000A3,
    VK_FORMAT_B8G8R8A8_SSCALED                              = 0x000000A4,
    VK_FORMAT_B8G8R8A8_UINT                                 = 0x000000A5,
    VK_FORMAT_B8G8R8A8_SINT                                 = 0x000000A6,
    VK_FORMAT_B8G8R8A8_SRGB                                 = 0x000000A7,
    VK_FORMAT_B10G10R10A2_UNORM                             = 0x000000A8,
    VK_FORMAT_B10G10R10A2_SNORM                             = 0x000000A9,
    VK_FORMAT_B10G10R10A2_USCALED                           = 0x000000AA,
    VK_FORMAT_B10G10R10A2_SSCALED                           = 0x000000AB,
    VK_FORMAT_B10G10R10A2_UINT                              = 0x000000AC,
    VK_FORMAT_B10G10R10A2_SINT                              = 0x000000AD,

    VK_ENUM_RANGE(FORMAT, UNDEFINED, B10G10R10A2_SINT)
} VkFormat;

// Shader stage enumerant
typedef enum VkShaderStage_
{
    VK_SHADER_STAGE_VERTEX                                  = 0,
    VK_SHADER_STAGE_TESS_CONTROL                            = 1,
    VK_SHADER_STAGE_TESS_EVALUATION                         = 2,
    VK_SHADER_STAGE_GEOMETRY                                = 3,
    VK_SHADER_STAGE_FRAGMENT                                = 4,
    VK_SHADER_STAGE_COMPUTE                                 = 5,

    VK_ENUM_RANGE(SHADER_STAGE, VERTEX, COMPUTE)
} VkShaderStage;

// Structure type enumerant
typedef enum VkStructureType_
{
    VK_STRUCTURE_TYPE_APPLICATION_INFO                        = 0,
    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO                      = 1,
    VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO                       = 2,
    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO                  = 3,
    VK_STRUCTURE_TYPE_ATTACHMENT_VIEW_CREATE_INFO             = 4,

    VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO               = 6,
    VK_STRUCTURE_TYPE_SHADER_CREATE_INFO                      = 7,
    VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO            = 8,
    VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO                     = 9,
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO       = 10,
    VK_STRUCTURE_TYPE_DYNAMIC_VIEWPORT_STATE_CREATE_INFO      = 11,
    VK_STRUCTURE_TYPE_DYNAMIC_RASTER_STATE_CREATE_INFO        = 12,
    VK_STRUCTURE_TYPE_DYNAMIC_COLOR_BLEND_STATE_CREATE_INFO   = 13,
    VK_STRUCTURE_TYPE_DYNAMIC_DEPTH_STENCIL_STATE_CREATE_INFO = 14,
    VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO                  = 15,
    VK_STRUCTURE_TYPE_EVENT_CREATE_INFO                       = 16,
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO                       = 17,
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO                   = 18,
    VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO                  = 19,
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO       = 20,
    VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO           = 21,
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO = 22,
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO = 23,
    VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO   = 24,
    VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO       = 25,
    VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO         = 26,
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO    = 27,
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO    = 28,
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO  = 29,
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO                       = 30,
    VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO                      = 31,
    VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO                 = 32,
    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO                 = 33,
    VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO                   = 34,

    VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO                 = 36,
    VK_STRUCTURE_TYPE_MEMORY_BARRIER                          = 37,
    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER                   = 38,
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER                    = 39,
    VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO             = 40,
    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET                    = 41,
    VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET                     = 42,
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO                    = 43,
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO             = 44,
    VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO              = 45,
    VK_STRUCTURE_TYPE_EXTENSION_PROPERTIES                    = 46,

    VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION                  = 47,
    VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION                     = 48,
    VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY                      = 49,
    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO                  = 50,

    VK_ENUM_RANGE(STRUCTURE_TYPE, APPLICATION_INFO, RENDER_PASS_BEGIN_INFO)
} VkStructureType;

// ------------------------------------------------------------------------------------------------
// Error and return codes

typedef enum VkResult_
{
    // Return codes for successful operation execution (> = 0)
    VK_SUCCESS                                              = 0x0000000,
    VK_UNSUPPORTED                                          = 0x0000001,
    VK_NOT_READY                                            = 0x0000002,
    VK_TIMEOUT                                              = 0x0000003,
    VK_EVENT_SET                                            = 0x0000004,
    VK_EVENT_RESET                                          = 0x0000005,
    VK_INCOMPLETE                                           = 0x0000006,

    // Error codes (negative values)
    VK_ERROR_UNKNOWN                                        = -(0x00000001),
    VK_ERROR_UNAVAILABLE                                    = -(0x00000002),
    VK_ERROR_INITIALIZATION_FAILED                          = -(0x00000003),
    VK_ERROR_OUT_OF_HOST_MEMORY                             = -(0x00000004),
    VK_ERROR_OUT_OF_DEVICE_MEMORY                           = -(0x00000005),
    VK_ERROR_DEVICE_ALREADY_CREATED                         = -(0x00000006),
    VK_ERROR_DEVICE_LOST                                    = -(0x00000007),
    VK_ERROR_INVALID_POINTER                                = -(0x00000008),
    VK_ERROR_INVALID_VALUE                                  = -(0x00000009),
    VK_ERROR_INVALID_HANDLE                                 = -(0x0000000A),
    VK_ERROR_INVALID_ORDINAL                                = -(0x0000000B),
    VK_ERROR_INVALID_MEMORY_SIZE                            = -(0x0000000C),
    VK_ERROR_INVALID_EXTENSION                              = -(0x0000000D),
    VK_ERROR_INVALID_LAYER                                  = -(0x0000000E),
    VK_ERROR_INVALID_FLAGS                                  = -(0x0000000F),
    VK_ERROR_INVALID_ALIGNMENT                              = -(0x00000010),
    VK_ERROR_INVALID_FORMAT                                 = -(0x00000011),
    VK_ERROR_INVALID_IMAGE                                  = -(0x00000012),
    VK_ERROR_INVALID_DESCRIPTOR_SET_DATA                    = -(0x00000013),
    VK_ERROR_INVALID_QUEUE_TYPE                             = -(0x00000014),
    VK_ERROR_INVALID_OBJECT_TYPE                            = -(0x00000015),
    VK_ERROR_UNSUPPORTED_SHADER_IL_VERSION                  = -(0x00000016),
    VK_ERROR_BAD_SHADER_CODE                                = -(0x00000017),
    VK_ERROR_BAD_PIPELINE_DATA                              = -(0x00000018),
    VK_ERROR_NOT_MAPPABLE                                   = -(0x00000019),
    VK_ERROR_MEMORY_MAP_FAILED                              = -(0x0000001A),
    VK_ERROR_MEMORY_UNMAP_FAILED                            = -(0x0000001B),
    VK_ERROR_INCOMPATIBLE_DEVICE                            = -(0x0000001C),
    VK_ERROR_INCOMPATIBLE_DRIVER                            = -(0x0000001D),
    VK_ERROR_INCOMPLETE_COMMAND_BUFFER                      = -(0x0000001E),
    VK_ERROR_BUILDING_COMMAND_BUFFER                        = -(0x0000001F),
    VK_ERROR_MEMORY_NOT_BOUND                               = -(0x00000020),
    VK_ERROR_INCOMPATIBLE_QUEUE                             = -(0x00000021),

    VK_MAX_ENUM(RESULT)
} VkResult;

// ------------------------------------------------------------------------------------------------
// Flags

// Device creation flags
typedef VkFlags VkDeviceCreateFlags;

// Queue capabilities
typedef VkFlags VkQueueFlags;
typedef enum VkQueueFlagBits_
{
    VK_QUEUE_GRAPHICS_BIT                                   = VK_BIT(0),    // Queue supports graphics operations
    VK_QUEUE_COMPUTE_BIT                                    = VK_BIT(1),    // Queue supports compute operations
    VK_QUEUE_DMA_BIT                                        = VK_BIT(2),    // Queue supports DMA operations
    VK_QUEUE_SPARSE_MEMMGR_BIT                              = VK_BIT(3),    // Queue supports sparse resource memory management operations
    VK_QUEUE_EXTENDED_BIT                                   = VK_BIT(30),   // Extended queue
} VkQueueFlagBits;

// Memory properties passed into vkAllocMemory().
typedef VkFlags VkMemoryPropertyFlags;
typedef enum VkMemoryPropertyFlagBits_
{
    VK_MEMORY_PROPERTY_DEVICE_ONLY                          = 0,            // If otherwise stated, then allocate memory on device
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT                     = VK_BIT(0),    // Memory should be mappable by host
    VK_MEMORY_PROPERTY_HOST_NON_COHERENT_BIT                = VK_BIT(1),    // Memory may not have i/o coherency so vkFlushMappedMemoryRanges and
                                                                            // vkInvalidateMappedMemoryRanges must be used flush/invalidate host cache
    VK_MEMORY_PROPERTY_HOST_UNCACHED_BIT                    = VK_BIT(2),    // Memory should not be cached by the host
    VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT              = VK_BIT(3),    // Memory should support host write combining
    VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT                 = 0x00000010,   // Memory may be allocated by the driver when it is required
} VkMemoryPropertyFlagBits;

// Memory output flags passed to resource transition commands
typedef VkFlags VkMemoryOutputFlags;
typedef enum VkMemoryOutputFlagBits_
{
    VK_MEMORY_OUTPUT_HOST_WRITE_BIT                         = VK_BIT(0),    // Controls output coherency of host writes
    VK_MEMORY_OUTPUT_SHADER_WRITE_BIT                       = VK_BIT(1),    // Controls output coherency of generic shader writes
    VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT                   = VK_BIT(2),    // Controls output coherency of color attachment writes
    VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT           = VK_BIT(3),    // Controls output coherency of depth/stencil attachment writes
    VK_MEMORY_OUTPUT_TRANSFER_BIT                           = VK_BIT(4),    // Controls output coherency of transfer operations
} VkMemoryOutputFlagBits;

// Memory input flags passed to resource transition commands
typedef VkFlags VkMemoryInputFlags;
typedef enum VkMemoryInputFlagBits_
{
    VK_MEMORY_INPUT_HOST_READ_BIT                           = VK_BIT(0),    // Controls input coherency of host reads
    VK_MEMORY_INPUT_INDIRECT_COMMAND_BIT                    = VK_BIT(1),    // Controls input coherency of indirect command reads
    VK_MEMORY_INPUT_INDEX_FETCH_BIT                         = VK_BIT(2),    // Controls input coherency of index fetches
    VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT              = VK_BIT(3),    // Controls input coherency of vertex attribute fetches
    VK_MEMORY_INPUT_UNIFORM_READ_BIT                        = VK_BIT(4),    // Controls input coherency of uniform buffer reads
    VK_MEMORY_INPUT_SHADER_READ_BIT                         = VK_BIT(5),    // Controls input coherency of generic shader reads
    VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT                    = VK_BIT(6),    // Controls input coherency of color attachment reads
    VK_MEMORY_INPUT_DEPTH_STENCIL_ATTACHMENT_BIT            = VK_BIT(7),    // Controls input coherency of depth/stencil attachment reads
    VK_MEMORY_INPUT_ATTACHMENT_BIT                          = VK_BIT(8),
    VK_MEMORY_INPUT_TRANSFER_BIT                            = VK_BIT(9),    // Controls input coherency of transfer operations
} VkMemoryInputFlagBits;

// Buffer usage flags
typedef VkFlags VkBufferUsageFlags;
typedef enum VkBufferUsageFlagBits_
{
    VK_BUFFER_USAGE_GENERAL                                 = 0,            // No special usage
    VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT                     = VK_BIT(0),    // Can be used as a source of transfer operations
    VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT                = VK_BIT(1),    // Can be used as a destination of transfer operations
    VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT                = VK_BIT(2),    // Can be used as TBO
    VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT                = VK_BIT(3),    // Can be used as IBO
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT                      = VK_BIT(4),    // Can be used as UBO
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT                      = VK_BIT(5),    // Can be used as SSBO
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT                        = VK_BIT(6),    // Can be used as source of fixed function index fetch (index buffer)
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT                       = VK_BIT(7),    // Can be used as source of fixed function vertex fetch (VBO)
    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT                     = VK_BIT(8),    // Can be the source of indirect parameters (e.g. indirect buffer, parameter buffer)
} VkBufferUsageFlagBits;

// Buffer creation flags
typedef VkFlags VkBufferCreateFlags;
typedef enum VkBufferCreateFlagBits_
{
    VK_BUFFER_CREATE_SPARSE_BIT                             = VK_BIT(0),    // Buffer should support sparse backing
    VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT                   = VK_BIT(1),    // Buffer should support sparse backing with partial residency
    VK_BUFFER_CREATE_SPARSE_ALIASED_BIT                     = VK_BIT(2),    // Buffer should support consistent data access to physical memoryblocks mapped into multiple locations of sparse buffers
} VkBufferCreateFlagBits;

// Shader stage flags
typedef VkFlags VkShaderStageFlags;
typedef enum VkShaderStageFlagBits_
{
    VK_SHADER_STAGE_VERTEX_BIT                              = VK_BIT(0),
    VK_SHADER_STAGE_TESS_CONTROL_BIT                        = VK_BIT(1),
    VK_SHADER_STAGE_TESS_EVALUATION_BIT                     = VK_BIT(2),
    VK_SHADER_STAGE_GEOMETRY_BIT                            = VK_BIT(3),
    VK_SHADER_STAGE_FRAGMENT_BIT                            = VK_BIT(4),
    VK_SHADER_STAGE_COMPUTE_BIT                             = VK_BIT(5),

    VK_SHADER_STAGE_ALL                                     = 0x7FFFFFFF,
} VkShaderStageFlagBits;

// Image usage flags
typedef VkFlags VkImageUsageFlags;
typedef enum VkImageUsageFlagBits_
{
    VK_IMAGE_USAGE_GENERAL                                  = 0,            // No special usage
    VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT                      = VK_BIT(0),    // Can be used as a source of transfer operations
    VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT                 = VK_BIT(1),    // Can be used as a destination of transfer operations
    VK_IMAGE_USAGE_SAMPLED_BIT                              = VK_BIT(2),    // Can be sampled from (SAMPLED_IMAGE and COMBINED_IMAGE_SAMPLER descriptor types)
    VK_IMAGE_USAGE_STORAGE_BIT                              = VK_BIT(3),    // Can be used as storage image (STORAGE_IMAGE descriptor type)
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT                     = VK_BIT(4),    // Can be used as framebuffer color attachment
    VK_IMAGE_USAGE_DEPTH_STENCIL_BIT                        = VK_BIT(5),    // Can be used as framebuffer depth/stencil attachment
    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT                 = VK_BIT(6),    // Image data not needed outside of rendering
    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT                     = VK_BIT(7),
} VkImageUsageFlagBits;

// Image creation flags
typedef VkFlags VkImageCreateFlags;
typedef enum VkImageCreateFlagBits_
{
    VK_IMAGE_CREATE_SPARSE_BIT                              = VK_BIT(0),    // Image should support sparse backing
    VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT                    = VK_BIT(1),    // Image should support sparse backing with partial residency
    VK_IMAGE_CREATE_SPARSE_ALIASED_BIT                      = VK_BIT(2),    // Image should support constant data access to physical memoryblocks mapped into multiple locations fo sparse images
    VK_IMAGE_CREATE_INVARIANT_DATA_BIT                      = VK_BIT(3),
    VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT                      = VK_BIT(4),    // Allows image views to have different format than the base image
    VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT                     = VK_BIT(5),    // Allows creating image views with cube type from the created image
} VkImageCreateFlagBits;

// Depth-stencil view creation flags
typedef VkFlags VkAttachmentViewCreateFlags;
typedef enum VkAttachmentViewCreateFlagBits_
{
    VK_ATTACHMENT_VIEW_CREATE_READ_ONLY_DEPTH_BIT           = VK_BIT(0),
    VK_ATTACHMENT_VIEW_CREATE_READ_ONLY_STENCIL_BIT         = VK_BIT(1),
} VkAttachmentViewCreateFlagBits;

// Pipeline creation flags
typedef VkFlags VkPipelineCreateFlags;
typedef enum VkPipelineCreateFlagBits_
{
    VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT             = VK_BIT(0),
    VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT                = VK_BIT(1),
    VK_PIPELINE_CREATE_DERIVATIVE_BIT                       = VK_BIT(2),
} VkPipelineCreateFlagBits;

// Channel flags
typedef VkFlags VkChannelFlags;
typedef enum VkChannelFlagBits_
{
    VK_CHANNEL_R_BIT                                        = VK_BIT(0),
    VK_CHANNEL_G_BIT                                        = VK_BIT(1),
    VK_CHANNEL_B_BIT                                        = VK_BIT(2),
    VK_CHANNEL_A_BIT                                        = VK_BIT(3),
} VkChannelFlagBits;

// Fence creation flags
typedef VkFlags VkFenceCreateFlags;
typedef enum VkFenceCreateFlagBits_
{
    VK_FENCE_CREATE_SIGNALED_BIT                            = VK_BIT(0),
} VkFenceCreateFlagBits;

// Sparse image format flags
typedef VkFlags VkSparseImageFormatFlags;
typedef enum VkSparseImageFormatFlagBits_
{
    VK_SPARSE_IMAGE_FMT_SINGLE_MIPTAIL_BIT                  = VK_BIT(0),
    VK_SPARSE_IMAGE_FMT_ALIGNED_MIP_SIZE_BIT                = VK_BIT(1),
    VK_SPARSE_IMAGE_FMT_NONSTD_BLOCK_SIZE_BIT               = VK_BIT(2),
} VkSparseImageFormatFlagBits;

// Sparse memory bind flags
typedef VkFlags VkSparseMemoryBindFlags;
typedef enum VkSparseMemoryBindFlagBits_
{
    VK_SPARSE_MEMORY_BIND_REPLICATE_64KIB_BLOCK_BIT         = VK_BIT(0),
} VkSparseMemoryBindFlagBits;

// Semaphore creation flags
typedef VkFlags VkSemaphoreCreateFlags;

// Format capability flags
typedef VkFlags VkFormatFeatureFlags;
typedef enum VkFormatFeatureFlagBits_
{
    VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT                     = VK_BIT(0),    // Format can be used for sampled images (SAMPLED_IMAGE and COMBINED_IMAGE_SAMPLER descriptor types)
    VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT                     = VK_BIT(1),    // Format can be used for storage images (STORAGE_IMAGE descriptor type)
    VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT              = VK_BIT(2),    // Format supports atomic operations in case it's used for storage images
    VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT              = VK_BIT(3),    // Format can be used for uniform texel buffers (TBOs)
    VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT              = VK_BIT(4),    // Format can be used for storage texel buffers (IBOs)
    VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT       = VK_BIT(5),    // Format supports atomic operations in case it's used for storage texel buffers
    VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT                     = VK_BIT(6),    // Format can be used for vertex buffers (VBOs)
    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT                  = VK_BIT(7),    // Format can be used for color attachment images
    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT            = VK_BIT(8),    // Format supports blending in case it's used for color attachment images
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT          = VK_BIT(9),    // Format can be used for depth/stencil attachment images
    VK_FORMAT_FEATURE_CONVERSION_BIT                        = VK_BIT(10),   // Format can be used as the source or destination of format converting blits
} VkFormatFeatureFlagBits;

typedef VkFlags VkSubpassDescriptionFlags;
typedef enum VkSubpassDescriptionFlagBits_
{
    VK_SUBPASS_DESCRIPTION_NO_OVERDRAW_BIT                  = 0x00000001,
} VkSubpassDescriptionFlagBits;

// Pipeline stage flags
typedef enum {
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT                       = VK_BIT(0),
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT                     = VK_BIT(1),
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT                      = VK_BIT(2),
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT                     = VK_BIT(3),
    VK_PIPELINE_STAGE_TESS_CONTROL_SHADER_BIT               = VK_BIT(4),
    VK_PIPELINE_STAGE_TESS_EVALUATION_SHADER_BIT            = VK_BIT(5),
    VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT                   = VK_BIT(6),
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT                   = VK_BIT(7),
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT              = VK_BIT(8),
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT               = VK_BIT(9),
    VK_PIPELINE_STAGE_ATTACHMENT_OUTPUT_BIT                 = VK_BIT(10),
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT                    = VK_BIT(11),
    VK_PIPELINE_STAGE_TRANSFER_BIT                          = VK_BIT(12),
    VK_PIPELINE_STAGE_HOST_BIT                              = VK_BIT(13),
    VK_PIPELINE_STAGE_ALL_GRAPHICS                          = 0x000007FF,
    VK_PIPELINE_STAGE_ALL_GPU_COMMANDS                      = 0x00001FFF,
} VkPipelineStageFlagBits;

typedef VkFlags VkPipelineStageFlags;

// Image aspect flags
typedef VkFlags VkImageAspectFlags;
typedef enum VkImageAspectFlagBits_
{
    VK_IMAGE_ASPECT_COLOR_BIT                               = VK_BIT(0),
    VK_IMAGE_ASPECT_DEPTH_BIT                               = VK_BIT(1),
    VK_IMAGE_ASPECT_STENCIL_BIT                             = VK_BIT(2),
} VkImageAspectFlagBits;

// Query control flags
typedef VkFlags VkQueryControlFlags;
typedef enum VkQueryControlFlagBits_
{
    VK_QUERY_CONTROL_CONSERVATIVE_BIT                       = VK_BIT(0),    // Allow conservative results to be collected by the query
} VkQueryControlFlagBits;

// Query result flags
typedef VkFlags VkQueryResultFlags;
typedef enum VkQueryResultFlagBits_
{
    VK_QUERY_RESULT_DEFAULT                                 = 0,           // Results of the queries are immediately written to the destination buffer as 32-bit values
    VK_QUERY_RESULT_64_BIT                                  = VK_BIT(0),   // Results of the queries are written to the destination buffer as 64-bit values
    VK_QUERY_RESULT_WAIT_BIT                                = VK_BIT(1),   // Results of the queries are waited on before proceeding with the result copy
    VK_QUERY_RESULT_WITH_AVAILABILITY_BIT                   = VK_BIT(2),   // Besides the results of the query, the availability of the results is also written
    VK_QUERY_RESULT_PARTIAL_BIT                             = VK_BIT(3),   // Copy the partial results of the query even if the final results aren't available
} VkQueryResultFlagBits;

// Shader module creation flags
typedef VkFlags VkShaderModuleCreateFlags;

// Shader creation flags
typedef VkFlags VkShaderCreateFlags;

// Event creation flags
typedef VkFlags VkEventCreateFlags;

// Command buffer creation flags
typedef VkFlags VkCmdBufferCreateFlags;

// Command buffer optimization flags
typedef VkFlags VkCmdBufferOptimizeFlags;
typedef enum VkCmdBufferOptimizeFlagBits_
{
    VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT                  = VK_BIT(0),
    VK_CMD_BUFFER_OPTIMIZE_PIPELINE_SWITCH_BIT              = VK_BIT(1),
    VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT              = VK_BIT(2),
    VK_CMD_BUFFER_OPTIMIZE_DESCRIPTOR_SET_SWITCH_BIT        = VK_BIT(3),
    VK_CMD_BUFFER_OPTIMIZE_NO_SIMULTANEOUS_USE_BIT = 0x00000010,
} VkCmdBufferOptimizeFlagBits;

// Pipeline statistics flags
typedef VkFlags VkQueryPipelineStatisticFlags;
typedef enum VkQueryPipelineStatisticFlagBits_ {
    VK_QUERY_PIPELINE_STATISTIC_IA_VERTICES_BIT             = VK_BIT(0),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_IA_PRIMITIVES_BIT           = VK_BIT(1),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_VS_INVOCATIONS_BIT          = VK_BIT(2),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_GS_INVOCATIONS_BIT          = VK_BIT(3),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_GS_PRIMITIVES_BIT           = VK_BIT(4),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_C_INVOCATIONS_BIT           = VK_BIT(5),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_C_PRIMITIVES_BIT            = VK_BIT(6),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_FS_INVOCATIONS_BIT          = VK_BIT(7),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_TCS_PATCHES_BIT             = VK_BIT(8),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_TES_INVOCATIONS_BIT         = VK_BIT(9),  // Optional
    VK_QUERY_PIPELINE_STATISTIC_CS_INVOCATIONS_BIT          = VK_BIT(10), // Optional
} VkQueryPipelineStatisticFlagBits;

// Memory mapping flags
typedef VkFlags VkMemoryMapFlags;

// Memory heap flags
typedef VkFlags VkMemoryHeapFlags;
typedef enum VkMemoryHeapFlagBits_ {
    VK_MEMORY_HEAP_HOST_LOCAL                               = VK_BIT(0),   // If set, heap represents host memory
} VkMemoryHeapFlagBits;


// ------------------------------------------------------------------------------------------------
// Vulkan structures

typedef struct VkOffset2D_
{
    int32_t                                     x;
    int32_t                                     y;
} VkOffset2D;

typedef struct VkOffset3D_
{
    int32_t                                     x;
    int32_t                                     y;
    int32_t                                     z;
} VkOffset3D;

typedef struct VkExtent2D_
{
    int32_t                                     width;
    int32_t                                     height;
} VkExtent2D;

typedef struct VkExtent3D_
{
    int32_t                                     width;
    int32_t                                     height;
    int32_t                                     depth;
} VkExtent3D;

typedef struct VkViewport_
{
    float                                       originX;
    float                                       originY;
    float                                       width;
    float                                       height;
    float                                       minDepth;
    float                                       maxDepth;
} VkViewport;

typedef struct VkRect2D_
{
    VkOffset2D                                  offset;
    VkExtent2D                                  extent;
} VkRect2D;

typedef struct VkRect3D_
{
    VkOffset3D                                  offset;
    VkExtent3D                                  extent;
} VkRect3D;

typedef struct VkChannelMapping_
{
    VkChannelSwizzle                            r;
    VkChannelSwizzle                            g;
    VkChannelSwizzle                            b;
    VkChannelSwizzle                            a;
} VkChannelMapping;

typedef struct VkPhysicalDeviceProperties_
{
    uint32_t                                    apiVersion;
    uint32_t                                    driverVersion;
    uint32_t                                    vendorId;
    uint32_t                                    deviceId;
    VkPhysicalDeviceType                        deviceType;
    char                                        deviceName[VK_MAX_PHYSICAL_DEVICE_NAME];
    uint8_t                                     pipelineCacheUUID[VK_UUID_LENGTH];
} VkPhysicalDeviceProperties;

typedef struct VkPhysicalDeviceFeatures_
{
    VkBool32                                    robustBufferAccess;
    VkBool32                                    fullDrawIndexUint32;
    VkBool32                                    imageCubeArray;
    VkBool32                                    independentBlend;
    VkBool32                                    geometryShader;
    VkBool32                                    tessellationShader;
    VkBool32                                    sampleRateShading;
    VkBool32                                    dualSourceBlend;
    VkBool32                                    logicOp;
    VkBool32                                    instancedDrawIndirect;
    VkBool32                                    depthClip;
    VkBool32                                    depthBiasClamp;
    VkBool32                                    fillModeNonSolid;
    VkBool32                                    depthBounds;
    VkBool32                                    wideLines;
    VkBool32                                    largePoints;
    VkBool32                                    textureCompressionETC2;
    VkBool32                                    textureCompressionASTC_LDR;
    VkBool32                                    textureCompressionBC;
    VkBool32                                    pipelineStatisticsQuery;
    VkBool32                                    vertexSideEffects;
    VkBool32                                    tessellationSideEffects;
    VkBool32                                    geometrySideEffects;
    VkBool32                                    fragmentSideEffects;
    VkBool32                                    shaderTessellationPointSize;
    VkBool32                                    shaderGeometryPointSize;
    VkBool32                                    shaderTextureGatherExtended;
    VkBool32                                    shaderStorageImageExtendedFormats;
    VkBool32                                    shaderStorageImageMultisample;
    VkBool32                                    shaderStorageBufferArrayConstantIndexing;
    VkBool32                                    shaderStorageImageArrayConstantIndexing;
    VkBool32                                    shaderUniformBufferArrayDynamicIndexing;
    VkBool32                                    shaderSampledImageArrayDynamicIndexing;
    VkBool32                                    shaderStorageBufferArrayDynamicIndexing;
    VkBool32                                    shaderStorageImageArrayDynamicIndexing;
    VkBool32                                    shaderClipDistance;
    VkBool32                                    shaderCullDistance;
    VkBool32                                    shaderFloat64;
    VkBool32                                    shaderInt64;
    VkBool32                                    shaderFloat16;
    VkBool32                                    shaderInt16;
    VkBool32                                    shaderResourceResidency;
    VkBool32                                    shaderResourceMinLOD;
    VkBool32                                    sparse;
    VkBool32                                    sparseResidencyBuffer;
    VkBool32                                    sparseResidencyImage2D;
    VkBool32                                    sparseResidencyImage3D;
    VkBool32                                    sparseResidency2Samples;
    VkBool32                                    sparseResidency4Samples;
    VkBool32                                    sparseResidency8Samples;
    VkBool32                                    sparseResidency16Samples;
    VkBool32                                    sparseResidencyStandard2DBlockShape;
    VkBool32                                    sparseResidencyStandard2DMSBlockShape;
    VkBool32                                    sparseResidencyStandard3DBlockShape;
    VkBool32                                    sparseResidencyAlignedMipSize;
    VkBool32                                    sparseResidencyNonResident;
    VkBool32                                    sparseResidencyNonResidentStrict;
    VkBool32                                    sparseResidencyAliased;
} VkPhysicalDeviceFeatures;

typedef struct VkPhysicalDeviceLimits_
{
    uint32_t                                    maxImageDimension1D;
    uint32_t                                    maxImageDimension2D;
    uint32_t                                    maxImageDimension3D;
    uint32_t                                    maxImageDimensionCube;
    uint32_t                                    maxImageArrayLayers;
    uint32_t                                    maxTexelBufferSize;
    uint32_t                                    maxUniformBufferSize;
    uint32_t                                    maxStorageBufferSize;
    uint32_t                                    maxPushConstantsSize;
    uint32_t                                    maxMemoryAllocationCount;
    VkDeviceSize                                maxInlineMemoryUpdateSize;
    uint32_t                                    maxBoundDescriptorSets;
    uint32_t                                    maxDescriptorSets;
    uint32_t                                    maxPerStageDescriptorSamplers;
    uint32_t                                    maxPerStageDescriptorUniformBuffers;
    uint32_t                                    maxPerStageDescriptorStorageBuffers;
    uint32_t                                    maxPerStageDescriptorSampledImages;
    uint32_t                                    maxPerStageDescriptorStorageImages;
    uint32_t                                    maxDescriptorSetSamplers;
    uint32_t                                    maxDescriptorSetUniformBuffers;
    uint32_t                                    maxDescriptorSetStorageBuffers;
    uint32_t                                    maxDescriptorSetSampledImages;
    uint32_t                                    maxDescriptorSetStorageImages;
    uint32_t                                    maxVertexInputAttributes;
    uint32_t                                    maxVertexInputAttributeOffset;
    uint32_t                                    maxVertexInputBindingStride;
    uint32_t                                    maxVertexOutputComponents;
    uint32_t                                    maxTessGenLevel;
    uint32_t                                    maxTessPatchSize;
    uint32_t                                    maxTessControlPerVertexInputComponents;
    uint32_t                                    maxTessControlPerVertexOutputComponents;
    uint32_t                                    maxTessControlPerPatchOutputComponents;
    uint32_t                                    maxTessControlTotalOutputComponents;
    uint32_t                                    maxTessEvaluationInputComponents;
    uint32_t                                    maxTessEvaluationOutputComponents;
    uint32_t                                    maxGeometryShaderInvocations;
    uint32_t                                    maxGeometryInputComponents;
    uint32_t                                    maxGeometryOutputComponents;
    uint32_t                                    maxGeometryOutputVertices;
    uint32_t                                    maxGeometryTotalOutputComponents;
    uint32_t                                    maxFragmentInputComponents;
    uint32_t                                    maxFragmentOutputBuffers;
    uint32_t                                    maxFragmentDualSourceBuffers;
    uint32_t                                    maxFragmentCombinedOutputResources;
    uint32_t                                    maxComputeSharedMemorySize;
    uint32_t                                    maxComputeWorkGroupCount[3];
    uint32_t                                    maxComputeWorkGroupInvocations;
    uint32_t                                    maxComputeWorkGroupSize[3];
    uint32_t                                    subPixelPrecisionBits;
    uint32_t                                    subTexelPrecisionBits;
    uint32_t                                    mipmapPrecisionBits;
    uint32_t                                    maxDrawIndexedIndexValue;
    uint32_t                                    maxDrawIndirectInstanceCount;
    VkBool32                                    primitiveRestartForPatches;
    float                                       maxSamplerLodBias;
    uint32_t                                    maxSamplerAnisotropy;
    uint32_t                                    maxViewports;
    uint32_t                                    maxDynamicViewportStates;
    uint32_t                                    maxViewportDimensions[2];
    float                                       viewportBoundsRange[2];
    uint32_t                                    viewportSubPixelBits;
    uint32_t                                    minMemoryMapAlignment;
    uint32_t                                    minTexelBufferOffsetAlignment;
    uint32_t                                    minUniformBufferOffsetAlignment;
    uint32_t                                    minStorageBufferOffsetAlignment;
    uint32_t                                    minTexelOffset;
    uint32_t                                    maxTexelOffset;
    uint32_t                                    minTexelGatherOffset;
    uint32_t                                    maxTexelGatherOffset;
    uint32_t                                    minInterpolationOffset;
    uint32_t                                    maxInterpolationOffset;
    uint32_t                                    subPixelInterpolationOffsetBits;
    uint32_t                                    maxFramebufferWidth;
    uint32_t                                    maxFramebufferHeight;
    uint32_t                                    maxFramebufferLayers;
    uint32_t                                    maxFramebufferColorSamples;
    uint32_t                                    maxFramebufferDepthSamples;
    uint32_t                                    maxFramebufferStencilSamples;
    uint32_t                                    maxColorAttachments;
    uint32_t                                    maxSampledImageColorSamples;
    uint32_t                                    maxSampledImageDepthSamples;
    uint32_t                                    maxSampledImageIntegerSamples;
    uint32_t                                    maxStorageImageSamples;
    uint32_t                                    maxSampleMaskWords;
    uint64_t                                    timestampFrequency;
    uint32_t                                    maxClipDistances;
    uint32_t                                    maxCullDistances;
    uint32_t                                    maxCombinedClipAndCullDistances;
    float                                       pointSizeRange[2];
    float                                       lineWidthRange[2];
    float                                       pointSizeGranularity;
    float                                       lineWidthGranularity;
} VkPhysicalDeviceLimits;


typedef struct VkPhysicalDevicePerformance_
{
    float                                       maxDeviceClock;
    float                                       aluPerClock;
    float                                       texPerClock;
    float                                       primsPerClock;
    float                                       pixelsPerClock;
} VkPhysicalDevicePerformance;

typedef struct VkExtensionProperties_
{
    char                                        extName[VK_MAX_EXTENSION_NAME];     // extension name
    uint32_t                                    version;                            // version of the extension specification
    uint32_t                                    specVersion;                        // version number constructed via VK_API_VERSION
} VkExtensionProperties;

typedef struct VkLayerProperties_
{
    char                                        layerName[VK_MAX_EXTENSION_NAME];   // extension name
    uint32_t                                    specVersion;                        // version of spec this layer is compatible with
    uint32_t                                    implVersion;                        // version of the layer
    char                                        description[VK_MAX_DESCRIPTION]; // additional description
} VkLayerProperties;

typedef struct VkApplicationInfo_
{
    VkStructureType                             sType;              // Type of structure. Should be VK_STRUCTURE_TYPE_APPLICATION_INFO
    const void*                                 pNext;              // Next structure in chain
    const char*                                 pAppName;
    uint32_t                                    appVersion;
    const char*                                 pEngineName;
    uint32_t                                    engineVersion;
    uint32_t                                    apiVersion;
} VkApplicationInfo;

typedef void* (VKAPI *PFN_vkAllocFunction)(
    void*                                       pUserData,
    size_t                                      size,
    size_t                                      alignment,
    VkSystemAllocType                           allocType);

typedef void (VKAPI *PFN_vkFreeFunction)(
    void*                                       pUserData,
    void*                                       pMem);

typedef struct VkAllocCallbacks_
{
    void*                                       pUserData;
    PFN_vkAllocFunction                         pfnAlloc;
    PFN_vkFreeFunction                          pfnFree;
} VkAllocCallbacks;

typedef struct VkDeviceQueueCreateInfo_
{
    uint32_t                                    queueNodeIndex;
    uint32_t                                    queueCount;
} VkDeviceQueueCreateInfo;

typedef struct VkDeviceCreateInfo_
{
    VkStructureType                             sType;                      // Should be VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
    const void*                                 pNext;                      // Pointer to next structure
    uint32_t                                    queueRecordCount;
    const VkDeviceQueueCreateInfo*              pRequestedQueues;
    uint32_t                                    layerCount;
    const char*const*                           ppEnabledLayerNames;        // Indicate extensions to enable by index value
    uint32_t                                    extensionCount;
    const char*const*                           ppEnabledExtensionNames;    // Indicate extensions to enable by index value
    const VkPhysicalDeviceFeatures*             pEnabledFeatures;
    VkDeviceCreateFlags                         flags;                      // Device creation flags
} VkDeviceCreateInfo;

typedef struct VkInstanceCreateInfo_
{
    VkStructureType                             sType;                      // Should be VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    const void*                                 pNext;                      // Pointer to next structure
    const VkApplicationInfo*                    pAppInfo;
    const VkAllocCallbacks*                     pAllocCb;
    uint32_t                                    layerCount;
    const char*const*                           ppEnabledLayerNames;        // Indicate extensions to enable by index value
    uint32_t                                    extensionCount;
    const char*const*                           ppEnabledExtensionNames;    // Indicate extensions to enable by index value
} VkInstanceCreateInfo;

typedef struct VkPhysicalDeviceQueueProperties_
{
    VkQueueFlags                                queueFlags;                 // Queue flags
    uint32_t                                    queueCount;
    VkBool32                                    supportsTimestamps;
} VkPhysicalDeviceQueueProperties;

typedef struct VkMemoryType_
{
    VkMemoryPropertyFlags                       propertyFlags;              // Memory properties of this memory type
    uint32_t                                    heapIndex;                  // Index of the memory heap allocations of this memory type are taken from
} VkMemoryType;

typedef struct VkMemoryHeap_
{
    VkDeviceSize                                size;                       // Available memory in the heap
    VkMemoryHeapFlags                           flags;                      // Flags for the heap
} VkMemoryHeap;

typedef struct VkPhysicalDeviceMemoryProperties_
{
    uint32_t                                    memoryTypeCount;
    VkMemoryType                                memoryTypes[VK_MAX_MEMORY_TYPES];
    uint32_t                                    memoryHeapCount;
    VkMemoryHeap                                memoryHeaps[VK_MAX_MEMORY_HEAPS];
} VkPhysicalDeviceMemoryProperties;

typedef struct VkMemoryAllocInfo_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO
    const void*                                 pNext;                      // Pointer to next structure
    VkDeviceSize                                allocationSize;             // Size of memory allocation
    uint32_t                                    memoryTypeIndex;            // Index of the memory type to allocate from
} VkMemoryAllocInfo;

typedef struct VkMappedMemoryRange_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE
    const void*                                 pNext;                      // Pointer to next structure
    VkDeviceMemory                              mem;                        // Mapped memory object
    VkDeviceSize                                offset;                     // Offset within the mapped memory the range starts from
    VkDeviceSize                                size;                       // Size of the range within the mapped memory
} VkMappedMemoryRange;

typedef struct VkMemoryRequirements_
{
    VkDeviceSize                                size;                       // Specified in bytes
    VkDeviceSize                                alignment;                  // Specified in bytes
    VkDeviceSize                                granularity;                // Granularity at which memory can be bound to resource sub-ranges specified in bytes (usually the page size)
    uint32_t                                    memoryTypeBits;             // Bitfield of the allowed memory type indices into memoryTypes[] for this object
} VkMemoryRequirements;

typedef struct VkSparseImageFormatProperties_
{
    VkImageAspect                               aspect;
    VkExtent3D                                  imageGranularity;
    VkSparseImageFormatFlags                    flags;
} VkSparseImageFormatProperties;

typedef struct VkSparseImageMemoryRequirements_
{
    VkSparseImageFormatProperties               formatProps;
    uint32_t                                    imageMipTailStartLOD;
    VkDeviceSize                                imageMipTailSize;
    VkDeviceSize                                imageMipTailOffset;
    VkDeviceSize                                imageMipTailStride;
} VkSparseImageMemoryRequirements;

typedef struct VkImageSubresource_
{
    VkImageAspect                               aspect;
    uint32_t                                    mipLevel;
    uint32_t                                    arraySlice;
} VkImageSubresource;

typedef struct VkSparseMemoryBindInfo
{
    VkDeviceSize                                offset;
    VkDeviceSize                                memOffset;
    VkDeviceMemory                              mem;
    VkSparseMemoryBindFlags                     flags;
} VkSparseMemoryBindInfo;

typedef struct VkFormatProperties_
{
    VkFormatFeatureFlags                        linearTilingFeatures;       // Format features in case of linear tiling
    VkFormatFeatureFlags                        optimalTilingFeatures;      // Format features in case of optimal tiling
} VkFormatProperties;

typedef struct VkDescriptorInfo_
{
    VkBufferView                                bufferView;                 // Buffer view to write to the descriptor (in case it's a buffer descriptor, otherwise should be VK_NULL_HANDLE)
    VkSampler                                   sampler;                    // Sampler to write to the descriptor (in case it's a SAMPLER or COMBINED_IMAGE_SAMPLER descriptor, otherwise should be VK_NULL_HANDLE)
    VkImageView                                 imageView;                  // Image view to write to the descriptor (in case it's a SAMPLED_IMAGE, STORAGE_IMAGE, or COMBINED_IMAGE_SAMPLER descriptor, otherwise should be VK_NULL_HANDLE)
    VkAttachmentView                            attachmentView;
    VkImageLayout                               imageLayout;                // Layout the image is expected to be in when accessed using this descriptor (only used if <imageView> is not VK_NULL_HANDLE)
} VkDescriptorInfo;

typedef struct VkWriteDescriptorSet_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
    const void*                                 pNext;                      // Pointer to next structure

    VkDescriptorSet                             destSet;                    // Destination descriptor set
    uint32_t                                    destBinding;                // Binding within the destination descriptor set to write
    uint32_t                                    destArrayElement;           // Array element within the destination binding to write

    uint32_t                                    count;                      // Number of descriptors to write (determines the size of the array pointed by <pDescriptors>)

    VkDescriptorType                            descriptorType;             // Descriptor type to write (determines which fields of the array pointed by <pDescriptors> are going to be used)
    const VkDescriptorInfo*                     pDescriptors;               // Array of info structures describing the descriptors to write
} VkWriteDescriptorSet;

typedef struct VkCopyDescriptorSet_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET
    const void*                                 pNext;                      // Pointer to next structure

    VkDescriptorSet                             srcSet;                     // Source descriptor set
    uint32_t                                    srcBinding;                 // Binding within the source descriptor set to copy from
    uint32_t                                    srcArrayElement;            // Array element within the source binding to copy from

    VkDescriptorSet                             destSet;                    // Destination descriptor set
    uint32_t                                    destBinding;                // Binding within the destination descriptor set to copy to
    uint32_t                                    destArrayElement;           // Array element within the destination binding to copy to

    uint32_t                                    count;                      // Number of descriptors to copy
} VkCopyDescriptorSet;

typedef struct VkBufferCreateInfo_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
    const void*                                 pNext;                      // Pointer to next structure.
    VkDeviceSize                                size;                       // Specified in bytes
    VkBufferUsageFlags                          usage;                      // Buffer usage flags
    VkBufferCreateFlags                         flags;                      // Buffer creation flags
} VkBufferCreateInfo;

typedef struct VkBufferViewCreateInfo_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO
    const void*                                 pNext;                      // Pointer to next structure.
    VkBuffer                                    buffer;
    VkBufferViewType                            viewType;
    VkFormat                                    format;                     // Optionally specifies format of elements
    VkDeviceSize                                offset;                     // Specified in bytes
    VkDeviceSize                                range;                      // View size specified in bytes
} VkBufferViewCreateInfo;

typedef struct VkImageSubresourceRange_
{
    VkImageAspect                               aspect;
    uint32_t                                    baseMipLevel;
    uint32_t                                    mipLevels;
    uint32_t                                    baseArraySlice;
    uint32_t                                    arraySize;
} VkImageSubresourceRange;

typedef struct VkMemoryBarrier_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_MEMORY_BARRIER
    const void*                                 pNext;                      // Pointer to next structure.

    VkMemoryOutputFlags                         outputMask;                 // Outputs the barrier should sync
    VkMemoryInputFlags                          inputMask;                  // Inputs the barrier should sync to
} VkMemoryBarrier;

typedef struct VkBufferMemoryBarrier_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER
    const void*                                 pNext;                      // Pointer to next structure.

    VkMemoryOutputFlags                         outputMask;                 // Outputs the barrier should sync
    VkMemoryInputFlags                          inputMask;                  // Inputs the barrier should sync to

    VkBuffer                                    buffer;                     // Buffer to sync

    VkDeviceSize                                offset;                     // Offset within the buffer to sync
    VkDeviceSize                                size;                       // Amount of bytes to sync
} VkBufferMemoryBarrier;

typedef struct VkImageMemoryBarrier_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER
    const void*                                 pNext;                      // Pointer to next structure.

    VkMemoryOutputFlags                         outputMask;                 // Outputs the barrier should sync
    VkMemoryInputFlags                          inputMask;                  // Inputs the barrier should sync to

    VkImageLayout                               oldLayout;                  // Current layout of the image
    VkImageLayout                               newLayout;                  // New layout to transition the image to

    VkImage                                     image;                      // Image to sync

    VkImageSubresourceRange                     subresourceRange;           // Subresource range to sync
} VkImageMemoryBarrier;

typedef struct VkImageCreateInfo_
{
    VkStructureType                             sType;                      // Must be VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
    const void*                                 pNext;                      // Pointer to next structure.
    VkImageType                                 imageType;
    VkFormat                                    format;
    VkExtent3D                                  extent;
    uint32_t                                    mipLevels;
    uint32_t                                    arraySize;
    uint32_t                                    samples;
    VkImageTiling                               tiling;
    VkImageUsageFlags                           usage;                      // Image usage flags
    VkImageCreateFlags                          flags;                      // Image creation flags
} VkImageCreateInfo;

typedef struct VkSubresourceLayout_
{
    VkDeviceSize                                offset;                 // Specified in bytes
    VkDeviceSize                                size;                   // Specified in bytes
    VkDeviceSize                                rowPitch;               // Specified in bytes
    VkDeviceSize                                depthPitch;             // Specified in bytes
} VkSubresourceLayout;

typedef struct VkImageViewCreateInfo_
{
    VkStructureType                             sType;                  // Must be VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
    const void*                                 pNext;                  // Pointer to next structure
    VkImage                                     image;
    VkImageViewType                             viewType;
    VkFormat                                    format;
    VkChannelMapping                            channels;
    VkImageSubresourceRange                     subresourceRange;
} VkImageViewCreateInfo;

typedef struct VkAttachmentViewCreateInfo_
{
    VkStructureType                             sType;                  // Must be VK_STRUCTURE_TYPE_COLOR_ATTACHMENT_VIEW_CREATE_INFO
    const void*                                 pNext;                  // Pointer to next structure
    VkImage                                     image;
    VkFormat                                    format;
    uint32_t                                    mipLevel;
    uint32_t                                    baseArraySlice;
    uint32_t                                    arraySize;
    VkAttachmentViewCreateFlags                 flags;                  // attachment view flags
} VkAttachmentViewCreateInfo;

typedef struct VkBufferCopy_
{
    VkDeviceSize                                srcOffset;              // Specified in bytes
    VkDeviceSize                                destOffset;             // Specified in bytes
    VkDeviceSize                                copySize;               // Specified in bytes
} VkBufferCopy;

typedef struct VkSparseImageMemoryBindInfo_
{
    VkImageSubresource                          subresource;
    VkOffset3D                                  offset;
    VkExtent3D                                  extent;
    VkDeviceSize                                memOffset;
    VkDeviceMemory                              mem;
    VkSparseMemoryBindFlags                     flags;
} VkSparseImageMemoryBindInfo;

typedef struct VkImageCopy_
{
    VkImageSubresource                          srcSubresource;
    VkOffset3D                                  srcOffset;             // Specified in pixels for both compressed and uncompressed images
    VkImageSubresource                          destSubresource;
    VkOffset3D                                  destOffset;            // Specified in pixels for both compressed and uncompressed images
    VkExtent3D                                  extent;                // Specified in pixels for both compressed and uncompressed images
} VkImageCopy;

typedef struct VkImageBlit_
{
    VkImageSubresource                          srcSubresource;
    VkOffset3D                                  srcOffset;              // Specified in pixels for both compressed and uncompressed images
    VkExtent3D                                  srcExtent;              // Specified in pixels for both compressed and uncompressed images
    VkImageSubresource                          destSubresource;
    VkOffset3D                                  destOffset;             // Specified in pixels for both compressed and uncompressed images
    VkExtent3D                                  destExtent;             // Specified in pixels for both compressed and uncompressed images
} VkImageBlit;

typedef struct VkBufferImageCopy_
{
    VkDeviceSize                                bufferOffset;           // Specified in bytes
    VkImageSubresource                          imageSubresource;
    VkOffset3D                                  imageOffset;            // Specified in pixels for both compressed and uncompressed images
    VkExtent3D                                  imageExtent;            // Specified in pixels for both compressed and uncompressed images
} VkBufferImageCopy;

typedef struct VkImageResolve_
{
    VkImageSubresource                          srcSubresource;
    VkOffset3D                                  srcOffset;
    VkImageSubresource                          destSubresource;
    VkOffset3D                                  destOffset;
    VkExtent3D                                  extent;
} VkImageResolve;

typedef struct VkShaderModuleCreateInfo_
{
    VkStructureType                             sType;              // Must be VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    const void*                                 pNext;              // Pointer to next structure
    size_t                                      codeSize;           // Specified in bytes
    const void*                                 pCode;              // Binary code of size codeSize
    VkShaderModuleCreateFlags                   flags;              // Reserved
} VkShaderModuleCreateInfo;

typedef struct VkShaderCreateInfo_
{
    VkStructureType                             sType;              // Must be VK_STRUCTURE_TYPE_SHADER_CREATE_INFO
    const void*                                 pNext;              // Pointer to next structure
    VkShaderModule                              module;             // Module containing entry point
    const char*                                 pName;              // Null-terminate entry point name
    VkShaderCreateFlags                         flags;              // Reserved
} VkShaderCreateInfo;

typedef struct VkPipelineCacheCreateInfo_
{
    VkStructureType                             sType;
    const void*                                 pNext;
    size_t                                      initialSize;
    const void*                                 initialData;
    size_t                                      maxSize;
} VkPipelineCacheCreateInfo;

typedef struct VkDescriptorSetLayoutBinding_
{
    VkDescriptorType                            descriptorType;     // Type of the descriptors in this binding
    uint32_t                                    arraySize;          // Number of descriptors in this binding
    VkShaderStageFlags                          stageFlags;         // Shader stages this binding is visible to
    const VkSampler*                            pImmutableSamplers; // Immutable samplers (used if descriptor type is SAMPLER or COMBINED_IMAGE_SAMPLER, is either NULL or contains <count> number of elements)
} VkDescriptorSetLayoutBinding;

typedef struct VkDescriptorSetLayoutCreateInfo_
{
    VkStructureType                             sType;              // Must be VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
    const void*                                 pNext;              // Pointer to next structure
    uint32_t                                    count;              // Number of bindings in the descriptor set layout
    const VkDescriptorSetLayoutBinding*         pBinding;           // Array of descriptor set layout bindings
} VkDescriptorSetLayoutCreateInfo;

typedef struct VkDescriptorTypeCount_
{
    VkDescriptorType                            type;
    uint32_t                                    count;
} VkDescriptorTypeCount;

typedef struct VkDescriptorPoolCreateInfo_
{
    VkStructureType                             sType;              // Must be VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO
    const void*                                 pNext;              // Pointer to next structure
    uint32_t                                    count;
    const VkDescriptorTypeCount*                pTypeCount;
} VkDescriptorPoolCreateInfo;

typedef struct VkLinkConstBuffer_
{
    uint32_t                                    bufferId;
    size_t                                      bufferSize;
    const void*                                 pBufferData;
} VkLinkConstBuffer;

typedef struct VkSpecializationMapEntry_
{
    uint32_t                                    constantId;         // The SpecConstant ID specified in the BIL
    uint32_t                                    offset;             // Offset of the value in the data block
} VkSpecializationMapEntry;

typedef struct VkSpecializationInfo_
{
    uint32_t                                    mapEntryCount;
    const VkSpecializationMapEntry*             pMap;               // mapEntryCount entries
    const void*                                 pData;
} VkSpecializationInfo;

typedef struct VkPipelineShaderStageCreateInfo_
{
    VkStructureType                             sType;          // Must be VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
    const void*                                 pNext;          // Pointer to next structure
    VkShaderStage                               stage;
    VkShader                                    shader;
    uint32_t                                    linkConstBufferCount;
    const VkLinkConstBuffer*                    pLinkConstBufferInfo;
    const VkSpecializationInfo*                 pSpecializationInfo;
} VkPipelineShaderStageCreateInfo;

typedef struct VkComputePipelineCreateInfo_
{
    VkStructureType                             sType;          // Must be VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    const void*                                 pNext;          // Pointer to next structure
    VkPipelineShaderStageCreateInfo             cs;
    VkPipelineCreateFlags                       flags;          // Pipeline creation flags
    VkPipelineLayout                            layout;         // Interface layout of the pipeline
    VkPipeline                                  basePipelineHandle;
    int32_t                                     basePipelineIndex;
} VkComputePipelineCreateInfo;

typedef struct VkVertexInputBindingDescription_
{
    uint32_t                                    binding;        // Vertex buffer binding id
    uint32_t                                    strideInBytes;  // Distance between vertices in bytes (0 = no advancement)

    VkVertexInputStepRate                       stepRate;       // Rate at which binding is incremented
} VkVertexInputBindingDescription;

typedef struct VkVertexInputAttributeDescription_
{
    uint32_t                                    location;       // location of the shader vertex attrib
    uint32_t                                    binding;        // Vertex buffer binding id

    VkFormat                                    format;         // format of source data

    uint32_t                                    offsetInBytes;  // Offset of first element in bytes from base of vertex
} VkVertexInputAttributeDescription;

typedef struct VkPipelineVertexInputStateCreateInfo_
{
    VkStructureType                             sType;          // Should be VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    const void*                                 pNext;          // Pointer to next structure

    uint32_t                                    bindingCount;   // number of bindings
    const VkVertexInputBindingDescription*      pVertexBindingDescriptions;

    uint32_t                                    attributeCount; // number of attributes
    const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
} VkPipelineVertexInputStateCreateInfo;

typedef struct VkPipelineInputAssemblyStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_PIPELINE_IA_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    VkPrimitiveTopology                         topology;
    VkBool32                                    disableVertexReuse;         // optional
    VkBool32                                    primitiveRestartEnable;
    uint32_t                                    primitiveRestartIndex;      // optional (GL45)
} VkPipelineInputAssemblyStateCreateInfo;

typedef struct VkPipelineTessellationStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_PIPELINE_TESS_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    uint32_t                                    patchControlPoints;
} VkPipelineTessellationStateCreateInfo;

typedef struct VkPipelineViewportStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_PIPELINE_VP_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    uint32_t                                    viewportCount;
    VkCoordinateOrigin                          clipOrigin;                 // optional (GL45)
    VkDepthMode                                 depthMode;                  // optional (GL45)
} VkPipelineViewportStateCreateInfo;

typedef struct VkPipelineRasterStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_PIPELINE_RS_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    VkBool32                                    depthClipEnable;
    VkBool32                                    rasterizerDiscardEnable;
    VkCoordinateOrigin                          pointOrigin;                // optional (GL45)
    VkProvokingVertex                           provokingVertex;            // optional (GL45)
    VkFillMode                                  fillMode;                   // optional (GL45)
    VkCullMode                                  cullMode;
    VkFrontFace                                 frontFace;
} VkPipelineRasterStateCreateInfo;

typedef struct VkPipelineMultisampleStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_PIPELINE_MS_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    uint32_t                                    rasterSamples;
    VkBool32                                    multisampleEnable;          // optional (GL45)
    VkBool32                                    sampleShadingEnable;        // optional (GL45)
    float                                       minSampleShading;           // optional (GL45)
    VkSampleMask                                sampleMask;
} VkPipelineMultisampleStateCreateInfo;

typedef struct VkPipelineColorBlendAttachmentState_
{
    VkBool32                                    blendEnable;
    VkFormat                                    format;
    VkBlend                                     srcBlendColor;
    VkBlend                                     destBlendColor;
    VkBlendOp                                   blendOpColor;
    VkBlend                                     srcBlendAlpha;
    VkBlend                                     destBlendAlpha;
    VkBlendOp                                   blendOpAlpha;
    VkChannelFlags                              channelWriteMask;
} VkPipelineColorBlendAttachmentState;

typedef struct VkPipelineColorBlendStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_PIPELINE_CB_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    VkBool32                                    alphaToCoverageEnable;
    VkBool32                                    logicOpEnable;
    VkLogicOp                                   logicOp;
    uint32_t                                    attachmentCount;    // # of pAttachments
    const VkPipelineColorBlendAttachmentState*          pAttachments;
} VkPipelineColorBlendStateCreateInfo;

typedef struct VkStencilOpState_
{
    VkStencilOp                                 stencilFailOp;
    VkStencilOp                                 stencilPassOp;
    VkStencilOp                                 stencilDepthFailOp;
    VkCompareOp                                 stencilCompareOp;
} VkStencilOpState;

typedef struct VkPipelineDepthStencilStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_PIPELINE_DS_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    VkBool32                                    depthTestEnable;
    VkBool32                                    depthWriteEnable;
    VkCompareOp                                 depthCompareOp;
    VkBool32                                    depthBoundsEnable;          // optional (depth_bounds_test)
    VkBool32                                    stencilTestEnable;
    VkStencilOpState                            front;
    VkStencilOpState                            back;
} VkPipelineDepthStencilStateCreateInfo;

typedef struct VkGraphicsPipelineCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    uint32_t                                    stageCount;
    const VkPipelineShaderStageCreateInfo*      pStages;    // One entry for each active shader stage
    const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
    const VkPipelineInputAssemblyStateCreateInfo*   pInputAssemblyState;
    const VkPipelineTessellationStateCreateInfo*    pTessellationState;
    const VkPipelineViewportStateCreateInfo*        pViewportState;
    const VkPipelineRasterStateCreateInfo*          pRasterState;
    const VkPipelineMultisampleStateCreateInfo*     pMultisampleState;
    const VkPipelineDepthStencilStateCreateInfo*    pDepthStencilState;
    const VkPipelineColorBlendStateCreateInfo*      pColorBlendState;
    VkPipelineCreateFlags                       flags;      // Pipeline creation flags
    VkPipelineLayout                            layout;     // Interface layout of the pipeline
    VkRenderPass                                renderPass;
    uint32_t                                    subpass;
    VkPipeline                                  basePipelineHandle;
    int32_t                                     basePipelineIndex;
} VkGraphicsPipelineCreateInfo;

typedef struct VkPipelineLayoutCreateInfo_
{
    VkStructureType                             sType;              // Must be VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    const void*                                 pNext;              // Pointer to next structure

    uint32_t                                    descriptorSetCount; // Number of descriptor sets interfaced by the pipeline
    const VkDescriptorSetLayout*                pSetLayouts;        // Array of <setCount> number of descriptor set layout objects defining the layout of the
} VkPipelineLayoutCreateInfo;

typedef struct VkSamplerCreateInfo_
{
    VkStructureType                             sType;          // Must be VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO
    const void*                                 pNext;          // Pointer to next structure
    VkTexFilter                                 magFilter;      // Filter mode for magnification
    VkTexFilter                                 minFilter;      // Filter mode for minifiation
    VkTexMipmapMode                             mipMode;        // Mipmap selection mode
    VkTexAddress                                addressU;
    VkTexAddress                                addressV;
    VkTexAddress                                addressW;
    float                                       mipLodBias;
    uint32_t                                    maxAnisotropy;
    VkCompareOp                                 compareOp;
    float                                       minLod;
    float                                       maxLod;
    VkBorderColor                               borderColor;
} VkSamplerCreateInfo;

typedef struct VkDynamicViewportStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_DYNAMIC_VIEWPORT_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    uint32_t                                    viewportAndScissorCount;  // number of entries in pViewports and pScissors
    const VkViewport*                           pViewports;
    const VkRect2D*                             pScissors;
} VkDynamicViewportStateCreateInfo;

typedef struct VkDynamicRasterStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_DYNAMIC_RASTER_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    float                                       depthBias;
    float                                       depthBiasClamp;
    float                                       slopeScaledDepthBias;
    float                                       lineWidth;          // optional (GL45) - Width of lines
} VkDynamicRasterStateCreateInfo;

typedef struct VkDynamicColorBlendStateCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_DYNAMIC_COLOR_BLEND_STATE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    float                                       blendConst[4];
} VkDynamicColorBlendStateCreateInfo;

typedef struct VkDynamicDepthStencilStateCreateInfo_
{
    VkStructureType                             sType;              // Must be VK_STRUCTURE_TYPE_DYNAMIC_DEPTH_STENCIL_STATE_CREATE_INFO
    const void*                                 pNext;              // Pointer to next structure
    float                                       minDepthBounds;     // optional (depth_bounds_test)
    float                                       maxDepthBounds;     // optional (depth_bounds_test)
    uint32_t                                    stencilReadMask;
    uint32_t                                    stencilWriteMask;
    uint32_t                                    stencilFrontRef;
    uint32_t                                    stencilBackRef;
} VkDynamicDepthStencilStateCreateInfo;

typedef struct VkCmdBufferCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    uint32_t                                    queueNodeIndex;
    VkCmdBufferLevel                            level;
    VkCmdBufferCreateFlags                      flags;      // Command buffer creation flags
} VkCmdBufferCreateInfo;

typedef struct VkCmdBufferBeginInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO
    const void*                                 pNext;      // Pointer to next structure

    VkCmdBufferOptimizeFlags                    flags;      // Command buffer optimization flags

    VkRenderPass                                renderPass;
    VkFramebuffer                               framebuffer;
} VkCmdBufferBeginInfo;

// Union allowing specification of floating point, integer, or unsigned integer color data. Actual value selected is based on format.
typedef union VkClearColorValue_
{
    float                                       f32[4];
    int32_t                                     s32[4];
    uint32_t                                    u32[4];
} VkClearColorValue;

typedef struct VkAttachmentBindInfo_
{
    VkAttachmentView                            view;
    VkImageLayout                               layout;
} VkAttachmentBindInfo;

typedef struct VkFramebufferCreateInfo_
{
    VkStructureType                             sType;  // Must be VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO
    const void*                                 pNext;  // Pointer to next structure

    VkRenderPass                                renderPass;
    uint32_t                                    attachmentCount;
    const VkAttachmentBindInfo*                 pAttachments;

    uint32_t                                    width;
    uint32_t                                    height;
    uint32_t                                    layers;
} VkFramebufferCreateInfo;

typedef struct VkAttachmentDescription_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION
    const void*                                 pNext;      // Pointer to next structure

    VkFormat                                    format;
    uint32_t                                    samples;
    VkAttachmentLoadOp                          loadOp;
    VkAttachmentStoreOp                         storeOp;
    VkAttachmentLoadOp                          stencilLoadOp;
    VkAttachmentStoreOp                         stencilStoreOp;
    VkImageLayout                               initialLayout;
    VkImageLayout                               finalLayout;
} VkAttachmentDescription;

typedef struct VkAttachmentReference_
{
    uint32_t                                    attachment;
    VkImageLayout                               layout;
} VkAttachmentReference;

typedef struct VkSubpassDescription_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION
    const void*                                 pNext;

    VkPipelineBindPoint                         pipelineBindPoint;
    VkSubpassDescriptionFlags                   flags;
    uint32_t                                    inputCount;
    const VkAttachmentReference*                inputAttachments;
    uint32_t                                    colorCount;
    const VkAttachmentReference*                colorAttachments;
    const VkAttachmentReference*                resolveAttachments;
    VkAttachmentReference                       depthStencilAttachment;
    uint32_t                                    preserveCount;
    const VkAttachmentReference*                preserveAttachments;
} VkSubpassDescription;

typedef struct VkSubpassDependency_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY
    const void*                                 pNext;
    uint32_t                                    srcSubpass;
    uint32_t                                    destSubpass;
    VkPipelineStageFlags                        srcStageMask;
    VkPipelineStageFlags                        destStageMask;
    VkMemoryOutputFlags                         outputMask;
    VkMemoryInputFlags                          inputMask;
    VkBool32                                    byRegion;
} VkSubpassDependency;

typedef struct VkRenderPassCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure

    uint32_t                                    attachmentCount;
    const VkAttachmentDescription*              pAttachments;
    uint32_t                                    subpassCount;
    const VkSubpassDescription*                 pSubpasses;
    uint32_t                                    dependencyCount;
    const VkSubpassDependency*                  pDependencies;
} VkRenderPassCreateInfo;

typedef struct VkEventCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_EVENT_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    VkEventCreateFlags                          flags;      // Event creation flags
} VkEventCreateInfo;

typedef struct VkFenceCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    VkFenceCreateFlags                          flags;      // Fence creation flags
} VkFenceCreateInfo;

typedef struct VkSemaphoreCreateInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    const void*                                 pNext;      // Pointer to next structure
    VkSemaphoreCreateFlags                      flags;      // Semaphore creation flags
} VkSemaphoreCreateInfo;

typedef struct VkQueryPoolCreateInfo_
{
    VkStructureType                             sType;              // Must be VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO
    const void*                                 pNext;              // Pointer to next structure
    VkQueryType                                 queryType;
    uint32_t                                    slots;
    VkQueryPipelineStatisticFlags               pipelineStatistics; // Optional
} VkQueryPoolCreateInfo;

typedef struct VkClearDepthStencilValue_
{
    float                                       depth;
    uint32_t                                    stencil;
} VkClearDepthStencilValue;

typedef union VkClearValue_
{
    VkClearColorValue                           color;
    VkClearDepthStencilValue                    ds;
} VkClearValue;

typedef struct VkRenderPassBeginInfo_
{
    VkStructureType                             sType;      // Must be VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO
    const void*                                 pNext;      // Pointer to next structure

    VkRenderPass                                renderPass;
    VkFramebuffer                               framebuffer;
    VkRect2D                                    renderArea;
    uint32_t                                    attachmentCount;
    const VkClearValue*                         pAttachmentClearValues;
} VkRenderPassBeginInfo;

typedef struct VkDrawIndirectCmd_
{
    uint32_t                                    vertexCount;
    uint32_t                                    instanceCount;
    uint32_t                                    firstVertex;
    uint32_t                                    firstInstance;
} VkDrawIndirectCmd;

typedef struct VkDrawIndexedIndirectCmd_
{
    uint32_t                                    indexCount;
    uint32_t                                    instanceCount;
    uint32_t                                    firstIndex;
    int32_t                                     vertexOffset;
    uint32_t                                    firstInstance;
} VkDrawIndexedIndirectCmd;

typedef struct VkDispatchIndirectCmd_
{
    uint32_t                                    x;
    uint32_t                                    y;
    uint32_t                                    z;
} VkDispatchIndirectCmd;

// ------------------------------------------------------------------------------------------------
// API functions
typedef VkResult (VKAPI *PFN_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance);
typedef VkResult (VKAPI *PFN_vkDestroyInstance)(VkInstance instance);
typedef VkResult (VKAPI *PFN_vkEnumeratePhysicalDevices)(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceFeatures)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceFormatInfo)(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties *pFormatInfo);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceLimits)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceLimits* pLimits);
typedef void *   (VKAPI *PFN_vkGetInstanceProcAddr)(VkInstance instance, const char * pName);
typedef void *   (VKAPI *PFN_vkGetDeviceProcAddr)(VkDevice device, const char * pName);
typedef VkResult (VKAPI *PFN_vkCreateDevice)(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice);
typedef VkResult (VKAPI *PFN_vkDestroyDevice)(VkDevice device);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDevicePerformance)(VkPhysicalDevice physicalDevice, VkPhysicalDevicePerformance* pPerformance);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceQueueCount)(VkPhysicalDevice physicalDevice, uint32_t* pCount);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceQueueProperties)(VkPhysicalDevice physicalDevice, uint32_t count, VkPhysicalDeviceQueueProperties* pQueueProperties);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);
typedef VkResult (VKAPI *PFN_vkGetGlobalExtensionProperties)(const char * pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceExtensionProperties)(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t *pCount, VkExtensionProperties* pProperties);
typedef VkResult (VKAPI *PFN_vkGetGlobalLayerProperties)(uint32_t* pCount, VkLayerProperties* pProperties);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceLayerProperties)(VkPhysicalDevice physicalDevice, uint32_t *pCount, VkLayerProperties* pProperties);
typedef VkResult (VKAPI *PFN_vkGetDeviceQueue)(VkDevice device, uint32_t queueNodeIndex, uint32_t queueIndex, VkQueue* pQueue);
typedef VkResult (VKAPI *PFN_vkQueueSubmit)(VkQueue queue, uint32_t cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence);
typedef VkResult (VKAPI *PFN_vkQueueWaitIdle)(VkQueue queue);
typedef VkResult (VKAPI *PFN_vkDeviceWaitIdle)(VkDevice device);
typedef VkResult (VKAPI *PFN_vkAllocMemory)(VkDevice device, const VkMemoryAllocInfo* pAllocInfo, VkDeviceMemory* pMem);
typedef VkResult (VKAPI *PFN_vkFreeMemory)(VkDevice device, VkDeviceMemory mem);
typedef VkResult (VKAPI *PFN_vkMapMemory)(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData);
typedef VkResult (VKAPI *PFN_vkUnmapMemory)(VkDevice device, VkDeviceMemory mem);
typedef VkResult (VKAPI *PFN_vkFlushMappedMemoryRanges)(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
typedef VkResult (VKAPI *PFN_vkInvalidateMappedMemoryRanges)(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
typedef VkResult (VKAPI *PFN_vkGetDeviceMemoryCommitment)(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes);
typedef VkResult (VKAPI *PFN_vkBindBufferMemory)(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset);
typedef VkResult (VKAPI *PFN_vkBindImageMemory)(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset);
typedef VkResult (VKAPI *PFN_vkGetBufferMemoryRequirements)(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements);
typedef VkResult (VKAPI *PFN_vkGetImageMemoryRequirements)(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements);
typedef VkResult (VKAPI *PFN_vkGetImageSparseMemoryRequirements)(VkDevice device, VkImage image, uint32_t* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements);
typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceSparseImageFormatProperties)(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, uint32_t samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pNumProperties, VkSparseImageFormatProperties* pProperties);
typedef VkResult (VKAPI *PFN_vkQueueBindSparseBufferMemory)(VkQueue queue, VkBuffer buffer, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo);
typedef VkResult (VKAPI *PFN_vkQueueBindSparseImageOpaqueMemory)(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo);
typedef VkResult (VKAPI *PFN_vkQueueBindSparseImageMemory)(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseImageMemoryBindInfo* pBindInfo);
typedef VkResult (VKAPI *PFN_vkCreateFence)(VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence);
typedef VkResult (VKAPI *PFN_vkDestroyFence)(VkDevice device, VkFence fence);
typedef VkResult (VKAPI *PFN_vkResetFences)(VkDevice device, uint32_t fenceCount, const VkFence* pFences);
typedef VkResult (VKAPI *PFN_vkGetFenceStatus)(VkDevice device, VkFence fence);
typedef VkResult (VKAPI *PFN_vkWaitForFences)(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout);
typedef VkResult (VKAPI *PFN_vkCreateSemaphore)(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore);
typedef VkResult (VKAPI *PFN_vkDestroySemaphore)(VkDevice device, VkSemaphore semaphore);
typedef VkResult (VKAPI *PFN_vkQueueSignalSemaphore)(VkQueue queue, VkSemaphore semaphore);
typedef VkResult (VKAPI *PFN_vkQueueWaitSemaphore)(VkQueue queue, VkSemaphore semaphore);
typedef VkResult (VKAPI *PFN_vkCreateEvent)(VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent);
typedef VkResult (VKAPI *PFN_vkDestroyEvent)(VkDevice device, VkEvent event);
typedef VkResult (VKAPI *PFN_vkGetEventStatus)(VkDevice device, VkEvent event);
typedef VkResult (VKAPI *PFN_vkSetEvent)(VkDevice device, VkEvent event);
typedef VkResult (VKAPI *PFN_vkResetEvent)(VkDevice device, VkEvent event);
typedef VkResult (VKAPI *PFN_vkCreateQueryPool)(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool);
typedef VkResult (VKAPI *PFN_vkDestroyQueryPool)(VkDevice device, VkQueryPool queryPool);
typedef VkResult (VKAPI *PFN_vkGetQueryPoolResults)(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t* pDataSize, void* pData, VkQueryResultFlags flags);
typedef VkResult (VKAPI *PFN_vkCreateBuffer)(VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer);
typedef VkResult (VKAPI *PFN_vkDestroyBuffer)(VkDevice device, VkBuffer buffer);
typedef VkResult (VKAPI *PFN_vkCreateBufferView)(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView);
typedef VkResult (VKAPI *PFN_vkDestroyBufferView)(VkDevice device, VkBufferView bufferView);
typedef VkResult (VKAPI *PFN_vkCreateImage)(VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage);
typedef VkResult (VKAPI *PFN_vkDestroyImage)(VkDevice device, VkImage image);
typedef VkResult (VKAPI *PFN_vkGetImageSubresourceLayout)(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout);
typedef VkResult (VKAPI *PFN_vkCreateImageView)(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView);
typedef VkResult (VKAPI *PFN_vkDestroyImageView)(VkDevice device, VkImageView imageView);
typedef VkResult (VKAPI *PFN_vkCreateAttachmentView)(VkDevice device, const VkAttachmentViewCreateInfo* pCreateInfo, VkAttachmentView* pView);
typedef VkResult (VKAPI *PFN_vkDestroyAttachmentView)(VkDevice device, VkAttachmentView attachmentView);
typedef VkResult (VKAPI *PFN_vkCreateShaderModule)(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule);
typedef VkResult (VKAPI *PFN_vkDestroyShaderModule)(VkDevice device, VkShaderModule shaderModule);
typedef VkResult (VKAPI *PFN_vkCreateShader)(VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader);
typedef VkResult (VKAPI *PFN_vkDestroyShader)(VkDevice device, VkShader shader);
typedef VkResult (VKAPI *PFN_vkCreatePipelineCache)(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache);
typedef VkResult (VKAPI *PFN_vkDestroyPipelineCache)(VkDevice device, VkPipelineCache pipelineCache);
typedef size_t (VKAPI *PFN_vkGetPipelineCacheSize)(VkDevice device, VkPipelineCache pipelineCache);
typedef VkResult (VKAPI *PFN_vkGetPipelineCacheData)(VkDevice device, VkPipelineCache pipelineCache, void* pData);
typedef VkResult (VKAPI *PFN_vkMergePipelineCaches)(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches);
typedef VkResult (VKAPI *PFN_vkCreateGraphicsPipelines)(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines);
typedef VkResult (VKAPI *PFN_vkCreateComputePipelines)(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines);
typedef VkResult (VKAPI *PFN_vkDestroyPipeline)(VkDevice device, VkPipeline pipeline);
typedef VkResult (VKAPI *PFN_vkCreatePipelineLayout)(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout);
typedef VkResult (VKAPI *PFN_vkDestroyPipelineLayout)(VkDevice device, VkPipelineLayout pipelineLayout);
typedef VkResult (VKAPI *PFN_vkCreateSampler)(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler);
typedef VkResult (VKAPI *PFN_vkDestroySampler)(VkDevice device, VkSampler sampler);
typedef VkResult (VKAPI *PFN_vkCreateDescriptorSetLayout)(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout);
typedef VkResult (VKAPI *PFN_vkDestroyDescriptorSetLayout)(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
typedef VkResult (VKAPI *PFN_vkCreateDescriptorPool)(VkDevice device, VkDescriptorPoolUsage poolUsage, uint32_t maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool);
typedef VkResult (VKAPI *PFN_vkDestroyDescriptorPool)(VkDevice device, VkDescriptorPool descriptorPool);
typedef VkResult (VKAPI *PFN_vkResetDescriptorPool)(VkDevice device, VkDescriptorPool descriptorPool);
typedef VkResult (VKAPI *PFN_vkAllocDescriptorSets)(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, uint32_t count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets, uint32_t* pCount);
typedef VkResult (VKAPI *PFN_vkFreeDescriptorSets)(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets);
typedef VkResult (VKAPI *PFN_vkUpdateDescriptorSets)(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies);
typedef VkResult (VKAPI *PFN_vkCreateDynamicViewportState)(VkDevice device, const VkDynamicViewportStateCreateInfo* pCreateInfo, VkDynamicViewportState* pState);
typedef VkResult (VKAPI *PFN_vkDestroyDynamicViewportState)(VkDevice device, VkDynamicViewportState dynamicViewportState);
typedef VkResult (VKAPI *PFN_vkCreateDynamicRasterState)(VkDevice device, const VkDynamicRasterStateCreateInfo* pCreateInfo, VkDynamicRasterState* pState);
typedef VkResult (VKAPI *PFN_vkDestroyDynamicRasterState)(VkDevice device, VkDynamicRasterState dynamicRasterState);
typedef VkResult (VKAPI *PFN_vkCreateDynamicColorBlendState)(VkDevice device, const VkDynamicColorBlendStateCreateInfo* pCreateInfo, VkDynamicColorBlendState* pState);
typedef VkResult (VKAPI *PFN_vkDestroyDynamicColorBlendState)(VkDevice device, VkDynamicColorBlendState dynamicColorBlendState);
typedef VkResult (VKAPI *PFN_vkCreateDynamicDepthStencilState)(VkDevice device, const VkDynamicDepthStencilStateCreateInfo* pCreateInfo, VkDynamicDepthStencilState* pState);
typedef VkResult (VKAPI *PFN_vkDestroyDynamicDepthStencilState)(VkDevice device, VkDynamicDepthStencilState dynamicDepthStencilState);
typedef VkResult (VKAPI *PFN_vkCreateCommandBuffer)(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer);
typedef VkResult (VKAPI *PFN_vkDestroyCommandBuffer)(VkDevice device, VkCmdBuffer commandBuffer);
typedef VkResult (VKAPI *PFN_vkBeginCommandBuffer)(VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo);
typedef VkResult (VKAPI *PFN_vkEndCommandBuffer)(VkCmdBuffer cmdBuffer);
typedef VkResult (VKAPI *PFN_vkResetCommandBuffer)(VkCmdBuffer cmdBuffer);
typedef void     (VKAPI *PFN_vkCmdBindPipeline)(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
typedef void     (VKAPI *PFN_vkCmdBindDynamicViewportState)(VkCmdBuffer cmdBuffer, VkDynamicViewportState dynamicViewportState);
typedef void     (VKAPI *PFN_vkCmdBindDynamicRasterState)(VkCmdBuffer cmdBuffer, VkDynamicRasterState dynamicRasterState);
typedef void     (VKAPI *PFN_vkCmdBindDynamicColorBlendState)(VkCmdBuffer cmdBuffer, VkDynamicColorBlendState dynamicColorBlendState);
typedef void     (VKAPI *PFN_vkCmdBindDynamicDepthStencilState)(VkCmdBuffer cmdBuffer, VkDynamicDepthStencilState dynamicDepthStencilState);
typedef void     (VKAPI *PFN_vkCmdBindDescriptorSets)(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
typedef void     (VKAPI *PFN_vkCmdBindIndexBuffer)(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);
typedef void     (VKAPI *PFN_vkCmdBindVertexBuffers)(VkCmdBuffer cmdBuffer, uint32_t startBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets);
typedef void     (VKAPI *PFN_vkCmdDraw)(VkCmdBuffer cmdBuffer, uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount);
typedef void     (VKAPI *PFN_vkCmdDrawIndexed)(VkCmdBuffer cmdBuffer, uint32_t firstIndex, uint32_t indexCount, int32_t vertexOffset, uint32_t firstInstance, uint32_t instanceCount);
typedef void     (VKAPI *PFN_vkCmdDrawIndirect)(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
typedef void     (VKAPI *PFN_vkCmdDrawIndexedIndirect)(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
typedef void     (VKAPI *PFN_vkCmdDispatch)(VkCmdBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z);
typedef void     (VKAPI *PFN_vkCmdDispatchIndirect)(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset);
typedef void     (VKAPI *PFN_vkCmdCopyBuffer)(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions);
typedef void     (VKAPI *PFN_vkCmdCopyImage)(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageCopy* pRegions);
typedef void     (VKAPI *PFN_vkCmdBlitImage)(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkTexFilter filter);
typedef void     (VKAPI *PFN_vkCmdCopyBufferToImage)(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions);
typedef void     (VKAPI *PFN_vkCmdCopyImageToBuffer)(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions);
typedef void     (VKAPI *PFN_vkCmdUpdateBuffer)(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData);
typedef void     (VKAPI *PFN_vkCmdFillBuffer)(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data);
typedef void     (VKAPI *PFN_vkCmdClearColorImage)(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
typedef void     (VKAPI *PFN_vkCmdClearDepthStencilImage)(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, float depth, uint32_t stencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
typedef void     (VKAPI *PFN_vkCmdClearColorAttachment)(VkCmdBuffer cmdBuffer, uint32_t colorAttachment, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rectCount, const VkRect3D* pRects);
typedef void     (VKAPI *PFN_vkCmdClearDepthStencilAttachment)(VkCmdBuffer cmdBuffer, VkImageAspectFlags imageAspectMask, VkImageLayout imageLayout, float depth, uint32_t stencil, uint32_t rectCount, const VkRect3D* pRects);
typedef void     (VKAPI *PFN_vkCmdResolveImage)(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageResolve* pRegions);
typedef void     (VKAPI *PFN_vkCmdSetEvent)(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
typedef void     (VKAPI *PFN_vkCmdResetEvent)(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
typedef void     (VKAPI *PFN_vkCmdWaitEvents)(VkCmdBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags sourceStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void** ppMemBarriers);
typedef void     (VKAPI *PFN_vkCmdPipelineBarrier)(VkCmdBuffer cmdBuffer, VkPipelineStageFlags sourceStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, uint32_t memBarrierCount, const void** ppMemBarriers);
typedef void     (VKAPI *PFN_vkCmdBeginQuery)(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkQueryControlFlags flags);
typedef void     (VKAPI *PFN_vkCmdEndQuery)(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot);
typedef void     (VKAPI *PFN_vkCmdResetQueryPool)(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount);
typedef void     (VKAPI *PFN_vkCmdWriteTimestamp)(VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset);
typedef void     (VKAPI *PFN_vkCmdCopyQueryPoolResults)(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags);
typedef VkResult (VKAPI *PFN_vkCreateFramebuffer)(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer);
typedef VkResult (VKAPI *PFN_vkDestroyFramebuffer)(VkDevice device, VkFramebuffer framebuffer);
typedef VkResult (VKAPI *PFN_vkCreateRenderPass)(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass);
typedef VkResult (VKAPI *PFN_vkDestroyRenderPass)(VkDevice device, VkRenderPass renderPass);
typedef void     (VKAPI *PFN_vkCmdBeginRenderPass)(VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkRenderPassContents contents);
typedef void     (VKAPI *PFN_vkCmdNextSubpass)(VkCmdBuffer cmdBuffer, VkRenderPassContents contents);
typedef void     (VKAPI *PFN_vkCmdEndRenderPass)(VkCmdBuffer cmdBuffer);
typedef void     (VKAPI *PFN_vkCmdExecuteCommands)(VkCmdBuffer cmdBuffer, uint32_t cmdBuffersCount, const VkCmdBuffer* pCmdBuffers);

#ifdef VK_PROTOTYPES

// Device initialization

VkResult VKAPI vkCreateInstance(
    const VkInstanceCreateInfo*                 pCreateInfo,
    VkInstance*                                 pInstance);

VkResult VKAPI vkDestroyInstance(
    VkInstance                                  instance);

VkResult VKAPI vkEnumeratePhysicalDevices(
    VkInstance                                  instance,
    uint32_t*                                   pPhysicalDeviceCount,
    VkPhysicalDevice*                           pPhysicalDevices);

VkResult VKAPI vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures);

VkResult VKAPI vkGetPhysicalDeviceFormatInfo(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatInfo);

VkResult VKAPI vkGetPhysicalDeviceLimits(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceLimits*                     pLimits);

void * VKAPI vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName);

void * VKAPI vkGetDeviceProcAddr(
    VkDevice                                    device,
    const char*                                 pName);
// Device functions

VkResult VKAPI vkCreateDevice(
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    VkDevice*                                   pDevice);

VkResult VKAPI vkDestroyDevice(
    VkDevice                                    device);

VkResult VKAPI vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties);

VkResult VKAPI vkGetPhysicalDevicePerformance(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDevicePerformance*                pPerformance);

VkResult VKAPI vkGetPhysicalDeviceQueueCount(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount);

VkResult VKAPI vkGetPhysicalDeviceQueueProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    count,
    VkPhysicalDeviceQueueProperties*            pQueueProperties);

VkResult VKAPI vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperies);

// Extension discovery functions

VkResult VKAPI vkGetGlobalExtensionProperties(
    const char*                                 pLayerName,
    uint32_t*                                   pCount,
    VkExtensionProperties*                      pProperties);

VkResult VKAPI vkGetPhysicalDeviceExtensionProperties(
    VkPhysicalDevice                            physicalDevice,
    const char*                                 pLayerName,
    uint32_t*                                   pCount,
    VkExtensionProperties*                      pProperties);

VkResult VKAPI vkGetGlobalLayerProperties(
    uint32_t*                                   pCount,
    VkLayerProperties*                          pProperties);

VkResult VKAPI vkGetPhysicalDeviceLayerProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount,
    VkLayerProperties*                          pProperties);

// Queue functions

VkResult VKAPI vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue);

VkResult VKAPI vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    cmdBufferCount,
    const VkCmdBuffer*                          pCmdBuffers,
    VkFence                                     fence);

VkResult VKAPI vkQueueWaitIdle(
    VkQueue                                     queue);

VkResult VKAPI vkDeviceWaitIdle(
    VkDevice                                    device);

// Memory functions

VkResult VKAPI vkAllocMemory(
    VkDevice                                    device,
    const VkMemoryAllocInfo*                    pAllocInfo,
    VkDeviceMemory*                             pMem);

VkResult VKAPI vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem);

VkResult VKAPI vkMapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem,
    VkDeviceSize                                offset,
    VkDeviceSize                                size,
    VkMemoryMapFlags                            flags,
    void**                                      ppData);

VkResult VKAPI vkUnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem);

VkResult VKAPI vkFlushMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memRangeCount,
    const VkMappedMemoryRange*                  pMemRanges);

VkResult VKAPI vkInvalidateMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memRangeCount,
    const VkMappedMemoryRange*                  pMemRanges);

VkResult VKAPI vkGetDeviceMemoryCommitment(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    VkDeviceSize*                               pCommittedMemoryInBytes);


// Memory management API functions

VkResult VKAPI vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset);

VkResult VKAPI vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset);

VkResult VKAPI vkGetBufferMemoryRequirements(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkMemoryRequirements*                       pMemoryRequirements);

VkResult VKAPI vkGetImageMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    VkMemoryRequirements*                       pMemoryRequirements);

VkResult VKAPI vkGetImageSparseMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    uint32_t*                                   pNumRequirements,
    VkSparseImageMemoryRequirements*            pSparseMemoryRequirements);

VkResult VKAPI vkGetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    uint32_t                                    samples,
    VkImageUsageFlags                           usage,
    VkImageTiling                               tiling,
    uint32_t*                                   pNumProperties,
    VkSparseImageFormatProperties*              pProperties);

VkResult VKAPI vkQueueBindSparseBufferMemory(
    VkQueue                                     queue,
    VkBuffer                                    buffer,
    uint32_t                                    numBindings,
    const VkSparseMemoryBindInfo*               pBindInfo);

VkResult VKAPI vkQueueBindSparseImageOpaqueMemory(
    VkQueue                                     queue,
    VkImage                                     image,
    uint32_t                                    numBindings,
    const VkSparseMemoryBindInfo*               pBindInfo);

VkResult VKAPI vkQueueBindSparseImageMemory(
    VkQueue                                     queue,
    VkImage                                     image,
    uint32_t                                    numBindings,
    const VkSparseImageMemoryBindInfo*          pBindInfo);

// Fence functions

VkResult VKAPI vkCreateFence(
    VkDevice                                    device,
    const VkFenceCreateInfo*                    pCreateInfo,
    VkFence*                                    pFence);

VkResult VKAPI vkDestroyFence(
    VkDevice                                    device,
    VkFence                                     fence);

VkResult VKAPI vkResetFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences);

VkResult VKAPI vkGetFenceStatus(
    VkDevice                                    device,
    VkFence                                     fence);

VkResult VKAPI vkWaitForFences(
    VkDevice                                    device,
    uint32_t                                    fenceCount,
    const VkFence*                              pFences,
    VkBool32                                    waitAll,
    uint64_t                                    timeout); // timeout in nanoseconds

// Queue semaphore functions

VkResult VKAPI vkCreateSemaphore(
    VkDevice                                    device,
    const VkSemaphoreCreateInfo*                pCreateInfo,
    VkSemaphore*                                pSemaphore);

VkResult VKAPI vkDestroySemaphore(
    VkDevice                                    device,
    VkSemaphore                                 semaphore);

VkResult VKAPI vkQueueSignalSemaphore(
    VkQueue                                     queue,
    VkSemaphore                                 semaphore);

VkResult VKAPI vkQueueWaitSemaphore(
    VkQueue                                     queue,
    VkSemaphore                                 semaphore);

// Event functions

VkResult VKAPI vkCreateEvent(
    VkDevice                                    device,
    const VkEventCreateInfo*                    pCreateInfo,
    VkEvent*                                    pEvent);

VkResult VKAPI vkDestroyEvent(
    VkDevice                                    device,
    VkEvent                                     event);

VkResult VKAPI vkGetEventStatus(
    VkDevice                                    device,
    VkEvent                                     event);

VkResult VKAPI vkSetEvent(
    VkDevice                                    device,
    VkEvent                                     event);

VkResult VKAPI vkResetEvent(
    VkDevice                                    device,
    VkEvent                                     event);

// Query functions

VkResult VKAPI vkCreateQueryPool(
    VkDevice                                    device,
    const VkQueryPoolCreateInfo*                pCreateInfo,
    VkQueryPool*                                pQueryPool);

VkResult VKAPI vkDestroyQueryPool(
    VkDevice                                    device,
    VkQueryPool                                 queryPool);

VkResult VKAPI vkGetQueryPoolResults(
    VkDevice                                    device,
    VkQueryPool                                 queryPool,
    uint32_t                                    startQuery,
    uint32_t                                    queryCount,
    size_t*                                     pDataSize,
    void*                                       pData,
    VkQueryResultFlags                          flags);

// Buffer functions

VkResult VKAPI vkCreateBuffer(
    VkDevice                                    device,
    const VkBufferCreateInfo*                   pCreateInfo,
    VkBuffer*                                   pBuffer);

VkResult VKAPI vkDestroyBuffer(
    VkDevice                                    device,
    VkBuffer                                    buffer);

// Buffer view functions

VkResult VKAPI vkCreateBufferView(
    VkDevice                                    device,
    const VkBufferViewCreateInfo*               pCreateInfo,
    VkBufferView*                               pView);

VkResult VKAPI vkDestroyBufferView(
    VkDevice                                    device,
    VkBufferView                                bufferView);

// Image functions

VkResult VKAPI vkCreateImage(
    VkDevice                                    device,
    const VkImageCreateInfo*                    pCreateInfo,
    VkImage*                                    pImage);

VkResult VKAPI vkDestroyImage(
    VkDevice                                    device,
    VkImage                                     image);

VkResult VKAPI vkGetImageSubresourceLayout(
    VkDevice                                    device,
    VkImage                                     image,
    const VkImageSubresource*                   pSubresource,
    VkSubresourceLayout*                        pLayout);

// Image view functions

VkResult VKAPI vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    VkImageView*                                pView);

VkResult VKAPI vkDestroyImageView(
    VkDevice                                    device,
    VkImageView                                 imageView);

VkResult VKAPI vkCreateAttachmentView(
    VkDevice                                    device,
    const VkAttachmentViewCreateInfo*           pCreateInfo,
    VkAttachmentView*                           pView);

VkResult VKAPI vkDestroyAttachmentView(
    VkDevice                                    device,
    VkAttachmentView                            AttachmentView);

// Shader functions

VkResult VKAPI vkCreateShaderModule(
    VkDevice                                    device,
    const VkShaderModuleCreateInfo*             pCreateInfo,
    VkShaderModule*                             pShaderModule);

VkResult VKAPI vkDestroyShaderModule(
    VkDevice                                    device,
    VkShaderModule                              shaderModule);

VkResult VKAPI vkCreateShader(
    VkDevice                                    device,
    const VkShaderCreateInfo*                   pCreateInfo,
    VkShader*                                   pShader);

VkResult VKAPI vkDestroyShader(
    VkDevice                                    device,
    VkShader                                    shader);

// Pipeline functions
VkResult VKAPI vkCreatePipelineCache(
    VkDevice                                    device,
    const VkPipelineCacheCreateInfo*            pCreateInfo,
    VkPipelineCache*                            pPipelineCache);

VkResult VKAPI vkDestroyPipelineCache(
    VkDevice                                    device,
    VkPipelineCache                             pipeline);

size_t VKAPI vkGetPipelineCacheSize(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache);

VkResult VKAPI vkGetPipelineCacheData(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    void*                                       pData);

VkResult VKAPI vkMergePipelineCaches(
    VkDevice                                    device,
    VkPipelineCache                             destCache,
    uint32_t                                    srcCacheCount,
    const VkPipelineCache*                      pSrcCaches);

VkResult VKAPI vkCreateGraphicsPipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    VkPipeline*                                 pPipelines);

VkResult VKAPI vkCreateComputePipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    count,
    const VkComputePipelineCreateInfo*          pCreateInfos,
    VkPipeline*                                 pPipelines);

VkResult VKAPI vkDestroyPipeline(
    VkDevice                                    device,
    VkPipeline                                  pipeline);

// Pipeline layout functions

VkResult VKAPI vkCreatePipelineLayout(
    VkDevice                                    device,
    const VkPipelineLayoutCreateInfo*           pCreateInfo,
    VkPipelineLayout*                           pPipelineLayout);

VkResult VKAPI vkDestroyPipelineLayout(
    VkDevice                                    device,
    VkPipelineLayout                            pipelineLayout);

// Sampler functions

VkResult VKAPI vkCreateSampler(
    VkDevice                                    device,
    const VkSamplerCreateInfo*                  pCreateInfo,
    VkSampler*                                  pSampler);

VkResult VKAPI vkDestroySampler(
    VkDevice                                    device,
    VkSampler                                   sampler);

// Descriptor set functions

VkResult VKAPI vkCreateDescriptorSetLayout(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    VkDescriptorSetLayout*                      pSetLayout);

VkResult VKAPI vkDestroyDescriptorSetLayout(
    VkDevice                                    device,
    VkDescriptorSetLayout                       descriptorSetLayout);

VkResult VKAPI vkCreateDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPoolUsage                       poolUsage,
    uint32_t                                    maxSets,
    const VkDescriptorPoolCreateInfo*           pCreateInfo,
    VkDescriptorPool*                           pDescriptorPool);

VkResult VKAPI vkDestroyDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool);

VkResult VKAPI vkResetDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool);

VkResult VKAPI vkAllocDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    VkDescriptorSetUsage                        setUsage,
    uint32_t                                    count,
    const VkDescriptorSetLayout*                pSetLayouts,
    VkDescriptorSet*                            pDescriptorSets,
    uint32_t*                                   pCount);

VkResult VKAPI vkFreeDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    count,
    const VkDescriptorSet*                      pDescriptorSets);

VkResult VKAPI vkUpdateDescriptorSets(
    VkDevice                                    device,
    uint32_t                                    writeCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites,
    uint32_t                                    copyCount,
    const VkCopyDescriptorSet*                  pDescriptorCopies);

// State object functions

VkResult VKAPI vkCreateDynamicViewportState(
    VkDevice                                    device,
    const VkDynamicViewportStateCreateInfo*     pCreateInfo,
    VkDynamicViewportState*                     pState);

VkResult VKAPI vkDestroyDynamicViewportState(
    VkDevice                                    device,
    VkDynamicViewportState                      dynamicViewportState);

VkResult VKAPI vkCreateDynamicRasterState(
    VkDevice                                    device,
    const VkDynamicRasterStateCreateInfo*       pCreateInfo,
    VkDynamicRasterState*                       pState);

VkResult VKAPI vkDestroyDynamicRasterState(
    VkDevice                                    device,
    VkDynamicRasterState                        dynamicRasterState);

VkResult VKAPI vkCreateDynamicColorBlendState(
    VkDevice                                    device,
    const VkDynamicColorBlendStateCreateInfo*   pCreateInfo,
    VkDynamicColorBlendState*                   pState);

VkResult VKAPI vkDestroyDynamicColorBlendState(
    VkDevice                                    device,
    VkDynamicColorBlendState                    dynamicColorBlendState);

VkResult VKAPI vkCreateDynamicDepthStencilState(
    VkDevice                                    device,
    const VkDynamicDepthStencilStateCreateInfo* pCreateInfo,
    VkDynamicDepthStencilState*                 pState);

VkResult VKAPI vkDestroyDynamicDepthStencilState(
    VkDevice                                    device,
    VkDynamicDepthStencilState                  dynamicDepthStencilState);

// Command buffer functions

VkResult VKAPI vkCreateCommandBuffer(
    VkDevice                                    device,
    const VkCmdBufferCreateInfo*                pCreateInfo,
    VkCmdBuffer*                                pCmdBuffer);

VkResult VKAPI vkDestroyCommandBuffer(
    VkDevice                                    device,
    VkCmdBuffer                                 commandBuffer);

VkResult VKAPI vkBeginCommandBuffer(
    VkCmdBuffer                                 cmdBuffer,
    const VkCmdBufferBeginInfo*                 pBeginInfo);

VkResult VKAPI vkEndCommandBuffer(
    VkCmdBuffer                                 cmdBuffer);

VkResult VKAPI vkResetCommandBuffer(
    VkCmdBuffer                                 cmdBuffer);

// Command buffer building functions

void VKAPI vkCmdBindPipeline(
    VkCmdBuffer                                 cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline);

void VKAPI vkCmdBindDynamicViewportState(
     VkCmdBuffer                                 cmdBuffer,
     VkDynamicViewportState                      dynamicViewportState);

void VKAPI vkCmdBindDynamicRasterState(
     VkCmdBuffer                                 cmdBuffer,
     VkDynamicRasterState                        dynamicRasterState);

void VKAPI vkCmdBindDynamicColorBlendState(
     VkCmdBuffer                                 cmdBuffer,
     VkDynamicColorBlendState                    dynamicColorBlendState);

void VKAPI vkCmdBindDynamicDepthStencilState(
     VkCmdBuffer                                 cmdBuffer,
     VkDynamicDepthStencilState                  dynamicDepthStencilState);

void VKAPI vkCmdBindDescriptorSets(
    VkCmdBuffer                                 cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    setCount,
    const VkDescriptorSet*                      pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t*                             pDynamicOffsets);

void VKAPI vkCmdBindIndexBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType);

void VKAPI vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets);

void VKAPI vkCmdDraw(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    firstVertex,
    uint32_t                                    vertexCount,
    uint32_t                                    firstInstance,
    uint32_t                                    instanceCount);

void VKAPI vkCmdDrawIndexed(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    firstIndex,
    uint32_t                                    indexCount,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance,
    uint32_t                                    instanceCount);

void VKAPI vkCmdDrawIndirect(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    count,
    uint32_t                                    stride);

void VKAPI vkCmdDrawIndexedIndirect(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    count,
    uint32_t                                    stride);

void VKAPI vkCmdDispatch(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    x,
    uint32_t                                    y,
    uint32_t                                    z);

void VKAPI vkCmdDispatchIndirect(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset);

void VKAPI vkCmdCopyBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    srcBuffer,
    VkBuffer                                    destBuffer,
    uint32_t                                    regionCount,
    const VkBufferCopy*                         pRegions);

void VKAPI vkCmdCopyImage(
    VkCmdBuffer                                 cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     destImage,
    VkImageLayout                               destImageLayout,
    uint32_t                                    regionCount,
    const VkImageCopy*                          pRegions);

void VKAPI vkCmdBlitImage(
    VkCmdBuffer                                 cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     destImage,
    VkImageLayout                               destImageLayout,
    uint32_t                                    regionCount,
    const VkImageBlit*                          pRegions,
    VkTexFilter                                 filter);

void VKAPI vkCmdCopyBufferToImage(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    srcBuffer,
    VkImage                                     destImage,
    VkImageLayout                               destImageLayout,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions);

void VKAPI vkCmdCopyImageToBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkBuffer                                    destBuffer,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions);

void VKAPI vkCmdUpdateBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    destBuffer,
    VkDeviceSize                                destOffset,
    VkDeviceSize                                dataSize,
    const uint32_t*                             pData);

void VKAPI vkCmdFillBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    destBuffer,
    VkDeviceSize                                destOffset,
    VkDeviceSize                                fillSize,
    uint32_t                                    data);

void VKAPI vkCmdClearColorImage(
    VkCmdBuffer                                 cmdBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    const VkClearColorValue*                    pColor,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges);

void VKAPI vkCmdClearDepthStencilImage(
    VkCmdBuffer                                 cmdBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    float                                       depth,
    uint32_t                                    stencil,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges);

void VKAPI vkCmdClearColorAttachment(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    colorAttachment,
    VkImageLayout                               imageLayout,
    const VkClearColorValue*                    pColor,
    uint32_t                                    rectCount,
    const VkRect3D*                             pRects);

void VKAPI vkCmdClearDepthStencilAttachment(
    VkCmdBuffer                                 cmdBuffer,
    VkImageAspectFlags                          imageAspectMask,
    VkImageLayout                               imageLayout,
    float                                       depth,
    uint32_t                                    stencil,
    uint32_t                                    rectCount,
    const VkRect3D*                             pRects);

void VKAPI vkCmdResolveImage(
    VkCmdBuffer                                 cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     destImage,
    VkImageLayout                               destImageLayout,
    uint32_t                                    regionCount,
    const VkImageResolve*                       pRegions);

void VKAPI vkCmdSetEvent(
    VkCmdBuffer                                 cmdBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask);

void VKAPI vkCmdResetEvent(
    VkCmdBuffer                                 cmdBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask);

void VKAPI vkCmdWaitEvents(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    VkPipelineStageFlags                        sourceStageMask,
    VkPipelineStageFlags                        destStageMask,
    uint32_t                                    memBarrierCount,
    const void**                                ppMemBarriers);

void VKAPI vkCmdPipelineBarrier(
    VkCmdBuffer                                 cmdBuffer,
    VkPipelineStageFlags                        sourceStageMask,
    VkPipelineStageFlags                        destStageMask,
    VkBool32                                    byRegion,
    uint32_t                                    memBarrierCount,
    const void**                                ppMemBarriers);

void VKAPI vkCmdBeginQuery(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    slot,
    VkQueryControlFlags                         flags);

void VKAPI vkCmdEndQuery(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    slot);

void VKAPI vkCmdResetQueryPool(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    startQuery,
    uint32_t                                    queryCount);

void VKAPI vkCmdWriteTimestamp(
    VkCmdBuffer                                 cmdBuffer,
    VkTimestampType                             timestampType,
    VkBuffer                                    destBuffer,
    VkDeviceSize                                destOffset);

void VKAPI vkCmdCopyQueryPoolResults(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    startQuery,
    uint32_t                                    queryCount,
    VkBuffer                                    destBuffer,
    VkDeviceSize                                destOffset,
    VkDeviceSize                                destStride,
    VkQueryResultFlags                          flags);

VkResult VKAPI vkCreateFramebuffer(
    VkDevice                                    device,
    const VkFramebufferCreateInfo*              pCreateInfo,
    VkFramebuffer*                              pFramebuffer);

VkResult VKAPI vkDestroyFramebuffer(
    VkDevice                                    device,
    VkFramebuffer                               framebuffer);

VkResult VKAPI vkCreateRenderPass(
    VkDevice                                    device,
    const VkRenderPassCreateInfo*               pCreateInfo,
    VkRenderPass*                               pRenderPass);

void VKAPI vkCmdBeginRenderPass(
    VkCmdBuffer                                 cmdBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkRenderPassContents                        contents);

void VKAPI vkCmdNextSubpass(
    VkCmdBuffer                                 cmdBuffer,
    VkRenderPassContents                        contents);

VkResult VKAPI vkDestroyRenderPass(
    VkDevice                                    device,
    VkRenderPass                                renderPass);

void VKAPI vkCmdEndRenderPass(
    VkCmdBuffer                                 cmdBuffer);

void VKAPI vkCmdExecuteCommands(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    cmdBuffersCount,
    const VkCmdBuffer*                          pCmdBuffers);

#endif // VK_PROTOTYPES

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // __VULKAN_H__
