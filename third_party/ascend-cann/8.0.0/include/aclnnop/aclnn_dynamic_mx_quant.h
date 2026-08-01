
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_DYNAMIC_MX_QUANT_H_
#define ACLNN_DYNAMIC_MX_QUANT_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnDynamicMxQuantGetWorkspaceSize
 * parameters :
 * x : required
 * axis : optional
 * roundModeOptional : optional
 * dstType : optional
 * blocksize : optional
 * yOut : required
 * mxscaleOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnDynamicMxQuantGetWorkspaceSize(
    const aclTensor *x,
    int64_t axis,
    char *roundModeOptional,
    int64_t dstType,
    int64_t blocksize,
    const aclTensor *yOut,
    const aclTensor *mxscaleOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnDynamicMxQuant
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnDynamicMxQuant(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
