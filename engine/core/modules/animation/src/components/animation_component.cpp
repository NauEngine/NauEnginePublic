// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/animation/components/animation_component.h"

#include "nau/animation/animation_manager.h"
#include "nau/animation/playback/animation_instance.h"
#include "nau/animation/playback/animation_mixer.h"
#include "nau/utils/enum/enum_reflection.h"
#include "nau/scene/scene_object.h"
#include "../animation_helper.h"

namespace nau::animation
{
    NAU_IMPLEMENT_DYNAMIC_OBJECT(AnimationComponent)

    class AnimatableObjectTargetWrapper final : public IAnimationTarget
    {
        NAU_CLASS(nau::animation::AnimatableObjectTargetWrapper, rtti::RCPolicy::StrictSingleThread, IAnimationTarget)

    public:
        AnimatableObjectTargetWrapper(scene::ObjectWeakRef<scene::NauObject> targetObject)
            : m_target(targetObject)
        {
        }

        virtual void* getTarget(const rtti::TypeInfo& requestedTarget, IAnimationPlayer* player) override
        {
            if (auto* target = m_target.get())
            {
                if (IAnimationTarget* animTarget = target->as<IAnimationTarget*>())
                {
                    return animTarget->getTarget(requestedTarget, player);
                }
            }

            return nullptr;
        }

    private:
        scene::ObjectWeakRef<scene::NauObject> m_target;
    };

    AnimationComponent::AnimationComponent()
    {
        m_name = "Animation Component";
    }

    AnimationComponent::~AnimationComponent() = default;

    void AnimationComponent::onAfterComponentRestored()
    {
        auto animationController = AnimationHelper::createAnimationController(m_animControllerType);
        setController(animationController);

        for (const AnimTrackCreationInfo& trackCI : m_tracksCreationInfo)
        {
            auto animInstance = rtti::createInstance<animation::AnimationInstance>(fmt::format("{}.{}", trackCI.animationName, trackCI.channelTargetPath).c_str(), trackCI.animationAsset);
            
            const std::string_view stdStringViewPlayMode(trackCI.playMode.c_str()); 
            auto playModeParseResult = EnumTraits<nau::animation::PlayMode>::parse(stdStringViewPlayMode);
            if (!playModeParseResult.isError())
            {
                animInstance->setPlayMode(*playModeParseResult);
            }

            if (trackCI.blendMethod == "mix")
            {
                animInstance->setBlendMethod(animation::AnimationBlendMethod::Mix);
            }
            else if (trackCI.blendMethod == "additive")
            {
                animInstance->setBlendMethod(animation::AnimationBlendMethod::Additive);
            }
            else
            {
                // valid case for keyframe animations
            }

            animInstance->setWeight(trackCI.initialWeight);

            getOrCreateController()->addAnimation(animInstance);
        }
    }

    void AnimationComponent::updateComponent(float dt)
    {
        if (auto* controller = m_controller.get())
        {
            m_frameTransform = m_rootTransform;

            controller->update(dt, this);

            applyTransform();
        }
    }

    async::Task<> AnimationComponent::activateComponentAsync()
    {
        m_rootTransform = getParentObject().getWorldTransform();

        if (auto* controller = m_controller.get())
        {
            co_await controller->load();
        }

        if (auto* animManager = AnimationManager::get(this))
        {
            animManager->registerAnimationComponent(this);
        }
    }

    void AnimationComponent::deactivateComponent()
    {
        if (auto* animManager = AnimationManager::get(this))
        {
            animManager->unregisterAnimationComponent(this);
        }
    }

    void AnimationComponent::animateTransform(const nau::math::Transform& transform)
    {
        m_pendingTransforms.set(TransformAnimationActions::Translation, TransformAnimationActions::Rotation, TransformAnimationActions::Scale);
        m_frameTransform = m_frameTransform * transform;
    }

    void AnimationComponent::animateTranslation(const math::vec3& translation)
    {
        m_pendingTransforms |= TransformAnimationActions::Translation;
        m_frameTransform.addTranslation(translation);
    }

    void AnimationComponent::animateRotation(const math::quat& rotation)
    {
        m_pendingTransforms |= TransformAnimationActions::Rotation;
        m_frameTransform.addRotation(rotation);
    }

    void AnimationComponent::animateScale(const math::vec3& scale)
    {
        m_pendingTransforms |= TransformAnimationActions::Scale;
        m_frameTransform.addScale(scale);
    }

