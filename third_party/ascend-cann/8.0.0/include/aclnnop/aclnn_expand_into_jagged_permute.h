
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_EXPAND_INTO_JAGGED_PERMUTE_H_
#define ACLNN_EXPAND_INTO_JAGGED_PERMUTE_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnExpandIntoJaggedPermuteGetWorkspaceSize
 * parameters :
 * permute : required
 * inputOffsets : required
 * outputOffsets : required
 * outputSize : required
 * out : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnExpandIntoJaggedPermuteGetWorkspaceSize(
    const aclTensor *permute,
    const aclTensor *inputOffsets,
    const aclTensor *outputOffsets,
    int64_t outputSize,
    const aclTensor *out,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnExpandIntoJaggedPermute
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnExpandIntoJaggedPermute(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
