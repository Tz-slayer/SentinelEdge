
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_MOE_TOKEN_PERMUTE_WITH_EP_H_
#define ACLNN_MOE_TOKEN_PERMUTE_WITH_EP_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnMoeTokenPermuteWithEpGetWorkspaceSize
 * parameters :
 * tokens : required
 * indices : required
 * probsOptional : optional
 * rangeOptional : optional
 * numOutTokens : optional
 * paddedMode : optional
 * permuteTokensOut : required
 * sortedIndicesOut : required
 * permuteProbsOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenPermuteWithEpGetWorkspaceSize(
    const aclTensor *tokens,
    const aclTensor *indices,
    const aclTensor *probsOptional,
    const aclIntArray *rangeOptional,
    int64_t numOutTokens,
    bool paddedMode,
    const aclTensor *permuteTokensOut,
    const aclTensor *sortedIndicesOut,
    const aclTensor *permuteProbsOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnMoeTokenPermuteWithEp
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenPermuteWithEp(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
