
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_KV_RMS_NORM_ROPE_CACHE_H_
#define ACLNN_KV_RMS_NORM_ROPE_CACHE_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnKvRmsNormRopeCacheGetWorkspaceSize
 * parameters :
 * kv : required
 * gamma : required
 * cos : required
 * sin : required
 * index : required
 * kCacheRef : required
 * ckvCacheRef : required
 * kRopeScaleOptional : optional
 * cKvScaleOptional : optional
 * kRopeOffsetOptional : optional
 * cKvOffsetOptional : optional
 * epsilon : optional
 * cacheModeOptional : optional
 * isOutputKv : optional
 * kCacheRef : required
 * ckvCacheRef : required
 * kRopeOut : required
 * cKvOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnKvRmsNormRopeCacheGetWorkspaceSize(
    const aclTensor *kv,
    const aclTensor *gamma,
    const aclTensor *cos,
    const aclTensor *sin,
    const aclTensor *index,
    aclTensor *kCacheRef,
    aclTensor *ckvCacheRef,
    const aclTensor *kRopeScaleOptional,
    const aclTensor *cKvScaleOptional,
    const aclTensor *kRopeOffsetOptional,
    const aclTensor *cKvOffsetOptional,
    double epsilon,
    char *cacheModeOptional,
    bool isOutputKv,
    const aclTensor *kRopeOut,
    const aclTensor *cKvOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnKvRmsNormRopeCache
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnKvRmsNormRopeCache(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
