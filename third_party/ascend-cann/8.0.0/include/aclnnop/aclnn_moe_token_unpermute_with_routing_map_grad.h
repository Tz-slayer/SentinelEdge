
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_MOE_TOKEN_UNPERMUTE_WITH_ROUTING_MAP_GRAD_H_
#define ACLNN_MOE_TOKEN_UNPERMUTE_WITH_ROUTING_MAP_GRAD_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnMoeTokenUnpermuteWithRoutingMapGradGetWorkspaceSize
 * parameters :
 * unpermutedTokensGrad : required
 * outIndex : required
 * permuteTokenId : required
 * routingMapOptional : optional
 * permutedTokensOptional : optional
 * probsOptional : optional
 * dropAndPad : optional
 * restoreShapeOptional : optional
 * permutedTokensGradOut : required
 * probsGradOutOptional : optional
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenUnpermuteWithRoutingMapGradGetWorkspaceSize(
    const aclTensor *unpermutedTokensGrad,
    const aclTensor *outIndex,
    const aclTensor *permuteTokenId,
    const aclTensor *routingMapOptional,
    const aclTensor *permutedTokensOptional,
    const aclTensor *probsOptional,
    bool dropAndPad,
    const aclIntArray *restoreShapeOptional,
    const aclTensor *permutedTokensGradOut,
    const aclTensor *probsGradOutOptional,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnMoeTokenUnpermuteWithRoutingMapGrad
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenUnpermuteWithRoutingMapGrad(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
