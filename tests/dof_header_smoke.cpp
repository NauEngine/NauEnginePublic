#include "nau/mgpu/bridge/dof_bridge.h"
#include <type_traits>

static_assert(!std::is_copy_constructible<nau::mgpu::bridge::DofBridge>::value);
static_assert(sizeof(nau::mgpu::bridge::DofParameters) == 6 * sizeof(float));

int main()
{
    const nau::mgpu::bridge::DofParameters parameters;
    return parameters.taps == 32.0f && parameters.maxCoC == 3.4f ? 0 : 1;
}
