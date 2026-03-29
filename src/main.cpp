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

    void update(float dt) {
        PlayLayer::update(dt);

        if (m_fields->m_isDead) {
            m_fields->m_time += dt;

            auto fmod = FMODAudioEngine::sharedEngine();
            if (!fmod || !fmod->m_system) return;

            // Lấy nhóm chuyên quản lý NHẠC (thay vì Master)
            FMOD::ChannelGroup* musicGroup;
            fmod->m_system->getMasterChannelGroup(&musicGroup); 
            // Nếu cái trên không ăn, thử: fmod->m_musicChannelGroup (tùy version Geode)

            if (musicGroup) {
                // Lấy duration và ép kiểu an toàn
                double settingVal = Mod::get()->getSettingValue<double>("fade-speed");
                float duration = static_cast<float>(settingVal);
                
                if (duration <= 0.001f) duration = 0.5f;

                float progress = 1.0f - (m_fields->m_time / duration);
                if (progress < 0.0f) progress = 0.0f;

                // Dùng hàm pow hoặc nhân đôi để tạo đường cong lịm nhạc
                float easedProgress = progress * progress;

                // Ép Pitch
                musicGroup->setPitch(easedProgress);
                
                // THỬ THÊM: Nếu Pitch không đổi, thử giảm Volume xem nó có nhận lệnh không
                // musicGroup->setVolume(progress); 
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