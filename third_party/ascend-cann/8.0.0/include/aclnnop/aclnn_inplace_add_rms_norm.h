
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_INPLACE_ADD_RMS_NORM_H_
#define ACLNN_INPLACE_ADD_RMS_NORM_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnInplaceAddRmsNormGetWorkspaceSize
 * parameters :
 * x1Ref : required
 * x2Ref : required
 * gamma : required
 * epsilon : optional
 * x1Ref : required
 * rstdOut : required
 * x2Ref : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnInplaceAddRmsNormGetWorkspaceSize(
    aclTensor *x1Ref,
    aclTensor *x2Ref,
    const aclTensor *gamma,
    double epsilon,
    const aclTensor *rstdOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnInplaceAddRmsNorm
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnInplaceAddRmsNorm(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
