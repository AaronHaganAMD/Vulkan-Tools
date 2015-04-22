/*
 * GLAVE & vulkan
 *
 * Copyright (C) 2015 LunarG, Inc. and Valve Corporation
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
#include "loader_platform.h"
#include "glave_snapshot.h"
#include "vk_struct_string_helper.h"

#define LAYER_NAME_STR "GlaveSnapshot"
#define LAYER_ABBREV_STR "GLVSnap"

static VkLayerDispatchTable nextTable;
static VkBaseLayerObject *pCurObj;

// The following is #included again to catch certain OS-specific functions being used:
#include "loader_platform.h"
#include "layers_config.h"
#include "layers_msg.h"

static LOADER_PLATFORM_THREAD_ONCE_DECLARATION(tabOnce);
static int objLockInitialized = 0;
static loader_platform_thread_mutex objLock;

// The 'masterSnapshot' which gets the delta merged into it when 'GetSnapshot()' is called.
static GLV_VK_SNAPSHOT s_snapshot = {0};

// The 'deltaSnapshot' which tracks all object creation and deletion.
static GLV_VK_SNAPSHOT s_delta = {0};


//=============================================================================
// Helper structure for a GLAVE vulkan snapshot.
// These can probably be auto-generated at some point.
//=============================================================================

void glv_vk_malloc_and_copy(void** ppDest, size_t size, const void* pSrc)
{
    *ppDest = malloc(size);
    memcpy(*ppDest, pSrc, size);
}

VkDeviceCreateInfo* glv_deepcopy_VkDeviceCreateInfo(const VkDeviceCreateInfo* pSrcCreateInfo)
{
    VkDeviceCreateInfo* pDestCreateInfo;

    // NOTE: partially duplicated code from add_VkDeviceCreateInfo_to_packet(...)
    {
        uint32_t i;
        glv_vk_malloc_and_copy((void**)&pDestCreateInfo, sizeof(VkDeviceCreateInfo), pSrcCreateInfo);
        glv_vk_malloc_and_copy((void**)&pDestCreateInfo->pRequestedQueues, pSrcCreateInfo->queueRecordCount*sizeof(VkDeviceQueueCreateInfo), pSrcCreateInfo->pRequestedQueues);

        if (pSrcCreateInfo->extensionCount > 0)
        {
            glv_vk_malloc_and_copy((void**)&pDestCreateInfo->ppEnabledExtensionNames, pSrcCreateInfo->extensionCount * sizeof(char *), pSrcCreateInfo->ppEnabledExtensionNames);
            for (i = 0; i < pSrcCreateInfo->extensionCount; i++)
            {
                glv_vk_malloc_and_copy((void**)&pDestCreateInfo->ppEnabledExtensionNames[i], strlen(pSrcCreateInfo->ppEnabledExtensionNames[i]) + 1, pSrcCreateInfo->ppEnabledExtensionNames[i]);
            }
        }
        VkLayerCreateInfo *pSrcNext = ( VkLayerCreateInfo *) pSrcCreateInfo->pNext;
        VkLayerCreateInfo **ppDstNext = ( VkLayerCreateInfo **) &pDestCreateInfo->pNext;
        while (pSrcNext != NULL)
        {
            if ((pSrcNext->sType == VK_STRUCTURE_TYPE_LAYER_CREATE_INFO) && pSrcNext->layerCount > 0)
            {
                glv_vk_malloc_and_copy((void**)ppDstNext, sizeof(VkLayerCreateInfo), pSrcNext);
                glv_vk_malloc_and_copy((void**)&(*ppDstNext)->ppActiveLayerNames, pSrcNext->layerCount * sizeof(char*), pSrcNext->ppActiveLayerNames);
                for (i = 0; i < pSrcNext->layerCount; i++)
                {
                    glv_vk_malloc_and_copy((void**)&(*ppDstNext)->ppActiveLayerNames[i], strlen(pSrcNext->ppActiveLayerNames[i]) + 1, pSrcNext->ppActiveLayerNames[i]);
                }

                ppDstNext = (VkLayerCreateInfo**) &(*ppDstNext)->pNext;
            }
            pSrcNext = (VkLayerCreateInfo*) pSrcNext->pNext;
        }
    }

    return pDestCreateInfo;
}

void glv_deepfree_VkDeviceCreateInfo(VkDeviceCreateInfo* pCreateInfo)
{
    uint32_t i;
    if (pCreateInfo->pRequestedQueues != NULL)
    {
        free((void*)pCreateInfo->pRequestedQueues);
    }

    if (pCreateInfo->ppEnabledExtensionNames != NULL)
    {
        for (i = 0; i < pCreateInfo->extensionCount; i++)
        {
            free((void*)pCreateInfo->ppEnabledExtensionNames[i]);
        }
        free((void*)pCreateInfo->ppEnabledExtensionNames);
    }

    VkLayerCreateInfo *pSrcNext = (VkLayerCreateInfo*)pCreateInfo->pNext;
    while (pSrcNext != NULL)
    {
        VkLayerCreateInfo* pTmp = (VkLayerCreateInfo*)pSrcNext->pNext;
        if ((pSrcNext->sType == VK_STRUCTURE_TYPE_LAYER_CREATE_INFO) && pSrcNext->layerCount > 0)
        {
            for (i = 0; i < pSrcNext->layerCount; i++)
            {
                free((void*)pSrcNext->ppActiveLayerNames[i]);
            }

            free((void*)pSrcNext->ppActiveLayerNames);
            free(pSrcNext);
        }
        pSrcNext = pTmp;
    }

    free(pCreateInfo);
}

void glv_vk_snapshot_copy_createdevice_params(GLV_VK_SNAPSHOT_CREATEDEVICE_PARAMS* pDest, VkPhysicalGpu gpu, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice)
{
    pDest->gpu = gpu;

    pDest->pCreateInfo = glv_deepcopy_VkDeviceCreateInfo(pCreateInfo);

    pDest->pDevice = (VkDevice*)malloc(sizeof(VkDevice));
    *pDest->pDevice = *pDevice;
}

void glv_vk_snapshot_destroy_createdevice_params(GLV_VK_SNAPSHOT_CREATEDEVICE_PARAMS* pSrc)
{
    memset(&pSrc->gpu, 0, sizeof(VkPhysicalGpu));

    glv_deepfree_VkDeviceCreateInfo(pSrc->pCreateInfo);
    pSrc->pCreateInfo = NULL;

    free(pSrc->pDevice);
    pSrc->pDevice = NULL;
}



// add a new node to the global and object lists, then return it so the caller can populate the object information.
static GLV_VK_SNAPSHOT_LL_NODE* snapshot_insert_object(GLV_VK_SNAPSHOT* pSnapshot, void* pObject, VK_OBJECT_TYPE type)
{
    // Create a new node
    GLV_VK_SNAPSHOT_LL_NODE* pNewObjNode = (GLV_VK_SNAPSHOT_LL_NODE*)malloc(sizeof(GLV_VK_SNAPSHOT_LL_NODE));
    memset(pNewObjNode, 0, sizeof(GLV_VK_SNAPSHOT_LL_NODE));
    pNewObjNode->obj.pVkObject = pObject;
    pNewObjNode->obj.objType = type;
    pNewObjNode->obj.status = OBJSTATUS_NONE;

    // insert at front of global list
    pNewObjNode->pNextGlobal = pSnapshot->pGlobalObjs;
    pSnapshot->pGlobalObjs = pNewObjNode;

    // insert at front of object list
    pNewObjNode->pNextObj = pSnapshot->pObjectHead[type];
    pSnapshot->pObjectHead[type] = pNewObjNode;

    // increment count
    pSnapshot->globalObjCount++;
    pSnapshot->numObjs[type]++;

    return pNewObjNode;
}

// This is just a helper function to snapshot_remove_object(..). It is not intended for this to be called directly.
static void snapshot_remove_obj_type(GLV_VK_SNAPSHOT* pSnapshot, void* pObj, VK_OBJECT_TYPE objType) {
    GLV_VK_SNAPSHOT_LL_NODE *pTrav = pSnapshot->pObjectHead[objType];
    GLV_VK_SNAPSHOT_LL_NODE *pPrev = pSnapshot->pObjectHead[objType];
    while (pTrav) {
        if (pTrav->obj.pVkObject == pObj) {
            pPrev->pNextObj = pTrav->pNextObj;
            // update HEAD of Obj list as needed
            if (pSnapshot->pObjectHead[objType] == pTrav)
            {
                pSnapshot->pObjectHead[objType] = pTrav->pNextObj;
            }
            assert(pSnapshot->numObjs[objType] > 0);
            pSnapshot->numObjs[objType]--;
            return;
        }
        pPrev = pTrav;
        pTrav = pTrav->pNextObj;
    }
    char str[1024];
    sprintf(str, "OBJ INTERNAL ERROR : Obj %p was in global list but not in %s list", pObj, string_VK_OBJECT_TYPE(objType));
    layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, pObj, 0, GLVSNAPSHOT_INTERNAL_ERROR, LAYER_ABBREV_STR, str);
}

// Search global list to find object,
// if found:
// remove object from obj_type list using snapshot_remove_obj_type()
// remove object from global list,
// return object.
// else:
// Report message that we didn't see it get created,
// return NULL.
static GLV_VK_SNAPSHOT_LL_NODE* snapshot_remove_object(GLV_VK_SNAPSHOT* pSnapshot, void* pObject)
{
    GLV_VK_SNAPSHOT_LL_NODE *pTrav = pSnapshot->pGlobalObjs;
    GLV_VK_SNAPSHOT_LL_NODE *pPrev = pSnapshot->pGlobalObjs;
    while (pTrav)
    {
        if (pTrav->obj.pVkObject == pObject)
        {
            snapshot_remove_obj_type(pSnapshot, pObject, pTrav->obj.objType);
            pPrev->pNextGlobal = pTrav->pNextGlobal;
            // update HEAD of global list if needed
            if (pSnapshot->pGlobalObjs == pTrav)
            {
                pSnapshot->pGlobalObjs = pTrav->pNextGlobal;
            }
            assert(pSnapshot->globalObjCount > 0);
            pSnapshot->globalObjCount--;
            return pTrav;
        }
        pPrev = pTrav;
        pTrav = pTrav->pNextGlobal;
    }

    // Object not found.
    char str[1024];
    sprintf(str, "Object %p was not found in the created object list. It should be added as a deleted object.", pObject);
    layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, pObject, 0, GLVSNAPSHOT_UNKNOWN_OBJECT, LAYER_ABBREV_STR, str);
    return NULL;
}

// Add a new deleted object node to the list
static void snapshot_insert_deleted_object(GLV_VK_SNAPSHOT* pSnapshot, void* pObject, VK_OBJECT_TYPE type)
{
    // Create a new node
    GLV_VK_SNAPSHOT_DELETED_OBJ_NODE* pNewObjNode = (GLV_VK_SNAPSHOT_DELETED_OBJ_NODE*)malloc(sizeof(GLV_VK_SNAPSHOT_DELETED_OBJ_NODE));
    memset(pNewObjNode, 0, sizeof(GLV_VK_SNAPSHOT_DELETED_OBJ_NODE));
    pNewObjNode->objType = type;
    pNewObjNode->pVkObject = pObject;

    // insert at front of list
    pNewObjNode->pNextObj = pSnapshot->pDeltaDeletedObjects;
    pSnapshot->pDeltaDeletedObjects = pNewObjNode;

    // increment count
    pSnapshot->deltaDeletedObjectCount++;
}

// Note: the parameters after pSnapshot match the order of vkCreateDevice(..)
static void snapshot_insert_device(GLV_VK_SNAPSHOT* pSnapshot, VkPhysicalGpu gpu, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice)
{
    GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(pSnapshot, *pDevice, VK_OBJECT_TYPE_DEVICE);
    pNode->obj.pStruct = malloc(sizeof(GLV_VK_SNAPSHOT_DEVICE_NODE));

    GLV_VK_SNAPSHOT_DEVICE_NODE* pDevNode = (GLV_VK_SNAPSHOT_DEVICE_NODE*)pNode->obj.pStruct;
    glv_vk_snapshot_copy_createdevice_params(&pDevNode->params, gpu, pCreateInfo, pDevice);

    // insert at front of device list
    pNode->pNextObj = pSnapshot->pDevices;
    pSnapshot->pDevices = pNode;

    // increment count
    pSnapshot->deviceCount++;
}

static void snapshot_remove_device(GLV_VK_SNAPSHOT* pSnapshot, VkDevice device)
{
    GLV_VK_SNAPSHOT_LL_NODE* pFoundObject = snapshot_remove_object(pSnapshot, device);

    if (pFoundObject != NULL)
    {
        GLV_VK_SNAPSHOT_LL_NODE *pTrav = pSnapshot->pDevices;
        GLV_VK_SNAPSHOT_LL_NODE *pPrev = pSnapshot->pDevices;
        while (pTrav != NULL)
        {
            if (pTrav->obj.pVkObject == device)
            {
                pPrev->pNextObj = pTrav->pNextObj;
                // update HEAD of Obj list as needed
                if (pSnapshot->pDevices == pTrav)
                    pSnapshot->pDevices = pTrav->pNextObj;

                // delete the object
                if (pTrav->obj.pStruct != NULL)
                {
                    GLV_VK_SNAPSHOT_DEVICE_NODE* pDevNode = (GLV_VK_SNAPSHOT_DEVICE_NODE*)pTrav->obj.pStruct;
                    glv_vk_snapshot_destroy_createdevice_params(&pDevNode->params);
                    free(pDevNode);
                }
                free(pTrav);

                if (pSnapshot->deviceCount > 0)
                {
                    pSnapshot->deviceCount--;
                }
                else
                {
                    // TODO: Callback WARNING that too many devices were deleted
                    assert(!"DeviceCount <= 0 means that too many devices were deleted.");
                }
                return;
            }
            pPrev = pTrav;
            pTrav = pTrav->pNextObj;
        }
    }

    // If the code got here, then the device wasn't in the devices list.
    // That means we should add this device to the deleted items list.
    snapshot_insert_deleted_object(&s_delta, device, VK_OBJECT_TYPE_DEVICE);
}

// Traverse global list and return type for given object
static VK_OBJECT_TYPE ll_get_obj_type(VkObject object) {
    GLV_VK_SNAPSHOT_LL_NODE *pTrav = s_delta.pGlobalObjs;
    while (pTrav) {
        if (pTrav->obj.pVkObject == object)
            return pTrav->obj.objType;
        pTrav = pTrav->pNextGlobal;
    }
    char str[1024];
    sprintf(str, "Attempting look-up on obj %p but it is NOT in the global list!", (void*)object);
    layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, object, 0, GLVSNAPSHOT_MISSING_OBJECT, LAYER_ABBREV_STR, str);
    return VK_OBJECT_TYPE_UNKNOWN;
}

static void ll_increment_use_count(void* pObj, VK_OBJECT_TYPE objType) {
    GLV_VK_SNAPSHOT_LL_NODE *pTrav = s_delta.pObjectHead[objType];
    while (pTrav) {
        if (pTrav->obj.pVkObject == pObj) {
            pTrav->obj.numUses++;
            return;
        }
        pTrav = pTrav->pNextObj;
    }

    // If we do not find obj, insert it and then increment count
    // TODO: we can't just create the object, because we don't know what it was created with.
    // Instead, we need to make a list of referenced objects. When the delta is merged with a snapshot, we'll need
    // to confirm that the referenced objects actually exist in the snapshot; otherwise I guess the merge should fail.
    char str[1024];
    sprintf(str, "Unable to increment count for obj %p, will add to list as %s type and increment count", pObj, string_VK_OBJECT_TYPE(objType));
    layerCbMsg(VK_DBG_MSG_WARNING, VK_VALIDATION_LEVEL_0, pObj, 0, GLVSNAPSHOT_UNKNOWN_OBJECT, LAYER_ABBREV_STR, str);

//    ll_insert_obj(pObj, objType);
//    ll_increment_use_count(pObj, objType);
}

// Set selected flag state for an object node
static void set_status(void* pObj, VK_OBJECT_TYPE objType, OBJECT_STATUS status_flag) {
    if (pObj != NULL) {
        GLV_VK_SNAPSHOT_LL_NODE *pTrav = s_delta.pObjectHead[objType];
        while (pTrav) {
            if (pTrav->obj.pVkObject == pObj) {
                pTrav->obj.status |= status_flag;
                return;
            }
            pTrav = pTrav->pNextObj;
        }

        // If we do not find it print an error
        char str[1024];
        sprintf(str, "Unable to set status for non-existent object %p of %s type", pObj, string_VK_OBJECT_TYPE(objType));
        layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, pObj, 0, GLVSNAPSHOT_UNKNOWN_OBJECT, LAYER_ABBREV_STR, str);
    }
}

// Track selected state for an object node
static void track_object_status(void* pObj, VkStateBindPoint stateBindPoint) {
    GLV_VK_SNAPSHOT_LL_NODE *pTrav = s_delta.pObjectHead[VK_OBJECT_TYPE_CMD_BUFFER];

    while (pTrav) {
        if (pTrav->obj.pVkObject == pObj) {
            if (stateBindPoint == VK_STATE_BIND_VIEWPORT) {
                pTrav->obj.status |= OBJSTATUS_VIEWPORT_BOUND;
            } else if (stateBindPoint == VK_STATE_BIND_RASTER) {
                pTrav->obj.status |= OBJSTATUS_RASTER_BOUND;
            } else if (stateBindPoint == VK_STATE_BIND_COLOR_BLEND) {
                pTrav->obj.status |= OBJSTATUS_COLOR_BLEND_BOUND;
            } else if (stateBindPoint == VK_STATE_BIND_DEPTH_STENCIL) {
                pTrav->obj.status |= OBJSTATUS_DEPTH_STENCIL_BOUND;
            }
            return;
        }
        pTrav = pTrav->pNextObj;
    }

    // If we do not find it print an error
    char str[1024];
    sprintf(str, "Unable to track status for non-existent Command Buffer object %p", pObj);
    layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, pObj, 0, GLVSNAPSHOT_UNKNOWN_OBJECT, LAYER_ABBREV_STR, str);
}

// Reset selected flag state for an object node
static void reset_status(void* pObj, VK_OBJECT_TYPE objType, OBJECT_STATUS status_flag) {
    GLV_VK_SNAPSHOT_LL_NODE *pTrav = s_delta.pObjectHead[objType];
    while (pTrav) {
        if (pTrav->obj.pVkObject == pObj) {
            pTrav->obj.status &= ~status_flag;
            return;
        }
        pTrav = pTrav->pNextObj;
    }

    // If we do not find it print an error
    char str[1024];
    sprintf(str, "Unable to reset status for non-existent object %p of %s type", pObj, string_VK_OBJECT_TYPE(objType));
    layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, pObj, 0, GLVSNAPSHOT_UNKNOWN_OBJECT, LAYER_ABBREV_STR, str);
}

#include "vk_dispatch_table_helper.h"
static void initGlaveSnapshot(void)
{
    const char *strOpt;
    // initialize GlaveSnapshot options
    getLayerOptionEnum(LAYER_NAME_STR "ReportLevel", (uint32_t *) &g_reportingLevel);
    g_actionIsDefault = getLayerOptionEnum(LAYER_NAME_STR "DebugAction", (uint32_t *) &g_debugAction);

    if (g_debugAction & VK_DBG_LAYER_ACTION_LOG_MSG)
    {
        strOpt = getLayerOption(LAYER_NAME_STR "LogFilename");
        if (strOpt)
        {
            g_logFile = fopen(strOpt, "w");
        }
        if (g_logFile == NULL)
            g_logFile = stdout;
    }

    PFN_vkGetProcAddr fpNextGPA;
    fpNextGPA = pCurObj->pGPA;
    assert(fpNextGPA);

    layer_initialize_dispatch_table(&nextTable, fpNextGPA, (VkPhysicalGpu) pCurObj->nextObject);
    if (!objLockInitialized)
    {
        // TODO/TBD: Need to delete this mutex sometime.  How???
        loader_platform_thread_create_mutex(&objLock);
        objLockInitialized = 1;
    }
}

//=============================================================================
// vulkan entrypoints
//=============================================================================
VK_LAYER_EXPORT VkResult VKAPI vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance)
{
    VkResult result = nextTable.CreateInstance(pCreateInfo, pInstance);
    loader_platform_thread_lock_mutex(&objLock);
    snapshot_insert_object(&s_delta, *pInstance, VK_OBJECT_TYPE_INSTANCE);
    loader_platform_thread_unlock_mutex(&objLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyInstance(VkInstance instance)
{
    VkResult result = nextTable.DestroyInstance(instance);
    loader_platform_thread_lock_mutex(&objLock);
    snapshot_remove_object(&s_delta, (void*)instance);
    loader_platform_thread_unlock_mutex(&objLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkEnumerateGpus(VkInstance instance, uint32_t maxGpus, uint32_t* pGpuCount, VkPhysicalGpu* pGpus)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)instance, VK_OBJECT_TYPE_INSTANCE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.EnumerateGpus(instance, maxGpus, pGpuCount, pGpus);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetGpuInfo(VkPhysicalGpu gpu, VkPhysicalGpuInfoType infoType, size_t* pDataSize, void* pData)
{
    VkBaseLayerObject* gpuw = (VkBaseLayerObject *) gpu;
    pCurObj = gpuw;
    loader_platform_thread_once(&tabOnce, initGlaveSnapshot);
    VkResult result = nextTable.GetGpuInfo((VkPhysicalGpu)gpuw->nextObject, infoType, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDevice(VkPhysicalGpu gpu, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice)
{
    VkBaseLayerObject* gpuw = (VkBaseLayerObject *) gpu;
    pCurObj = gpuw;
    loader_platform_thread_once(&tabOnce, initGlaveSnapshot);
    VkResult result = nextTable.CreateDevice((VkPhysicalGpu)gpuw->nextObject, pCreateInfo, pDevice);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        snapshot_insert_device(&s_delta, gpu, pCreateInfo, pDevice);
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyDevice(VkDevice device)
{
    VkResult result = nextTable.DestroyDevice(device);
    loader_platform_thread_lock_mutex(&objLock);
    snapshot_remove_device(&s_delta, device);
    loader_platform_thread_unlock_mutex(&objLock);

    // Report any remaining objects in LL
    GLV_VK_SNAPSHOT_LL_NODE *pTrav = s_delta.pGlobalObjs;
    while (pTrav != NULL)
    {
        if (pTrav->obj.objType == VK_OBJECT_TYPE_SWAP_CHAIN_IMAGE_WSI ||
            pTrav->obj.objType == VK_OBJECT_TYPE_SWAP_CHAIN_MEMORY_WSI)
        {
            GLV_VK_SNAPSHOT_LL_NODE *pDel = pTrav;
            pTrav = pTrav->pNextGlobal;
            snapshot_remove_object(&s_delta, (void*)(pDel->obj.pVkObject));
        } else {
            char str[1024];
            sprintf(str, "OBJ ERROR : %s object %p has not been destroyed (was used %lu times).", string_VK_OBJECT_TYPE(pTrav->obj.objType), pTrav->obj.pVkObject, pTrav->obj.numUses);
            layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, device, 0, GLVSNAPSHOT_OBJECT_LEAK, LAYER_ABBREV_STR, str);
            pTrav = pTrav->pNextGlobal;
        }
    }

    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkEnumerateLayers(VkPhysicalGpu gpu, size_t maxStringSize, size_t* pLayerCount, char* const* pOutLayers, void* pReserved)
{
    if (gpu != NULL) {
        VkBaseLayerObject* gpuw = (VkBaseLayerObject *) gpu;
        loader_platform_thread_lock_mutex(&objLock);
        ll_increment_use_count((void*)gpu, VK_OBJECT_TYPE_PHYSICAL_GPU);
        loader_platform_thread_unlock_mutex(&objLock);
        pCurObj = gpuw;
        loader_platform_thread_once(&tabOnce, initGlaveSnapshot);
        VkResult result = nextTable.EnumerateLayers((VkPhysicalGpu)gpuw->nextObject, maxStringSize, pLayerCount, pOutLayers, pReserved);
        return result;
    } else {
        if (pLayerCount == NULL || pOutLayers == NULL || pOutLayers[0] == NULL)
            return VK_ERROR_INVALID_POINTER;
        // This layer compatible with all GPUs
        *pLayerCount = 1;
        strncpy((char *) pOutLayers[0], LAYER_NAME_STR, maxStringSize);
        return VK_SUCCESS;
    }
}

VK_LAYER_EXPORT VkResult VKAPI vkGetDeviceQueue(VkDevice device, uint32_t queueNodeIndex, uint32_t queueIndex, VkQueue* pQueue)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkQueueSubmit(VkQueue queue, uint32_t cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence)
{
    set_status((void*)fence, VK_OBJECT_TYPE_FENCE, OBJSTATUS_FENCE_IS_SUBMITTED);
    VkResult result = nextTable.QueueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkQueueWaitIdle(VkQueue queue)
{
    VkResult result = nextTable.QueueWaitIdle(queue);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDeviceWaitIdle(VkDevice device)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.DeviceWaitIdle(device);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkAllocMemory(VkDevice device, const VkMemoryAllocInfo* pAllocInfo, VkGpuMemory* pMem)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.AllocMemory(device, pAllocInfo, pMem);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pMem, VK_OBJECT_TYPE_GPU_MEMORY);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkFreeMemory(VkDevice device, VkGpuMemory mem)
{
    VkResult result = nextTable.FreeMemory(device, mem);
    loader_platform_thread_lock_mutex(&objLock);
    snapshot_remove_object(&s_delta, (void*)mem);
    loader_platform_thread_unlock_mutex(&objLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkSetMemoryPriority(VkDevice device, VkGpuMemory mem, VkMemoryPriority priority)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)mem, VK_OBJECT_TYPE_GPU_MEMORY);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.SetMemoryPriority(device, mem, priority);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkMapMemory(VkDevice device, VkGpuMemory mem, VkFlags flags, void** ppData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)mem, VK_OBJECT_TYPE_GPU_MEMORY);
    loader_platform_thread_unlock_mutex(&objLock);
    set_status((void*)mem, VK_OBJECT_TYPE_GPU_MEMORY, OBJSTATUS_GPU_MEM_MAPPED);
    VkResult result = nextTable.MapMemory(device, mem, flags, ppData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkUnmapMemory(VkDevice device, VkGpuMemory mem)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)mem, VK_OBJECT_TYPE_GPU_MEMORY);
    loader_platform_thread_unlock_mutex(&objLock);
    reset_status((void*)mem, VK_OBJECT_TYPE_GPU_MEMORY, OBJSTATUS_GPU_MEM_MAPPED);
    VkResult result = nextTable.UnmapMemory(device, mem);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkPinSystemMemory(VkDevice device, const void* pSysMem, size_t memSize, VkGpuMemory* pMem)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.PinSystemMemory(device, pSysMem, memSize, pMem);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetMultiGpuCompatibility(VkPhysicalGpu gpu0, VkPhysicalGpu gpu1, VkGpuCompatibilityInfo* pInfo)
{
    VkBaseLayerObject* gpuw = (VkBaseLayerObject *) gpu0;
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)gpu0, VK_OBJECT_TYPE_PHYSICAL_GPU);
    loader_platform_thread_unlock_mutex(&objLock);
    pCurObj = gpuw;
    loader_platform_thread_once(&tabOnce, initGlaveSnapshot);
    VkResult result = nextTable.GetMultiGpuCompatibility((VkPhysicalGpu)gpuw->nextObject, gpu1, pInfo);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkOpenSharedMemory(VkDevice device, const VkMemoryOpenInfo* pOpenInfo, VkGpuMemory* pMem)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.OpenSharedMemory(device, pOpenInfo, pMem);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkOpenSharedSemaphore(VkDevice device, const VkSemaphoreOpenInfo* pOpenInfo, VkSemaphore* pSemaphore)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.OpenSharedSemaphore(device, pOpenInfo, pSemaphore);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkOpenPeerMemory(VkDevice device, const VkPeerMemoryOpenInfo* pOpenInfo, VkGpuMemory* pMem)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.OpenPeerMemory(device, pOpenInfo, pMem);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkOpenPeerImage(VkDevice device, const VkPeerImageOpenInfo* pOpenInfo, VkImage* pImage, VkGpuMemory* pMem)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.OpenPeerImage(device, pOpenInfo, pImage, pMem);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDestroyObject(VkDevice device, VkObjectType objType, VkObject object)
{
    VkResult result = nextTable.DestroyObject(device, objType, object);
    loader_platform_thread_lock_mutex(&objLock);
    snapshot_remove_object(&s_delta, (void*)object);
    loader_platform_thread_unlock_mutex(&objLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetObjectInfo(VkDevice device, VkObjectType objType, VkObject object, VkObjectInfoType infoType, size_t* pDataSize, void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)object, ll_get_obj_type(object));
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetObjectInfo(device, objType, object, infoType, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkBindObjectMemory(VkQueue queue, VkObjectType objType, VkObject object, uint32_t allocationIdx, VkGpuMemory mem, VkGpuSize offset)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)object, ll_get_obj_type(object));
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.BindObjectMemory(queue, objType, object, allocationIdx, mem, offset);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkBindObjectMemoryRange(VkQueue queue, VkObjectType objType, VkObject object, uint32_t allocationIdx, VkGpuSize rangeOffset, VkGpuSize rangeSize, VkGpuMemory mem, VkGpuSize memOffset)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)object, ll_get_obj_type(object));
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.BindObjectMemoryRange(queue, objType, object, allocationIdx, rangeOffset, rangeSize, mem, memOffset);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkBindImageMemoryRange(VkImage image, uint32_t allocationIdx, const VkImageMemoryBindInfo* pBindInfo, VkGpuMemory mem, VkGpuSize memOffset)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)image, VK_OBJECT_TYPE_IMAGE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.BindImageMemoryRange(image, allocationIdx, pBindInfo, mem, memOffset);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateFence(device, pCreateInfo, pFence);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pFence, VK_OBJECT_TYPE_FENCE);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetFenceStatus(VkDevice device, VkFence fence)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)fence, VK_OBJECT_TYPE_FENCE);
    loader_platform_thread_unlock_mutex(&objLock);
    // Warn if submitted_flag is not set
    VkResult result = nextTable.GetFenceStatus(device, fence);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, bool32_t waitAll, uint64_t timeout)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.WaitForFences(device, fenceCount, pFences, waitAll, timeout);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateSemaphore(device, pCreateInfo, pSemaphore);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pSemaphore, VK_OBJECT_TYPE_QUEUE_SEMAPHORE);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkQueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore)
{
    VkResult result = nextTable.QueueSignalSemaphore(queue, semaphore);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
    VkResult result = nextTable.QueueWaitSemaphore(queue, semaphore);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateEvent(device, pCreateInfo, pEvent);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pEvent, VK_OBJECT_TYPE_EVENT);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetEventStatus(VkDevice device, VkEvent event)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)event, VK_OBJECT_TYPE_EVENT);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetEventStatus(device, event);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkSetEvent(VkDevice device, VkEvent event)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)event, VK_OBJECT_TYPE_EVENT);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.SetEvent(device, event);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkResetEvent(VkDevice device, VkEvent event)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)event, VK_OBJECT_TYPE_EVENT);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.ResetEvent(device, event);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateQueryPool(device, pCreateInfo, pQueryPool);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pQueryPool, VK_OBJECT_TYPE_QUERY_POOL);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t* pDataSize, void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)queryPool, VK_OBJECT_TYPE_QUERY_POOL);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetQueryPoolResults(device, queryPool, startQuery, queryCount, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetFormatInfo(VkDevice device, VkFormat format, VkFormatInfoType infoType, size_t* pDataSize, void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetFormatInfo(device, format, infoType, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateBuffer(device, pCreateInfo, pBuffer);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pBuffer, VK_OBJECT_TYPE_BUFFER);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateBufferView(device, pCreateInfo, pView);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pView, VK_OBJECT_TYPE_BUFFER_VIEW);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateImage(device, pCreateInfo, pImage);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pImage, VK_OBJECT_TYPE_IMAGE);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkGetImageSubresourceInfo(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceInfoType infoType, size_t* pDataSize, void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)image, VK_OBJECT_TYPE_IMAGE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetImageSubresourceInfo(device, image, pSubresource, infoType, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateImageView(device, pCreateInfo, pView);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pView, VK_OBJECT_TYPE_IMAGE_VIEW);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateColorAttachmentView(VkDevice device, const VkColorAttachmentViewCreateInfo* pCreateInfo, VkColorAttachmentView* pView)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateColorAttachmentView(device, pCreateInfo, pView);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pView, VK_OBJECT_TYPE_COLOR_ATTACHMENT_VIEW);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDepthStencilView(VkDevice device, const VkDepthStencilViewCreateInfo* pCreateInfo, VkDepthStencilView* pView)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateDepthStencilView(device, pCreateInfo, pView);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pView, VK_OBJECT_TYPE_DEPTH_STENCIL_VIEW);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateShader(VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateShader(device, pCreateInfo, pShader);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pShader, VK_OBJECT_TYPE_SHADER);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateGraphicsPipeline(VkDevice device, const VkGraphicsPipelineCreateInfo* pCreateInfo, VkPipeline* pPipeline)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateGraphicsPipeline(device, pCreateInfo, pPipeline);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pPipeline, VK_OBJECT_TYPE_PIPELINE);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateComputePipeline(VkDevice device, const VkComputePipelineCreateInfo* pCreateInfo, VkPipeline* pPipeline)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateComputePipeline(device, pCreateInfo, pPipeline);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pPipeline, VK_OBJECT_TYPE_PIPELINE);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkStorePipeline(VkDevice device, VkPipeline pipeline, size_t* pDataSize, void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)pipeline, VK_OBJECT_TYPE_PIPELINE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.StorePipeline(device, pipeline, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkLoadPipeline(VkDevice device, size_t dataSize, const void* pData, VkPipeline* pPipeline)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.LoadPipeline(device, dataSize, pData, pPipeline);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateSampler(device, pCreateInfo, pSampler);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pSampler, VK_OBJECT_TYPE_SAMPLER);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDescriptorSetLayout( VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateDescriptorSetLayout(device, pCreateInfo, pSetLayout);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkBeginDescriptorPoolUpdate(VkDevice device, VkDescriptorUpdateMode updateMode)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.BeginDescriptorPoolUpdate(device, updateMode);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkEndDescriptorPoolUpdate(VkDevice device, VkCmdBuffer cmd)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.EndDescriptorPoolUpdate(device, cmd);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDescriptorPool(VkDevice device, VkDescriptorPoolUsage poolUsage, uint32_t maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.ResetDescriptorPool(device, descriptorPool);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkAllocDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, uint32_t count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets, uint32_t* pCount)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.AllocDescriptorSets(device, descriptorPool, setUsage, count, pSetLayouts, pDescriptorSets, pCount);
    if (result == VK_SUCCESS)
    {
        for (uint32_t i = 0; i < *pCount; i++) {
            loader_platform_thread_lock_mutex(&objLock);
            GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, pDescriptorSets[i], VK_OBJECT_TYPE_DESCRIPTOR_SET);
            pNode->obj.pStruct = NULL;
            loader_platform_thread_unlock_mutex(&objLock);
        }
    }
    return result;
}

VK_LAYER_EXPORT void VKAPI vkClearDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.ClearDescriptorSets(device, descriptorPool, count, pDescriptorSets);
}

VK_LAYER_EXPORT void VKAPI vkUpdateDescriptors(VkDevice device, VkDescriptorSet descriptorSet, uint32_t updateCount, const void** ppUpdateArray)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)descriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.UpdateDescriptors(device, descriptorSet, updateCount, ppUpdateArray);
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicViewportState(VkDevice device, const VkDynamicVpStateCreateInfo* pCreateInfo, VkDynamicVpState* pState)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateDynamicViewportState(device, pCreateInfo, pState);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pState, VK_OBJECT_TYPE_DYNAMIC_VP_STATE_OBJECT);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicRasterState(VkDevice device, const VkDynamicRsStateCreateInfo* pCreateInfo, VkDynamicRsState* pState)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateDynamicRasterState(device, pCreateInfo, pState);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pState, VK_OBJECT_TYPE_DYNAMIC_RS_STATE_OBJECT);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicColorBlendState(VkDevice device, const VkDynamicCbStateCreateInfo* pCreateInfo, VkDynamicCbState* pState)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateDynamicColorBlendState(device, pCreateInfo, pState);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pState, VK_OBJECT_TYPE_DYNAMIC_CB_STATE_OBJECT);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateDynamicDepthStencilState(VkDevice device, const VkDynamicDsStateCreateInfo* pCreateInfo, VkDynamicDsState* pState)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateDynamicDepthStencilState(device, pCreateInfo, pState);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pState, VK_OBJECT_TYPE_DYNAMIC_DS_STATE_OBJECT);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateCommandBuffer(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateCommandBuffer(device, pCreateInfo, pCmdBuffer);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pCmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkBeginCommandBuffer(VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.BeginCommandBuffer(cmdBuffer, pBeginInfo);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkEndCommandBuffer(VkCmdBuffer cmdBuffer)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    reset_status((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER, (OBJSTATUS_VIEWPORT_BOUND    |
                                                                OBJSTATUS_RASTER_BOUND      |
                                                                OBJSTATUS_COLOR_BLEND_BOUND |
                                                                OBJSTATUS_DEPTH_STENCIL_BOUND));
    VkResult result = nextTable.EndCommandBuffer(cmdBuffer);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkResetCommandBuffer(VkCmdBuffer cmdBuffer)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.ResetCommandBuffer(cmdBuffer);
    return result;
}

VK_LAYER_EXPORT void VKAPI vkCmdBindPipeline(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);
}

VK_LAYER_EXPORT void VKAPI vkCmdBindDynamicStateObject(VkCmdBuffer cmdBuffer, VkStateBindPoint stateBindPoint, VkDynamicStateObject state)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    track_object_status((void*)cmdBuffer, stateBindPoint);
    nextTable.CmdBindDynamicStateObject(cmdBuffer, stateBindPoint, state);
}

VK_LAYER_EXPORT void VKAPI vkCmdBindDescriptorSets(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkDescriptorSetLayoutChain layoutChain, uint32_t layoutChainSlot, uint32_t count, const VkDescriptorSet* pDescriptorSets, const uint32_t* pUserData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layoutChain, layoutChainSlot, count, pDescriptorSets, pUserData);
}

VK_LAYER_EXPORT void VKAPI vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers
    const VkGpuSize*                            pOffsets)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);
}

VK_LAYER_EXPORT void VKAPI vkCmdBindIndexBuffer(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkGpuSize offset, VkIndexType indexType)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);
}

VK_LAYER_EXPORT void VKAPI vkCmdDraw(VkCmdBuffer cmdBuffer, uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDraw(cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);
}

VK_LAYER_EXPORT void VKAPI vkCmdDrawIndexed(VkCmdBuffer cmdBuffer, uint32_t firstIndex, uint32_t indexCount, int32_t vertexOffset, uint32_t firstInstance, uint32_t instanceCount)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDrawIndexed(cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
}

VK_LAYER_EXPORT void VKAPI vkCmdDrawIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkGpuSize offset, uint32_t count, uint32_t stride)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDrawIndirect(cmdBuffer, buffer, offset, count, stride);
}

VK_LAYER_EXPORT void VKAPI vkCmdDrawIndexedIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkGpuSize offset, uint32_t count, uint32_t stride)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDrawIndexedIndirect(cmdBuffer, buffer, offset, count, stride);
}

VK_LAYER_EXPORT void VKAPI vkCmdDispatch(VkCmdBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDispatch(cmdBuffer, x, y, z);
}

VK_LAYER_EXPORT void VKAPI vkCmdDispatchIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkGpuSize offset)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDispatchIndirect(cmdBuffer, buffer, offset);
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyBuffer(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageCopy* pRegions)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdCopyImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyBufferToImage(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);
}

VK_LAYER_EXPORT void VKAPI vkCmdCopyImageToBuffer(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);
}

VK_LAYER_EXPORT void VKAPI vkCmdCloneImageData(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdCloneImageData(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout);
}

VK_LAYER_EXPORT void VKAPI vkCmdUpdateBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkGpuSize destOffset, VkGpuSize dataSize, const uint32_t* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdUpdateBuffer(cmdBuffer, destBuffer, destOffset, dataSize, pData);
}

VK_LAYER_EXPORT void VKAPI vkCmdFillBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkGpuSize destOffset, VkGpuSize fillSize, uint32_t data)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdFillBuffer(cmdBuffer, destBuffer, destOffset, fillSize, data);
}

VK_LAYER_EXPORT void VKAPI vkCmdClearColorImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, VkClearColor color, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdClearColorImage(cmdBuffer, image, imageLayout, color, rangeCount, pRanges);
}

VK_LAYER_EXPORT void VKAPI vkCmdClearDepthStencil(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, float depth, uint32_t stencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdClearDepthStencil(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);
}

VK_LAYER_EXPORT void VKAPI vkCmdResolveImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t rectCount, const VkImageResolve* pRects)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdResolveImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, rectCount, pRects);
}

VK_LAYER_EXPORT void VKAPI vkCmdSetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipeEvent pipeEvent)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdSetEvent(cmdBuffer, event, pipeEvent);
}

VK_LAYER_EXPORT void VKAPI vkCmdResetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipeEvent pipeEvent)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdResetEvent(cmdBuffer, event, pipeEvent);
}

VK_LAYER_EXPORT void VKAPI vkCmdWaitEvents(VkCmdBuffer cmdBuffer, const VkEventWaitInfo* pWaitInfo)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdWaitEvents(cmdBuffer, pWaitInfo);
}

VK_LAYER_EXPORT void VKAPI vkCmdPipelineBarrier(VkCmdBuffer cmdBuffer, const VkPipelineBarrier* pBarrier)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdPipelineBarrier(cmdBuffer, pBarrier);
}

VK_LAYER_EXPORT void VKAPI vkCmdBeginQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkFlags flags)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdBeginQuery(cmdBuffer, queryPool, slot, flags);
}

VK_LAYER_EXPORT void VKAPI vkCmdEndQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdEndQuery(cmdBuffer, queryPool, slot);
}

VK_LAYER_EXPORT void VKAPI vkCmdResetQueryPool(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdResetQueryPool(cmdBuffer, queryPool, startQuery, queryCount);
}

VK_LAYER_EXPORT void VKAPI vkCmdWriteTimestamp(VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkGpuSize destOffset)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdWriteTimestamp(cmdBuffer, timestampType, destBuffer, destOffset);
}

VK_LAYER_EXPORT void VKAPI vkCmdInitAtomicCounters(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, uint32_t startCounter, uint32_t counterCount, const uint32_t* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdInitAtomicCounters(cmdBuffer, pipelineBindPoint, startCounter, counterCount, pData);
}

VK_LAYER_EXPORT void VKAPI vkCmdLoadAtomicCounters(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, uint32_t startCounter, uint32_t counterCount, VkBuffer srcBuffer, VkGpuSize srcOffset)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdLoadAtomicCounters(cmdBuffer, pipelineBindPoint, startCounter, counterCount, srcBuffer, srcOffset);
}

VK_LAYER_EXPORT void VKAPI vkCmdSaveAtomicCounters(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, uint32_t startCounter, uint32_t counterCount, VkBuffer destBuffer, VkGpuSize destOffset)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdSaveAtomicCounters(cmdBuffer, pipelineBindPoint, startCounter, counterCount, destBuffer, destOffset);
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateFramebuffer(device, pCreateInfo, pFramebuffer);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateRenderPass(device, pCreateInfo, pRenderPass);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pRenderPass, VK_OBJECT_TYPE_RENDER_PASS);
        pNode->obj.pStruct = NULL;
        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;
}

VK_LAYER_EXPORT void VKAPI vkCmdBeginRenderPass(VkCmdBuffer cmdBuffer, const VkRenderPassBegin *pRenderPassBegin)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdBeginRenderPass(cmdBuffer, pRenderPassBegin);
}

VK_LAYER_EXPORT void VKAPI vkCmdEndRenderPass(VkCmdBuffer cmdBuffer, VkRenderPass renderPass)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdEndRenderPass(cmdBuffer, renderPass);
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgSetValidationLevel(VkDevice device, VkValidationLevel validationLevel)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.DbgSetValidationLevel(device, validationLevel);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgRegisterMsgCallback(VkInstance instance, VK_DBG_MSG_CALLBACK_FUNCTION pfnMsgCallback, void* pUserData)
{
    // This layer intercepts callbacks
    VK_LAYER_DBG_FUNCTION_NODE *pNewDbgFuncNode = (VK_LAYER_DBG_FUNCTION_NODE*)malloc(sizeof(VK_LAYER_DBG_FUNCTION_NODE));
    if (!pNewDbgFuncNode)
        return VK_ERROR_OUT_OF_MEMORY;
    pNewDbgFuncNode->pfnMsgCallback = pfnMsgCallback;
    pNewDbgFuncNode->pUserData = pUserData;
    pNewDbgFuncNode->pNext = g_pDbgFunctionHead;
    g_pDbgFunctionHead = pNewDbgFuncNode;
    // force callbacks if DebugAction hasn't been set already other than initial value
    if (g_actionIsDefault) {
        g_debugAction = VK_DBG_LAYER_ACTION_CALLBACK;
    }    VkResult result = nextTable.DbgRegisterMsgCallback(instance, pfnMsgCallback, pUserData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgUnregisterMsgCallback(VkInstance instance, VK_DBG_MSG_CALLBACK_FUNCTION pfnMsgCallback)
{
    VK_LAYER_DBG_FUNCTION_NODE *pTrav = g_pDbgFunctionHead;
    VK_LAYER_DBG_FUNCTION_NODE *pPrev = pTrav;
    while (pTrav) {
        if (pTrav->pfnMsgCallback == pfnMsgCallback) {
            pPrev->pNext = pTrav->pNext;
            if (g_pDbgFunctionHead == pTrav)
                g_pDbgFunctionHead = pTrav->pNext;
            free(pTrav);
            break;
        }
        pPrev = pTrav;
        pTrav = pTrav->pNext;
    }
    if (g_pDbgFunctionHead == NULL)
    {
        if (g_actionIsDefault)
            g_debugAction = VK_DBG_LAYER_ACTION_LOG_MSG;
        else
            g_debugAction &= ~VK_DBG_LAYER_ACTION_CALLBACK;
    }
    VkResult result = nextTable.DbgUnregisterMsgCallback(instance, pfnMsgCallback);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgSetMessageFilter(VkDevice device, int32_t msgCode, VK_DBG_MSG_FILTER filter)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.DbgSetMessageFilter(device, msgCode, filter);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgSetObjectTag(VkObject object, size_t tagSize, const void* pTag)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)object, ll_get_obj_type(object));
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.DbgSetObjectTag(object, tagSize, pTag);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgSetGlobalOption(VkInstance instance, VK_DBG_GLOBAL_OPTION dbgOption, size_t dataSize, const void* pData)
{
    VkResult result = nextTable.DbgSetGlobalOption(instance, dbgOption, dataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI vkDbgSetDeviceOption(VkDevice device, VK_DBG_DEVICE_OPTION dbgOption, size_t dataSize, const void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.DbgSetDeviceOption(device, dbgOption, dataSize, pData);
    return result;
}

VK_LAYER_EXPORT void VKAPI vkCmdDbgMarkerBegin(VkCmdBuffer cmdBuffer, const char* pMarker)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDbgMarkerBegin(cmdBuffer, pMarker);
}

VK_LAYER_EXPORT void VKAPI vkCmdDbgMarkerEnd(VkCmdBuffer cmdBuffer)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)cmdBuffer, VK_OBJECT_TYPE_CMD_BUFFER);
    loader_platform_thread_unlock_mutex(&objLock);
    nextTable.CmdDbgMarkerEnd(cmdBuffer);
}

VK_LAYER_EXPORT VkResult VKAPI xglGetDisplayInfoWSI(VkDisplayWSI display, VkDisplayInfoTypeWSI infoType, size_t* pDataSize, void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)display, VK_OBJECT_TYPE_DISPLAY_WSI);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetDisplayInfoWSI(display, infoType, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI xglCreateSwapChainWSI(VkDevice device, const VkSwapChainCreateInfoWSI* pCreateInfo, VkSwapChainWSI* pSwapChain)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)device, VK_OBJECT_TYPE_DEVICE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.CreateSwapChainWSI(device, pCreateInfo, pSwapChain);
    if (result == VK_SUCCESS)
    {
        loader_platform_thread_lock_mutex(&objLock);

#if 0
        GLV_VK_SNAPSHOT_LL_NODE* pNode = snapshot_insert_object(&s_delta, *pImage, VK_OBJECT_TYPE_IMAGE);
        pNode->obj.pStruct = NULL;

        GLV_VK_SNAPSHOT_LL_NODE* pMemNode = snapshot_insert_object(&s_delta, *pMem, VK_OBJECT_TYPE_PRESENTABLE_IMAGE_MEMORY);
        pMemNode->obj.pStruct = NULL;
#else
        snapshot_insert_object(&s_delta, (void*)*pSwapChain, VK_OBJECT_TYPE_SWAP_CHAIN_WSI);
#endif

        loader_platform_thread_unlock_mutex(&objLock);
    }
    return result;

}

VK_LAYER_EXPORT VkResult VKAPI xglDestroySwapChainWSI(VkSwapChainWSI swapChain)
{
    VkResult result = nextTable.DestroySwapChainWSI(swapChain);
    loader_platform_thread_lock_mutex(&objLock);
    snapshot_remove_object(&s_delta, (void*)swapChain);
    loader_platform_thread_unlock_mutex(&objLock);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI xglGetSwapChainInfoWSI(VkSwapChainWSI swapChain, VkSwapChainInfoTypeWSI infoType, size_t* pDataSize, void* pData)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)swapChain, VK_OBJECT_TYPE_SWAP_CHAIN_WSI);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.GetSwapChainInfoWSI(swapChain, infoType, pDataSize, pData);
    return result;
}

VK_LAYER_EXPORT VkResult VKAPI xglQueuePresentWSI(VkQueue queue, const VkPresentInfoWSI* pPresentInfo)
{
    loader_platform_thread_lock_mutex(&objLock);
    ll_increment_use_count((void*)queue, VK_OBJECT_TYPE_QUEUE);
    loader_platform_thread_unlock_mutex(&objLock);
    VkResult result = nextTable.QueuePresentWSI(queue, pPresentInfo);
    return result;
}

//=================================================================================================
// Exported methods
//=================================================================================================
void glvSnapshotStartTracking(void)
{
    assert(!"Not Implemented");
}

//=================================================================================================
GLV_VK_SNAPSHOT glvSnapshotGetDelta(void)
{
    // copy the delta by merging it into an empty snapshot
    GLV_VK_SNAPSHOT empty;
    memset(&empty, 0, sizeof(GLV_VK_SNAPSHOT));

    return glvSnapshotMerge(&s_delta, &empty);
}

//=================================================================================================
GLV_VK_SNAPSHOT glvSnapshotGetSnapshot(void)
{
    // copy the master snapshot by merging it into an empty snapshot
    GLV_VK_SNAPSHOT empty;
    memset(&empty, 0, sizeof(GLV_VK_SNAPSHOT));

    return glvSnapshotMerge(&s_snapshot, &empty);
}

//=================================================================================================
void glvSnapshotPrintDelta()
{
    char str[2048];
    GLV_VK_SNAPSHOT_LL_NODE* pTrav = s_delta.pGlobalObjs;
    sprintf(str, "==== DELTA SNAPSHOT contains %lu objects, %lu devices, and %lu deleted objects", s_delta.globalObjCount, s_delta.deviceCount, s_delta.deltaDeletedObjectCount);
    layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, NULL, 0, GLVSNAPSHOT_SNAPSHOT_DATA, LAYER_ABBREV_STR, str);

    // print all objects
    if (s_delta.globalObjCount > 0)
    {
        sprintf(str, "======== DELTA SNAPSHOT Created Objects:");
        layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, pTrav->obj.pVkObject, 0, GLVSNAPSHOT_SNAPSHOT_DATA, LAYER_ABBREV_STR, str);
        while (pTrav != NULL)
        {
            sprintf(str, "\t%s obj %p", string_VK_OBJECT_TYPE(pTrav->obj.objType), pTrav->obj.pVkObject);
            layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, pTrav->obj.pVkObject, 0, GLVSNAPSHOT_SNAPSHOT_DATA, LAYER_ABBREV_STR, str);
            pTrav = pTrav->pNextGlobal;
        }
    }

    // print devices
    if (s_delta.deviceCount > 0)
    {
        GLV_VK_SNAPSHOT_LL_NODE* pDeviceNode = s_delta.pDevices;
        sprintf(str, "======== DELTA SNAPSHOT Devices:");
        layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, NULL, 0, GLVSNAPSHOT_SNAPSHOT_DATA, LAYER_ABBREV_STR, str);
        while (pDeviceNode != NULL)
        {
            GLV_VK_SNAPSHOT_DEVICE_NODE* pDev = (GLV_VK_SNAPSHOT_DEVICE_NODE*)pDeviceNode->obj.pStruct;
            char * createInfoStr = vk_print_vkdevicecreateinfo(pDev->params.pCreateInfo, "\t\t");
            sprintf(str, "\t%s obj %p:\n%s", string_VK_OBJECT_TYPE(VK_OBJECT_TYPE_DEVICE), pDeviceNode->obj.pVkObject, createInfoStr);
            layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, pDeviceNode->obj.pVkObject, 0, GLVSNAPSHOT_SNAPSHOT_DATA, LAYER_ABBREV_STR, str);
            pDeviceNode = pDeviceNode->pNextObj;
        }
    }

    // print deleted objects
    if (s_delta.deltaDeletedObjectCount > 0)
    {
        GLV_VK_SNAPSHOT_DELETED_OBJ_NODE* pDelObjNode = s_delta.pDeltaDeletedObjects;
        sprintf(str, "======== DELTA SNAPSHOT Deleted Objects:");
        layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, NULL, 0, GLVSNAPSHOT_SNAPSHOT_DATA, LAYER_ABBREV_STR, str);
        while (pDelObjNode != NULL)
        {
            sprintf(str, "         %s obj %p", string_VK_OBJECT_TYPE(pDelObjNode->objType), pDelObjNode->pVkObject);
            layerCbMsg(VK_DBG_MSG_UNKNOWN, VK_VALIDATION_LEVEL_0, pDelObjNode->pVkObject, 0, GLVSNAPSHOT_SNAPSHOT_DATA, LAYER_ABBREV_STR, str);
            pDelObjNode = pDelObjNode->pNextObj;
        }
    }
}

void glvSnapshotStopTracking(void)
{
    assert(!"Not Implemented");
}

void glvSnapshotClear(void)
{
    assert(!"Not Implemented");
}

GLV_VK_SNAPSHOT glvSnapshotMerge(const GLV_VK_SNAPSHOT* const pDelta, const GLV_VK_SNAPSHOT* const pSnapshot)
{
    assert(!"Not Implemented");
}




//=============================================================================
// Old Exported methods
//=============================================================================
uint64_t glvSnapshotGetObjectCount(VK_OBJECT_TYPE type)
{
    uint64_t retVal = (type == VK_OBJECT_TYPE_ANY) ? s_delta.globalObjCount : s_delta.numObjs[type];
    return retVal;
}

VkResult glvSnapshotGetObjects(VK_OBJECT_TYPE type, uint64_t objCount, GLV_VK_SNAPSHOT_OBJECT_NODE *pObjNodeArray)
{
    // This bool flags if we're pulling all objs or just a single class of objs
    bool32_t bAllObjs = (type == VK_OBJECT_TYPE_ANY);
    // Check the count first thing
    uint64_t maxObjCount = (bAllObjs) ? s_delta.globalObjCount : s_delta.numObjs[type];
    if (objCount > maxObjCount) {
        char str[1024];
        sprintf(str, "OBJ ERROR : Received objTrackGetObjects() request for %lu objs, but there are only %lu objs of type %s", objCount, maxObjCount, string_VK_OBJECT_TYPE(type));
        layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, 0, 0, GLVSNAPSHOT_OBJCOUNT_MAX_EXCEEDED, LAYER_ABBREV_STR, str);
        return VK_ERROR_INVALID_VALUE;
    }

    GLV_VK_SNAPSHOT_LL_NODE* pTrav = (bAllObjs) ? s_delta.pGlobalObjs : s_delta.pObjectHead[type];

    for (uint64_t i = 0; i < objCount; i++) {
        if (!pTrav) {
            char str[1024];
            sprintf(str, "OBJ INTERNAL ERROR : Ran out of %s objs! Should have %lu, but only copied %lu and not the requested %lu.", string_VK_OBJECT_TYPE(type), maxObjCount, i, objCount);
            layerCbMsg(VK_DBG_MSG_ERROR, VK_VALIDATION_LEVEL_0, 0, 0, GLVSNAPSHOT_INTERNAL_ERROR, LAYER_ABBREV_STR, str);
            return VK_ERROR_UNKNOWN;
        }
        memcpy(&pObjNodeArray[i], pTrav, sizeof(GLV_VK_SNAPSHOT_OBJECT_NODE));
        pTrav = (bAllObjs) ? pTrav->pNextGlobal : pTrav->pNextObj;
    }
    return VK_SUCCESS;
}

void glvSnapshotPrintObjects(void)
{
    glvSnapshotPrintDelta();
}

#include "vk_generic_intercept_proc_helper.h"
VK_LAYER_EXPORT void* VKAPI vkGetProcAddr(VkPhysicalGpu gpu, const char* funcName)
{
    VkBaseLayerObject* gpuw = (VkBaseLayerObject *) gpu;
    void* addr;
    if (gpu == NULL)
        return NULL;
    pCurObj = gpuw;
    loader_platform_thread_once(&tabOnce, initGlaveSnapshot);

    addr = layer_intercept_proc(funcName);
    if (addr)
        return addr;
    else if (!strncmp("glvSnapshotGetObjectCount", funcName, sizeof("glvSnapshotGetObjectCount")))
        return glvSnapshotGetObjectCount;
    else if (!strncmp("glvSnapshotGetObjects", funcName, sizeof("glvSnapshotGetObjects")))
        return glvSnapshotGetObjects;
    else if (!strncmp("glvSnapshotPrintObjects", funcName, sizeof("glvSnapshotPrintObjects")))
        return glvSnapshotPrintObjects;
    else if (!strncmp("glvSnapshotStartTracking", funcName, sizeof("glvSnapshotStartTracking")))
        return glvSnapshotStartTracking;
    else if (!strncmp("glvSnapshotGetDelta", funcName, sizeof("glvSnapshotGetDelta")))
        return glvSnapshotGetDelta;
    else if (!strncmp("glvSnapshotGetSnapshot", funcName, sizeof("glvSnapshotGetSnapshot")))
        return glvSnapshotGetSnapshot;
    else if (!strncmp("glvSnapshotPrintDelta", funcName, sizeof("glvSnapshotPrintDelta")))
        return glvSnapshotPrintDelta;
    else if (!strncmp("glvSnapshotStopTracking", funcName, sizeof("glvSnapshotStopTracking")))
        return glvSnapshotStopTracking;
    else if (!strncmp("glvSnapshotClear", funcName, sizeof("glvSnapshotClear")))
        return glvSnapshotClear;
    else if (!strncmp("glvSnapshotMerge", funcName, sizeof("glvSnapshotMerge")))
        return glvSnapshotMerge;
    else {
        if (gpuw->pGPA == NULL)
            return NULL;
        return gpuw->pGPA((VkPhysicalGpu)gpuw->nextObject, funcName);
    }
}

