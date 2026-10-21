#include "SceneShared.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;

namespace TrainSim::Render {

namespace {
// Seated rest pose shared with onboard passengers: hips and knees at right angles and both
// hands resting on the lap. CNA's stock clips have no seated animation.
void applySeatedRestPose(std::vector<Matrix>& bones)
{
    for (const auto bone : {AvatarBone::HipLeft,AvatarBone::HipRight})
        bones[static_cast<std::size_t>(bone)] = rotatedBone(
            bones[static_cast<std::size_t>(bone)],Matrix::CreateRotationX(1.55f));
    for (const auto bone : {AvatarBone::KneeLeft,AvatarBone::KneeRight})
        bones[static_cast<std::size_t>(bone)] = rotatedBone(
            bones[static_cast<std::size_t>(bone)],Matrix::CreateRotationX(-1.55f));
    for (bool left : {true,false})
    {
        const auto shoulder = static_cast<std::size_t>(left ? AvatarBone::ShoulderLeft : AvatarBone::ShoulderRight);
        const auto elbow = static_cast<std::size_t>(left ? AvatarBone::ElbowLeft : AvatarBone::ElbowRight);
        bones[shoulder] = rotatedBone(bones[shoulder],
            Matrix::CreateRotationZ(left ? 0.22f : -0.22f)*Matrix::CreateRotationX(0.15f));
        bones[elbow] = rotatedBone(bones[elbow],Matrix::CreateRotationX(0.85f));
    }
}
} // namespace

void SceneRenderer::addNextAvatarVisual()
{
    const std::size_t index = avatars_.size();
    auto visual = std::make_unique<AvatarVisual>();
    const AvatarBodyType body = index % 2 == 0 ? AvatarBodyType::Female : AvatarBodyType::Male;
    AvatarDescription description = AvatarDescription::CreateRandom(body);
    if (index == 0)
    {
        // Slot zero is the conductor's reserved visual. A native CNA hat selects the matching
        // hair-under-headwear mesh, avoiding the intersections an external rigid cap would cause.
        // Select, rather than alter, a generated description whose stock clothing palette also
        // supplies a consistent service blue for the top and accessory/headwear tint.
        AvatarDescription best = description;
        int bestScore = staffAppearanceScore(best);
        for (int attempt = 0; attempt < 16'383 && bestScore < 6; ++attempt)
        {
            AvatarDescription candidate = AvatarDescription::CreateRandom(body);
            const int score = staffAppearanceScore(candidate);
            if (score <= bestScore) continue;
            best = std::move(candidate);
            bestScore = score;
        }
        description = std::move(best);
    }
    else
    {
        // Random avatars are valuable for population variety, but unrestricted headwear can
        // produce construction-like helmets or novelty silhouettes that dominate a quiet train
        // scene. Select a civilian-looking result with ordinary hair and restrained lower-body
        // colours. This remains a choice among untouched CNA descriptions, not a dependency patch.
        AvatarDescription best = description;
        int bestScore = civilianAppearanceScore(best);
        // A bounded search prefers seat-compatible trousers over catalog-v3 skirts whose hem
        // stretches under the custom seated pose. Descriptions remain untouched; unknown
        // catalogs are not filtered. This is best-effort selection, not a cloth deformation fix.
        for (int attempt = 0; attempt < 127 && bestScore < 29; ++attempt)
        {
            AvatarDescription candidate = AvatarDescription::CreateRandom(body);
            const int score = civilianAppearanceScore(candidate);
            if (score <= bestScore) continue;
            best = std::move(candidate);
            bestScore = score;
        }
        description = std::move(best);
        ++civilianAvatarDescriptionCount_;
    }
    visual->description = std::make_unique<AvatarDescription>(std::move(description));
    visual->renderer = std::make_unique<AvatarRenderer>(visual->description.get(), false);
    visual->renderer->setAmbientLightColorProperty(Vector3{0.50f,0.50f,0.54f});
    visual->renderer->setLightColorProperty(Vector3{0.70f,0.67f,0.60f});
    visual->renderer->setLightDirectionProperty(Vector3{-0.35f,-0.65f,-0.55f});
    const auto parentBones = visual->renderer->getParentBonesProperty();
    for (std::size_t bone = 0; bone < visual->parentBones.size(); ++bone)
        visual->parentBones[bone] = parentBones[static_cast<int>(bone)];
    visual->animation = std::make_unique<AvatarAnimation>(avatarPreset(index));
    avatars_.push_back(std::move(visual));
}

void SceneRenderer::drawAvatars(const Matrix& view, const Matrix& projection,
                                const Sim::PassengerSystem& passengers, double seconds,
                                float renderDelta,
                                float playerX, float playerZ,
    RenderStats& stats)
{
    if (avatars_.empty()) return;
    const auto tick = System::TimeSpan::FromTicks(166667);
    for (auto& avatar : avatars_) avatar->animation->Update(tick, true);

    if (avatarBuildCooldownFrames_ > 0) --avatarBuildCooldownFrames_;
    AvatarVisual& newest = *avatars_.back();
    const bool poolNeedsAnother = avatars_.size() < totalAvatarBudget(graphicsQuality_);
    if (!newest.gpuPrimed
        && newest.renderer->getStateProperty() == AvatarRendererState::Ready)
    {
        // CNA builds avatar models asynchronously and performs their first texture upload in
        // Draw(). Prime every completed renderer off-camera, including the final pool member.
        // Starting another background build is deferred across a short bounded quiescence window,
        // so this upload crosses several presentation boundaries before loader work resumes.
        newest.renderer->setWorldProperty(Matrix::CreateTranslation(0.0f,-1'000.0f,0.0f));
        newest.renderer->setViewProperty(view);
        newest.renderer->setProjectionProperty(projection);
        newest.renderer->Draw(newest.animation.get());
        newest.gpuPrimed = true;
        avatarBuildCooldownFrames_ = kAvatarUploadQuiescenceFrames;
    }
    else if (poolNeedsAnother && newest.gpuPrimed && avatarBuildCooldownFrames_ == 0)
    {
        addNextAvatarVisual();
    }
    auto& readyVisuals = readyAvatarScratch_;
    readyVisuals.clear();
    const std::size_t onboardBudget = onboardAvatarBudget(graphicsQuality_);
    for (std::size_t index = 0;
         index < std::min(avatars_.size(), onboardBudget); ++index)
        if (avatars_[index]->renderer->getStateProperty() == AvatarRendererState::Ready)
            readyVisuals.push_back(index);
    if (readyVisuals.empty()) return;

    auto& visible = visiblePassengerScratch_;
    visible.clear();
    for (const Sim::Passenger& passenger : passengers.passengers())
    {
        if (passenger.state == Sim::PassengerState::Offboard) continue;
        if (std::abs(static_cast<float>(passenger.z) - playerZ) > 28.0f) continue;
        // Keep ordinary conversational distance visible. Player/NPC collision already holds
        // approaching actors just over a metre away; only suppress the pathological case where
        // the near plane would enter an avatar because an AI state changed on top of the camera.
        if (std::hypot(static_cast<float>(passenger.x) - playerX,
                       static_cast<float>(passenger.z) - playerZ) < 0.42f) continue;
        visible.push_back(&passenger);
    }

    // Give stationary aisle and vestibule passengers a concrete social focus instead of leaving
    // them as isolated forward-facing figures. Pair each one with the nearest unclaimed seated
    // passenger in the same car. Keeping this presentation relationship deterministic and local
    // avoids changing the navigation/collision simulation while still producing reciprocal gaze.
    std::vector<std::pair<const Sim::Passenger*,const Sim::Passenger*>> onboardSocialPairs;
    for (const Sim::Passenger& standing : passengers.passengers())
    {
        if (standing.role != Sim::PassengerRole::Passenger || standing.seat.has_value()
            || standing.exitingSeat.has_value()
            || (standing.state != Sim::PassengerState::Standing
                && (standing.state != Sim::PassengerState::WaitingAtVestibule
                    || std::abs(standing.targetZ-standing.z) >= 0.12)))
            continue;
        const Sim::Passenger* nearest = nullptr;
        double nearestDistanceSquared = 2.65*2.65;
        for (const Sim::Passenger& seatedCandidate : passengers.passengers())
        {
            if (seatedCandidate.role != Sim::PassengerRole::Passenger
                || seatedCandidate.state != Sim::PassengerState::Seated
                || !seatedCandidate.seat.has_value() || seatedCandidate.car != standing.car)
                continue;
            if (std::any_of(onboardSocialPairs.begin(),onboardSocialPairs.end(),
                    [&seatedCandidate](const auto& pair) {
                        return pair.second == &seatedCandidate;
                    }))
                continue;
            const double deltaX = seatedCandidate.x-standing.x;
            const double deltaZ = seatedCandidate.z-standing.z;
            const double distanceSquared = deltaX*deltaX+deltaZ*deltaZ;
            if (distanceSquared < 0.65*0.65 || distanceSquared >= nearestDistanceSquared)
                continue;
            nearest = &seatedCandidate;
            nearestDistanceSquared = distanceSquared;
        }
        if (nearest != nullptr)
            onboardSocialPairs.emplace_back(&standing,nearest);
    }
    const auto hasPersistentAssignment = [this](const Sim::Passenger& passenger) {
        return std::any_of(onboardAvatarAssignments_.begin(),onboardAvatarAssignments_.end(),
            [&passenger](const std::optional<AvatarAssignment>& assignment) {
                return assignment.has_value()
                    && assignment->passengerId == passenger.id
                    && assignment->appearanceSeed == passenger.appearanceSeed;
            });
    };
    std::stable_sort(visible.begin(), visible.end(),
        [playerX,playerZ,&hasPersistentAssignment](const auto* a, const auto* b) {
        const auto rolePriority = [](Sim::PassengerRole role) {
            if (role == Sim::PassengerRole::Conductor) return 0;
            if (role == Sim::PassengerRole::Driver) return 1;
            return 2;
        };
        const int priorityA = rolePriority(a->role);
        const int priorityB = rolePriority(b->role);
        if (priorityA != priorityB) return priorityA < priorityB;
        // A modest hysteresis bonus preserves a face and outfit while two nearby passengers trade
        // places around the pool boundary. A substantially closer newcomer still wins, so an
        // assigned person at the edge of the 28 m visibility window cannot starve the foreground.
        const float distanceA = std::hypot(static_cast<float>(a->x) - playerX,
                                           static_cast<float>(a->z) - playerZ)
                              - (hasPersistentAssignment(*a) ? 4.0f : 0.0f);
        const float distanceB = std::hypot(static_cast<float>(b->x) - playerX,
                                           static_cast<float>(b->z) - playerZ)
                              - (hasPersistentAssignment(*b) ? 4.0f : 0.0f);
        return distanceA < distanceB;
    });

    // Soft blob shadows are intentionally local to the train. They ground CNA avatars against
    // the carriage floor without a shadow-map pass or renderer-specific assumptions.
    if (graphicsQuality_ >= 1)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        const std::size_t shadowCount = std::min(visible.size(), readyVisuals.size());
        for (std::size_t index = 0; index < shadowCount; ++index)
        {
            const Sim::Passenger& passenger = *visible[index];
            const bool seated = passenger.state == Sim::PassengerState::Seated
                             || passenger.state == Sim::PassengerState::LookingOutside
                             || passenger.state == Sim::PassengerState::Talking;
            const Matrix shadowWorld = Matrix::CreateScale(seated ? 0.34f : 0.31f, 1.0f,
                                                            seated ? 0.29f : 0.22f)
                                     * Matrix::CreateTranslation(static_cast<float>(passenger.x),
                                                                 kTrainFloorElevation+0.086f,
                                                                 static_cast<float>(passenger.z));
            drawMesh(*contactShadow_, shadowWorld, view, projection, stats);
        }
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }

    const std::size_t drawableCount = std::min(visible.size(), readyVisuals.size());
    const Matrix avatarViewProjection = view*projection;
    const auto assignmentIsActive = [&visible,drawableCount](const AvatarAssignment& assignment) {
        return std::any_of(visible.begin(), visible.begin() + static_cast<std::ptrdiff_t>(drawableCount),
            [&assignment](const Sim::Passenger* passenger) {
                return passenger->id == assignment.passengerId
                    && passenger->appearanceSeed == assignment.appearanceSeed;
            });
    };
    for (std::size_t slot = 0; slot < onboardAvatarAssignments_.size(); ++slot)
        if (slot >= readyVisuals.size()
            || (onboardAvatarAssignments_[slot].has_value()
                && !assignmentIsActive(*onboardAvatarAssignments_[slot])))
            onboardAvatarAssignments_[slot].reset();

    std::size_t drawn = 0;
    std::size_t submitted = 0;
    for (const Sim::Passenger* passengerPointer : visible)
    {
        if (drawn >= drawableCount) break;
        const Sim::Passenger& passenger = *passengerPointer;
        const bool seated = passenger.state == Sim::PassengerState::Seated
                         || passenger.state == Sim::PassengerState::LookingOutside
                         || passenger.state == Sim::PassengerState::Talking;
        std::optional<std::size_t> visualSlot;
        for (std::size_t slot = 0; slot < readyVisuals.size(); ++slot)
            if (onboardAvatarAssignments_[slot].has_value()
                && (passenger.role == Sim::PassengerRole::Conductor ? slot == 0 : slot != 0)
                && onboardAvatarAssignments_[slot]->passengerId == passenger.id
                && onboardAvatarAssignments_[slot]->appearanceSeed == passenger.appearanceSeed)
            {
                visualSlot = slot;
                break;
            }
        if (!visualSlot.has_value())
        {
            const std::size_t first = passenger.role == Sim::PassengerRole::Conductor ? 0U : 1U;
            const std::size_t count = passenger.role == Sim::PassengerRole::Conductor
                ? std::min<std::size_t>(1U,readyVisuals.size())
                : readyVisuals.size() - std::min(first,readyVisuals.size());
            const std::size_t preferred = count == 0 ? 0U
                : first + passenger.appearanceSeed % count;
            for (std::size_t offset = 0; offset < count; ++offset)
            {
                const std::size_t candidate = first + (preferred-first+offset) % count;
                if (onboardAvatarAssignments_[candidate].has_value()) continue;
                onboardAvatarAssignments_[candidate] = AvatarAssignment{
                    passenger.id, passenger.appearanceSeed};
                AvatarVisual& assignedVisual = *avatars_[readyVisuals[candidate]];
                assignedVisual.seatedPoseBlend = seated ? 1.0f : 0.0f;
                assignedVisual.walkingPoseBlend = 0.0f;
                assignedVisual.gazeYaw = 0.0f;
                assignedVisual.gazePitch = 0.0f;
                assignedVisual.gaitPhaseRadians = seededAvatarGaitPhase(
                    passenger.appearanceSeed);
                assignedVisual.motionPositionInitialized = false;
                assignedVisual.facingInitialized = false;
                visualSlot = candidate;
                break;
            }
        }
        if (!visualSlot.has_value()) continue;
        AvatarVisual& visual = *avatars_[readyVisuals[*visualSlot]];
        const float seatedTarget = seated ? 1.0f : 0.0f;
        const float seatedStep = renderDelta / 0.55f;
        visual.seatedPoseBlend += std::clamp(
            seatedTarget-visual.seatedPoseBlend,-seatedStep,seatedStep);
        const float poseBlend = visual.seatedPoseBlend * visual.seatedPoseBlend
                              * (3.0f-2.0f*visual.seatedPoseBlend);
        const Sim::Seat* exitingSeat = nullptr;
        if (passenger.exitingSeat.has_value()
            && *passenger.exitingSeat < passengers.seats().size())
            exitingSeat = &passengers.seats()[*passenger.exitingSeat];
        const AvatarSeatExitPlacement seatExitPlacement = stabilizeAvatarSeatExit(
            passenger.x,passenger.z,
            exitingSeat != nullptr ? exitingSeat->x : passenger.x,
            exitingSeat != nullptr ? exitingSeat->z : passenger.z,
            poseBlend,exitingSeat != nullptr);
        const float seatTransferWeight = avatarSeatTransferWeight(
            poseBlend,seated || exitingSeat != nullptr);
        const float avatarX = static_cast<float>(seatExitPlacement.x);
        const int poseRow = Sim::TrainInterior::seatRowIndex(passenger.car,
            exitingSeat != nullptr ? exitingSeat->z : passenger.z);
        // Clear the 0.35 m cushion front with the knee before the shin turns down. Keep this
        // visual fit separate from the navigation/seat-occupancy coordinates.
        const float seatForward = poseRow%2 == 0 ? -1.0f : 1.0f;
        const float avatarZ = static_cast<float>(seatExitPlacement.z)+seatForward*0.10f*poseBlend;
        const bool submitAvatar = sphereMayBeInView(avatarViewProjection,
            Vector3{avatarX,kTrainFloorElevation+1.0f,avatarZ},1.5f);
        if (seatTransferWeight > 0.001f)
            ++stats.seatTransitionAvatars;
        const bool walking = passenger.state == Sim::PassengerState::Walking
                          || passenger.state == Sim::PassengerState::WalkingToSeat
                          || passenger.state == Sim::PassengerState::PreparingToLeave
                          || passenger.state == Sim::PassengerState::VisitingToilet
                          || passenger.state == Sim::PassengerState::LeavingToilet
                          || passenger.state == Sim::PassengerState::Boarding
                          || passenger.state == Sim::PassengerState::Alighting;
        const AvatarGaitStep gait = advanceAvatarGait(
            visual.gaitPhaseRadians,visual.previousPassengerX,visual.previousPassengerZ,
            passenger.x,passenger.z,visual.motionPositionInitialized,walking);
        visual.gaitPhaseRadians = gait.phaseRadians;
        visual.previousPassengerX = passenger.x;
        visual.previousPassengerZ = passenger.z;
        visual.motionPositionInitialized = true;
        // Drive the leg layer from actual displacement. A navigator waiting for another person
        // now settles its feet instead of walking on the spot, and render-only screenshot warm-up
        // cannot advance a frozen passenger's stride.
        const float walkingTarget = gait.moving ? 1.0f : 0.0f;
        const float walkingStep = renderDelta / (gait.moving ? 0.18f : 0.24f);
        visual.walkingPoseBlend += std::clamp(
            walkingTarget-visual.walkingPoseBlend,-walkingStep,walkingStep);
        const float locomotionBlend = visual.walkingPoseBlend*(1.0f-poseBlend);
        if (gait.moving && locomotionBlend > 0.001f)
            ++stats.distanceDrivenGaitAvatars;
        const float gaitSwing = std::sin(visual.gaitPhaseRadians);
        const float standingY = 0.06f
                              + 0.022f*std::abs(gaitSwing)*locomotionBlend;
        const float y = kTrainFloorElevation + standingY + (0.03f-standingY)*poseBlend;
        const float scale = 0.96f + (0.88f-0.96f)*poseBlend;
        const auto socialPair = std::find_if(
            onboardSocialPairs.begin(),onboardSocialPairs.end(),
            [&passenger](const auto& pair) {
                return pair.first == &passenger || pair.second == &passenger;
            });
        const Sim::Passenger* onboardSocialPartner = socialPair == onboardSocialPairs.end()
            ? nullptr : socialPair->first == &passenger ? socialPair->second : socialPair->first;
        const bool onboardSocial = onboardSocialPartner != nullptr && !walking && !gait.moving;
        const bool standingSocialSource = onboardSocial
            && socialPair->first == &passenger;
        float yaw = avatarFacingDirection(0.0,passenger.targetZ-passenger.z);
        if (passenger.state == Sim::PassengerState::Boarding
            || passenger.state == Sim::PassengerState::Alighting)
            yaw = passenger.targetX >= passenger.x ? -1.5707963f : 1.5707963f;
        if (passenger.state == Sim::PassengerState::WalkingToSeat
            && std::abs(passenger.targetZ - passenger.z) < 0.09)
            yaw = passenger.targetX >= passenger.x ? -1.5707963f : 1.5707963f;
        if (exitingSeat != nullptr)
            yaw = exitingSeat->x < 0.0 ? -1.5707963f : 1.5707963f;
        if (passenger.role == Sim::PassengerRole::Driver)
            yaw = avatarFacingDirection(0.0,passenger.z >= 0.0 ? 1.0 : -1.0);
        if (passenger.seat.has_value())
        {
            const int row = Sim::TrainInterior::seatRowIndex(passenger.car, passenger.z);
            // Looking outside must not rotate the seated pelvis and knees into the armrest.
            // Keep the seat orientation authoritative and express the glance in upper bones.
            yaw = avatarSeatFacing(row);
        }
        if (standingSocialSource)
            yaw = avatarFacingDirection(
                static_cast<float>(onboardSocialPartner->x-passenger.x),
                static_cast<float>(onboardSocialPartner->z-passenger.z));
        if (!onboardSocial && !seated && passenger.role != Sim::PassengerRole::Driver
            && std::abs(passenger.x) < 0.50)
        {
            const bool passingShoulderToShoulder = std::any_of(
                passengers.passengers().begin(),passengers.passengers().end(),
                [&passenger](const Sim::Passenger& other) {
                    if (&other == &passenger || other.role == Sim::PassengerRole::Driver
                        || other.seat.has_value() || std::abs(other.x) >= 0.50
                        || passenger.x*other.x >= 0.0) return false;
                    return std::abs(passenger.z-other.z) < 0.72
                        && std::hypot(passenger.x-other.x,passenger.z-other.z) < 0.92;
                });
            if (passingShoulderToShoulder)
                yaw += passenger.x < 0.0 ? -0.42f : 0.42f;
        }
        if (!visual.facingInitialized)
        {
            visual.facingYaw = yaw;
            visual.facingInitialized = true;
        }
        else
        {
            const float yawDelta = std::atan2(
                std::sin(yaw-visual.facingYaw),std::cos(yaw-visual.facingYaw));
            const float maximumTurn = renderDelta*(walking ? 4.8f : 3.2f);
            visual.facingYaw += std::clamp(yawDelta,-maximumTurn,maximumTurn);
        }
        yaw = visual.facingYaw;
        float gazeTargetYaw = 0.0f;
        float gazeTargetPitch = 0.0f;
        bool gazeActive = seated || (!walking && passenger.role != Sim::PassengerRole::Driver);
        const bool readingIdle = seated
            && passenger.state == Sim::PassengerState::Seated
            && !onboardSocial
            && passenger.appearanceSeed%4U == 0U;
        const bool phoneIdle = !walking
            && passenger.role == Sim::PassengerRole::Passenger
            && (passenger.state == Sim::PassengerState::Seated
                || passenger.state == Sim::PassengerState::Standing
                || passenger.state == Sim::PassengerState::WaitingAtVestibule)
            && !readingIdle && !onboardSocial && passenger.appearanceSeed%5U == 1U;
        const float playerDistance = std::hypot(
            avatarX-playerX,avatarZ-playerZ);
        const bool playerAcknowledgement = gazeActive
            && playerDistance > 0.70f && playerDistance < 2.55f
            && (passenger.role == Sim::PassengerRole::Conductor
                || passenger.appearanceSeed%3U == 0U)
            && !onboardSocial
            && passenger.state != Sim::PassengerState::Talking
            && passenger.state != Sim::PassengerState::LookingOutside;
        const bool conductorUsingDevice = passenger.role == Sim::PassengerRole::Conductor
            && !walking && !seated;
        if (passenger.state == Sim::PassengerState::Talking)
        {
            const auto partner = std::find_if(
                passengers.passengers().begin(),passengers.passengers().end(),
                [&passenger](const Sim::Passenger& candidate) {
                    return candidate.id != passenger.id
                        && candidate.state == Sim::PassengerState::Talking
                        && candidate.car == passenger.car
                        && candidate.seat.has_value()
                        && std::abs(candidate.z-passenger.z) < 0.05
                        && candidate.x*passenger.x > 0.0
                        && std::abs(candidate.x-passenger.x) < 0.75;
                });
            if (partner != passengers.passengers().end())
            {
                const float absolutePartnerYaw = avatarFacingDirection(
                    static_cast<float>(partner->x-passenger.x),
                    static_cast<float>(partner->z-passenger.z));
                const float relativePartnerYaw = std::atan2(
                    std::sin(absolutePartnerYaw-yaw),std::cos(absolutePartnerYaw-yaw));
                gazeTargetYaw = std::clamp(relativePartnerYaw,-0.52f,0.52f);
            }
        }
        else if (onboardSocial)
        {
            const float absolutePartnerYaw = avatarFacingDirection(
                static_cast<float>(onboardSocialPartner->x-passenger.x),
                static_cast<float>(onboardSocialPartner->z-passenger.z));
            const float relativePartnerYaw = std::atan2(
                std::sin(absolutePartnerYaw-yaw),std::cos(absolutePartnerYaw-yaw));
            gazeTargetYaw = std::clamp(relativePartnerYaw,-0.52f,0.52f);
            gazeTargetPitch = standingSocialSource ? 0.075f : -0.065f;
        }
        else if (passenger.state == Sim::PassengerState::LookingOutside)
        {
            gazeTargetYaw = avatarWindowGaze(passenger.x,yaw)+0.045f*std::sin(
                static_cast<float>(seconds)*0.31f
                + static_cast<float>(passenger.appearanceSeed%173U)*0.047f);
            gazeTargetPitch = -0.035f+0.025f*std::sin(
                static_cast<float>(seconds)*0.19f
                + static_cast<float>(passenger.appearanceSeed%89U)*0.061f);
        }
        else if (readingIdle)
        {
            gazeTargetPitch = 0.16f;
            gazeTargetYaw = 0.035f*std::sin(
                static_cast<float>(seconds)*0.37f
                + static_cast<float>(passenger.appearanceSeed%67U));
        }
        else if (phoneIdle)
        {
            gazeTargetPitch = 0.18f;
            gazeTargetYaw = 0.025f*std::sin(
                static_cast<float>(seconds)*0.29f
                + static_cast<float>(passenger.appearanceSeed%61U));
        }
        else if (playerAcknowledgement)
        {
            const float absolutePlayerYaw = avatarFacingDirection(
                playerX-avatarX,playerZ-avatarZ);
            const float relativePlayerYaw = std::atan2(
                std::sin(absolutePlayerYaw-yaw),std::cos(absolutePlayerYaw-yaw));
            gazeTargetYaw = std::clamp(relativePlayerYaw,-0.48f,0.48f);
            gazeTargetPitch = -0.025f;
        }
        else if (gazeActive)
        {
            gazeTargetYaw = 0.16f*std::sin(
                static_cast<float>(seconds)*0.23f
                + static_cast<float>(passenger.appearanceSeed%151U)*0.052f);
            gazeTargetPitch = 0.025f*std::sin(
                static_cast<float>(seconds)*0.17f
                + static_cast<float>(passenger.appearanceSeed%107U)*0.043f);
        }
        if (conductorUsingDevice && !playerAcknowledgement)
            gazeTargetPitch = 0.14f;
        const float gazeStep = renderDelta*0.85f;
        visual.gazeYaw += std::clamp(
            gazeTargetYaw-visual.gazeYaw,-gazeStep,gazeStep);
        visual.gazePitch += std::clamp(
            gazeTargetPitch-visual.gazePitch,-gazeStep,gazeStep);
        const bool gazePoseActive = std::abs(visual.gazeYaw) > 0.002f
                                 || std::abs(visual.gazePitch) > 0.002f;
        if (gazeActive) ++stats.animatedGazeAvatars;
        if (readingIdle) ++stats.readingIdleAvatars;
        if (phoneIdle) ++stats.phoneCheckingAvatars;
        if (onboardSocial || passenger.state == Sim::PassengerState::Talking)
            ++stats.onboardSocialAvatars;
        if (playerAcknowledgement) ++stats.playerAcknowledgingAvatars;
        const Matrix world = Matrix::CreateScale(scale)
                           * Matrix::CreateRotationY(yaw)
                           * Matrix::CreateTranslation(avatarX,y,avatarZ);
        const auto cabinLight = avatarCabinLighting(
            effect_->DirectionalLight0.getDirectionProperty(),tunnelLightingMix_);
        visual.renderer->setAmbientLightColorProperty(Vector3{0.56f,0.58f,0.62f});
        visual.renderer->setLightDirectionProperty(cabinLight.direction);
        visual.renderer->setLightColorProperty(cabinLight.diffuse);
        visual.renderer->setWorldProperty(world);
        visual.renderer->setViewProperty(view);
        visual.renderer->setProjectionProperty(projection);
        if (poseBlend > 0.001f || locomotionBlend > 0.001f || gazePoseActive
            || onboardSocial)
        {
            // CNA's stock set has expressive standing idles but no seated or walking clip.
            // Preserve its animated upper body and layer only the major local joints we need.
            const auto source = visual.animation->getBoneTransformsProperty();
            auto& bones = visual.boneScratch;
            bones.assign(source.begin(), source.end());
            const auto hipLeft = static_cast<std::size_t>(AvatarBone::HipLeft);
            const auto hipRight = static_cast<std::size_t>(AvatarBone::HipRight);
            const auto kneeLeft = static_cast<std::size_t>(AvatarBone::KneeLeft);
            const auto kneeRight = static_cast<std::size_t>(AvatarBone::KneeRight);
            const auto ankleLeft = static_cast<std::size_t>(AvatarBone::AnkleLeft);
            const auto ankleRight = static_cast<std::size_t>(AvatarBone::AnkleRight);
            const auto toeLeft = static_cast<std::size_t>(AvatarBone::ToeLeft);
            const auto toeRight = static_cast<std::size_t>(AvatarBone::ToeRight);
            const auto backLower = static_cast<std::size_t>(AvatarBone::BackLower);
            bones[backLower] = rotatedBone(
                bones[backLower],Matrix::CreateRotationX(
                    -0.12f*seatTransferWeight));
            bones[hipLeft] = rotatedBone(
                bones[hipLeft], Matrix::CreateRotationX(
                    1.55f*poseBlend + gaitSwing*0.48f*locomotionBlend));
            bones[hipRight] = rotatedBone(
                bones[hipRight], Matrix::CreateRotationX(
                    1.55f*poseBlend - gaitSwing*0.48f*locomotionBlend));
            bones[kneeLeft] = rotatedBone(
                bones[kneeLeft], Matrix::CreateRotationX(
                    -1.55f*poseBlend
                    + std::max(0.0f,-gaitSwing)*0.55f*locomotionBlend));
            bones[kneeRight] = rotatedBone(
                bones[kneeRight], Matrix::CreateRotationX(
                    -1.55f*poseBlend
                    + std::max(0.0f,gaitSwing)*0.55f*locomotionBlend));
            bones[ankleLeft] = rotatedBone(
                bones[ankleLeft],Matrix::CreateRotationX(
                    gaitSwing*0.18f*locomotionBlend));
            bones[ankleRight] = rotatedBone(
                bones[ankleRight],Matrix::CreateRotationX(
                    -gaitSwing*0.18f*locomotionBlend));
            bones[toeLeft] = rotatedBone(
                bones[toeLeft],Matrix::CreateRotationX(
                    std::max(0.0f,-gaitSwing)*0.11f*locomotionBlend));
            bones[toeRight] = rotatedBone(
                bones[toeRight],Matrix::CreateRotationX(
                    std::max(0.0f,gaitSwing)*0.11f*locomotionBlend));
            float conversationalHeadYaw = 0.0f;
            if (poseBlend > 0.001f)
            {
                // Stock Stand1 clasps hands behind the back; that pose intersects the seat.
                // Establish a lap-rest base before activity-specific gestures override it.
                // Keep native translations and blend during the existing sit/stand transition.
                for (bool left : {true,false})
                {
                    const auto shoulder = static_cast<std::size_t>(left
                        ? AvatarBone::ShoulderLeft : AvatarBone::ShoulderRight);
                    const auto elbow = static_cast<std::size_t>(left
                        ? AvatarBone::ElbowLeft : AvatarBone::ElbowRight);
                    const auto wrist = static_cast<std::size_t>(left
                        ? AvatarBone::WristLeft : AvatarBone::WristRight);
                    const Matrix shoulderRest = rotatedBone(bones[shoulder],
                        Matrix::CreateRotationZ(left ? 0.22f : -0.22f)
                        * Matrix::CreateRotationX(0.15f));
                    const Matrix elbowRest = rotatedBone(bones[elbow],Matrix::CreateRotationX(0.85f));
                    const Matrix wristRest = rotatedBone(bones[wrist],Matrix::getIdentityProperty());
                    bones[shoulder] = blendBone(bones[shoulder],shoulderRest,poseBlend);
                    bones[elbow] = blendBone(bones[elbow],elbowRest,poseBlend);
                    bones[wrist] = blendBone(bones[wrist],wristRest,poseBlend);
                }
            }
            if (seated)
            {
                if (passenger.state == Sim::PassengerState::Talking)
                {
                    const float gesture = std::sin(static_cast<float>(seconds) * 2.15f
                        + static_cast<float>(passenger.appearanceSeed % 131U) * 0.071f);
                    conversationalHeadYaw = gesture*0.10f*poseBlend;
                    const bool useLeftArm = passenger.appearanceSeed % 2U == 0U;
                    const auto shoulder = static_cast<std::size_t>(useLeftArm
                        ? AvatarBone::ShoulderLeft : AvatarBone::ShoulderRight);
                    const auto elbow = static_cast<std::size_t>(useLeftArm
                        ? AvatarBone::ElbowLeft : AvatarBone::ElbowRight);
                    bones[shoulder] = rotatedBone(
                        bones[shoulder], Matrix::CreateRotationX(
                            (-0.16f + gesture * 0.11f)*poseBlend));
                    bones[elbow] = rotatedBone(
                        bones[elbow], Matrix::CreateRotationZ(
                            (useLeftArm ? -1.0f : 1.0f)
                            * (0.24f + std::abs(gesture) * 0.12f)*poseBlend));
                }
                else if (readingIdle)
                {
                    const auto shoulderLeft = static_cast<std::size_t>(
                        AvatarBone::ShoulderLeft);
                    const auto shoulderRight = static_cast<std::size_t>(
                        AvatarBone::ShoulderRight);
                    const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
                    const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
                    bones[shoulderLeft] = rotatedBone(
                        bones[shoulderLeft],Matrix::CreateRotationX(-0.24f*poseBlend));
                    bones[shoulderRight] = rotatedBone(
                        bones[shoulderRight],Matrix::CreateRotationX(-0.24f*poseBlend));
                    bones[elbowLeft] = rotatedBone(
                        bones[elbowLeft],Matrix::CreateRotationZ(-0.48f*poseBlend));
                    bones[elbowRight] = rotatedBone(
                        bones[elbowRight],Matrix::CreateRotationZ(0.48f*poseBlend));
                }
            }
            if (standingSocialSource)
            {
                const float gesture = std::sin(static_cast<float>(seconds)*1.73f
                    + static_cast<float>(passenger.appearanceSeed%127U)*0.083f);
                conversationalHeadYaw += gesture*0.045f;
                const bool useLeftArm = passenger.appearanceSeed%2U == 0U;
                const auto shoulder = static_cast<std::size_t>(useLeftArm
                    ? AvatarBone::ShoulderLeft : AvatarBone::ShoulderRight);
                const auto elbow = static_cast<std::size_t>(useLeftArm
                    ? AvatarBone::ElbowLeft : AvatarBone::ElbowRight);
                bones[shoulder] = rotatedBone(
                    bones[shoulder],Matrix::CreateRotationX(-0.10f+gesture*0.055f));
                bones[elbow] = rotatedBone(
                    bones[elbow],Matrix::CreateRotationZ(
                        (useLeftArm ? -1.0f : 1.0f)
                        * (0.16f+std::abs(gesture)*0.10f)));
            }
            if (conductorUsingDevice)
            {
                const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
                const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
                const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
                const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft],Matrix::CreateRotationX(-0.42f));
                bones[shoulderRight] = rotatedBone(
                    bones[shoulderRight],Matrix::CreateRotationX(-0.42f));
                bones[elbowLeft] = rotatedBone(
                    bones[elbowLeft],Matrix::CreateRotationZ(-0.68f));
                bones[elbowRight] = rotatedBone(
                    bones[elbowRight],Matrix::CreateRotationZ(0.68f));
            }
            if (phoneIdle)
            {
                const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
                const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
                const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
                const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft],Matrix::CreateRotationX(-0.31f));
                bones[shoulderRight] = rotatedBone(
                    bones[shoulderRight],Matrix::CreateRotationX(-0.47f));
                bones[elbowLeft] = rotatedBone(
                    bones[elbowLeft],Matrix::CreateRotationZ(-0.46f));
                bones[elbowRight] = rotatedBone(
                    bones[elbowRight],Matrix::CreateRotationZ(0.72f));
            }
            if (gazePoseActive || std::abs(conversationalHeadYaw) > 0.001f)
            {
                const auto backUpper = static_cast<std::size_t>(AvatarBone::BackUpper);
                bones[backUpper] = rotatedBone(
                    bones[backUpper],
                    Matrix::CreateRotationY(visual.gazeYaw*0.18f)
                    * Matrix::CreateRotationX(visual.gazePitch*0.08f));
                const auto head = static_cast<std::size_t>(AvatarBone::Head);
                bones[head] = rotatedBone(
                    bones[head],
                    Matrix::CreateRotationY(visual.gazeYaw+conversationalHeadYaw)
                    * Matrix::CreateRotationX(visual.gazePitch));
            }
            if (locomotionBlend > 0.001f)
            {
                const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
                const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft], Matrix::CreateRotationX(
                        -gaitSwing*0.32f*locomotionBlend));
                bones[shoulderRight] = rotatedBone(
                    bones[shoulderRight], Matrix::CreateRotationX(
                        gaitSwing*0.32f*locomotionBlend));
            }
            applyPassengerProportions(bones);
            if (submitAvatar)
                visual.renderer->Draw(bones, visual.animation->getExpressionProperty());
        }
        else
        {
            if (submitAvatar)
            {
                const auto source=visual.animation->getBoneTransformsProperty();
                auto& bones=visual.boneScratch;
                bones.assign(source.begin(),source.end());
                applyPassengerProportions(bones);
                visual.renderer->Draw(bones,visual.animation->getExpressionProperty());
            }
        }
        if (!submitAvatar)
        {
            ++drawn; // Preserve the existing nearby-assigned population diagnostic.
            ++stats.culledOnboardAvatars;
            continue;
        }
        ++submitted;
        if (phoneIdle)
        {
            const Vector3 localPhone{0.06f,1.12f,-0.28f};
            const Vector3 phoneOffset = Vector3::Transform(
                localPhone,Matrix::CreateRotationY(yaw));
            const Matrix phoneWorld = Matrix::CreateRotationX(-0.32f)
                * Matrix::CreateRotationY(yaw)
                * Matrix::CreateTranslation(
                    avatarX+phoneOffset.X,
                    y+localPhone.Y,
                    avatarZ+phoneOffset.Z);
            drawMesh(*passengerPhone_,phoneWorld,view,projection,stats);
        }
        if (passenger.role == Sim::PassengerRole::Conductor)
        {
            const Vector3 localDevice = conductorUsingDevice
                ? Vector3{0.0f,1.08f,-0.27f} : Vector3{0.25f,1.02f,0.02f};
            const Vector3 deviceOffset = Vector3::Transform(
                localDevice,Matrix::CreateRotationY(yaw));
            const Matrix deviceWorld = Matrix::CreateRotationX(
                    conductorUsingDevice ? -0.24f : 0.0f)
                * Matrix::CreateRotationY(yaw)
                * Matrix::CreateTranslation(
                    avatarX+deviceOffset.X,
                    y+localDevice.Y,
                    avatarZ+deviceOffset.Z);
            drawMesh(*conductorHandheld_,deviceWorld,view,projection,stats);
            ++stats.conductorHandheldDevices;
        }
        ++drawn;
    }
    stats.visiblePassengers = static_cast<int>(drawn);
    stats.drawCalls += static_cast<int>(submitted);
}