    void* AnimationComponent::getTarget(const rtti::TypeInfo& requestedTarget, IAnimationPlayer* player)
    {
        for (auto& targetData : m_targets)
        {
            if (targetData.target && (!targetData.player || targetData.player == player))
            {
                if (auto* usableTarget = targetData.target->getTarget(requestedTarget, player))
                {
                    return usableTarget;
                }
            }
        }

        return ITransformAnimatable::getTarget(requestedTarget, player);
    }

    scene::SceneObject* AnimationComponent::getOwner()
    {
        return &getParentObject();
    }

    void AnimationComponent::addAnimation(nau::Ptr<AnimationInstance> animation)
    {
        getOrCreateController()->addAnimation(animation);

        updateTrackSerializationInfo(animation);
    }

    void AnimationComponent::addAnimationTarget(IAnimationTarget::Ptr target)
    {
        if (target)
        {
            if (auto* nauObject = target->as<scene::NauObject*>())
            {
                auto wrapper = rtti::createInstance<AnimatableObjectTargetWrapper>(nauObject);
                m_targets.emplace_back(std::move(wrapper), nullptr);
            }
            else
            {
                m_targets.emplace_back(std::move(target), nullptr);
            }
        }
    }

    void AnimationComponent::addCustomAnimationTarget(IAnimationTarget::Ptr target, IAnimationPlayer::Ptr player)
    {
        if (target)
        {
            if (auto* nauObject = target->as<scene::NauObject*>())
            {
                auto wrapper = rtti::createInstance<AnimatableObjectTargetWrapper>(nauObject);
                m_targets.push_back({ std::move(wrapper), std::move(player) });
            }
            else
            {
                m_targets.push_back({ std::move(target), std::move(player) });
            }
        }
    }

    void AnimationComponent::setController(nau::Ptr<AnimationController> controller)
    {
        m_controller.reset();
        m_controller = controller;

        m_animControllerType = controller ? controller->getControllerTypeName() : "";
    }

    AnimationController* AnimationComponent::getController() const
    {
        return m_controller.get();
    }

    AnimationController* AnimationComponent::getOrCreateController()
    {
        if (!m_controller)
        {
            NAU_LOG_DEBUG("Creating default direct animation controller: {}", m_name);
            auto controller = AnimationHelper::createAnimationController("direct");
            m_controller = controller;
        }

        return m_controller.get();
    }

    const eastl::string& AnimationComponent::getName() const
    {
        return m_name;
    }

    void AnimationComponent::applyTransform()
    {
        if (m_pendingTransforms.has(TransformAnimationActions::Translation, TransformAnimationActions::Rotation, TransformAnimationActions::Scale))
        {
            getParentObject().setTransform(m_frameTransform);
        }
        else
        {
            if (m_pendingTransforms.has(TransformAnimationActions::Translation))
            {
                getParentObject().setTranslation(m_frameTransform.getTranslation());
            }
            if (m_pendingTransforms.has(TransformAnimationActions::Rotation))
            {
                getParentObject().setRotation(m_frameTransform.getRotation());
            }
            if (m_pendingTransforms.has(TransformAnimationActions::Scale))
            {
                getParentObject().setScale(m_frameTransform.getScale());
            }
        }

        m_pendingTransforms.clear();
    }

    void AnimationComponent::updateTrackSerializationInfo(nau::Ptr<AnimationInstance> animInstancePtr)
    {
        if (auto* animInstance = animInstancePtr.get())
        {
            AnimTrackCreationInfo* creationInfoIt = eastl::find_if(
                m_tracksCreationInfo.begin(),
                m_tracksCreationInfo.end(),
                [animInstance](auto& it)
                {
                    return it.owningInstance.acquire().get() == animInstance;
                });

            if (creationInfoIt == m_tracksCreationInfo.end())
            {
                creationInfoIt = &m_tracksCreationInfo.emplace_back();
                creationInfoIt->owningInstance = animInstance;
            }

            creationInfoIt->animationName = animInstance->getName();
            auto playModeValue = EnumTraits<nau::animation::PlayMode>::toString(animInstance->getPlayMode());
            creationInfoIt->playMode = eastl::string { playModeValue.data(), playModeValue.size() };
            creationInfoIt->initialWeight = animInstance->getWeight();
            creationInfoIt->channelTargetPath = "";
            switch (animInstance->getBlendMethod())
            {
            case AnimationBlendMethod::Mix:
                creationInfoIt->blendMethod = "mix";
                break;
            case AnimationBlendMethod::Additive:
                creationInfoIt->blendMethod = "additive";
                break;
            }
            creationInfoIt->animationAsset = animInstance->getAssetRef();
        }
    }

}  // namespace nau::animation
