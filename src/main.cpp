#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer)
{
    struct Fields
    {
        bool m_isDead = false;
        float m_time = 0.0f;
    };

    bool init(GJGameLevel *level, bool useReplay, bool dontRun)
    {
        if (!PlayLayer::init(level, useReplay, dontRun))
            return false;
        this->scheduleUpdate();
        return true;
    }

    void update(float dt)
    {
        PlayLayer::update(dt);

        if (m_fields->m_isDead)
        {
            m_fields->m_time += dt;

            auto fmod = FMODAudioEngine::sharedEngine();
            if (!fmod || !fmod->m_system)
                return;

            FMOD::ChannelGroup *masterGroup;
            fmod->m_system->getMasterChannelGroup(&masterGroup);

            if (masterGroup)
            {
                float duration = static_cast<float>(Mod::get()->getSettingValue<double>("fade-speed"));
                if (duration <= 0.f)
                    duration = 0.5f; // Chống chia cho 0

                float progress = 1.0f - (m_fields->m_time / duration);
                if (progress < 0.0f)
                    progress = 0.0f;

                // Ease curve: Giúp nhạc lịm dần tự nhiên hơn
                float easedProgress = progress * progress;

                // Áp dụng lên MasterGroup để lịm TOÀN BỘ game
                masterGroup->setPitch(easedProgress);
            }
        }
    }

    void destroyPlayer(PlayerObject *player, GameObject *obj)
    {
        PlayLayer::destroyPlayer(player, obj);
        if (!m_fields->m_isDead)
        {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
        }
    }

    void resetLevel()
    {
        // Reset Pitch về 1.0 ngay lập tức trước khi reset màn
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod)
        {
            FMOD::ChannelGroup *masterGroup;
            fmod->m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup)
            {
                masterGroup->setPitch(1.0f);
            }
        }

        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;

        PlayLayer::resetLevel();
    }
};