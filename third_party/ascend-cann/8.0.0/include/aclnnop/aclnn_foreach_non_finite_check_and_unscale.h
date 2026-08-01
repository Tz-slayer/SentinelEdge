
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_FOREACH_NON_FINITE_CHECK_AND_UNSCALE_H_
#define ACLNN_FOREACH_NON_FINITE_CHECK_AND_UNSCALE_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnForeachNonFiniteCheckAndUnscaleGetWorkspaceSize
 * parameters :
 * scaledGrads : dynamic
 * foundInf : required
 * invScale : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnForeachNonFiniteCheckAndUnscaleGetWorkspaceSize(
    const aclTensorList *scaledGrads,
    const aclTensor *foundInf,
    const aclTensor *invScale,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnForeachNonFiniteCheckAndUnscale
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnForeachNonFiniteCheckAndUnscale(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
