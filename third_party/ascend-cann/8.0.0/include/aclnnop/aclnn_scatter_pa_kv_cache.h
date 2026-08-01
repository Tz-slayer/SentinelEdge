
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_SCATTER_PA_KV_CACHE_H_
#define ACLNN_SCATTER_PA_KV_CACHE_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnScatterPaKvCacheGetWorkspaceSize
 * parameters :
 * key : required
 * keyCacheRef : required
 * slotMapping : required
 * value : required
 * valueCacheRef : required
 * compressLensOptional : optional
 * compressSeqOffsetOptional : optional
 * seqLensOptional : optional
 * cacheModeOptional : optional
 * scatterModeOptional : optional
 * stridesOptional : optional
 * offsetsOptional : optional
 * keyCacheRef : required
 * valueCacheRef : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnScatterPaKvCacheGetWorkspaceSize(
    const aclTensor *key,
    aclTensor *keyCacheRef,
    const aclTensor *slotMapping,
    const aclTensor *value,
    aclTensor *valueCacheRef,
    const aclTensor *compressLensOptional,
    const aclTensor *compressSeqOffsetOptional,
    const aclTensor *seqLensOptional,
    char *cacheModeOptional,
    char *scatterModeOptional,
    const aclIntArray *stridesOptional,
    const aclIntArray *offsetsOptional,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnScatterPaKvCache
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnScatterPaKvCache(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
