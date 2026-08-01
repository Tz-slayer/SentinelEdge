
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_MOE_TOKEN_PERMUTE_WITH_EP_GRAD_H_
#define ACLNN_MOE_TOKEN_PERMUTE_WITH_EP_GRAD_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnMoeTokenPermuteWithEpGradGetWorkspaceSize
 * parameters :
 * permutedTokensOutputGrad : required
 * sortedIndices : required
 * permutedProbsOutputGradOptional : optional
 * numTopk : required
 * rangeOptional : optional
 * paddedMode : optional
 * tokenGradOut : required
 * probsGradOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenPermuteWithEpGradGetWorkspaceSize(
    const aclTensor *permutedTokensOutputGrad,
    const aclTensor *sortedIndices,
    const aclTensor *permutedProbsOutputGradOptional,
    int64_t numTopk,
    const aclIntArray *rangeOptional,
    bool paddedMode,
    const aclTensor *tokenGradOut,
    const aclTensor *probsGradOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnMoeTokenPermuteWithEpGrad
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenPermuteWithEpGrad(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
