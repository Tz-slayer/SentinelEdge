
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_MOE_TOKEN_UNPERMUTE_WITH_EP_GRAD_H_
#define ACLNN_MOE_TOKEN_UNPERMUTE_WITH_EP_GRAD_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnMoeTokenUnpermuteWithEpGradGetWorkspaceSize
 * parameters :
 * unpermutedTokensGrad : required
 * sortedIndices : required
 * permutedTokensOptional : optional
 * probsOptional : optional
 * paddedMode : optional
 * restoreShapeOptional : optional
 * rangeOptional : optional
 * topkNum : optional
 * permutedTokensGradOut : required
 * probsGradOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenUnpermuteWithEpGradGetWorkspaceSize(
    const aclTensor *unpermutedTokensGrad,
    const aclTensor *sortedIndices,
    const aclTensor *permutedTokensOptional,
    const aclTensor *probsOptional,
    bool paddedMode,
    const aclIntArray *restoreShapeOptional,
    const aclIntArray *rangeOptional,
    int64_t topkNum,
    const aclTensor *permutedTokensGradOut,
    const aclTensor *probsGradOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnMoeTokenUnpermuteWithEpGrad
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenUnpermuteWithEpGrad(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
