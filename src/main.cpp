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
            if (fmod && fmod->m_system)
            {
                // Lấy duration an toàn
                double settingVal = Mod::get()->getSettingValue<double>("fade-speed");
                float duration = static_cast<float>(settingVal);
                if (duration <= 0.001f)
                    duration = 0.5f;

                float progress = 1.0f - (m_fields->m_time / duration);
                if (progress < 0.0f)
                    progress = 0.0f;

                // Lấy Master Group của hệ thống FMOD
                FMOD::ChannelGroup *masterGroup;
                fmod->m_system->getMasterChannelGroup(&masterGroup);

                if (masterGroup)
                {
                    // Ép Pitch lên toàn bộ âm thanh đầu ra
                    masterGroup->setPitch(progress * progress);

                    // Ghi log để Mèo check trong Console (Phím ~)
                    // Nếu hiện dòng này mà nhạc không méo -> Có mod khác đang tranh chấp MasterGroup
                    log::info("Osu Pitch: {}", progress * progress);
                }
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