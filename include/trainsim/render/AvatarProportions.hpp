#pragma once

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Quaternion.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBone.hpp"
#include <span>

namespace TrainSim::Render {

// Apply once to a fresh animation pose, never to an already adjusted previous-frame pose.
// CNA's documented custom-bone path propagates local scale to face, hair and native headwear.
inline void applyPassengerProportions(std::span<Microsoft::Xna::Framework::Matrix> bones)
{
    using namespace Microsoft::Xna::Framework;
    const auto head=static_cast<std::size_t>(GamerServices::AvatarBone::Head);
    if (head >= bones.size()) return;
    bones[head]=Matrix::CreateScale(0.74f)*bones[head];
}

// Moves a rotated, translated bone part of the way to another pose. Matrix::Lerp of two rotations
// shrinks and shears the rows in between, which CNA's AvatarRenderer refuses as XNA's
// Matrix::Decompose does ("Every avatar bone transform must be decomposable"); the rotations are
// blended as quaternions and the translations linearly instead.
[[nodiscard]] inline Microsoft::Xna::Framework::Matrix blendBone(
    const Microsoft::Xna::Framework::Matrix& from, const Microsoft::Xna::Framework::Matrix& to,
    float amount)
{
    using namespace Microsoft::Xna::Framework;
    if (amount <= 0.0f) return from;
    if (amount >= 1.0f) return to;
    const Quaternion rotation = Quaternion::Slerp(Quaternion::CreateFromRotationMatrix(from),
                                                  Quaternion::CreateFromRotationMatrix(to),amount);
    return Matrix::CreateFromQuaternion(rotation)
        * Matrix::CreateTranslation(Vector3::Lerp(from.getTranslationProperty(),
                                                  to.getTranslationProperty(),amount));
}

} // namespace TrainSim::Render