void SceneRenderer::drawPlatformPeople(const Matrix& view, const Matrix& projection,
                                       const Sim::Route& route,
                                       const Sim::ServiceState& service, RenderStats& stats)
{
    const std::size_t stationIndex = route.nearestStation(service.routeDistanceMetres);
    const Sim::Station& station = route.stations()[stationIndex];
    if (std::abs(station.distanceMetres - service.routeDistanceMetres) > 260.0) return;

    const double platformSide = station.platformOnLeft ? -1.0 : 1.0;
    // Uneven paired spacing reads as small travel parties rather than a procedural picket line.
    // Smaller stations select the middle subset and therefore keep their platform population
    // compact around the stopping point.
    const bool terminal = stationIndex == 0 || stationIndex+1 == route.stations().size();
    const auto offsets = platformWaitingOffsets(terminal,station.major);
    const std::size_t offsetStart = graphicsQuality_ == 0 ? 2 : graphicsQuality_ == 1 ? 1 : 0;
    const std::size_t count = graphicsQuality_ == 0 ? 2
                            : graphicsQuality_ == 1 ? (station.major ? 4 : 3)
                                                    : (station.major ? offsets.size() : 4);
    const std::size_t platformStart = onboardAvatarBudget(graphicsQuality_);
    if (avatars_.size() < platformStart + count) return;
    for (std::size_t index = platformStart; index < platformStart + count; ++index)
        if (avatars_[index]->renderer->getStateProperty() != AvatarRendererState::Ready) return;

    const Vector3 canopyRays=sunRayDirection(route.sample(station.distanceMetres).headingRadians,
                                             shadowCastingPosition(atmosphere_.sun));
    const float canopyLength=platformCanopyLength(terminal,station.major);
    const auto canopyShadow=canopyShadowBounds(canopyLength,kPlatformSurfaceElevation+1.05f,
                                               canopyRays);
    const auto luggageShadow=canopyShadowBounds(canopyLength,kPlatformSurfaceElevation+0.45f,
                                                canopyRays);
    const auto platformLight=stationLighting(atmosphere_);
    const Vector3 platformKeyLight=sunRayDirection(
        route.sample(service.routeDistanceMetres).headingRadians,atmosphere_.keyLight);
    std::array<LocalRoutePoint, 6> points{};
    for (std::size_t index = 0; index < count; ++index)
        points[index] = localRoutePoint(route, service.routeDistanceMetres,
                                        station.distanceMetres + offsets[offsetStart + index],
                                        platformSide * platformWaitingLaterals[offsetStart+index]);

    if (graphicsQuality_ >= 1)
    {
        effect_->setLightingEnabledProperty(false);
        effect_->setDiffuseColorProperty(Vector3::One);
        device_.setBlendStateProperty(BlendState::NonPremultiplied);
        device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
        for (std::size_t index = 0; index < count; ++index)
        {
            const LocalRoutePoint& point = points[index];
            if (std::abs(point.z) > 145.0f) continue;
            const Matrix shadowWorld = Matrix::CreateScale(0.32f, 1.0f, 0.23f)
                                     * Matrix::CreateRotationY(point.yaw)
                                     * Matrix::CreateTranslation(
                                         point.x, point.y+kPlatformSurfaceElevation+0.006f,
                                         point.z);
            drawMesh(*contactShadow_, shadowWorld, view, projection, stats);
        }
        device_.setBlendStateProperty(BlendState::Opaque);
        device_.setDepthStencilStateProperty(DepthStencilState::Default);
        effect_->setLightingEnabledProperty(true);
    }

    int drawn = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const LocalRoutePoint& point = points[index];
        if (std::abs(point.z) > 145.0f) continue;
        AvatarVisual& visual = *avatars_[platformStart + index];
        const float faceTrack = platformSide < 0.0 ? -kPi * 0.5f : kPi * 0.5f;
        float localFacing = faceTrack;
        // Select the same physical waiting position at every quality tier. Low skips the first
        // two offsets, so basing this on the authored offset index keeps the phone user near the
        // train centre instead of moving them forty metres down the platform on High.
        const bool phoneUser = offsetStart+index == 3U;
        const bool socialPose = station.major
            && platformConversationSlot(offsetStart+index,offsetStart,count);
        if (phoneUser)
        {
            // A side-on stance exposes both the device and arm pose to passengers looking out;
            // the remaining figures continue to face the train or their conversation partner.
            localFacing = platformSide < 0.0 ? 0.24f : kPi-0.24f;
        }
        else if (socialPose)
        {
            // Face the actual partner, including their lateral stagger. Conversation spacing
            // is 1.6 m rather than the unrelated waiting figures' several-metre gaps.
            const std::size_t slot = offsetStart+index;
            const std::size_t partner = slot ^ 1U;
            localFacing = avatarFacingDirection(
                platformSide*(platformWaitingLaterals[partner]-platformWaitingLaterals[slot]),
                offsets[partner]-offsets[slot]);
        }
        else if (index != 0)
            localFacing += (index%2U == 0U ? -0.16f : 0.13f);
        const float avatarYaw = point.yaw + localFacing;
        const Matrix world = Matrix::CreateScale(0.94f)
                           * Matrix::CreateRotationY(avatarYaw)
                           * Matrix::CreateTranslation(
                               point.x, point.y+kPlatformSurfaceElevation+0.01f, point.z);
        // These visuals share the onboard pool: always restore exterior illumination before
        // drawing a platform figure so a previous cabin/tunnel draw cannot leak its settings.
        visual.renderer->setAmbientLightColorProperty(platformLight.ambient);
        visual.renderer->setLightDirectionProperty(platformKeyLight);
        const float shade=canopyShadeAt(canopyShadow,
            static_cast<float>(platformSide*(platformWaitingLaterals[offsetStart+index]-5.0)),
            static_cast<float>(offsets[offsetStart+index]));
        visual.renderer->setLightColorProperty(platformLight.diffuse*(1.0f-0.88f*shade));
        visual.renderer->setWorldProperty(world);
        visual.renderer->setViewProperty(view);
        visual.renderer->setProjectionProperty(projection);
        Vector3 phonePosition = Vector3::Zero;
        if (phoneUser || socialPose)
        {
            // Platform figures share the same CNA avatar pool as onboard passengers. Layer a
            // small amount of purposeful waiting behaviour over the stock idle so the crowd no
            // longer reads as identical mannequins: one traveller checks a phone, while major-
            // station pairs exchange alternating restrained hand/head gestures.
            const auto source = visual.animation->getBoneTransformsProperty();
            auto& bones = visual.boneScratch;
            bones.assign(source.begin(),source.end());
            const auto shoulderLeft = static_cast<std::size_t>(AvatarBone::ShoulderLeft);
            const auto shoulderRight = static_cast<std::size_t>(AvatarBone::ShoulderRight);
            const auto elbowLeft = static_cast<std::size_t>(AvatarBone::ElbowLeft);
            const auto elbowRight = static_cast<std::size_t>(AvatarBone::ElbowRight);
            const auto head = static_cast<std::size_t>(AvatarBone::Head);
            if (phoneUser)
            {
                // CNA's documented custom-bone sample establishes positive Z on the left
                // shoulder as the dependable raised-arm direction. Bend the forearm back toward
                // the face, then attach the handset to that exact wrist below.
                bones[shoulderLeft] = rotatedBone(
                    bones[shoulderLeft],Matrix::CreateRotationZ(1.02f));
                bones[elbowLeft] = rotatedBone(
                    bones[elbowLeft],Matrix::CreateRotationZ(-1.10f));
                bones[head] = rotatedBone(
                    bones[head],Matrix::CreateRotationZ(-0.055f));
                const auto wrist = static_cast<std::size_t>(AvatarBone::WristLeft);
                Matrix wristLocal = bones[wrist];
                for (int parent = visual.parentBones[wrist]; parent >= 0;
                     parent = visual.parentBones[static_cast<std::size_t>(parent)])
                    wristLocal = wristLocal*bones[static_cast<std::size_t>(parent)];
                phonePosition = Vector3::Transform(Vector3::Zero,wristLocal*world);
                phonePosition.Y += 0.055f;
                ++stats.platformPhoneUsers;
            }
            else
            {
                const float gesture = std::sin(
                    static_cast<float>(service.serviceElapsedSeconds)*1.63f
                    + static_cast<float>(index)*1.21f);
                const bool useLeftArm = index%2U == 0U;
                const auto shoulder = useLeftArm ? shoulderLeft : shoulderRight;
                const auto elbow = useLeftArm ? elbowLeft : elbowRight;
                bones[shoulder] = rotatedBone(
                    bones[shoulder],Matrix::CreateRotationX(-0.12f+gesture*0.09f));
                bones[elbow] = rotatedBone(
                    bones[elbow],Matrix::CreateRotationZ(
                        (useLeftArm ? -1.0f : 1.0f)*(0.20f+std::abs(gesture)*0.16f)));
                bones[head] = rotatedBone(
                    bones[head],Matrix::CreateRotationY(
                        (useLeftArm ? -1.0f : 1.0f)*(0.07f+gesture*0.035f)));
                ++stats.platformSocialAvatars;
            }
            applyPassengerProportions(bones);
            visual.renderer->Draw(bones,visual.animation->getExpressionProperty());
        }
        else
        {
            const auto source=visual.animation->getBoneTransformsProperty();
            auto& bones=visual.boneScratch;
            bones.assign(source.begin(),source.end());
            applyPassengerProportions(bones);
            visual.renderer->Draw(bones,visual.animation->getExpressionProperty());
        }
        ++drawn;

        // Match a passenger's handheld props to their canopy light, not the full-sun world
        // effect left by track rendering. Luggage samples its own lower receiver and location.
        effect_->setAmbientLightColorProperty(platformLight.ambient);
        effect_->DirectionalLight0.setDiffuseColorProperty(platformLight.diffuse*(1.0f-0.88f*shade));
        effect_->setDiffuseColorProperty(Vector3::One);
        if (phoneUser)
        {
            // Attach the prop to the posed wrist rather than relying on a guessed chest-space
            // offset. CNA bone transforms are parent-local, matching XNA's row-vector matrix
            // order, so walking the published parent chain produces the same joint position the
            // avatar renderer consumes. This remains correct across body-height variants.
            const Matrix phoneWorld = Matrix::CreateRotationZ(-0.055f)
                * Matrix::CreateRotationY(avatarYaw)
                * Matrix::CreateTranslation(phonePosition);
            drawMesh(*passengerPhone_,phoneWorld,view,projection,stats);
        }

        // The first person at every station is a clearly readable platform employee.
        if (index == 0)
            drawBox(orientedBoxMatrix(point.x, point.y + 1.93f, point.z,
                                      0.58f, 0.18f, 0.34f, point.yaw + localFacing),
                    view, projection, Color(226,190,42,255), stats);

        if (index % 2U == 1U)
        {
            const double luggageDistance = station.distanceMetres
                + offsets[offsetStart+index] + (index%4U == 1U ? 0.48 : -0.48);
            const LocalRoutePoint luggage = localRoutePoint(
                route,service.routeDistanceMetres,luggageDistance,platformSide*5.27);
            const Matrix luggageWorld = Matrix::CreateRotationY(luggage.yaw+kPi*0.5f)
                * Matrix::CreateTranslation(
                    luggage.x,luggage.y+kPlatformSurfaceElevation+0.01f,luggage.z);
            const float luggageShade=canopyShadeAt(luggageShadow,
                static_cast<float>(platformSide*0.27),
                static_cast<float>(luggageDistance-station.distanceMetres));
            effect_->DirectionalLight0.setDiffuseColorProperty(
                platformLight.diffuse*(1.0f-0.88f*luggageShade));
            drawMesh(*platformLuggage_,luggageWorld,view,projection,stats);
            ++stats.platformLuggageItems;
        }
    }
    // ENV-136: the rest of the platform pool sits on the benches, waits with a suitcase or reads
    // the timetable. Medium shows one seated figure and one traveller; Low none.
    const auto extras = platformExtraFigures(terminal,station.major);
    const std::size_t platformBudget = platformAvatarBudget(graphicsQuality_);
    const std::size_t extraCount = std::min<std::size_t>(
        graphicsQuality_ >= 2 ? extras.size() : graphicsQuality_ == 1 ? 2U : 0U,
        platformBudget > count ? platformBudget-count : 0U);
    for (std::size_t k = 0; k < extraCount; ++k)
    {
        const std::size_t avatarIndex = platformStart+count+k;
        if (avatarIndex >= avatars_.size()) break;
        AvatarVisual& visual = *avatars_[avatarIndex];
        if (visual.renderer->getStateProperty() != AvatarRendererState::Ready) continue;
        const PlatformExtraFigure& figure = extras[k];
        const LocalRoutePoint point = localRoutePoint(route,service.routeDistanceMetres,
            station.distanceMetres+figure.offset,platformSide*figure.lateral);
        if (std::abs(point.z) > 145.0f) continue;
        const bool seated = figure.pose == PlatformExtraPose::BenchSeated;
        const float faceTrack = platformSide < 0.0 ? -kPi*0.5f : kPi*0.5f;
        float localFacing = faceTrack+(k%2U == 0U ? 0.10f : -0.12f);
        if (figure.pose == PlatformExtraPose::ReadingTimetable) localFacing = faceTrack+kPi;
        else if (figure.pose == PlatformExtraPose::WithSuitcase)
            localFacing = faceTrack+(k%4U == 1U ? 0.55f : -0.45f);
        const float avatarYaw = point.yaw+localFacing;
        const float rootY = point.y+kPlatformSurfaceElevation
            +(seated ? kPlatformBenchSeatHeight-kPlatformSeatedRootDrop : 0.01f);
        const Matrix world = Matrix::CreateScale(seated ? 0.90f : 0.94f)
                           * Matrix::CreateRotationY(avatarYaw)
                           * Matrix::CreateTranslation(point.x,rootY,point.z);
        const float shade=canopyShadeAt(canopyShadow,
            static_cast<float>(figure.lateral-5.0),static_cast<float>(figure.offset));
        if (graphicsQuality_ >= 1 && !seated)
        {
            effect_->setLightingEnabledProperty(false);
            effect_->setDiffuseColorProperty(Vector3::One);
            device_.setBlendStateProperty(BlendState::NonPremultiplied);
            device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
            drawMesh(*contactShadow_,Matrix::CreateScale(0.32f,1.0f,0.23f)
                * Matrix::CreateRotationY(point.yaw)
                * Matrix::CreateTranslation(point.x,point.y+kPlatformSurfaceElevation+0.006f,point.z),
                view,projection,stats);
            device_.setBlendStateProperty(BlendState::Opaque);
            device_.setDepthStencilStateProperty(DepthStencilState::Default);
            effect_->setLightingEnabledProperty(true);
        }
        visual.renderer->setAmbientLightColorProperty(platformLight.ambient);
        visual.renderer->setLightDirectionProperty(platformKeyLight);
        visual.renderer->setLightColorProperty(platformLight.diffuse*(1.0f-0.88f*shade));
        visual.renderer->setWorldProperty(world);
        visual.renderer->setViewProperty(view);
        visual.renderer->setProjectionProperty(projection);
        const auto source = visual.animation->getBoneTransformsProperty();
        auto& bones = visual.boneScratch;
        bones.assign(source.begin(),source.end());
        if (seated)
        {
            applySeatedRestPose(bones);
            ++stats.platformSeatedAvatars;
        }
        else if (figure.pose == PlatformExtraPose::ReadingTimetable)
        {
            const auto head = static_cast<std::size_t>(AvatarBone::Head);
            bones[head] = rotatedBone(bones[head],Matrix::CreateRotationX(0.16f));
        }
        applyPassengerProportions(bones);
        visual.renderer->Draw(bones,visual.animation->getExpressionProperty());
        ++drawn;
        if (figure.pose == PlatformExtraPose::WithSuitcase)
        {
            // The traveller's own case stands at their side, handle up, its broad face parallel
            // to their body. Placing it in the figure's frame keeps the hanging hand and the
            // turned stance clear of the shell for either facing.
            const float caseSide = k%4U == 1U ? 1.0f : -1.0f;
            const Vector3 caseOffset = Vector3::Transform(Vector3{caseSide*0.43f,0.0f,0.05f},
                                                          Matrix::CreateRotationY(avatarYaw));
            const float luggageShade=canopyShadeAt(luggageShadow,
                static_cast<float>(figure.lateral-5.0),static_cast<float>(figure.offset));
            effect_->setAmbientLightColorProperty(platformLight.ambient);
            effect_->DirectionalLight0.setDiffuseColorProperty(
                platformLight.diffuse*(1.0f-0.88f*luggageShade));
            effect_->setDiffuseColorProperty(Vector3::One);
            drawMesh(*platformLuggage_,Matrix::CreateRotationY(avatarYaw+kPi*0.5f)
                * Matrix::CreateTranslation(point.x+caseOffset.X,
                    point.y+kPlatformSurfaceElevation+0.01f,point.z+caseOffset.Z),
                view,projection,stats);
            ++stats.platformLuggageItems;
            ++stats.platformSuitcaseTravellers;
        }
    }
    const auto restoredLight=exteriorLighting(tunnelLightingMix_,atmosphere_.light);
    effect_->setAmbientLightColorProperty(restoredLight.ambient);
    effect_->DirectionalLight0.setDiffuseColorProperty(restoredLight.diffuse);
    stats.drawCalls += drawn;
    stats.visiblePassengers += drawn;
}


} // namespace TrainSim::Render
